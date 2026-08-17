/*
 * test_link.c — unit tests for the hardened connection FSM.
 *
 * Exercises ssap_link_on_event / ssap_link_tick without hardware:
 * state gating, connect-failure paths, reject-revert, supervision and
 * command timeouts, stale-event filtering. (Command *sending* is not
 * covered — it needs /dev/hwsle.)
 */

#include "ssap_link.h"
#include <stdio.h>
#include <string.h>

static int g_fail = 0;
static uint16_t g_conn_handle = 0;
static uint16_t g_conn_fail_status = 0;
static uint16_t g_disc_reason = 0;
static int g_conn_cb = 0, g_fail_cb = 0, g_disc_cb = 0;

static void on_connected(uint16_t handle, void *ctx)
{
    (void)ctx;
    g_conn_handle = handle;
    g_conn_cb++;
}
static void on_connect_failed(uint16_t status, void *ctx)
{
    (void)ctx;
    g_conn_fail_status = status;
    g_fail_cb++;
}
static void on_disconnected(uint16_t reason, void *ctx)
{
    (void)ctx;
    g_disc_reason = reason;
    g_disc_cb++;
}

#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s (line %d)\n", msg, __LINE__); g_fail++; } \
    else { printf("ok: %s\n", msg); } \
} while (0)

#define RESET_CBS() do { \
    g_conn_handle = 0; g_conn_fail_status = 0; g_disc_reason = 0; \
    g_conn_cb = 0; g_fail_cb = 0; g_disc_cb = 0; \
} while (0)

