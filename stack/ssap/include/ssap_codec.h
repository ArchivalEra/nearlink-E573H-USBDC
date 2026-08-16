/*
 * ssap_codec.h — SSAP (SparkLink Service Access Protocol) PDU codec.
 *
 * Byte-level encode/decode of SSAP PDUs per ssap_pkt.h (Apache-2.0,
 * OpenHarmony communication_nearlink_service). Ported for the WS73
 * NearLink dongle host stack. Zero OHOS deps.
 *
 * Frame: [msgCode 1B][msgCtrl 1B][payload...]
 *   msgCode low bits = opcode (0x01..0x14), high bit = reply mask (0x80)
 */

#ifndef SSAP_CODEC_H
#define SSAP_CODEC_H

#include <stdint.h>
#include <stddef.h>

/* ---- msg codes (SSAP_MsgCode_E) ---- */
enum {
    SSAP_MSG_ERROR_RSP            = 0x01,
    SSAP_MSG_EXCHANGE_INFO_REQ    = 0x02,
    SSAP_MSG_EXCHANGE_INFO_RSP    = 0x03,
    SSAP_MSG_FIND_STRUCTURE_REQ   = 0x04,
    SSAP_MSG_FIND_STRUCTURE_RSP   = 0x05,
    SSAP_MSG_FIND_STRUCTURE_BY_UUID_REQ = 0x06,
    SSAP_MSG_FIND_STRUCTURE_BY_UUID_RSP = 0x07,
    SSAP_MSG_READ_REQ             = 0x08,
    SSAP_MSG_READ_RSP             = 0x09,
    SSAP_MSG_READ_BY_UUID_REQ     = 0x0A,
    SSAP_MSG_READ_BY_UUID_RSP     = 0x0B,
    SSAP_MSG_WRITE_CMD            = 0x0C,
    SSAP_MSG_WRITE_REQ            = 0x0D,
    SSAP_MSG_WRITE_RSP            = 0x0E,
    SSAP_MSG_VALUE_NTF            = 0x0F,
    SSAP_MSG_VALUE_IND            = 0x10,
    SSAP_MSG_VALUE_ACK            = 0x11,
    SSAP_MSG_CALL_METHOD_CMD      = 0x12,
    SSAP_MSG_CALL_METHOD_REQ      = 0x13,
    SSAP_MSG_CALL_METHOD_RSP      = 0x14,
};

/* ---- trans types (SSAP_TransType_E) ---- */
#define SSAP_REPLY_MASK 0x80
enum {
    SSAP_TRANS_CMD = 0x01,
    SSAP_TRANS_REQ = 0x02 | SSAP_REPLY_MASK, /* 0x82 */
    SSAP_TRANS_RSP = 0x03,
    SSAP_TRANS_NOTI = 0x04,
    SSAP_TRANS_IND = 0x05 | SSAP_REPLY_MASK, /* 0x85 */
    SSAP_TRANS_ACK = 0x06,
};

/* ---- versions ---- */
enum {
    SSAP_VERSION_1_0 = 0x0001,
    SSAP_VERSION_1_1 = 0x0101,
    SSAP_VERSION_1_2 = 0x0201,
    SSAP_VERSION_1_3 = 0x0301,
};
#define SSAP_MTU_DEFAULT 251
#define SSAP_MTU_MAX     1024

/* ---- fragment ctrl ---- */
#define SSAP_CTRL_FRAG_BEGIN 0x00
#define SSAP_CTRL_FRAG_MID   0x01
#define SSAP_CTRL_FRAG_END   0x02
#define SSAP_CTRL_NO_FRAG    0x03

/* ---- find types ---- */
enum {
    SSAP_FIND_SERVICE_STRUCTURE = 0,
    SSAP_FIND_PRIMARY_SERVICE = 1,
    SSAP_FIND_REFERENCE_SERVICE = 2,
    SSAP_FIND_PROPERTY = 3,
    SSAP_FIND_METHOD = 4,
    SSAP_FIND_EVENT = 5,
};

