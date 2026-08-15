/*
 * ws73-probe — Phase-1 user-space validator for the HiSilicon WS73 NearLink
 * USB dongle (VID:PID ffff:3733, product string "00000000").
 *
 * libusb-based implementation of the boot-stage firmware download handshake
 * (wire spec derived from the WS73 Linux SDK — see repo docs/USB-PROTOCOL.md
 * and wayfinder ticket 01). Runs entirely in user space, never touches a
 * kernel driver.
 *
 * Modes:
 *   probe (default)        enumerate + report device state, classify boot vs
 *                          kernel config. NEVER writes to the device.
 *   --fw=<path>@<addr>     run the boot handshake: [READM conn-check]
 *                          WRITEM <trim>, then per --fw file a FILES chunked
 *                          download (64-byte SHA-256 header stripped, ≤32 KB
 *                          chunks), then QUIT and wait ≤5 s for the device
 *                          to re-enumerate into the 5-EP kernel config.
 *                          May be given multiple times; addr is hex, e.g.
 *                          --fw=ws73.bin@0x400000.
 *
 * Options:
 *   --trim <hex>           CMU XO trim written by the first WRITEM
 *                          (default 0x83c = SDK CONFIG_INI_CMU_XO_TRIM).
 *   --skip-readm           skip the optional READM connection check.
 *   --skip-writem          skip the WRITEM trim step.
 *   --skip-verify          skip the SHA-256 header check of firmware files.
 *   --wait-ms <n>          re-enumeration wait budget after QUIT (default 5000).
 *   --verbose              hex-dump every TX command and RX reply.
 *
 * Exit codes: 0 ok · 1 device not found/unsupported · 2 handshake failed ·
 *             3 usage error.
 *
 * Build: make
 */

#include <errno.h>
#include <inttypes.h>
#include <libusb-1.0/libusb.h>
#include <openssl/sha.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define WS73_VID       0xffff
#define WS73_PID       0x3733
#define ALT_VID        0x12d1
#define ALT_PID        0x897d

#define MAX_FW_BUF     (32 * 1024)              /* SDK MAX_FIRMWARE_FILE_TX_BUF_LEN */
#define SHA_HEADER_LEN 64
#define CMD_BUF_LEN    512
#define RECV_BUF_LEN   512
#define WRITE_TIMEOUT  30000                     /* USB_DOWNLOAD_FW_TIMEOUT */
#define READ_TIMEOUT   2000                      /* READ_MEG_TIMEOUT */
#define RETRIES        3                         /* HOST_DEV_TIMEOUT */
#define CHUNK_PAUSE_US 5000                      /* FILE_CMD_WAIT_TIME_MIN */

#define MAX_FW_FILES   8

struct opts {
    const char *fw[MAX_FW_FILES]; /* "path@addr" */
    unsigned    nfw;
    unsigned    trim;
    int         skip_readm;
    int         skip_writem;
    int         skip_verify;
    int         wait_ms;
    int         verbose;
    int         readm_only;      /* --readm-only: READM conn-check only, no writes */
    int         sel_set;         /* --port <bus>-<port> */
    int         sel_bus;
    int         sel_port;
};

static int fread_sha_header(const char *path, char out[SHA_HEADER_LEN + 1]);

/* ---------------------------------------------------------------- helpers */

static void die(const char *msg)
{
    fprintf(stderr, "ws73-probe: %s\n", msg);
    exit(2);
}

static void hexdump(const char *tag, const unsigned char *b, size_t n)
{
    size_t i;
    printf("  %-8s[%zu] ", tag, n);
    for (i = 0; i < n; i++)
        printf("%02x", b[i]);
    printf("\n");
}

static int bulk_write(libusb_device_handle *h, unsigned char ep,
                      const void *buf, int len)
{
    int xfer = 0;
    int rc = libusb_bulk_transfer(h, ep, (unsigned char *)buf, len, &xfer,
                                  WRITE_TIMEOUT);
    if (rc != 0)
        fprintf(stderr, "  bulk-out EP 0x%02x failed: %s\n", ep,
                libusb_error_name(rc));
    return rc;
}

