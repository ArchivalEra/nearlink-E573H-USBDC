/*
 * kernel-observe — passive listener for a WS73 in kernel (5-EP) mode.
 *
 * Zero-protocol: just opens the device and watches the interrupt-IN and
 * bulk-IN pipes for anything the device pushes unsolicited. No writes.
 *
 * Usage:
 *   sudo kernel-observe [--port <bus>-<port>] [--seconds <n>] [--verbose]
 *
 * Build: cc -O2 -Wall -o kernel-observe kernel-observe.c $(pkg-config --cflags --libs libusb-1.0)
 */

#include <libusb-1.0/libusb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define WS73_VID 0xffff
#define WS73_PID 0x3733

static void hex(const unsigned char *b, int n)
{
    int i;
    for (i = 0; i < n; i++)
        printf("%02x", b[i]);
}

static libusb_device *find(libusb_context *ctx, int sel_set,
                           int sel_bus, int sel_port)
{
    libusb_device **list;
    ssize_t cnt = libusb_get_device_list(ctx, &list), i;
    for (i = 0; i < cnt; i++) {
        struct libusb_device_descriptor dd;
        if (libusb_get_device_descriptor(list[i], &dd) != 0)
            continue;
        if (dd.idVendor == WS73_VID && dd.idProduct == WS73_PID) {
            if (sel_set &&
                (libusb_get_bus_number(list[i]) != sel_bus ||
                 libusb_get_port_number(list[i]) != sel_port))
                continue;
            libusb_ref_device(list[i]);
            libusb_free_device_list(list, 1);
            return list[i];
        }
    }
    libusb_free_device_list(list, 1);
    return NULL;
}

int main(int argc, char **argv)
{
    int sel_set = 0, sel_bus = 0, sel_port = 0, seconds = 8, verbose = 0;
    libusb_context *ctx = NULL;
    libusb_device *dev;
    libusb_device_handle *h = NULL;
    struct libusb_config_descriptor *cfg;
    const struct libusb_interface_descriptor *a;
    unsigned char ep_int = 0, ep_bulk_in = 0;
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            const char *p = argv[++i];
            char *dash = strchr(p, '-');
            if (!dash) { fprintf(stderr, "--port <bus>-<port>\n"); return 3; }
            sel_bus = atoi(p);
            sel_port = atoi(dash + 1);
            sel_set = 1;
        } else if (strcmp(argv[i], "--seconds") == 0 && i + 1 < argc) {
            seconds = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        } else {
            fprintf(stderr, "usage: %s [--port <bus>-<port>] [--seconds n] [--verbose]\n", argv[0]);
            return 3;
        }
    }

    if (libusb_init(&ctx) != 0) { fprintf(stderr, "libusb_init failed\n"); return 1; }
    dev = find(ctx, sel_set, sel_bus, sel_port);
    if (!dev) { fprintf(stderr, "no ffff:3733 found\n"); return 1; }
    if (libusb_open(dev, &h) != 0) { fprintf(stderr, "open failed\n"); return 1; }
    if (libusb_claim_interface(h, 0) != 0) { fprintf(stderr, "claim failed\n"); return 1; }

    if (libusb_get_active_config_descriptor(dev, &cfg) == 0) {
        a = &cfg->interface[0].altsetting[0];
        printf("eps=%u: ", a->bNumEndpoints);
        for (i = 0; i < (int)a->bNumEndpoints; i++) {
            unsigned char addr = a->endpoint[i].bEndpointAddress;
            int t = a->endpoint[i].bmAttributes & 0x03;
            printf("%02x:%s ", addr, (t == 3) ? "int" : (addr & 0x80) ? "bin" : "bout");
            if (t == 3) ep_int = addr;
            else if (addr & 0x80) ep_bulk_in = addr;
        }
        printf("\n");
        libusb_free_config_descriptor(cfg);
    }

    printf("listening %ds: INT 0x%02x, bulk-IN 0x%02x (no writes)\n",
           seconds, ep_int, ep_bulk_in);

    /* alternate blocking reads with short timeouts so we can report both */
    {
        unsigned char buf[4096];
        int elapsed = 0, round = 0;
        while (elapsed < seconds) {
            int xfer = 0, rc;
            int which = round++ % 2;
            if (which == 0 && ep_int) {
                rc = libusb_bulk_transfer(h, ep_int, buf, sizeof(buf), &xfer, 2000);
            } else if (ep_bulk_in) {
                rc = libusb_bulk_transfer(h, ep_bulk_in, buf, sizeof(buf), &xfer, 2000);
            } else {
                break;
            }
            elapsed += 2;
            if (rc == 0 && xfer > 0) {
                printf("[t=%ds] %s %d bytes: ", elapsed,
                       (which == 0) ? "INT " : "BULK", xfer);
                if (verbose)
                    hex(buf, xfer > 64 ? 64 : xfer);
                else
                    hex(buf, xfer > 16 ? 16 : xfer);
                printf("%s\n", xfer > 64 ? "…" : "");
            } else if (rc == LIBUSB_ERROR_TIMEOUT) {
                if (verbose) printf("[t=%ds] %s timeout\n", elapsed,
                                    (which == 0) ? "INT " : "BULK");
            } else if (rc != 0) {
                printf("[t=%ds] %s err %s\n", elapsed,
                       (which == 0) ? "INT " : "BULK", libusb_error_name(rc));
            }
        }
    }

    libusb_close(h);
    libusb_exit(ctx);
    printf("done.\n");
    return 0;
}
