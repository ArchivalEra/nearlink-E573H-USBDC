/*
 * kernel-init — bring up the SLE channel on a WS73 in kernel (5-EP) mode,
 * following the full host init sequence (ticket 07 / assets/kernel-init-seq.md):
 *
 *   1. wait for D2H_MSG_BSP_READY on INT EP (8-byte usb_dev_notification, bit0)
 *   2. push customize/INI data on BSLE_MSG_QUEUE(10):
 *        queue-switch descriptor frame: [usb_package(12)][hcc_descr_header{1}][queue_id]
 *        data frame: [usb_package(12)][hcc_header 05 0A pay_len 00][bsle_msg_tag{type=0,len=140}]
 *                    [140-byte bfgn_bt_customization_stru]
 *   3. wait for CUSTOMIZE_RECEIVED (bulk IN: tag type=2, device_msg=2)
 *   4. control transfer H2D_MSG_SLE_OPEN=29 (0x21/req0/data=1D000000)
 *   5. wait for DEVICE_ACTION_STATUS (bulk IN: tag type=4, device_msg=2)
 *
 * Usage: sudo kernel-init [--port <bus>-<port>] [--mac AA:BB:CC:DD:EE:FF]
 *                         [--timeout-ms n] [--verbose]
 * Build: cc -O2 -Wall -o kernel-init kernel-init.c $(pkg-config --cflags --libs libusb-1.0)
 */

#include <libusb-1.0/libusb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define WS73_VID 0xffff
#define WS73_PID 0x3733

#define BMREQ_H2D         0x21
#define HOST_TO_DEV_SEND_MSG 0

#define EP_DATA_IN        0x81   /* bulk IN, data channel (index 0) */
#define EP_DATA_OUT       0x01   /* bulk OUT (index 1) */
#define EP_INT_IN         0x83   /* interrupt IN (index 2) */
#define EP_REG_OUT        0x02
#define EP_REG_IN         0x82

#define QUEUE_BSLE_MSG    10
#define QUEUE_SLE_DATA    8
#define SERVICE_BSLE_MSG  5
#define SERVICE_SLE       0x0A

#define TAG_CUSTOMIZE     0
#define TAG_DEVICE_STATUS 2
#define TAG_DEVICE_ACTION 4

#define INI_LEN           140
#define CUSTOMIZE_RECEIVED 2
#define BOOT_FINISH       1
#define STATUS_SLE_OPEN   2