int main(void)
{
    ssap_link_t link;

    /* 1. gating: 0x0015 while IDLE must be ignored (stale event) */
    {
        ssap_link_init(&link);
        uint8_t ev[5] = {0x00, 0x01, 0x00, 0x64, 0x00};
        ssap_link_on_event(&link, DLI_CONNECTION_COMPLETE_EVT, ev, sizeof(ev));
        CHECK(link.state == SSAP_LINK_IDLE, "0x0015 in IDLE ignored");
    }

    RESET_CBS();
    /* 2. connect-failure: 0x0015 with error status aborts, no CONNECTED */
    {
        ssap_link_init(&link);
        link.on_connect_failed = on_connect_failed;
        link.state = SSAP_LINK_CONNECTING;
        uint8_t ev[5] = {0x0F, 0x01, 0x00, 0x64, 0x00}; /* status 0x0F */
        ssap_link_on_event(&link, DLI_CONNECTION_COMPLETE_EVT, ev, sizeof(ev));
        CHECK(link.state == SSAP_LINK_IDLE, "0x0015 error -> IDLE");
        CHECK(g_fail_cb == 1 && g_conn_fail_status == 0x0F, "connect failed cb fired");
    }

    RESET_CBS();
    /* 3. success path: 0x0015 -> CONNECTED with handle; gate requires CONNECTING */
    {
        ssap_link_init(&link);
        link.on_connected = on_connected;
        link.state = SSAP_LINK_CONNECTING;
        uint8_t ev[5] = {0x00, 0x2A, 0x00, 0x64, 0x00};
        ssap_link_on_event(&link, DLI_CONNECTION_COMPLETE_EVT, ev, sizeof(ev));
        CHECK(link.state == SSAP_LINK_CONNECTED, "0x0015 ok -> CONNECTED");
        CHECK(g_conn_cb == 1 && g_conn_handle == 0x002A, "connected cb handle");
        /* now a duplicate late 0x0015 must be dropped (state gate) */
        uint8_t ev2[5] = {0x00, 0x2B, 0x00, 0x64, 0x00};
        ssap_link_on_event(&link, DLI_CONNECTION_COMPLETE_EVT, ev2, sizeof(ev2));
        CHECK(link.conn_handle == 0x002A, "late duplicate 0x0015 dropped");
    }

    RESET_CBS();
    /* 4. reject-revert: 0x1403 CmdComplete error reverts DISCONNECTING->CONNECTED */
    {
        ssap_link_init(&link);
        link.state = SSAP_LINK_DISCONNECTING;
        link.conn_handle = 0x0001;
        uint8_t ev[4] = {0x01, 0x03, 0x14, 0x0F}; /* num_pkts, opcode 0x1403, status 0x0F */
        ssap_link_on_event(&link, DLI_CMD_COMPLETE_EVT, ev, sizeof(ev));
        CHECK(link.state == SSAP_LINK_CONNECTED, "0x1403 reject reverts to CONNECTED");
    }

    RESET_CBS();
    /* 5. 0x1401 rejected -> IDLE + failed cb */
    {
        ssap_link_init(&link);
        link.on_connect_failed = on_connect_failed;
        link.state = SSAP_LINK_CONNECTING;
        uint8_t ev[4] = {0x01, 0x01, 0x14, 0x0B}; /* opcode 0x1401, status 0x0B */
        ssap_link_on_event(&link, DLI_CMD_STATUS_EVT, ev, sizeof(ev));
        CHECK(link.state == SSAP_LINK_IDLE, "0x1401 reject -> IDLE");
        CHECK(g_fail_cb == 1 && g_conn_fail_status == 0x0B, "0x1401 fail cb status");
    }

    RESET_CBS();
    /* 6. 0x0005 stale handle mismatch ignored while CONNECTED */
    {
        ssap_link_init(&link);
        link.on_disconnected = on_disconnected;
        link.state = SSAP_LINK_CONNECTED;
        link.conn_handle = 0x002A;
        uint8_t ev[3] = {0x00, 0x99, 0x00}; /* handle 0x0099 != 0x002A */
        ssap_link_on_event(&link, DLI_DISCONNECTION_COMPLETE_EVT, ev, sizeof(ev));
        CHECK(link.state == SSAP_LINK_CONNECTED, "stale 0x0005 (handle mismatch) ignored");
        uint8_t ev2[3] = {0x13, 0x2A, 0x00}; /* matching handle + reason */
        ssap_link_on_event(&link, DLI_DISCONNECTION_COMPLETE_EVT, ev2, sizeof(ev2));
        CHECK(link.state == SSAP_LINK_IDLE && g_disc_reason == 0x13, "0x0005 match -> IDLE");
    }

    RESET_CBS();
    /* 7. connect timeout: no 0x0015 in 5 s -> cancel + failed */
    {
        ssap_link_init(&link);
        link.on_connect_failed = on_connect_failed;
        link.state = SSAP_LINK_CONNECTING;
        link.target_addr[0] = 0x01;
        ssap_link_tick(&link, 1000);
        ssap_link_tick(&link, 7000); /* 6 s elapsed */
        CHECK(link.state == SSAP_LINK_IDLE, "connect timeout -> IDLE");
        CHECK(g_fail_cb == 1 && g_conn_fail_status == 0xB0, "connect timeout failed cb");
    }

    RESET_CBS();
    /* 8. supervision timeout: CONNECTED idle past supervision -> reason 0x08 */
    {
        ssap_link_init(&link);
        link.on_disconnected = on_disconnected;
        link.state = SSAP_LINK_CONNECTED;
        link.supervision_timeout_ms = 5000;
        ssap_link_tick(&link, 1000);
        ssap_link_tick(&link, 2000); /* activity anchored */
        ssap_link_mark_activity(&link, 2500);
        ssap_link_tick(&link, 9000); /* > 5 s past activity */
        CHECK(link.state == SSAP_LINK_IDLE, "supervision timeout -> IDLE");
        CHECK(g_disc_cb == 1 && g_disc_reason == 0x08, "supervision reason 0x08");
    }

    RESET_CBS();
    /* 9. disconnect timeout: DISCONNECTING stuck -> force IDLE */
    {
        ssap_link_init(&link);
        link.on_disconnected = on_disconnected;
        link.state = SSAP_LINK_DISCONNECTING;
        ssap_link_tick(&link, 1000);
        ssap_link_tick(&link, 7000);
        CHECK(link.state == SSAP_LINK_IDLE, "disconnect timeout -> IDLE");
    }

    /* 10. duplicate-peer rejection needs a live transport; the guard alone is
     * that connect() from a non-IDLE state always fails */
    {
        ssap_link_init(&link);
        link.state = SSAP_LINK_CONNECTING;
        uint8_t addr[6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
        CHECK(ssap_link_connect(&link, addr, NULL) == -1, "connect while busy rejected");
    }

    if (g_fail == 0)
        printf("\nALL LINK TESTS PASSED\n");
    else
        printf("\n%d TEST(S) FAILED\n", g_fail);
    return g_fail ? 1 : 0;
}