static int bulk_read(libusb_device_handle *h, unsigned char ep,
                     unsigned char *buf, int max, int *got)
{
    int rc = libusb_bulk_transfer(h, ep, buf, max, got, READ_TIMEOUT);
    if (rc != 0)
        fprintf(stderr, "  bulk-in EP 0x%02x failed: %s\n", ep,
                libusb_error_name(rc));
    return rc;
}

/* Send one command (with trailing 0x20) and expect an ASCII prefix reply. */
static int cmd_expect(libusb_device_handle *h, unsigned char ep_out,
                      unsigned char ep_in, const char *cmd, const char *expect,
                      int verbose)
{
    char line[CMD_BUF_LEN];
    int len, retry;

    for (retry = 0; retry < RETRIES; retry++) {
        len = snprintf(line, sizeof(line), "%s ", cmd); /* trailing 0x20 */
        if (verbose)
            hexdump("TX", (unsigned char *)line, (size_t)len);
        if (bulk_write(h, ep_out, line, len) != 0)
            return -1;
        if (expect[0] == '\0')
            return 0;                       /* e.g. QUIT: no reply */

        {
            unsigned char r[RECV_BUF_LEN];
            int got = 0;
            if (bulk_read(h, ep_in, r, (int)sizeof(r), &got) != 0)
                return -1;
            r[got < (int)sizeof(r) ? got : (int)sizeof(r) - 1] = 0;
            if (verbose)
                hexdump("RX", r, (size_t)got);
            if ((size_t)got >= strlen(expect) &&
                memcmp(r, expect, strlen(expect)) == 0)
                return 0;
            fprintf(stderr, "  (retry %d) reply not '%s', got %d bytes\n",
                    retry + 1, expect, got);
        }
    }
    return -1;
}

/* Send one command and read `want` raw bytes back (e.g. READM register). */
static int cmd_expect_raw(libusb_device_handle *h, unsigned char ep_out,
                          unsigned char ep_in, const char *cmd,
                          unsigned char *out, int want, int verbose)
{
    char line[CMD_BUF_LEN];
    int len = snprintf(line, sizeof(line), "%s ", cmd);
    unsigned char r[RECV_BUF_LEN];
    int got = 0, retry;

    for (retry = 0; retry < RETRIES; retry++) {
        if (verbose)
            hexdump("TX", (unsigned char *)line, (size_t)len);
        if (bulk_write(h, ep_out, line, len) != 0)
            return -1;
        got = 0;
        if (bulk_read(h, ep_in, r, want > (int)sizeof(r) ? (int)sizeof(r) : want,
                      &got) != 0)
            return -1;
        if (got == want) {
            if (verbose)
                hexdump("RX", r, (size_t)got);
            memcpy(out, r, (size_t)want);
            return 0;
        }
        fprintf(stderr, "  (retry %d) expected %d raw bytes, got %d\n",
                retry + 1, want, got);
    }
    return -1;
}

/* ------------------------------------------------------------- firmware */

static void sha256_hex(const unsigned char *data, size_t len, char out[65])
{
    unsigned char d[SHA256_DIGEST_LENGTH];
    int i;
    SHA256(data, len, d);
    for (i = 0; i < (int)SHA256_DIGEST_LENGTH; i++)
        sprintf(out + 2 * i, "%02x", d[i]);
    out[64] = 0;
}

static int fread_sha_header(const char *path, char out[SHA_HEADER_LEN + 1])
{
    FILE *f = fopen(path, "rb");
    size_t n;
    if (!f)
        return -1;
    n = fread(out, 1, SHA_HEADER_LEN, f);
    fclose(f);
    out[n] = 0;
    return (n == SHA_HEADER_LEN) ? 0 : -1;
}

