/*
 * test_server.c — SSAP server unit tests.
 *
 * Verifies service table + dispatch: add service/property, handle
 * EXCHANGE_INFO / FIND_STRUCTURE / READ / WRITE.
 */

#include "ssap_codec.h"
#include "ssap_server.h"
#include <stdio.h>
#include <string.h>

static int g_fail = 0;
static uint8_t g_last_tx[256];
static size_t g_last_tx_len = 0;

static int fake_send(const uint8_t *pdu, size_t len)
{
    if (len > sizeof(g_last_tx))
        return -1;
    memcpy(g_last_tx, pdu, len);
    g_last_tx_len = len;
    return (int)len;
}

static int fake_read(uint16_t handle, uint8_t *out, uint16_t *out_len, uint16_t max_len)
{
    (void)handle;
    const char *v = "hello";
    size_t n = strlen(v);
    if (n > max_len)
        return -1;
    memcpy(out, v, n);
    *out_len = (uint16_t)n;
    return 0;
}

static int fake_write(uint16_t handle, const uint8_t *value, uint16_t len)
{
    (void)handle; (void)value; (void)len;
    return 0;
}

#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s (line %d)\n", msg, __LINE__); g_fail++; } \
    else { printf("ok: %s\n", msg); } \
} while (0)

int main(void)
{
    ssap_server_t srv;
    ssap_server_init(&srv, fake_send);

    /* add service + property */
    uint16_t svc = ssap_server_add_service(&srv, 0x1234, 1);
    CHECK(svc != 0, "add service returns handle");
    uint16_t prop = ssap_server_add_property(&srv, svc, 0x5678,
                                             SSAP_OP_READ | SSAP_OP_WRITE,
                                             0, fake_read, fake_write);
    CHECK(prop != 0 && prop > svc, "add property returns handle > service");

    /* EXCHANGE_INFO_REQ */
    {
        uint8_t req[8];
        size_t n = ssap_encode_exchange_info(req, sizeof(req),
                                             SSAP_MSG_EXCHANGE_INFO_REQ, 0x03, 251, SSAP_VERSION_1_3);
        CHECK(ssap_server_dispatch(&srv, req, n) == (int)g_last_tx_len, "exchange handled");
        CHECK(g_last_tx[0] == SSAP_MSG_EXCHANGE_INFO_RSP, "exchange rsp opcode");
    }

    /* FIND_STRUCTURE (primary service) */
    {
        uint8_t req[8];
        size_t n = ssap_encode_find_struct_req(req, sizeof(req),
                                               SSAP_FIND_PRIMARY_SERVICE, 0, 0,
                                               0x0001, 0xFFFF, NULL, 0);
        CHECK(ssap_server_dispatch(&srv, req, n) == 0, "find handled");
        CHECK(g_last_tx[0] == SSAP_MSG_FIND_STRUCTURE_RSP, "find rsp opcode");
        CHECK(g_last_tx[4] == SSAP_ITEM_PRIMARY_SERVICE, "find returns primary service");
    }

    /* READ_REQ */
    {
        uint8_t req[8];
        uint16_t h = prop;
        uint8_t t = 0;
        size_t n = ssap_encode_read_req(req, sizeof(req), &h, &t, 1);
        CHECK(ssap_server_dispatch(&srv, req, n) == (int)g_last_tx_len, "read handled");
        CHECK(g_last_tx[0] == SSAP_MSG_READ_RSP, "read rsp opcode");
        CHECK(g_last_tx[4] == 5 && g_last_tx[5] == 0 &&
              memcmp(g_last_tx + 6, "hello", 5) == 0, "read returns 'hello'");
    }

    /* WRITE_CMD */
    {
        uint8_t req[16];
        size_t n = ssap_encode_write(req, sizeof(req), SSAP_MSG_WRITE_CMD,
                                     prop, 0, (const uint8_t *)"xyz", 3);
        CHECK(ssap_server_dispatch(&srv, req, n) == 0, "write_cmd handled (no rsp)");
    }

    /* NOTIFY */
    {
        CHECK(ssap_server_notify(&srv, prop, (const uint8_t *)"hi", 2, 0) > 0, "notify sends");
        CHECK(g_last_tx[0] == SSAP_MSG_VALUE_NTF, "notify opcode");
    }

    if (g_fail == 0)
        printf("\nALL SERVER TESTS PASSED\n");
    else
        printf("\n%d TEST(S) FAILED\n", g_fail);
    return g_fail ? 1 : 0;
}
