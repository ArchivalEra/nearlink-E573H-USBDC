/*
 * ssap_link.h — SSAP connection establishment helper for WS73.
 *
 * Wraps the DLI command sequence (per OHOS CM layer analysis):
 *   connect: DLI_CREATE_CONNECTION 0x1401 → wait CONN_COMPLETE 0x0015
 *            → DLI_READ_REMOTE_VERSION 0x1802 → DLI_SET_DATA_LEN 0x1804
 *   SSAP then rides TCID 0x0A (SMTC) via hwsle_transport.
 *
 * State machine: IDLE → CONNECTING → CONNECTED → (SSAP active) → DISCONNECTING.
 */

#ifndef SSAP_LINK_H
#define SSAP_LINK_H

#include <stdint.h>
#include <stddef.h>

/* DLI opcodes used by the link layer (from dli_opcode.h / our hardware) */
#define DLI_CREATE_CONNECTION      0x1401
#define DLI_CANCEL_CREATE_CONNECTION 0x1402
#define DLI_DISCONNECT             0x1403
#define DLI_READ_REMOTE_VERSION    0x1802
#define DLI_SET_DATA_LEN           0x1804
#define DLI_CONNECTION_UPDATE      0x1807

/* events */
#define DLI_CMD_STATUS_EVT         0x0001
#define DLI_CMD_COMPLETE_EVT       0x0002
#define DLI_CONNECTION_COMPLETE_EVT 0x0015
#define DLI_DISCONNECTION_COMPLETE_EVT 0x0005

/* DLI address length */
#ifndef SLE_ADDR_LEN
#define SLE_ADDR_LEN 6
#endif

/* DLI_ConnectionCreateParam fields (dli_cmd_struct.h) */
typedef struct {
    uint8_t  version;         /* protocol version */
    uint8_t  localIndex;
    uint8_t  peerAddr[SLE_ADDR_LEN]; /* 6 bytes */
    uint8_t  peerAddrType;
    uint16_t connIntervalMin; /* 1.25ms units; 0x64 default = 125ms */
    uint16_t connIntervalMax;
    uint16_t maxLatency;
    uint16_t supervisionTimeout; /* 10ms units; 0x1F4 = 5s */
    uint16_t minCeLength;
    uint16_t maxCeLength;
    uint8_t  scanInterval;    /* 0.125ms units */
    uint8_t  scanWindow;
    uint8_t  scanType;        /* 0 passive / 1 active */
    uint8_t  initiatePhys;    /* bit0 = 1M */
    uint8_t  filterPolicy;
    uint8_t  ownAddrType;
} ssap_conn_param_t;

/* link state */
typedef enum {
    SSAP_LINK_IDLE,
    SSAP_LINK_CONNECTING,
    SSAP_LINK_CONNECTED,
    SSAP_LINK_DISCONNECTING,
} ssap_link_state_t;

typedef struct {
    ssap_link_state_t state;
    uint16_t conn_handle;   /* from CONN_COMPLETE */
    uint8_t  role;          /* 0=G, 1=T */
    uint16_t conn_interval;
    uint16_t supervision_timeout;
    /* callbacks */
    void (*on_connected)(uint16_t conn_handle, void *ctx);
    void (*on_disconnected)(uint16_t reason, void *ctx);
    void *ctx;
} ssap_link_t;

void ssap_link_init(ssap_link_t *link);
/* Start connecting to peer (6-byte addr). Returns 0 if command sent. */
int ssap_link_connect(ssap_link_t *link, const uint8_t peer_addr[SLE_ADDR_LEN],
                      const ssap_conn_param_t *param);
int ssap_link_disconnect(ssap_link_t *link);
/* Feed a DLI event (from hwsle_transport event parsing); updates state. */
void ssap_link_on_event(ssap_link_t *link, uint16_t opcode, const uint8_t *data, size_t len);

#endif /* SSAP_LINK_H */
