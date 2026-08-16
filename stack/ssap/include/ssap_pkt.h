/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef SSAP_PKT_H
#define SSAP_PKT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#define SSAP_ERROR_RSP_ERRCODE_OFFSET 5     // 错误响应报文错误码偏移

#define SSAP_EXCHANGE_INFO_PKT_LEN 6        // 信息交换报文长度
#define SSAP_STACK_MTU_DEFAULT 251          // 星闪服务MTU缺省值
#define SSAP_STACK_MTU_MAX 1024             // 星闪服务MTU最大值
#define SSAP_EXCHANGE_VERSION SSAP_VERSION_1_3 // 星闪服务Version号
#define SSAP_PDU_BASE_LEN 2                 // msgCode+ctl长度
#define SSAP_HANDLE_LEN sizeof(uint16_t)

// 服务发现报文相关
// 信息指示，用于mix返回
#define SSAP_FIND_INFO_INDICATION_LEN sizeof(uint8_t)

#define SSAP_FIND_PRIMARY_SERVICE_MEMBER_LEN sizeof(uint8_t)
#define SSAP_FIND_PRIMARY_SERVICE_BASE_LEN (SSAP_HANDLE_LEN + SSAP_HANDLE_LEN + SSAP_FIND_PRIMARY_SERVICE_MEMBER_LEN)
#define SSAP_FIND_PRIMARY_SERVICE_STD_LEN   (SSAP_FIND_PRIMARY_SERVICE_BASE_LEN + SSAP_UUID16_LEN)
#define SSAP_FIND_PRIMARY_SERVICE_CUS_LEN   (SSAP_FIND_PRIMARY_SERVICE_BASE_LEN + SSAP_UUID128_LEN)

#define SSAP_FIND_OPERATION_LEN sizeof(uint32_t)
#define SSAP_FIND_DESCRIPTOR_COUNT_LEN sizeof(uint8_t)
#define SSAP_FIND_PROPERTY_BASE_LEN (SSAP_HANDLE_LEN + SSAP_FIND_OPERATION_LEN + SSAP_FIND_DESCRIPTOR_COUNT_LEN)
#define SSAP_FIND_PROPERTY_STD_LEN   (SSAP_FIND_PROPERTY_BASE_LEN + SSAP_UUID16_LEN)
#define SSAP_FIND_PROPERTY_CUS_LEN   (SSAP_FIND_PROPERTY_BASE_LEN + SSAP_UUID128_LEN)
#define SSAP_FIND_METHOD_EXTRA_LEN (SSAP_FIND_OPERATION_LEN + SSAP_FIND_DESCRIPTOR_COUNT_LEN)
#define SSAP_FIND_METHOD_STD_LEN   (SSAP_HANDLE_LEN + SSAP_UUID16_LEN + SSAP_FIND_METHOD_EXTRA_LEN)
#define SSAP_FIND_METHOD_CUS_LEN   (SSAP_HANDLE_LEN + SSAP_UUID128_LEN + SSAP_FIND_METHOD_EXTRA_LEN)

#define SSAP_FIND_STRUCTURE_TYPE_LEN sizeof(uint8_t)
#define SSAP_FIND_STRUCTURE_BASE_LNE (SSAP_HANDLE_LEN + SSAP_FIND_STRUCTURE_TYPE_LEN + SSAP_FIND_DESCRIPTOR_COUNT_LEN)

#define SSAP_FIND_STRUCTURE_STD_LEN    (SSAP_FIND_STRUCTURE_BASE_LNE + SSAP_UUID16_LEN + SSAP_FIND_OPERATION_LEN)
#define SSAP_FIND_STRUCTURE_CUS_LEN    (SSAP_FIND_STRUCTURE_BASE_LNE + SSAP_UUID128_LEN + SSAP_FIND_OPERATION_LEN)

#define SSAP_FIND_INFO_INDICATION_STD 0
#define SSAP_FIND_INFO_INDICATION_CUS 1

