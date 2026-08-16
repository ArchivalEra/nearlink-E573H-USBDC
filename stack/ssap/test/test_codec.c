/*
 * test_codec.c — unit tests for the SSAP codec.
 *
 * Verifies byte-exact encoding/decoding against the layouts in
 * ssap_pkt.h (OHOS). Run: make test
 */

#include "ssap_codec.h"
#include <stdio.h>
#include <string.h>

static int g_fail = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s (line %d)\n", msg, __LINE__); g_fail++; } \
    else { printf("ok: %s\n", msg); } \
} while (0)

int main(void)
{
    uint8_t buf[512];

    /* 1. EXCHANGE_INFO_REQ: opcode 0x02, ctrl mtu|version, mtu=251 version=1.3 */
    {
        size_t n = ssap_encode_exchange_info(buf, sizeof(buf), 0x02, 0x03, 251, SSAP_VERSION_1_3);
        CHECK(n == 6, "exchange_info len 6");
        CHECK(buf[0] == 0x02 && buf[1] == 0x03, "exchange_info hdr");
        CHECK(buf[2] == 0xFB && buf[3] == 0x00, "exchange_info mtu LE 251");
        CHECK(buf[4] == 0x01 && buf[5] == 0x03, "exchange_info version 0x0301");
        uint16_t mtu = 0, ver = 0;
        CHECK(ssap_decode_exchange_info(buf, n, &mtu, &ver) == 4, "decode payload len");
        CHECK(mtu == 251 && ver == SSAP_VERSION_1_3, "decode values");
    }

    /* 2. FIND_STRUCTURE_REQ: find primary service, standard uuid */
    {
        size_t n = ssap_encode_find_struct_req(buf, sizeof(buf),
                                               SSAP_FIND_PRIMARY_SERVICE, 0, 0,
                                               0x0001, 0xFFFF, NULL, 0);
        CHECK(n == 6, "find_struct len 6 (no uuid)");
        CHECK(buf[0] == 0x04, "find_struct opcode");
        CHECK((buf[1] & 0x07) == SSAP_FIND_PRIMARY_SERVICE, "find_struct findType");
        CHECK(buf[2] == 0x01 && buf[3] == 0x00, "find_struct start 0x0001");
        CHECK(buf[4] == 0xFF && buf[5] == 0xFF, "find_struct end 0xFFFF");
    }

    /* 3. READ_REQ: single handle 0x0001, type 0 (data) */
    {
        uint16_t h = 0x0001; uint8_t t = 0;
        size_t n = ssap_encode_read_req(buf, sizeof(buf), &h, &t, 1);
        CHECK(n == 5, "read_req len 5");
        CHECK(buf[0] == 0x08 && buf[1] == SSAP_CTRL_NO_FRAG, "read_req hdr");
        CHECK(buf[2] == 0x01 && buf[3] == 0x00, "read_req handle");
        CHECK(buf[4] == 0x00, "read_req type");
    }

    /* 4. WRITE_CMD: handle 0x0001, type 0, value "Hi" */
    {
        size_t n = ssap_encode_write(buf, sizeof(buf), SSAP_MSG_WRITE_CMD,
                                     0x0001, 0, (const uint8_t *)"Hi", 2);
        CHECK(n == 7, "write_cmd len 7");
        CHECK(buf[0] == 0x0C, "write_cmd opcode");
        CHECK(buf[4] == 0x00 && buf[5] == 'H' && buf[6] == 'i', "write_cmd payload");
    }

    /* 5. VALUE_NTF: frag no-frag, type property, handle 0x0001, "AB" */
    {
        size_t n = ssap_encode_value(buf, sizeof(buf), SSAP_MSG_VALUE_NTF,
                                     SSAP_CTRL_NO_FRAG, 0, 0x0001,
                                     (const uint8_t *)"AB", 2);
        CHECK(n == 8, "value_ntf len 8");
        CHECK(buf[0] == 0x0F, "value_ntf opcode");
        CHECK(buf[2] == 0x01 && buf[3] == 0x00, "value_ntf handle");
        CHECK(buf[4] == 0x02 && buf[5] == 0x00, "value_ntf len LE 2");
        CHECK(buf[6] == 'A' && buf[7] == 'B', "value_ntf data");
    }

    /* 6. ERROR_RSP */
    {
        size_t n = ssap_encode_error_rsp(buf, sizeof(buf), SSAP_MSG_READ_REQ, 0x0001, 0x05);
        CHECK(n == 6, "error_rsp len 6");
        CHECK(buf[0] == 0x01 && buf[2] == 0x08, "error_rsp hdr+req code");
        CHECK(buf[3] == 0x01 && buf[4] == 0x00 && buf[5] == 0x05, "error_rsp handle+code");
    }

    /* 7. VALUE_ACK */
    {
        size_t n = ssap_encode_value_ack(buf, sizeof(buf), 0x0001);
        CHECK(n == 5, "value_ack len 5");
        CHECK(buf[0] == 0x11 && buf[2] == 0x01 && buf[3] == 0x00, "value_ack hdr+handle");
    }

    /* 8. trans types */
    CHECK(ssap_trans_type_of(SSAP_MSG_EXCHANGE_INFO_REQ) == SSAP_TRANS_REQ, "req trans");
    CHECK(ssap_trans_type_of(SSAP_MSG_READ_RSP) == SSAP_TRANS_RSP, "rsp trans");
    CHECK(ssap_trans_type_of(SSAP_MSG_VALUE_NTF) == SSAP_TRANS_NOTI, "ntf trans");
    CHECK(ssap_trans_type_of(SSAP_MSG_VALUE_IND) == SSAP_TRANS_IND, "ind trans");
    CHECK(ssap_trans_type_of(SSAP_MSG_WRITE_CMD) == SSAP_TRANS_CMD, "cmd trans");

    if (g_fail == 0)
        printf("\nALL TESTS PASSED\n");
    else
        printf("\n%d TEST(S) FAILED\n", g_fail);
    return g_fail ? 1 : 0;
}