static int send_file(libusb_device_handle *h, unsigned char ep_out,
                     unsigned char ep_in, const char *path,
                     unsigned long addr, const struct opts *o)
{
    FILE *f;
    unsigned char *buf;
    long filesz, content_len;
    unsigned long cur = addr;
    size_t off = 0;
    int chunks, idx;

    f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "  cannot open %s: %s\n", path, strerror(errno));
        return -1;
    }
    if (fseek(f, 0, SEEK_END) != 0 || (filesz = ftell(f)) < 0 ||
        fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return -1;
    }
    if (filesz <= (long)SHA_HEADER_LEN) {
        fprintf(stderr, "  %s too small (%ld B)\n", path, filesz);
        fclose(f);
        return -1;
    }
    content_len = filesz - SHA_HEADER_LEN;

    buf = malloc((size_t)content_len);
    if (!buf) {
        fclose(f);
        die("out of memory");
    }
    if (fread(buf, 1, (size_t)content_len, f) != (size_t)content_len) {
        fprintf(stderr, "  short read on %s\n", path);
        free(buf);
        fclose(f);
        return -1;
    }
    fclose(f);

    /* host-side SHA-256 check: header == sha256(content); header never sent */
    if (!o->skip_verify) {
        char head[SHA_HEADER_LEN + 1], calc[65];
        if (fread_sha_header(path, head)) {
            fprintf(stderr, "  %s: cannot read 64-byte SHA-256 header\n", path);
            free(buf);
            return -1;
        }
        sha256_hex(buf, (size_t)content_len, calc);
        if (strncmp(head, calc, SHA_HEADER_LEN) != 0) {
            fprintf(stderr,
                    "  %s: SHA-256 mismatch (%.16s… != sha256 %.16s…); "
                    "--skip-verify to force\n",
                    path, head, calc);
            free(buf);
            return -1;
        }
        printf("  sha256 ok for %s (%ld B content)\n", path, content_len);
    } else {
        printf("  sha256 check skipped for %s\n", path);
    }

    chunks = (int)((content_len + MAX_FW_BUF - 1) / MAX_FW_BUF);
    for (idx = 0; idx < chunks; idx++) {
        int thislen = (int)((content_len - (long)off) > MAX_FW_BUF
                                ? MAX_FW_BUF
                                : (content_len - (long)off));
        unsigned state = (chunks == 1)        ? 3
                       : (idx == 0)           ? 0
                       : (idx == chunks - 1)  ? 2
                                              : 1;
        char cmd[CMD_BUF_LEN];
        unsigned char reply[RECV_BUF_LEN];
        int got = 0;

        snprintf(cmd, sizeof(cmd), "FILES 1 0x%lx 0x%x 0x%x",
                 cur, (unsigned)thislen, state);
        if (cmd_expect(h, ep_out, ep_in, cmd, "READY", o->verbose) != 0) {
            fprintf(stderr, "  chunk %d/%d: FILES not acked (READY)\n",
                    idx + 1, chunks);
            free(buf);
            return -1;
        }
        usleep(CHUNK_PAUSE_US);                       /* >= 5 ms */
        if (bulk_write(h, ep_out, buf + off, thislen) != 0) {
            free(buf);
            return -1;
        }
        if (bulk_read(h, ep_in, reply, (int)sizeof(reply), &got) != 0 ||
            (size_t)got < strlen("FILES OK") ||
            memcmp(reply, "FILES OK", strlen("FILES OK")) != 0) {
            fprintf(stderr, "  chunk %d/%d: data not acked (FILES OK)\n",
                    idx + 1, chunks);
            free(buf);
            return -1;
        }
        printf("  chunk %d/%d: 0x%x B @ 0x%lx state 0x%x ok\n",
               idx + 1, chunks, thislen, cur, state);
        off += (size_t)thislen;
        cur += (unsigned long)thislen;
    }
    free(buf);
    return 0;
}

/* ------------------------------------------------------------ device i/o */