#define SSAP_ACK_RECV_FAIL 0
#define SSAP_ACK_RECV_SUCCESS 1

// READ报文相关
#define SSAP_READ_TYPE_LEN 1
#define SSAP_READ_REQ_PDU_LEN (SSAP_PDU_BASE_LEN + SSAP_HANDLE_LEN + SSAP_READ_TYPE_LEN)
#define SSAP_READ_RSP_CONTROL_SUCCESS 0     // 读取响应成功控制码
#define SSAP_READ_RSP_CONTROL_ERROR 1       // 读取响应错误控制码
#define SSAP_READ_RSP_ITEMREAD_FAILED 0     // 读取响应条目读取错误
#define SSAP_READ_RSP_ITEMREAD_SUCCESS 1    // 读取响应条目读取正常
#define SSAP_READ_BY_STANDARD_UUID_REQ_PKT_LEN \
    (SSAP_PDU_BASE_LEN + SSAP_HANDLE_LEN + SSAP_HANDLE_LEN + SSAP_READ_TYPE_LEN + SSAP_UUID16_LEN)
#define SSAP_READ_BY_CUSTOM_UUID_REQ_PKT_LEN   \
    (SSAP_PDU_BASE_LEN + SSAP_HANDLE_LEN + SSAP_HANDLE_LEN + SSAP_READ_TYPE_LEN + SSAP_UUID128_LEN)
#define SSAP_READ_BY_UUID_REQ_UUID_OFFSET (SSAP_PDU_BASE_LEN + SSAP_HANDLE_LEN + SSAP_HANDLE_LEN + SSAP_READ_TYPE_LEN)
#define SSAP_READ_BY_STANDARD_UUID_CONTROL 0x00      // 标准uuid读取请求控制码
#define SSAP_READ_BY_CUSTOM_UUID_CONTROL 0x01        // 自定义uuid读取请求控制码
#define SSAP_TYPE_DATA 0                             // 读取类型为数据域中的数据

#define SSAP_READ_RSP_DATA_OFFSET SSAP_PDU_BASE_LEN
#define SSAP_READ_BY_UUID_RSP_DATA_OFFSET (SSAP_PDU_BASE_LEN + SSAP_HANDLE_LEN)
#define SSAP_INDICATION_LEN sizeof(uint16_t)    // 数据指示长度
#define SSAP_READ_BY_UUID_RSP_MULTI_CONTROL 4   // 通过uuid读取响应报文多实例控制校验
#define SSAP_READ_BY_UUID_RSP_ERR_CONTROL 8     // 通过uuid读取响应报文错误控制校验
#define SSAP_READ_BY_UUID_RSP_ERROR_CONTROL 15  // 通过uuid读取响应报文错误指示控制校验

// WRITE报文相关
#define SSAP_WRITE_TYPE_LEN 1
#define SSAP_WRITE_ERROR_NUM 1
#define SSAP_WRITE_ERRORCODE 1
#define SSAP_WRITE_SUB_ITEM_COUNT_LEN 1
#define SSAP_WRITE_SUB_ITEM_DATA_LENGTH_LEN 2
#define SSAP_WRITE_REQ_DATA_OFFSET (SSAP_PDU_BASE_LEN + SSAP_HANDLE_LEN + SSAP_WRITE_TYPE_LEN)
#define SSAP_WRITE_RSP_PDU_LEN      SSAP_PDU_BASE_LEN
#define SSAP_WRITE_RSP_DATA_OFFSET (SSAP_PDU_BASE_LEN + SSAP_HANDLE_LEN + SSAP_WRITE_TYPE_LEN)
#define SSAP_WRITE_RSP_ERROR_OFFSET (SSAP_PDU_BASE_LEN + SSAP_WRITE_ERROR_NUM + SSAP_HANDLE_LEN + SSAP_WRITE_ERRORCODE)
#define SSAPS_WRITE_CMD_DATA_OFFSET (SSAP_PDU_BASE_LEN + SSAP_HANDLE_LEN + SSAP_WRITE_TYPE_LEN)
#define SSAP_WRITE_REQ_NEED_ORIGIN_RET 0x20     // 写入请求原值返回校验控制校验码