static void hexd(const char *tag, const unsigned char *b, int n)
{
    int i;
    printf("%s[%d] ", tag, n);
    for (i = 0; i < n && i < 96; i++)
        printf("%02x", b[i]);
    if (n > 96)
        printf("…");
    printf("\n");
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

static void put_u32(unsigned char *p, unsigned v)
{
    p[0] = v & 0xff; p[1] = (v >> 8) & 0xff;
    p[2] = (v >> 16) & 0xff; p[3] = (v >> 24) & 0xff;
}
static void put_u16(unsigned char *p, unsigned v)
{
    p[0] = v & 0xff; p[1] = (v >> 8) & 0xff;
}

/* wait up to timeout_ms for an 8-byte INT packet whose notification bitmask
 * has `bit` set (or any INT data at all when bit<0). Returns 0 on match. */
static int wait_int(libusb_device_handle *h, unsigned char ep, int bit,
                    int timeout_ms, int verbose)
{
    unsigned char buf[8];
    int deadline = timeout_ms, waited = 0;
    printf("  waiting for INT EP 0x%02x (bit%s%d, %d ms)...\n", ep,
           bit >= 0 ? " " : " any ", bit >= 0 ? bit : 0, timeout_ms);
    while (waited < deadline) {
        int xfer = 0;
        int rc = libusb_bulk_transfer(h, ep, buf, sizeof(buf), &xfer, 2000);
        waited += 2000;
        if (rc == 0 && xfer > 0) {
            unsigned notif = (unsigned)buf[0] | ((unsigned)buf[1] << 8) |
                             ((unsigned)buf[2] << 16) | ((unsigned)buf[3] << 24);
            printf("  INT packet: ");
            hexd("", buf, xfer);
            if (bit < 0 || (notif & (1u << bit)))
                return 0;
        } else if (rc == LIBUSB_ERROR_TIMEOUT) {
            if (verbose) printf("  INT timeout\n");
        } else if (rc != 0) {
            printf("  INT err: %s\n", libusb_error_name(rc));
            return -1;
        }
    }
    printf("  INT timeout after %d ms\n", timeout_ms);
    return -1;
}

/* read a bulk-IN packet, strip optional 92-byte scatter header; returns
 * payload length (>0) or <0 on error/timeout. Fills *payload. */
static int read_payload(libusb_device_handle *h, unsigned char ep,
                        unsigned char *payload, int max, int timeout_ms)
{
    unsigned char buf[4096];
    int xfer = 0;
    int rc = libusb_bulk_transfer(h, ep, buf, sizeof(buf), &xfer, timeout_ms);
    if (rc != 0) {
        if (rc != LIBUSB_ERROR_TIMEOUT)
            printf("  bulk-IN err: %s\n", libusb_error_name(rc));
        return -1;
    }
    /* detect 92-byte scatter header: msg_type==2 and queue at byte 8 */
    if (xfer >= 92 + 5 && buf[0] == 2) {
        int plen = xfer - 92;
        if (plen > max) plen = max;
        memcpy(payload, buf + 92, plen);
        return plen;
    }
    if (xfer > max) xfer = max;
    memcpy(payload, buf, xfer);
    return xfer;
}

/* wait for a BSLE status packet: tag.type == type and device_msg == msg */
static int wait_bsle_status(libusb_device_handle *h, unsigned char ep,
                            int type, unsigned msg, int timeout_ms, int verbose)
{
    unsigned char p[2048];
    int deadline = timeout_ms, waited = 0;
    printf("  waiting BSLE tag type=%d device_msg=%u (%d ms)...\n",
           type, msg, timeout_ms);
    while (waited < deadline) {
        int n = read_payload(h, ep, p, sizeof(p), 2000);
        waited += 2000;
        if (n <= 0)
            continue;
        if (verbose)
            hexd("  RX", p, n);
        /* netbuf: [5B hcc_header][4B tag][body] */
        if (n >= 9) {
            int tag_type = p[5] | (p[6] << 8);
            int tag_len = p[7] | (p[8] << 8);
            unsigned body = 0;
            if (n >= 13)
                body = (unsigned)p[9] | ((unsigned)p[10] << 8) |
                       ((unsigned)p[11] << 16) | ((unsigned)p[12] << 24);
            printf("  RX tag type=%d len=%d body=0x%x (queue byte0=0x%02x)\n",
                   tag_type, tag_len, body, p[1]);
            if (tag_type == type && body == msg)
                return 0;
        }
    }
    printf("  BSLE status timeout\n");
    return -1;
}

int main(int argc, char **argv)
{
    int sel_set = 0, sel_bus = 0, sel_port = 0, verbose = 0;
    unsigned char mac[6] = {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc};
    int mac_set = 0;
    libusb_context *ctx = NULL;
    libusb_device *dev;
    libusb_device_handle *h = NULL;
    int i, rc = 1;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            const char *p = argv[++i];
            char *dash = strchr(p, '-');
            if (!dash) { fprintf(stderr, "--port <bus>-<port>\n"); return 3; }
            sel_bus = atoi(p);
            sel_port = atoi(dash + 1);
            sel_set = 1;
        } else if (strcmp(argv[i], "--mac") == 0 && i + 1 < argc) {
            if (sscanf(argv[++i], "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                       &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]) != 6) {
                fprintf(stderr, "bad --mac\n");
                return 3;
            }
            mac_set = 1;
        } else if (strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        } else {
            fprintf(stderr, "usage: %s [--port <bus>-<port>] [--mac xx:..:xx] [--verbose]\n", argv[0]);
            return 3;
        }
    }

    if (libusb_init(&ctx) != 0) { fprintf(stderr, "libusb_init failed\n"); return 1; }
    dev = find(ctx, sel_set, sel_bus, sel_port);
    if (!dev) { fprintf(stderr, "no ffff:3733 found\n"); return 1; }
    if (libusb_open(dev, &h) != 0) { fprintf(stderr, "open failed\n"); return 1; }
    if (libusb_kernel_driver_active(h, 0) == 1)
        libusb_detach_kernel_driver(h, 0);
    if (libusb_claim_interface(h, 0) != 0) { fprintf(stderr, "claim failed\n"); return 1; }

    /* ---------- step 1: wait BSP_READY ---------- */
    printf("== step 1/5: wait D2H_MSG_BSP_READY ==\n");
    wait_int(h, EP_INT_IN, 0, 8000, verbose);

    /* ---------- step 2: push customize/INI ---------- */
    printf("== step 2/5: push customize/INI (140B) ==\n");
    {
        unsigned char qs[17], frame[800];
        int n;
        /* queue-switch descriptor frame */
        put_u32(qs, 3);            /* msg_type = USB_HCC_CONTROL_MSG */
        put_u32(qs + 4, 5);        /* len = descr(4)+queue(1) */
        put_u32(qs + 8, 0);
        put_u32(qs + 12, 1);       /* hcc_descr_header.descr_type = QUEUE_SWITCH */
        qs[16] = QUEUE_BSLE_MSG;   /* queue id */
        printf("  TX queue-switch frame (%d B)\n", (int)sizeof(qs));
        if (verbose) hexd("  ", qs, sizeof(qs));
        rc = libusb_bulk_transfer(h, EP_DATA_OUT, qs, sizeof(qs), &n, 3000);
        if (rc != 0) { printf("  queue-switch TX failed: %s\n", libusb_error_name(rc)); goto out; }

        /* data frame: usb_package + [hcc_header 05 0A 20 03 00][tag][140B ini] */
        memset(frame, 0, sizeof(frame));
        put_u32(frame, 0);             /* msg_type = USB_SINGLE_MSG */
        put_u32(frame + 4, 800);           /* usb_package.len = full netbuf */
        put_u32(frame + 8, 0);         /* reserve */
        frame[12] = (SERVICE_BSLE_MSG << 4) | 0; /* byte0: sub=0, service=5 -> 0x50? */
        /* NOTE: hcc_comm.h byte0 = sub_type:4 | service_type:4; service=5 -> 0x05 */
        frame[12] = SERVICE_BSLE_MSG;  /* 0x05 */
        frame[13] = QUEUE_BSLE_MSG;    /* 0x0A */
        put_u16(frame + 14, 800);          /* pay_len = netbuf buf_len */
        frame[16] = 0;                 /* padding byte 4 */
        put_u16(frame + 17, TAG_CUSTOMIZE);  /* tag.type = 0 */
        put_u16(frame + 19, INI_LEN);        /* tag.len = 140 */
        /* 140-byte struct at frame+21 */
        if (mac_set) {
            memcpy(frame + 21 + 0, mac, 6);      /* bfgn layout: MACs near end;
                                                    we only fill offsets we know */
        }
        printf("  TX customize frame (%d B incl. padding, queue %d, service %d)\n",
               (int)sizeof(frame), QUEUE_BSLE_MSG, SERVICE_BSLE_MSG);
        if (verbose) hexd("  ", frame, sizeof(frame));
        rc = libusb_bulk_transfer(h, EP_DATA_OUT, frame, sizeof(frame), &n, 3000);
        if (rc != 0) { printf("  customize TX failed: %s\n", libusb_error_name(rc)); goto out; }
    }

    /* ---------- step 3: wait CUSTOMIZE_RECEIVED ---------- */
    printf("== step 3/5: wait CUSTOMIZE_RECEIVED ==\n");
    wait_bsle_status(h, EP_DATA_IN, TAG_DEVICE_STATUS, CUSTOMIZE_RECEIVED,
                     8000, verbose);

    /* ---------- step 4: H2D_MSG_SLE_OPEN control ---------- */
    printf("== step 4/5: H2D_MSG_SLE_OPEN (29) control ==\n");
    {
        unsigned char payload[4] = {29, 0, 0, 0};
        rc = libusb_control_transfer(h, BMREQ_H2D, HOST_TO_DEV_SEND_MSG,
                                     0, 0, payload, 4, 2000);
        printf("  control xfer: %s (%d)\n",
               rc >= 0 ? "OK" : libusb_error_name(rc), rc);
    }

    /* ---------- step 5: wait SLE_OPEN ack ---------- */
    printf("== step 5/5: wait SLE_OPEN ack (ACTION_STATUS) ==\n");
    if (wait_bsle_status(h, EP_DATA_IN, TAG_DEVICE_ACTION, STATUS_SLE_OPEN,
                         8000, verbose) == 0) {
        printf("\n*** SLE channel OPEN — device acked SLE_OPEN ***\n");
        rc = 0;
    } else {
        printf("\nno SLE_OPEN ack within window (device state unknown).\n");
        rc = 2;
    }

    /* ---- diagnosis: after OPEN, peek INT for PANIC/notifications ---- */
    printf("  diag: peeking INT EP for device notifications (6s)...\n");
    {
        unsigned char b[8];
        int t;
        for (t = 0; t < 3; t++) {
            int x = 0;
            int r = libusb_bulk_transfer(h, EP_INT_IN, b, sizeof(b), &x, 2000);
            if (r == 0 && x > 0) {
                unsigned n = (unsigned)b[0] | ((unsigned)b[1] << 8) |
                             ((unsigned)b[2] << 16) | ((unsigned)b[3] << 24);
                printf("  diag INT: notif=0x%x bits=", n);
                if (n & 1) printf("BSP_READY ");
                if (n & (1u << 6)) printf("DEVICE_PANIC ");
                printf("\n");
            } else if (r != 0 && r != LIBUSB_ERROR_TIMEOUT) {
                printf("  diag INT err: %s\n", libusb_error_name(r));
                break;
            }
        }
    }

out:
    libusb_close(h);
    libusb_exit(ctx);
    return rc;
}
