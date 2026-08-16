/*
 * ssap_server.h — SSAP server core for the WS73 NearLink host stack.
 *
 * Service table + request dispatch, per the OHOS ssaps_server.c model
 * (Apache-2.0). Slim reimplementation: a service has properties/methods/
 * events with handles and operation bits; reads/writes/notifies dispatch
 * to app callbacks.
 */

#ifndef SSAP_SERVER_H
#define SSAP_SERVER_H

#include <stdint.h>
#include <stddef.h>

#define SSAP_HANDLE_START 0x0001
#define SSAP_HANDLE_SERVICE_CHANGE 0x000E
#define SSAP_HANDLE_HASH           0x000F
#define SSAP_MAX_DESCRIPTORS 8
#define SSAP_MAX_PROPERTIES 32
#define SSAP_MAX_VALUE_LEN 1024

typedef enum {
    SSAP_ITEM_PRIMARY_SERVICE = 0x01,
    SSAP_ITEM_SECONDARY_SERVICE = 0x02,
    SSAP_ITEM_PROPERTY = 0x03,
    SSAP_ITEM_METHOD = 0x04,
    SSAP_ITEM_EVENT = 0x05,
} ssap_item_type_t;

/* property callbacks */
typedef int (*ssap_read_cb)(uint16_t handle, uint8_t *out, uint16_t *out_len,
                            uint16_t max_len);
typedef int (*ssap_write_cb)(uint16_t handle, const uint8_t *value, uint16_t len);
typedef int (*ssap_notify_cb)(uint16_t handle, uint8_t *out, uint16_t *out_len,
                              uint16_t max_len);

typedef struct {
    uint16_t handle;
    uint16_t uuid16;            /* 0 = custom uuid (128-bit in service) */
    uint8_t  type;              /* ssap_item_type_t */
    uint32_t operation;         /* SSAP_OP_* bits */
    uint8_t  permission;        /* AUTH/ENCRYPT/AUTHZ */
    ssap_read_cb  read_cb;
    ssap_write_cb write_cb;
} ssap_property_t;

typedef struct {
    uint16_t start_handle;
    uint16_t end_handle;
    uint16_t uuid16;
    ssap_property_t properties[SSAP_MAX_PROPERTIES];
    uint8_t  property_count;
    uint8_t  is_primary;
} ssap_service_t;

typedef struct {
    ssap_service_t services[8];
    uint8_t service_count;
    uint16_t next_handle;
    /* connection state */
    uint16_t mtu;
    uint16_t version;
    uint8_t  connected;
    /* notify callback for VALUE_NTF/IND delivery to a peer */
    int (*send_frame)(const uint8_t *pdu, size_t len);
} ssap_server_t;

void ssap_server_init(ssap_server_t *srv, int (*send_frame)(const uint8_t *, size_t));

/* Add a service (primary=1); returns start handle or 0. */
uint16_t ssap_server_add_service(ssap_server_t *srv, uint16_t uuid16, uint8_t is_primary);

/* Add a property to the service starting at svc_handle; returns its handle or 0. */
uint16_t ssap_server_add_property(ssap_server_t *srv, uint16_t svc_handle,
                                  uint16_t uuid16, uint32_t operation,
                                  uint8_t permission, ssap_read_cb rc, ssap_write_cb wc);

/* Handle an incoming SSAP PDU (msgCode already validated). Returns 0 on handled. */
int ssap_server_dispatch(ssap_server_t *srv, const uint8_t *pdu, size_t len);

/* Send a notify/indicate to the peer. */
int ssap_server_notify(ssap_server_t *srv, uint16_t handle,
                       const uint8_t *value, uint16_t len, uint8_t indicate);

#endif /* SSAP_SERVER_H */