// 通知指示相关
#define SSAP_VALUE_ACK_PDU_MIN_LEN 3            // 确认报文最小长度
#define SSAP_VALUE_IND_PDU_MIN_LEN 7            // 指示报文最小长度
#define SSAP_VALUE_NTF_PDU_MIN_LEN 7            // 通知报文最小长度
#define SSAP_VALUE_IND_PDU_BASE_LEN 6           // 指示报文最小长度
#define SSAP_VALUE_NTF_PDU_BASE_LEN 6           // 通知报文最小长度
#define SSAP_VALUE_ITEM_LEN 2

typedef enum SSAP_TransType: uint8_t {
    SSAP_TRANS_INVALID = 0x00,
    SSAP_REPLY_MASK = 0x80,
    SSAP_TRANS_CMD  = 0x01,
    SSAP_TRANS_REQ  = 0x02 | SSAP_REPLY_MASK,
    SSAP_TRANS_RSP  = 0x03,
    SSAP_TRANS_NOTI = 0x04,
    SSAP_TRANS_IND  = 0x05 | SSAP_REPLY_MASK,
    SSAP_TRANS_ACK  = 0x06,
} SSAP_TransType_E;

/**
 * @brief SSAP opcode
 */
typedef enum SSAP_MsgCode {
    SSAP_ERROR_RSP = 0x01,
    SSAP_EXCHANGE_INFO_REQ,
    SSAP_EXCHANGE_INFO_RSP,
    SSAP_FIND_STRUCTURE_REQ,
    SSAP_FIND_STRUCTURE_RSP,
    SSAP_FIND_STRUCTURE_BY_UUID_REQ,
    SSAP_FIND_STRUCTURE_BY_UUID_RSP,
    SSAP_READ_REQ,
    SSAP_READ_RSP,
    SSAP_READ_BY_UUID_REQ,
    SSAP_READ_BY_UUID_RSP,
    SSAP_WRITE_CMD,
    SSAP_WRITE_REQ,
    SSAP_WRITE_RSP,
    SSAP_VALUE_NTF,
    SSAP_VALUE_IND,
    SSAP_VALUE_ACK,
    SSAP_CALL_METHOD_CMD,
    SSAP_CALL_METHOD_REQ,
    SSAP_CALL_METHOD_RSP,
    SSAP_CODE_MAX,
} SSAP_MsgCode_E;

/**
 * @brief SSAP 版本号，字节0：大版本号；字节1：小版本号
 */
typedef enum SSAP_Version {
    SSAP_VERSION_1_0 = 0x0001,
    SSAP_VERSION_1_1 = 0x0101,
    SSAP_VERSION_1_2 = 0x0201,
    SSAP_VERSION_1_3 = 0x0301,
} SSAP_Version_E;

#define SSAP_CTRL_FRAG_BEGIN 0b00   // 起始数据包
#define SSAP_CTRL_FRAG_MID 0b01     // 中间数据包
#define SSAP_CTRL_FRAG_END 0b10     // 结束数据包
#define SSAP_CTRL_NO_FRAG 0b11      // 单个完整数据包

#define SSAP_CTRL_WRITE_SUCCESS 0b00 // 写入成功
#define SSAP_CTRL_WRITE_PART 0b01    // 部分写入
#define SSAP_CTRL_WRITE_CANCEL 0b10  // 取消写入

#define SSAP_CTRL_WRITE_INSTANT 0b00 // 立即写入
#define SSAP_WRITE_ERROR_SINGLE_NUM 1

#define SSAP_CTRL_MULTI_SINGLE 0  // 单值读取
#define SSAP_CTRL_MULTI_MULTI 1   // 多值读取

