/*
 * ssap_link.c — SSAP connection helper implementation.
 *
 * Builds DLI_ConnectionCreateParam and drives the connect/disconnect
 * state machine from DLI events. Sends via hwsle_transport HCI cmds.
 */

#include "ssap_link.h"
#include "hwsle_transport.h"
#include <string.h>

void ssap_link_init(ssap_link_t *link)
{
    memset(link, 0, sizeof(*link));
    link->state = SSAP_LINK_IDLE;
}

static int put_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)(v >> 8);
    return 2;
}

int ssap_link_connect(ssap_link_t *link, const uint8_t peer_addr[SLE_ADDR_LEN],
                      const ssap_conn_param_t *param)
{
    if (!link || !peer_addr || link->state != SSAP_LINK_IDLE)
        return -1;

    uint8_t params[64];
    size_t n = 0;
    params[n++] = param ? param->version : 1;
    params[n++] = param ? param->localIndex : 0;
    memcpy(params + n, peer_addr, SLE_ADDR_LEN);
    n += SLE_ADDR_LEN;
    params[n++] = param ? param->peerAddrType : 0;
    n += put_u16(params + n, param ? param->connIntervalMin : 0x64);
    n += put_u16(params + n, param ? param->connIntervalMax : 0x64);
    n += put_u16(params + n, param ? param->maxLatency : 0);
    n += put_u16(params + n, param ? param->supervisionTimeout : 0x1F4);
    n += put_u16(params + n, param ? param->minCeLength : 0);
    n += put_u16(params + n, param ? param->maxCeLength : 0);
    params[n++] = param ? param->scanInterval : 0x20;
    params[n++] = param ? param->scanWindow : 0x20;
    params[n++] = param ? param->scanType : 1;
    params[n++] = param ? param->initiatePhys : 0x01;
    params[n++] = param ? param->filterPolicy : 0;
    params[n++] = param ? param->ownAddrType : 0;

    int r = hwsle_transport_send_hci_cmd(DLI_CREATE_CONNECTION, params, n);
    if (r > 0) {
        link->state = SSAP_LINK_CONNECTING;
        return 0;
    }
    return -1;
}

int ssap_link_disconnect(ssap_link_t *link)
{
    if (!link || link->state != SSAP_LINK_CONNECTED)
        return -1;
    uint8_t params[3];
    params[0] = (uint8_t)(link->conn_handle & 0xFF);
    params[1] = (uint8_t)(link->conn_handle >> 8);
    params[2] = 0x13; /* local host terminated */
    int r = hwsle_transport_send_hci_cmd(DLI_DISCONNECT, params, sizeof(params));
    if (r > 0) {
        link->state = SSAP_LINK_DISCONNECTING;
        return 0;
    }
    return -1;
}

void ssap_link_on_event(ssap_link_t *link, uint16_t opcode, const uint8_t *data, size_t len)
{
    if (!link)
        return;
    switch (opcode) {
    case DLI_CONNECTION_COMPLETE_EVT: {
        /* DLI_ConnectionCompleteEvt: role(1) connHandle(2) connInterval(2) ... */
        if (len < 5)
            return;
        link->role = data[0];
        link->conn_handle = (uint16_t)(data[1] | ((uint16_t)data[2] << 8));
        link->conn_interval = (uint16_t)(data[3] | ((uint16_t)data[4] << 8));
        link->state = SSAP_LINK_CONNECTED;
        /* post-connect: read remote version, then set data len */
        hwsle_transport_send_hci_cmd(DLI_READ_REMOTE_VERSION, data, 2);
        uint8_t dl[4] = {data[1], data[2], 0xFF, 0x00}; /* handle + max octets */
        hwsle_transport_send_hci_cmd(DLI_SET_DATA_LEN, dl, sizeof(dl));
        if (link->on_connected)
            link->on_connected(link->conn_handle, link->ctx);
        break;
    }
    case DLI_DISCONNECTION_COMPLETE_EVT: {
        uint8_t reason = (len > 0) ? data[0] : 0;
        link->state = SSAP_LINK_IDLE;
        if (link->on_disconnected)
            link->on_disconnected(reason, link->ctx);
        break;
    }
    default:
        break;
    }
}