struct devinfo {
    unsigned char ep_in, ep_out;      /* boot: bulk in/out (index 0/1) */
    unsigned char int_in, reg_out, reg_in; /* kernel: indices 2/3/4 */
    unsigned char bNumEndpoints;
};

static void print_descriptor(libusb_device *dev)
{
    struct libusb_device_descriptor dd;
    struct libusb_config_descriptor *cfg;
    int i;

    if (libusb_get_device_descriptor(dev, &dd) != 0)
        return;
    printf("device     %04x:%04x bcdDevice %04x bcdUSB %04x class %02x\n",
           dd.idVendor, dd.idProduct, dd.bcdDevice, dd.bcdUSB,
           dd.bDeviceClass);
    if (libusb_get_active_config_descriptor(dev, &cfg) == 0) {
        printf("config     bNumInterfaces %u bmAttributes %02x MaxPower %umA\n",
               cfg->bNumInterfaces, cfg->bmAttributes, cfg->MaxPower);
        for (i = 0; i < (int)cfg->bNumInterfaces; i++) {
            const struct libusb_interface_descriptor *a =
                &cfg->interface[i].altsetting[0];
            int j;
            printf("interface  %u class %02x/%02x/%02x eps %u\n",
                   a->bInterfaceNumber, a->bInterfaceClass,
                   a->bInterfaceSubClass, a->bInterfaceProtocol,
                   a->bNumEndpoints);
            for (j = 0; j < (int)a->bNumEndpoints; j++) {
                const struct libusb_endpoint_descriptor *e = &a->endpoint[j];
                const char *t =
                    (e->bmAttributes & 0x03) == LIBUSB_TRANSFER_TYPE_BULK
                        ? "bulk"
                        : (e->bmAttributes & 0x03) ==
                                  LIBUSB_TRANSFER_TYPE_INTERRUPT
                              ? "int "
                              : "?   ";
                printf("  ep %02x %s %uB %s\n", e->bEndpointAddress, t,
                       e->wMaxPacketSize,
                       (e->bEndpointAddress & 0x80) ? "IN " : "OUT");
            }
        }
        libusb_free_config_descriptor(cfg);
    }
}

/* Find our device; returns a device with refcount +1 (caller frees).
 * When sel_set, match the given bus/port (e.g. --port 1-4); otherwise the
 * first matching device wins. */
static int find_device(libusb_context *ctx, libusb_device **out,
                       int sel_set, int sel_bus, int sel_port)
{
    libusb_device **list;
    ssize_t cnt, i;

    cnt = libusb_get_device_list(ctx, &list);
    if (cnt < 0) {
        fprintf(stderr, "libusb_get_device_list: %s\n",
                libusb_error_name((int)cnt));
        return -1;
    }
    for (i = 0; i < cnt; i++) {
        struct libusb_device_descriptor dd;
        if (libusb_get_device_descriptor(list[i], &dd) != 0)
            continue;
        if ((dd.idVendor == WS73_VID && dd.idProduct == WS73_PID) ||
            (dd.idVendor == ALT_VID && dd.idProduct == ALT_PID)) {
            if (sel_set &&
                (libusb_get_bus_number(list[i]) != sel_bus ||
                 libusb_get_port_number(list[i]) != sel_port))
                continue;
            libusb_ref_device(list[i]);
            *out = list[i];
            libusb_free_device_list(list, 1);
            return 0;
        }
    }
    libusb_free_device_list(list, 1);
    return -1;
}