/**
 * @brief SSAP_ERROR_RSP
 *   0               1               2               3
 *   0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |     0x01      |     ctrl      | req msg code  |     handle
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *                  |   err code    |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
typedef struct SSAP_PduErrRsp {
    uint8_t msgCode;
    uint8_t msgCtrl;                // 保留
    uint8_t msgCodeReq;             // 产生错误的请求消息码
    uint16_t errHandle;             // 产生错误的请求句柄
    uint8_t errorCode;              // 错误码
} __attribute__((packed)) SSAP_PduErrRsp_S;

typedef enum SSAP_PduErrCode {
    SSAP_ERRCODE_SUCCESS = 0x0,         // 成功
    SSAP_ERRCODE_INVALID_PDU,           // PDU无效
    SSAP_ERRCODE_UNSUPPORT_PDU,         // PDU不支持
    SSAP_ERRCODE_UNKNOWN,               // 未知错误
    SSAP_ERRCODE_INVALID_HANDLE,        // 无效句柄
    SSAP_ERRCODE_NO_RESOURCE,           // 资源不足
    SSAP_ERRCODE_FORBID_READ,           // 禁止读取
    SSAP_ERRCODE_FORBID_WRITE,          // 禁止写入
    SSAP_ERRCODE_UNAUTHENTICATED,       // 客户端未认证
    SSAP_ERRCODE_UNAUTHORIZED,          // 客户端未授权
    SSAP_ERRCODE_UNENCRYPTED,           // 承载未加密
    SSAP_ERRCODE_ITEM_INEXIST,          // 未找到条目
    SSAP_ERRCODE_METHOD_ACCESS,         // 方法访问错误
    SSAP_ERRCODE_DATA_TYPE,             // 数据类型错误
    SSAP_ERRCODE_DATA_LENGTH,           // 数据值长度错误
    SSAP_ERRCODE_DATA_RANGE,            // 值超出范围
    SSAP_ERRCODE_SERVER_FRAG,           // 服务端不支持分包
    SSAP_ERRCODE_ITEM_OVER_LIMIT,       // 条目数量超限

    // 上层应用自定义错误码
    SSAP_ERRCODE_TIMEOUT = 0xB0,        // 请求超时
    SSAP_ERRCODE_MAX,
} SSAP_PduErrCode_E;

/**
 * @brief SSAP_EXCHANGE_INFO_REQ/SSAP_EXCHANGE_INFO_RSP结构
 *   0               1               2               3
 *   0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |   0x02/0x03   |     ctrl      |              MTU              |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  !           VERSION             |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
*/
typedef struct SSAP_PduExchangePkt {
    uint8_t msgCode;
    struct {
        uint8_t mtu : 1;             // 标志是否携带mtu信息
        uint8_t version : 1;         // 标志是否携带version信息
        uint8_t extMsgCtrl : 1;      // 标志是否携带扩展消息控制码
        uint8_t reliable : 1;        // 标志是否支持SSAP可靠模式传输通道
        uint8_t fragment : 1;        // 标志是否支持SSAP分包
        uint8_t multiProcessing : 1; // 标志是否支持多值处理
        uint8_t resv : 2;
    } ctrl;
    uint16_t msgMtu;
    uint16_t msgVersion;
} __attribute__((packed)) SSAP_PduExchangePkt_S;

