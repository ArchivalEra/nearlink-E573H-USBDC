/*
 * hwsle_transport.c — SSAP transport adapter for /dev/hwsle.
 *
 * Wire format per our hardware verification (SLE-CONTROL-PLANE.md):
 *   HCI cmd:  [0xA1][opcode u16 LE][plen u16 LE][params]
 *   HCI event:[0xA2]...
 *   ACB data: [0xA3][tcid u16 LE][len u16 LE][payload]
 *   ICB data: [0xA4]...
 */

#include "hwsle_transport.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int g_fd = -1;
static ssap_recv_fn g_recv_cb = NULL;

static int put_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)(v >> 8);
    return 2;
}

int hwsle_transport_open(ssap_recv_fn recv_cb)
{
    g_fd = open(HWSLE_DEV, O_RDWR | O_NONBLOCK);
    if (g_fd < 0) {
        fprintf(stderr, "hwsle: open %s failed: %s\n", HWSLE_DEV, strerror(errno));
        return -1;
    }
    g_recv_cb = recv_cb;
    return g_fd;
}

void hwsle_transport_close(void)
{
    if (g_fd >= 0) {
        close(g_fd);
        g_fd = -1;
    }
    g_recv_cb = NULL;
}

int hwsle_transport_send_acb(uint16_t tcid, const uint8_t *payload, size_t len)
{
    if (g_fd < 0 || !payload)
        return -1;
    /* [0xA3][tcid u16][len u16][payload] */
    uint8_t hdr[5];
    hdr[0] = HCI_DATATYPE_ACB;
    put_u16(hdr + 1, tcid);
    put_u16(hdr + 3, (uint16_t)len);
    ssize_t w = write(g_fd, hdr, sizeof(hdr));
    if (w != (ssize_t)sizeof(hdr)) {
        fprintf(stderr, "hwsle: acb header write failed (%zd)\n", w);
        return -1;
    }
    w = write(g_fd, payload, len);
    if (w != (ssize_t)len) {
        fprintf(stderr, "hwsle: acb payload write failed (%zd)\n", w);
        return -1;
    }
    return (int)(sizeof(hdr) + len);
}

int hwsle_transport_send_ssap(const uint8_t *pdu, size_t len)
{
    return hwsle_transport_send_acb(TCID_SLE_SMTC, pdu, len);
}

int hwsle_transport_send_hci_cmd(uint16_t opcode, const uint8_t *params, size_t plen)
{
    if (g_fd < 0)
        return -1;
    /* [0xA1][opcode u16][plen u16][params] */
    uint8_t hdr[5];
    hdr[0] = HCI_DATATYPE_CMD;
    put_u16(hdr + 1, opcode);
    put_u16(hdr + 3, (uint16_t)plen);
    ssize_t w = write(g_fd, hdr, sizeof(hdr));
    if (w != (ssize_t)sizeof(hdr))
        return -1;
    if (plen && params) {
        w = write(g_fd, params, plen);
        if (w != (ssize_t)plen)
            return -1;
    }
    return (int)(sizeof(hdr) + plen);
}

void hwsle_transport_run(void)
{
    if (g_fd < 0)
        return;
    uint8_t buf[2048];
    for (;;) {
        struct pollfd pfd = {.fd = g_fd, .events = POLLIN};
        int pr = poll(&pfd, 1, 500);
        if (pr < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        if (pr == 0)
            continue;
        ssize_t r = read(g_fd, buf, sizeof(buf));
        if (r <= 0)
            continue;
        /* Parse frames: each read may contain [type][payload...] */
        size_t off = 0;
        while (off < (size_t)r) {
            uint8_t type = buf[off];
            if (type == HCI_DATATYPE_ACB && off + 5 <= (size_t)r) {
                uint16_t tcid = (uint16_t)(buf[off + 1] | ((uint16_t)buf[off + 2] << 8));
                uint16_t len = (uint16_t)(buf[off + 3] | ((uint16_t)buf[off + 4] << 8));
                if (off + 5 + len > (size_t)r)
                    break; /* incomplete frame */
                if (tcid == TCID_SLE_SMTC && g_recv_cb)
                    g_recv_cb(buf + off + 5, len);
                off += 5 + len;
            } else if (type == HCI_DATATYPE_EVENT) {
                /* skip event: [0xA2][hdr...] — minimal: advance by 5 + payload */
                if (off + 5 <= (size_t)r) {
                    /* event plen at offset 3-4 per our HCI analysis */
                    uint16_t elen = (uint16_t)(buf[off + 3] | ((uint16_t)buf[off + 4] << 8));
                    off += 5 + elen;
                } else {
                    break;
                }
            } else {
                off++;
            }
        }
    }
}