/* ---- property operation bits ---- */
#define SSAP_OP_READ        0x01
#define SSAP_OP_WRITE_NO_RSP 0x02
#define SSAP_OP_WRITE        0x04
#define SSAP_OP_NOTIFY       0x08
#define SSAP_OP_INDICATE     0x10
#define SSAP_OP_BROADCAST    0x20

/* ---- special handles ---- */
#define SSAP_HANDLE_SERVICE_CHANGE 0x000E
#define SSAP_HANDLE_HASH           0x000F

/* ---- item types ---- */
enum {
    SSAP_ITEM_PRIMARY_SERVICE = 0x01,
    SSAP_ITEM_SECONDARY_SERVICE = 0x02,
    SSAP_ITEM_PROPERTY = 0x03,
    SSAP_ITEM_METHOD = 0x04,
    SSAP_ITEM_EVENT = 0x05,
    SSAP_ITEM_SERVICE_REFERENCE = 0x06,
    SSAP_ITEM_DESCRIPTOR = 0x07,
};

/* ---- UUID: 16-bit standard or 128-bit custom (base 0x37BEA880-FC70-11EA-B720-000000000000) ---- */
#define SSAP_UUID16_LEN   2
#define SSAP_UUID128_LEN  16

/* ---- codec API ---- */

/* Encode EXCHANGE_INFO_REQ/RSP (opcode 0x02/0x03).
 * msgCtrl bits: mtu(bit0), version(bit1), extMsgCtrl(bit2), reliable(3), fragment(4), multiProcessing(5). */
size_t ssap_encode_exchange_info(uint8_t *out, size_t out_sz,
                                 uint8_t opcode, uint8_t ctrl,
                                 uint16_t mtu, uint16_t version);

/* Decode EXCHANGE_INFO; returns payload len or -1. */
int ssap_decode_exchange_info(const uint8_t *pdu, size_t len,
                              uint16_t *mtu, uint16_t *version);

/* Encode FIND_STRUCTURE_REQ (0x04). find_type: 0..5; item_type 0..2; rsp_mode 0/1.
 * uuid may be NULL (2-byte standard) or 16-byte custom. */
size_t ssap_encode_find_struct_req(uint8_t *out, size_t out_sz,
                                   uint8_t find_type, uint8_t item_type,
                                   uint8_t rsp_mode, uint16_t start_h,
                                   uint16_t end_h, const uint8_t *uuid,
                                   uint8_t uuid_len);

/* Encode READ_REQ (0x08). items: array of (handle u16, type u8). */
size_t ssap_encode_read_req(uint8_t *out, size_t out_sz,
                            const uint16_t *handles, const uint8_t *types,
                            size_t count);

/* Encode WRITE_CMD (0x0C) / WRITE_REQ (0x0D). */
size_t ssap_encode_write(uint8_t *out, size_t out_sz, uint8_t opcode,
                         uint16_t handle, uint8_t type,
                         const uint8_t *value, uint16_t value_len);

/* Encode VALUE_NTF (0x0F) / VALUE_IND (0x10). type bit: 0=property, 1=event. */
size_t ssap_encode_value(uint8_t *out, size_t out_sz, uint8_t opcode,
                         uint8_t frag, uint8_t type,
                         uint16_t handle, const uint8_t *value, uint16_t value_len);

/* Encode ERROR_RSP (0x01). */
size_t ssap_encode_error_rsp(uint8_t *out, size_t out_sz,
                             uint8_t msg_code_req, uint16_t handle, uint8_t err_code);

/* Encode VALUE_ACK (0x11). */
size_t ssap_encode_value_ack(uint8_t *out, size_t out_sz, uint16_t handle);

/* Get opcode from msgCode (low 7 bits). */
static inline uint8_t ssap_opcode_of(uint8_t msg_code) { return msg_code & 0x7F; }
/* Get trans type from opcode. */
uint8_t ssap_trans_type_of(uint8_t opcode);

#endif /* SSAP_CODEC_H */
