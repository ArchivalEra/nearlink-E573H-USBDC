/*
 * ssap_codec.c — SSAP PDU codec implementation.
 *
 * Byte-level encode/decode per ssap_pkt.h (Apache-2.0). Little-endian
 * on the wire. All lengths are u16 LE where applicable.
 */

#include "ssap_codec.h"
#include <string.h>

uint8_t ssap_trans_type_of(uint8_t opcode)
{
    switch (opcode & 0x7F) {
    case SSAP_MSG_EXCHANGE_INFO_REQ:
    case SSAP_MSG_FIND_STRUCTURE_REQ:
    case SSAP_MSG_FIND_STRUCTURE_BY_UUID_REQ:
    case SSAP_MSG_READ_REQ:
    case SSAP_MSG_READ_BY_UUID_REQ:
    case SSAP_MSG_WRITE_REQ:
    case SSAP_MSG_CALL_METHOD_REQ:
        return SSAP_TRANS_REQ;
    case SSAP_MSG_EXCHANGE_INFO_RSP:
    case SSAP_MSG_FIND_STRUCTURE_RSP:
    case SSAP_MSG_FIND_STRUCTURE_BY_UUID_RSP:
    case SSAP_MSG_READ_RSP:
    case SSAP_MSG_READ_BY_UUID_RSP:
    case SSAP_MSG_WRITE_RSP:
    case SSAP_MSG_CALL_METHOD_RSP:
        return SSAP_TRANS_RSP;
    case SSAP_MSG_VALUE_NTF:
        return SSAP_TRANS_NOTI;
    case SSAP_MSG_VALUE_IND:
        return SSAP_TRANS_IND;
    case SSAP_MSG_VALUE_ACK:
        return SSAP_TRANS_ACK;
    case SSAP_MSG_WRITE_CMD:
    case SSAP_MSG_CALL_METHOD_CMD:
    default:
        return SSAP_TRANS_CMD;
    }
}

static size_t put_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)(v >> 8);
    return 2;
}

static uint16_t get_u16(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

size_t ssap_encode_exchange_info(uint8_t *out, size_t out_sz,
                                 uint8_t opcode, uint8_t ctrl,
                                 uint16_t mtu, uint16_t version)
{
    /* PDU: msgCode(1) + msgCtrl(1) + [mtu u16?] + [version u16?] */
    size_t len = 0;
    if (out_sz < 6)
        return 0;
    out[len++] = opcode;
    out[len++] = ctrl;
    if (ctrl & 0x01) { /* mtu flag */
        if (out_sz - len < 2) return 0;
        len += put_u16(out + len, mtu);
    }
    if (ctrl & 0x02) { /* version flag */
        if (out_sz - len < 2) return 0;
        len += put_u16(out + len, version);
    }
    return len;
}

int ssap_decode_exchange_info(const uint8_t *pdu, size_t len,
                              uint16_t *mtu, uint16_t *version)
{
    if (len < 2)
        return -1;
    uint8_t ctrl = pdu[1];
    size_t off = 2;
    if (ctrl & 0x01) {
        if (len - off < 2) return -1;
        if (mtu) *mtu = get_u16(pdu + off);
        off += 2;
    }
    if (ctrl & 0x02) {
        if (len - off < 2) return -1;
        if (version) *version = get_u16(pdu + off);
        off += 2;
    }
    return (int)(off - 2);
}

size_t ssap_encode_find_struct_req(uint8_t *out, size_t out_sz,
                                   uint8_t find_type, uint8_t item_type,
                                   uint8_t rsp_mode, uint16_t start_h,
                                   uint16_t end_h, const uint8_t *uuid,
                                   uint8_t uuid_len)
{
    if (out_sz < 7 || (uuid && uuid_len > 16))
        return 0;
    size_t len = 0;
    out[len++] = SSAP_MSG_FIND_STRUCTURE_REQ;
    out[len++] = (uint8_t)((find_type & 0x07) | ((item_type & 0x03) << 3) |
                           ((rsp_mode & 0x01) << 5));
    len += put_u16(out + len, start_h);
    len += put_u16(out + len, end_h);
    if (uuid) {
        if (out_sz - len < uuid_len) return 0;
        memcpy(out + len, uuid, uuid_len);
        len += uuid_len;
    }
    return len;
}

size_t ssap_encode_read_req(uint8_t *out, size_t out_sz,
                            const uint16_t *handles, const uint8_t *types,
                            size_t count)
{
    size_t need = 2 + count * 3;
    if (out_sz < need || count == 0)
        return 0;
    size_t len = 0;
    out[len++] = SSAP_MSG_READ_REQ;
    out[len++] = SSAP_CTRL_NO_FRAG; /* fragment bits: no-frag */
    for (size_t i = 0; i < count; i++) {
        len += put_u16(out + len, handles[i]);
        out[len++] = types[i];
    }
    return len;
}

size_t ssap_encode_write(uint8_t *out, size_t out_sz, uint8_t opcode,
                         uint16_t handle, uint8_t type,
                         const uint8_t *value, uint16_t value_len)
{
    size_t need = 2 + 2 + 1 + value_len;
    if (out_sz < need)
        return 0;
    size_t len = 0;
    out[len++] = opcode;
    out[len++] = SSAP_CTRL_NO_FRAG;
    len += put_u16(out + len, handle);
    out[len++] = type;
    if (value_len && value)
        memcpy(out + len, value, value_len);
    len += value_len;
    return len;
}

size_t ssap_encode_value(uint8_t *out, size_t out_sz, uint8_t opcode,
                         uint8_t frag, uint8_t type,
                         uint16_t handle, const uint8_t *value, uint16_t value_len)
{
    size_t need = 2 + 2 + 2 + value_len;
    if (out_sz < need)
        return 0;
    size_t len = 0;
    out[len++] = opcode;
    out[len++] = (uint8_t)(((frag & 0x03) << 0) | ((type & 0x01) << 2));
    len += put_u16(out + len, handle);
    len += put_u16(out + len, value_len);
    if (value_len && value)
        memcpy(out + len, value, value_len);
    len += value_len;
    return len;
}

size_t ssap_encode_error_rsp(uint8_t *out, size_t out_sz,
                             uint8_t msg_code_req, uint16_t handle, uint8_t err_code)
{
    if (out_sz < 7)
        return 0;
    size_t len = 0;
    out[len++] = SSAP_MSG_ERROR_RSP;
    out[len++] = 0;
    out[len++] = msg_code_req;
    len += put_u16(out + len, handle);
    out[len++] = err_code;
    return len;
}

size_t ssap_encode_value_ack(uint8_t *out, size_t out_sz, uint16_t handle)
{
    if (out_sz < 5)
        return 0;
    size_t len = 0;
    out[len++] = SSAP_MSG_VALUE_ACK;
    out[len++] = 0;
    len += put_u16(out + len, handle);
    out[len++] = 0x01; /* success */
    return len;
}