static int classify(libusb_device *dev, struct devinfo *di)
{
    struct libusb_config_descriptor *cfg;
    const struct libusb_interface_descriptor *a;
    int j;

    memset(di, 0, sizeof(*di));
    if (libusb_get_active_config_descriptor(dev, &cfg) != 0)
        return -1;
    if (cfg->bNumInterfaces < 1) {
        libusb_free_config_descriptor(cfg);
        return -1;
    }
    a = &cfg->interface[0].altsetting[0];
    di->bNumEndpoints = a->bNumEndpoints;
    for (j = 0; j < (int)a->bNumEndpoints; j++) {
        unsigned char addr = a->endpoint[j].bEndpointAddress;
        switch (j) {                    /* role by index, per SDK */
        case 0: di->ep_in = addr; break;
        case 1: di->ep_out = addr; break;
        case 2: di->int_in = addr; break;
        case 3: di->reg_out = addr; break;
        case 4: di->reg_in = addr; break;
        default: break;
        }
    }
    libusb_free_config_descriptor(cfg);
    return 0;
}

/* ---------------------------------------------------------------- main */

int main(int argc, char **argv)
{
    struct opts o = {.trim = 0x83c, .wait_ms = 5000};
    libusb_context *ctx = NULL;
    libusb_device *dev = NULL;
    libusb_device_handle *h = NULL;
    struct devinfo di;
    int i, rc = 1;
    int flash = 0;
    const char *mode = "probe";

    for (i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--fw=", 5) == 0) {
            if (o.nfw >= MAX_FW_FILES)
                die("too many --fw");
            o.fw[o.nfw++] = argv[i] + 5;
        } else if (strcmp(argv[i], "--trim") == 0 && i + 1 < argc) {
            o.trim = (unsigned)strtoul(argv[++i], NULL, 16);
        } else if (strcmp(argv[i], "--skip-readm") == 0) {
            o.skip_readm = 1;
        } else if (strcmp(argv[i], "--skip-writem") == 0) {
            o.skip_writem = 1;
        } else if (strcmp(argv[i], "--skip-verify") == 0) {
            o.skip_verify = 1;
        } else if (strcmp(argv[i], "--readm-only") == 0) {
            o.readm_only = 1;
        } else if (strcmp(argv[i], "--wait-ms") == 0 && i + 1 < argc) {
            o.wait_ms = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            const char *p = argv[++i];
            char *dash;
            o.sel_bus = atoi(p);
            dash = strchr(p, '-');
            if (!dash) {
                fprintf(stderr, "--port expects <bus>-<port>, e.g. --port 1-4\n");
                return 3;
            }
            o.sel_port = atoi(dash + 1);
            o.sel_set = 1;
        } else if (strcmp(argv[i], "--verbose") == 0) {
            o.verbose = 1;
        } else {
            fprintf(stderr,
                    "usage: %s [probe] [--fw=path@addr]... [--trim hex] "
                    "[--port <bus>-<port>] [--readm-only] [--skip-readm] "
                    "[--skip-writem] [--skip-verify] [--wait-ms n] [--verbose]\n",
                    argv[0]);
            return 3;
        }
    }
    if (o.nfw || o.readm_only)
        flash = 1;

    if (libusb_init(&ctx) != 0)
        die("libusb_init failed");

    if (find_device(ctx, &dev, o.sel_set, o.sel_bus, o.sel_port) != 0) {
        fprintf(stderr,
                "ws73-probe: no ffff:3733 (or 12d1:897d) device found%s\n",
                o.sel_set ? " at selected port" : "");
        goto out;
    }
    if (libusb_open(dev, &h) != 0) {
        fprintf(stderr,
                "ws73-probe: cannot open device (need root or udev rules)\n");
        goto out;
    }
    if (libusb_kernel_driver_active(h, 0) == 1) {
        printf("detaching kernel driver on interface 0\n");
        libusb_detach_kernel_driver(h, 0);
    }
    if (libusb_claim_interface(h, 0) != 0) {
        fprintf(stderr, "ws73-probe: claim_interface(0) failed\n");
        goto out;
    }

    print_descriptor(dev);
    if (classify(dev, &di) != 0)
        die("cannot read interface descriptor");
    mode = flash ? "flash" : "probe";
    printf("mode       %s (bNumEndpoints %u)\n", mode, di.bNumEndpoints);
    printf("eps        IN 0x%02x OUT 0x%02x", di.ep_in, di.ep_out);
    if (di.int_in)
        printf(" INT 0x%02x", di.int_in);
    if (di.reg_out && di.reg_in)
        printf(" REG 0x%02x/0x%02x", di.reg_out, di.reg_in);
    printf("\n");

    if (!flash) {
        printf("\ndry-run: no writes performed. "
               "Add --fw=ws73.bin@0x400000 to run the boot handshake.\n");
        rc = 0;
        goto out;
    }

    /* ---- boot handshake ---- */
    printf("\n== boot handshake ==\n");
    if (!o.skip_readm) {
        unsigned char reg[4];
        if (cmd_expect_raw(h, di.ep_out, di.ep_in, "READM 0x40019380 4",
                           reg, 4, o.verbose) == 0)
            printf("  READM conn-check ok, reg=%02x%02x%02x%02x\n",
                   reg[0], reg[1], reg[2], reg[3]);
        else {
            fprintf(stderr,
                    "  READM conn-check failed; device may not be in boot "
                    "state (use --skip-readm to continue)\n");
            rc = 2;
            goto out;
        }
    }
    if (o.readm_only) {
        printf("\nreadm-only: no writes performed.\n");
        rc = 0;
        goto out;
    }
    if (!o.skip_writem) {
        char c[CMD_BUF_LEN];
        snprintf(c, sizeof(c), "WRITEM 4 0x40019408 0x%04x", o.trim);
        if (cmd_expect(h, di.ep_out, di.ep_in, c, "WRITEM OK", o.verbose) != 0) {
            fprintf(stderr, "  WRITEM trim failed\n");
            rc = 2;
            goto out;
        }
        printf("  WRITEM trim 0x%04x ok\n", o.trim);
    }

    for (i = 0; i < (int)o.nfw; i++) {
        const char *at = strrchr(o.fw[i], '@');
        char path[512];
        unsigned long addr;
        if (!at) {
            fprintf(stderr, "  --fw=%s needs @addr (e.g. @0x400000)\n", o.fw[i]);
            rc = 3;
            goto out;
        }
        snprintf(path, sizeof(path), "%.*s", (int)(at - o.fw[i]), o.fw[i]);
        addr = strtoul(at + 1, NULL, 16);
        printf("  downloading %s -> 0x%lx\n", path, addr);
        if (send_file(h, di.ep_out, di.ep_in, path, addr, &o) != 0) {
            rc = 2;
            goto out;
        }
    }

    printf("  QUIT\n");
    if (cmd_expect(h, di.ep_out, di.ep_in, "QUIT", "", o.verbose) != 0) {
        fprintf(stderr, "  QUIT send failed\n");
        rc = 2;
        goto out;
    }

    /* ---- wait for re-enumeration into kernel config ---- */
    {
        int waited = 0, ok = 0;
        printf("\nwaiting up to %d ms for re-enumeration...\n", o.wait_ms);
        while (waited < o.wait_ms) {
            libusb_device *d2 = NULL;
            struct devinfo di2;
            usleep(100000);
            waited += 100;
            if (find_device(ctx, &d2, o.sel_set, o.sel_bus, o.sel_port) != 0)
                continue;
            if (classify(d2, &di2) == 0 && di2.bNumEndpoints == 5) {
                printf("  re-enumerated: 5 endpoints (kernel mode) OK\n");
                ok = 1;
            }
            libusb_unref_device(d2);
            if (ok)
                break;
        }
        if (!ok) {
            fprintf(stderr, "  re-enumeration timeout (%d ms)\n", o.wait_ms);
            rc = 2;
            goto out;
        }
    }

    rc = 0;
    printf("\nDONE: firmware handshake completed.\n");

out:
    if (h)
        libusb_close(h);
    if (dev)
        libusb_unref_device(dev);
    if (ctx)
        libusb_exit(ctx);
    return rc;
}