/**
 * @brief SSAP_FIND_STRUCTURE_REQ/SSAP_FIND_STRUCTURE_BY_UUID_REQ结构
 *   0               1               2               3
 *   0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |   0x04/0x06   |     ctrl      |        start handle           |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  !          end handle           |       uuid(length 2/16)...
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
typedef struct SSAP_PduFindStructReq {
    uint8_t msgCode;
    struct {
        uint8_t findType : 3;       // 查找类型,服务结构(0b000)/首要服务(0b001)/引用服务(0b010)/属性(0b011)/方法(0b100)/事件(0b101)
        uint8_t itemType : 2;       // 条目类型,仅标准条目(0b00)/厂商自定义(0b01)/混合(0b10)/RFU(0b11)
        uint8_t rspMode : 1;        // 响应类型,0-单次请求,一次响应;1-单次请求,多次响应
        uint8_t resv : 2;
    } ctrl;
    uint16_t startHandle;
    uint16_t endHandle;
    uint8_t uuid[0];                // 变长,根据pdu总长度减去其余字段长度,来计算uuid长度
} __attribute__((packed)) SSAP_PduFindStructReq_S;

/**
 * @brief SSAP_FIND_STRUCTURE_RSP/SSAP_FIND_STRUCTURE_BY_UUID_RSP结构
 *   0               1               2               3
 *   0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |   0x05/0x07   |     ctrl      |        data...
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
typedef struct SSAP_PduFindStructRsp {
    uint8_t msgCode;
    struct {
        uint8_t fragment : 2;
        uint8_t itemType : 2;
        uint8_t resv : 4;
    } ctrl;
    uint8_t data[0];
} __attribute__((packed)) SSAP_PduFindStructRsp_S;

typedef struct SSAP_FindInfoIndicator {
    uint8_t count : 7;
    uint8_t type : 1;
} __attribute__((packed)) SSAP_FindInfoIndicator_S;

/**
 * @brief SSAP_READ_REQ结构
 *   0               1               2               3
 *   0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |     0x08      |     ctrl      |           items...
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
typedef struct SSAP_PduReadReq {
    uint8_t msgCode;
    union {
        uint8_t msgCtrl;
        struct {
            uint8_t fragment : 2;       // 分片标记
            uint8_t resv : 6;
        } ctrl;
    };
    uint8_t items[0];
} __attribute__((packed)) SSAP_PduReadReq_S;

typedef struct Ssap_PduReadReqItem {
    uint16_t handle;
    uint8_t type;                   // 0-数据域中的数据值，其它匹配描述符类型
} __attribute__((packed)) Ssap_PduReadReqItem_S;

/**
 * @brief SSAP_READ_RSP结构
 *   0               1               2               3
 *   0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |     0x09      |     ctrl      |           items...
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
typedef struct SSAP_PduReadRsp {
    uint8_t msgCode;
    union {
        uint8_t msgCtrl;
        struct {
            uint8_t fragment : 2;       // 分片标记
            uint8_t multi : 1;          // 0表示单个值，1表示多个值
            uint8_t error : 1;          // 是否包含error，0表示没有错误，1表示有错误
            uint8_t resv : 4;
        } ctrl;
    };
    uint8_t items[0];               // 若读取单个值，直接返回读取的value；若读取多个值，返回多个struct ssap_pdu_read_rsp_item
} __attribute__((packed)) SSAP_PduReadRsp_S;

typedef struct SSAP_PduReadRspItem {
    uint16_t length : 15;           // 若读取正常，则表示数据长度；若读取失败，则保存错误码
    uint16_t success : 1;           // 1：读取正常，0：读取失败
    uint8_t value[0];               // 长度为length的数据值
} __attribute__((packed)) SSAP_PduReadRspItem_S;

/**
 * @brief SSAP_READ_BY_UUID_REQ结构
 *   0               1               2               3
 *   0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |     0x0A      |     ctrl      |        start handle           |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  !          end handle           |      type     |     uuid(length 2/16)...
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
typedef struct SSAP_PduReadByUuidReq {
    uint8_t msgCode;
    struct {
        uint8_t uuidType : 1;       // 0：标准UUID，1：自定义UUID
        uint8_t resv : 7;
    } ctrl;
    uint16_t handleStart;           // 查找的起始句柄
    uint16_t handleEnd;             // 查找的结束句柄
    uint8_t dataType;               // 读取的数据类型
    uint8_t uuid[0];                // 指定属性、方法或者事件的UUID，长度为2或16
} __attribute__((packed)) SSAP_PduReadByUuidReq_S;

/**
 * @brief SSAP_READ_BY_UUID_RSP结构
 *   0               1               2               3
 *   0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |     0x0B      |     ctrl      |           items...
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
typedef struct SSAP_PduReadByUuidRsp {
    uint8_t msgCode;
    union {
        uint8_t msgCtrl;
        struct {
            uint8_t fragment : 2;       // 分片标记
            uint8_t multi : 1;          // 0表示单个实例，1表示多个实例
            uint8_t error : 1;          // 是否包含error
            uint8_t : 4;
        } ctrl;
    };
    uint8_t items[0];               // 若读取单个值，返回{handle,value}；若读取多个值，返回多个struct ssap_pdu_read_rsp_item
} __attribute__((packed)) SSAP_PduReadByUuidRsp_S;

typedef struct SSAP_PduReadByUuidRspItem {
    uint16_t handle;
    uint16_t length : 15;           // 若读取正常，则表示数据长度，若读取失败，则保存错误码
    uint16_t success : 1;           // 1：读取正常，0：读取失败
    uint8_t value[0];               // 长度为length的数据值
} __attribute__((packed)) SSAP_PduReadByUuidRspItem_S;

typedef struct SSAP_PduReadByUuidRspSingleItem {
    uint16_t handle;
    uint8_t value[0];               // 长度为length的数据值
} __attribute__((packed)) SSAP_PduReadByUuidRspSingleItem_S;

/**
 * @brief SSAP_WRITE_CMD结构
 *   0               1               2               3
 *   0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |     0x0C      |     ctrl      |           items...
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
typedef struct SSAP_PduWriteCmd {
    uint8_t msgCode;
    union {
        uint8_t msgCtrl;
        struct {
            uint8_t fragment : 2;       // 分片标记
            uint8_t multi : 1;          // 0表示单个值，1表示多个值
            uint8_t oper : 2;           // 写入操作指示，0b00表示立即写入，0b01表示值未完成传输，需继续传输后续值，0b10表示取消写入
            uint8_t recv : 3;
        } ctrl;
    };
    uint8_t items[0];                   // 写入的内容
} __attribute__((packed)) SSAP_PduWriteCmd_S;

typedef struct SSAP_PduWriteCmdItem {
    uint16_t handle;
    uint8_t type;
    uint8_t value[0];
} __attribute__((packed)) SSAP_PduWriteCmdItem_S;

/**
 * @brief SSAP_WRITE_REQ结构
 *   0               1               2               3
 *   0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |     0x0D      |     ctrl      |           items...
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
typedef struct SSAP_PduWriteReq {
    uint8_t msgCode;
    union {
        uint8_t msgCtrl;
        struct {
            uint8_t fragment : 2;       // 分片标记
            uint8_t multi : 1;          // 0表示单个值，1表示多个值
            uint8_t oper : 2;           // 写入操作指示，0b00表示立即写入，0b01表示值未完成传输，需继续传输后续值，0b10表示取消写入
            uint8_t verify : 1;         // 0=不需原值返回校验；1=需原值返回校验
            uint8_t resv : 2;
        } ctrl;
    };
    uint8_t items[0];
} __attribute__((packed)) SSAP_PduWriteReq_S;

typedef struct SSAP_PduWriteReqItem {
    uint16_t handle;                // 写入的句柄
    uint8_t type;                   // 写入的数据类型
    uint8_t value[0];               // 写入的数据值
} __attribute__((packed)) SSAP_PduWriteReqItem_S;

typedef struct SSAP_PduWriteMultiSubItem {
    uint8_t type;                   // 写入的数据类型
    uint16_t len;                   // 写入的数据长度
    uint8_t value[0];               // 写入的数据值
} __attribute__((packed)) SSAP_PduWriteMultiSubItem_S;

typedef struct SSAP_PduWriteMultiItem {
    uint16_t handle;                                    // 写入的句柄
    uint8_t subItemCount;                               // 写入的元组数
    SSAP_PduWriteMultiSubItem_S subItem[0];          // 写入的{数据类型,数据长度,数据值}
} __attribute__((packed)) SSAP_PduWriteMultiItem_S;

/**
 * @brief SSAP_WRITE_RSP结构
 *   0               1               2               3
 *   0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |     0x0E      |     ctrl      |           items...
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
typedef struct SSAP_PduWriteRsp {
    uint8_t msgCode;
    union {
        uint8_t msgCtrl;
        struct {
            uint8_t fragment : 2;       // 分片标记
            uint8_t result : 2;         // 指示服务端写入结果；写入成功（0b00）部分写入成功（0b01）取消写入（0b10）
            uint8_t resv : 4;
        } ctrl;
    };
    uint8_t items[0];               // 多值响应时为多个(handle, errorCode)
} __attribute__((packed)) SSAP_PduWriteRsp_S;

typedef struct SSAP_PduWriteRspErrorItem {
    uint16_t handle;                // 写入句柄
    uint8_t errorCode;              // 错误码
} __attribute__((packed)) SSAP_PduWriteRspErrorItem_S;

typedef struct SSAP_PduWriteRspErrorInfo {
    uint8_t errorNum;               // 错误数量
    SSAP_PduWriteRspErrorItem_S errList[0];
} __attribute__((packed)) SSAP_PduWriteRspErrorInfo_S;

typedef struct SSAP_PduWriteRspItem {
    uint16_t handle;                // 写入句柄
    uint8_t type;                   // 写入数据类型
    uint8_t value[0];               // 写入的数据值
} __attribute__((packed)) SSAP_PduWriteRspItem_S;

/**
 * @brief SSAP_VALUE_NTF或SSAP_VALUE_IND结构
 *   0               1               2               3
 *   0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |   0x0F/0x10   |     ctrl      |           items...
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
typedef struct SSAP_PduValue {
    uint8_t msgCode;
    union {
        uint8_t msgCtrl;
        struct {
            uint8_t fragment : 2;   // 分片标记
            uint8_t type : 1;       // 0=属性值的通知/指示；1=事件的通知/指示
            uint8_t resv : 5;
        } ctrl;
    };
    uint8_t items[0];
} __attribute__((packed)) SSAP_PduValueNtf_S, SSAP_PduValueInd_S;

typedef struct SSAP_PduValueItem {
    uint16_t handle;
    uint16_t length;
    uint8_t value[0];
} __attribute__((packed)) SSAP_PduValueNtfItem_S, SSAP_PduValueIndItem_S;

/**
 * @brief SSAP_VALUE_ACK的消息头部
 *   0               1               2               3
 *   0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |     0x11      |     ctrl      |           items...
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
typedef struct SSAP_PduValueAck {
    uint8_t msgCode;
    union {
        uint8_t msgCtrl;
        struct {
            uint8_t fragment : 2;       // 分片标记
            uint8_t type : 1;           // 0=属性值的指示确认；1=事件的指示确认
            uint8_t resv : 5;
        } ctrl;
    };
    uint8_t result[0];              // 确认标志：1字节，表示客户端的接收确认；0=接收失败；1=接收成功
} __attribute__((packed)) SSAP_PduValueAck_S;

/**
 * @brief SSAP_CALL_METHOD_CMD/SSAP_CALL_METHOD_REQ结构
 *   0               1               2               3
 *   0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |   0x12/0x13   |     ctrl      |            handle             |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  !           params...
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
*/
typedef struct SSAP_PduCallMethod {
    uint8_t msgCode;
    union {
        uint8_t msgCtrl;
        struct {
            uint8_t fragment : 2;
            uint8_t resv : 6;
        } ctrl;
    };
    uint16_t handle;
    uint8_t param[0];
} __attribute__((packed)) SSAP_PduCallMethodCmd_S, SSAP_PduCallMethodReq_S;

/**
 * @brief SSAP_CALL_METHOD_RSP结构
 *   0               1               2               3
 *   0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |     0x14      |     ctrl      |           result...
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
typedef struct SSAP_PduCallMethodRsp {
    uint8_t msgCode;
    union {
        uint8_t msgCtrl;
        struct {
            uint8_t fragment : 2;
            uint8_t resv : 6;
        } ctrl;
    };
    uint8_t result[0];
} __attribute__((packed)) SSAP_PduCallMethodRsp_S;
#ifdef __cplusplus
}
#endif

#endif