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

    /* EXCHANGE_INFO_REQ — capability bits: reliable(3)+multiProcessing(5) for v1.3 */
    {
        uint8_t req[8];
        size_t n = ssap_encode_exchange_info(req, sizeof(req),
                                             SSAP_MSG_EXCHANGE_INFO_REQ, 0x03, 251, SSAP_VERSION_1_3);
        CHECK(ssap_server_dispatch(&srv, req, n) == (int)g_last_tx_len, "exchange handled");
        CHECK(g_last_tx[0] == SSAP_MSG_EXCHANGE_INFO_RSP, "exchange rsp opcode");
        /* v1.3: ctrl should have bit3=reliable + bit5=multiProcessing = 0x2B */
        CHECK(g_last_tx[1] == 0x2B, "exchange v1.3 ctrl = 0x2B (mtu+ver+reliable+multi)");
    }

    /* FIND_STRUCTURE (primary service, v1.3 default: [start][end][uuid][member]) */
    {
        uint8_t req[8];
        size_t n = ssap_encode_find_struct_req(req, sizeof(req),
                                               SSAP_FIND_PRIMARY_SERVICE, 0, 0,
                                               0x0001, 0xFFFF, NULL, 0);
        CHECK(ssap_server_dispatch(&srv, req, n) == 0, "find handled");
        CHECK(g_last_tx[0] == SSAP_MSG_FIND_STRUCTURE_RSP, "find rsp opcode");
        /* member = [start u16][end u16][uuid16][memberValue bitmap] = 9 B */
        CHECK(g_last_tx_len == 9, "find v1.3 member len 9 (2 hdr + 7 member)");
        CHECK(g_last_tx[2] == 0x01 && g_last_tx[3] == 0x00, "find start_handle 0x0001");
        CHECK(g_last_tx[6] == 0x34 && g_last_tx[7] == 0x12, "find uuid16 0x1234 LE");
        CHECK(g_last_tx[8] == 0x02, "find memberValue bitmap PROPERTY (0x02)");
    }

    /* FIND_STRUCTURE after v1.0 negotiation: [start][end][member] — no uuid */
    {
        uint8_t req[8];
        size_t n = ssap_encode_exchange_info(req, sizeof(req),
                                             SSAP_MSG_EXCHANGE_INFO_REQ, 0x02, 0, 1);
        CHECK(ssap_server_dispatch(&srv, req, n) > 0, "exchange v1.0 handled");
        n = ssap_encode_find_struct_req(req, sizeof(req),
                                        SSAP_FIND_PRIMARY_SERVICE, 0, 0,
                                        0x0001, 0xFFFF, NULL, 0);
        CHECK(ssap_server_dispatch(&srv, req, n) == 0, "find v1.0 handled");
        CHECK(g_last_tx_len == 7, "find v1.0 member len 7 (2 hdr + 5 member, no uuid)");
        CHECK(g_last_tx[6] == 0x02, "find v1.0 memberValue bitmap at offset 6");
    }

    /* READ_REQ */
    {
        uint8_t req[8];
        uint16_t h = prop;
        uint8_t t = 0;
        size_t n = ssap_encode_read_req(req, sizeof(req), &h, &t, 1);
        CHECK(ssap_server_dispatch(&srv, req, n) == (int)g_last_tx_len, "read handled");
        CHECK(g_last_tx[0] == SSAP_MSG_READ_RSP, "read rsp opcode");
        /* single value: [msgCode][ctrl][value...] — no handle/len prefix */
        CHECK(g_last_tx_len == 7, "read rsp len 7 (2 hdr + 5 data)");
        CHECK(memcmp(g_last_tx + 2, "hello", 5) == 0, "read returns 'hello' at offset 2");
    }

    /* READ_REQ to unknown handle -> READ_RSP ctrl.error=1 + 2-byte item */
    {
        uint8_t req[8];
        uint16_t h = 0x7F7F;
        uint8_t t = 0;
        size_t n = ssap_encode_read_req(req, sizeof(req), &h, &t, 1);
        CHECK(ssap_server_dispatch(&srv, req, n) == (int)g_last_tx_len, "read bad handle handled");
        CHECK(g_last_tx[0] == SSAP_MSG_READ_RSP, "read err rsp opcode 0x09");
        CHECK((g_last_tx[1] & 0x08) != 0, "read err ctrl.error bit set");
        CHECK(g_last_tx_len == 4 && g_last_tx[2] == SSAP_ERRCODE_INVALID_HANDLE,
              "read err 2-byte item carries INVALID_HANDLE");
    }

    /* WRITE_CMD */
    {
        uint8_t req[16];
        size_t n = ssap_encode_write(req, sizeof(req), SSAP_MSG_WRITE_CMD,
                                     prop, 0, (const uint8_t *)"xyz", 3);
        CHECK(ssap_server_dispatch(&srv, req, n) == 0, "write_cmd handled (no rsp)");
    }

    /* WRITE_REQ -> WRITE_RSP (success: 2 bytes, ctrl.result=0) */
    {
        uint8_t req[16];
        size_t n = ssap_encode_write(req, sizeof(req), SSAP_MSG_WRITE_REQ,
                                     prop, 0, (const uint8_t *)"abc", 3);
        CHECK(ssap_server_dispatch(&srv, req, n) == (int)g_last_tx_len, "write_req handled");
        CHECK(g_last_tx[0] == SSAP_MSG_WRITE_RSP, "write_req rsp opcode 0x0E");
        CHECK(g_last_tx_len == 2 && g_last_tx[1] == 0x00, "write_req success rsp (2B, result 0)");
    }

    /* WRITE_REQ to unknown handle -> WRITE_RSP with error items
     * [ctrl][errorNum][handle u16][code] = 6 bytes */
    {
        uint8_t req[16];
        size_t n = ssap_encode_write(req, sizeof(req), SSAP_MSG_WRITE_REQ,
                                     0x7F7F, 0, (const uint8_t *)"abc", 3);
        CHECK(ssap_server_dispatch(&srv, req, n) == (int)g_last_tx_len, "write_req bad handle handled");
        CHECK(g_last_tx[0] == SSAP_MSG_WRITE_RSP && g_last_tx[1] == 0x01, "write_req err rsp ctrl");
        CHECK(g_last_tx_len == 6 && g_last_tx[5] == SSAP_ERRCODE_INVALID_HANDLE, "write_req err code");
    }

    /* SERVICE_CHANGE: after add_service, a VALUE_NTF on handle 0x000E is sent */
    {
        ssap_server_t srv3;
        ssap_server_init(&srv3, fake_send);
        uint16_t svc3 = ssap_server_add_service(&srv3, 0x9999, 1);
        CHECK(svc3 != 0, "add service for change test");
        CHECK(g_last_tx[0] == SSAP_MSG_VALUE_NTF, "service change: NTF opcode");
        CHECK(g_last_tx_len == 8, "service change: 8 bytes (hdr+handle+start+end)");
        CHECK(g_last_tx[2] == 0x0E && g_last_tx[3] == 0x00,
              "service change: handle 0x000E");
        CHECK(g_last_tx[4] == (svc3 & 0xFF) && g_last_tx[5] == (svc3 >> 8),
              "service change: start handle");
    }
    {
        /* re-add service+property with NOTIFY op to get CCCD */
        ssap_server_t srv2;
        ssap_server_init(&srv2, fake_send);
        uint16_t svc2 = ssap_server_add_service(&srv2, 0xABCD, 1);
        uint16_t prop2 = ssap_server_add_property(&srv2, svc2, 0x1122,
                            SSAP_OP_READ | SSAP_OP_NOTIFY, 0, fake_read, NULL);
        (void)prop2;
        uint8_t req[8];
        size_t n = ssap_encode_find_struct_req(req, sizeof(req),
                                               SSAP_FIND_PROPERTY, 0, 0,
                                               0x0001, 0xFFFF, NULL, 0);
        ssap_server_dispatch(&srv2, req, n);
        /* v1.3 property member: [handle 2][uuid 2][op 4][descCount 1][descType 1] = 11 */
        CHECK(g_last_tx_len == 12, "find_property with CCCD desc len 12");
        CHECK(g_last_tx[10] == 1, "descCount = 1");
        CHECK(g_last_tx[11] == 0x02, "descType = 0x02 (CLIENT_CONFIG / CCCD)");
    }
    {
        uint8_t req[10];
        /* [0x06][ctrl: findType=PROPERTY(0x03)|itemType=STD(0x00)][start u16][end u16][uuid16 0x5678] */
        req[0] = SSAP_MSG_FIND_STRUCTURE_BY_UUID_REQ;
        req[1] = 0x03; /* findType=PROPERTY(0x03) */
        req[2] = 0x01; req[3] = 0x00; /* start 0x0001 */
        req[4] = 0xFF; req[5] = 0xFF; /* end 0xFFFF */
        req[6] = 0x78; req[7] = 0x56; /* uuid 0x5678 LE */
        size_t n = ssap_server_dispatch(&srv, req, 8);
        CHECK(n == 0, "find_by_uuid handled (returns 0)");
        CHECK(g_last_tx[0] == SSAP_MSG_FIND_STRUCTURE_BY_UUID_RSP, "find_by_uuid rsp opcode");
    }
    {
        uint8_t req[10];
        /* [0x0A][ctrl:uuidType=0][start u16][end u16][dataType=0][uuid16 0x5678] */
        req[0] = SSAP_MSG_READ_BY_UUID_REQ;
        req[1] = 0x00; /* uuidType=0 (std 16-bit) */
        req[2] = 0x01; req[3] = 0x00; /* start 0x0001 */
        req[4] = 0xFF; req[5] = 0xFF; /* end 0xFFFF */
        req[6] = 0x00; /* dataType */
        req[7] = 0x78; req[8] = 0x56; /* uuid 0x5678 LE */
        size_t n = ssap_server_dispatch(&srv, req, 9);
        CHECK(n > 0, "read_by_uuid handled");
        CHECK(g_last_tx[0] == SSAP_MSG_READ_BY_UUID_RSP, "read_by_uuid rsp opcode");
        CHECK((g_last_tx[1] & 0x08) == 0, "read_by_uuid no error bit");
        CHECK(g_last_tx_len == 9, "read_by_uuid len 9 (2 hdr + 2 handle + 5 data)");
        CHECK(g_last_tx[2] == (prop & 0xFF) && g_last_tx[3] == 0, "read_by_uuid handle");
        CHECK(memcmp(g_last_tx + 4, "hello", 5) == 0, "read_by_uuid returns 'hello'");
    }
    {
        /* without CCCD: notify should be rejected */
        CHECK(ssap_server_notify(&srv, prop, (const uint8_t *)"hi", 2, 0) < 0,
              "notify blocked without CCCD");
        /* write CCCD = 0x0001 to prop_handle+1 */
        uint8_t cccd_req[10];
        size_t cn = ssap_encode_write(cccd_req, sizeof(cccd_req), SSAP_MSG_WRITE_REQ,
                                      prop + 1, 0, (const uint8_t *)"\x01\x00", 2);
        CHECK(ssap_server_dispatch(&srv, cccd_req, cn) > 0, "CCCD write handled");
        CHECK(g_last_tx[0] == SSAP_MSG_WRITE_RSP && g_last_tx_len == 2,
              "CCCD write success (2B)");
        /* now notify should succeed */
        CHECK(ssap_server_notify(&srv, prop, (const uint8_t *)"hi", 2, 0) > 0,
              "notify sends after CCCD");
        CHECK(g_last_tx[0] == SSAP_MSG_VALUE_NTF, "notify opcode");
    }

    if (g_fail == 0)
        printf("\nALL SERVER TESTS PASSED\n");
    else
        printf("\n%d TEST(S) FAILED\n", g_fail);
    return g_fail ? 1 : 0;
}
