/*
 * ssap_link.c — SSAP connection helper implementation.
 *
 * Builds DLI_ConnectionCreateParam and drives the connect/disconnect
 * state machine from DLI events. Sends via hwsle_transport HCI cmds.
 *
 * Hardened per OSPL-CONN-FSM.md: connect/disconnect timeouts (tick),
 * CmdComplete/CmdStatus rejection revert, stale-event gating,
 * duplicate-peer rejection, supervision timeout, 0x1804 retry.
 */

#include "ssap_link.h"
#include "hwsle_transport.h"
#include <string.h>

void ssap_link_init(ssap_link_t *link)
{
    memset(link, 0, sizeof(*link));
    link->state = SSAP_LINK_IDLE;
    link->supervision_timeout_ms = SSAP_LINK_SUP_TO_DEFAULT_MS;
}

static int put_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)(v >> 8);
    return 2;
}

static void start_timeout(ssap_link_t *link, uint8_t bucket)
{
    link->timeout_bucket = bucket;
    link->first_tick_ms = 0; /* anchored on first tick after the state change */
}

int ssap_link_connect(ssap_link_t *link, const uint8_t peer_addr[SLE_ADDR_LEN],
                      const ssap_conn_param_t *param)
{
    if (!link || !peer_addr)
        return -1;
    if (link->state != SSAP_LINK_IDLE) {
        /* duplicate-peer rejection (mirror OSPL EEXIST): a non-idle link
         * already targets this peer */
        if (memcmp(link->target_addr, peer_addr, SLE_ADDR_LEN) == 0)
            return -1;
        return -1;
    }

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
        memcpy(link->target_addr, peer_addr, SLE_ADDR_LEN);
        link->supervision_timeout_ms =
            (param ? param->supervisionTimeout : 0x1F4) * 10u;
        link->dlen_retries = 0;
        link->state = SSAP_LINK_CONNECTING;
        start_timeout(link, 1);
        return 0;
    }
    return -1;
}

int ssap_link_disconnect(ssap_link_t *link)
{
    if (!link)
        return -1;
    if (link->state == SSAP_LINK_CONNECTED) {
        uint8_t params[3];
        params[0] = (uint8_t)(link->conn_handle & 0xFF);
        params[1] = (uint8_t)(link->conn_handle >> 8);
        params[2] = 0x13; /* local host terminated */
        int r = hwsle_transport_send_hci_cmd(DLI_DISCONNECT, params, sizeof(params));
        if (r > 0) {
            link->state = SSAP_LINK_DISCONNECTING;
            start_timeout(link, 2);
            return 0;
        }
        return -1;
    }
    if (link->state == SSAP_LINK_CONNECTING) {
        /* cancel a pending connect: 0x1402 with the peer address */
        int r = hwsle_transport_send_hci_cmd(DLI_CANCEL_CREATE_CONNECTION,
                                             link->target_addr, SLE_ADDR_LEN);
        if (r > 0) {
            link->state = SSAP_LINK_DISCONNECTING;
            start_timeout(link, 2);
            return 0;
        }
        return -1;
    }
    return -1;
}

void ssap_link_mark_activity(ssap_link_t *link, uint32_t now_ms)
{
    if (link && link->state == SSAP_LINK_CONNECTED)
        link->last_activity_ms = now_ms;
}

/* Locate a command's opcode echo + status in a CmdStatus/CmdComplete event
 * payload. Observed layouts: [opcode u16][status][...] and
 * [num_pkts][opcode u16][status][...] (position of the extra byte is not
 * pinned by our notes — see OSPL-DLI-CROSSCHECK.md). Try offsets 0..2.
 * Returns 0 with *out_status set on match, -1 otherwise. */
static int find_cmd_echo(const uint8_t *data, size_t len, uint16_t want,
                         uint8_t *out_status)
{
    for (size_t off = 0; off + 3 <= len && off < 3; off++) {
        uint16_t op = (uint16_t)(data[off] | ((uint16_t)data[off + 1] << 8));
        if (op == want) {
            *out_status = data[off + 2];
            return 0;
        }
    }
    return -1;
}

void ssap_link_on_event(ssap_link_t *link, uint16_t opcode, const uint8_t *data, size_t len)
{
    uint8_t status;

    if (!link)
        return;
    switch (opcode) {
    case DLI_CONNECTION_COMPLETE_EVT: {
        /* Layout per standard/OSPL: [status][connHandle u16][...]; our earlier
         * role-first reading is contested (OSPL-CONN-FSM.md open question).
         * connHandle is at data[1..2] in both layouts; only treat byte 0 as a
         * failure when it is one of the unambiguous error statuses — a role
         * value (0/1) can never equal 0x06/0x0B/0x0F/0x1E. */
        if (len < 3)
            return;
        uint8_t b0 = data[0];
        if (b0 == 0x06 || b0 == 0x0B || b0 == 0x0F || b0 == 0x1E) {
            /* connect rejected — do not send 0x1802/0x1804 */
            link->state = SSAP_LINK_IDLE;
            link->timeout_bucket = 0;
            link->first_tick_ms = 0;
            if (link->on_connect_failed)
                link->on_connect_failed(b0, link->ctx);
            break;
        }
        /* gate: a late 0x0015 (stale from an earlier attempt) must not flip
         * us into CONNECTED from any other state */
        if (link->state != SSAP_LINK_CONNECTING)
            break;
        link->conn_handle = (uint16_t)(data[1] | ((uint16_t)data[2] << 8));
        link->conn_interval = (len >= 5) ? (uint16_t)(data[3] | ((uint16_t)data[4] << 8)) : 0;
        link->state = SSAP_LINK_CONNECTED;
        link->timeout_bucket = 0;
        link->first_tick_ms = 0;
        link->last_activity_ms = 0; /* anchored by tick */
        link->dlen_retries = 0;
        /* post-connect: read remote version (param = conn handle only, not
         * the event buffer), then set data len */
        uint8_t hdl[2] = {data[1], data[2]};
        hwsle_transport_send_hci_cmd(DLI_READ_REMOTE_VERSION, hdl, sizeof(hdl));
        uint8_t dl[4] = {data[1], data[2], 0xFF, 0x00}; /* handle + max octets */
        hwsle_transport_send_hci_cmd(DLI_SET_DATA_LEN, dl, sizeof(dl));
        if (link->on_connected)
            link->on_connected(link->conn_handle, link->ctx);
        break;
    }
    case DLI_DISCONNECTION_COMPLETE_EVT: {
        /* accept from any active state (OSPL T11: any-state removal), but a
         * stale 0x0005 carrying a different conn handle must not tear down a
         * fresh link */
        if (link->state == SSAP_LINK_IDLE)
            break;
        if (link->state == SSAP_LINK_CONNECTED && len >= 3) {
            uint16_t ev_h = (uint16_t)(data[1] | ((uint16_t)data[2] << 8));
            if (ev_h != 0 && ev_h != link->conn_handle)
                break;
        }
        uint8_t reason = (len > 0) ? data[0] : 0;
        link->state = SSAP_LINK_IDLE;
        link->timeout_bucket = 0;
        link->first_tick_ms = 0;
        if (link->on_disconnected)
            link->on_disconnected(reason, link->ctx);
        break;
    }
    case DLI_CMD_STATUS_EVT:
    case DLI_CMD_COMPLETE_EVT: {
        if (!data || len < 3)
            break;
        /* 0x1401 rejected → abort connect (T3/T5) */
        if (find_cmd_echo(data, len, DLI_CREATE_CONNECTION, &status) == 0 && status != 0) {
            if (link->state == SSAP_LINK_CONNECTING) {
                link->state = SSAP_LINK_IDLE;
                link->timeout_bucket = 0;
                link->first_tick_ms = 0;
                if (link->on_connect_failed)
                    link->on_connect_failed(status, link->ctx);
            }
            break;
        }
        /* 0x1402 cancel accepted → disconnected (T9) */
        if (find_cmd_echo(data, len, DLI_CANCEL_CREATE_CONNECTION, &status) == 0 && status == 0) {
            if (link->state == SSAP_LINK_DISCONNECTING) {
                link->state = SSAP_LINK_IDLE;
                link->timeout_bucket = 0;
                link->first_tick_ms = 0;
                if (link->on_disconnected)
                    link->on_disconnected(0x13, link->ctx);
            }
            break;
        }
        /* 0x1403 rejected → revert to CONNECTED (T10) */
        if (find_cmd_echo(data, len, DLI_DISCONNECT, &status) == 0 && status != 0) {
            if (link->state == SSAP_LINK_DISCONNECTING)
                link->state = SSAP_LINK_CONNECTED;
            break;
        }
        /* 0x1804 SET_DATA_LEN returns 0x06 while no conn exists — retry a
         * couple of times before giving up */
        if (find_cmd_echo(data, len, DLI_SET_DATA_LEN, &status) == 0 && status == 0x06 &&
            link->state == SSAP_LINK_CONNECTED && link->dlen_retries < 2) {
            link->dlen_retries++;
            uint8_t dl[4] = {(uint8_t)(link->conn_handle & 0xFF),
                             (uint8_t)(link->conn_handle >> 8), 0xFF, 0x00};
            hwsle_transport_send_hci_cmd(DLI_SET_DATA_LEN, dl, sizeof(dl));
        }
        break;
    }
    default:
        break;
    }
}

void ssap_link_tick(ssap_link_t *link, uint32_t now_ms)
{
    if (!link)
        return;
    if (link->first_tick_ms == 0)
        link->first_tick_ms = now_ms;

    switch (link->state) {
    case SSAP_LINK_CONNECTING:
        if (now_ms - link->first_tick_ms > SSAP_LINK_CMD_TIMEOUT_MS) {
            /* stuck connect: cancel and report failure */
            hwsle_transport_send_hci_cmd(DLI_CANCEL_CREATE_CONNECTION,
                                         link->target_addr, SLE_ADDR_LEN);
            link->state = SSAP_LINK_IDLE;
            link->timeout_bucket = 0;
            link->first_tick_ms = 0;
            if (link->on_connect_failed)
                link->on_connect_failed(0xB0, link->ctx); /* timeout */
        }
        break;
    case SSAP_LINK_DISCONNECTING:
        if (now_ms - link->first_tick_ms > SSAP_LINK_CMD_TIMEOUT_MS) {
            link->state = SSAP_LINK_IDLE;
            link->timeout_bucket = 0;
            link->first_tick_ms = 0;
            if (link->on_disconnected)
                link->on_disconnected(0x08, link->ctx);
        }
        break;
    case SSAP_LINK_CONNECTED:
        if (link->last_activity_ms == 0)
            link->last_activity_ms = now_ms;
        if (now_ms - link->last_activity_ms > link->supervision_timeout_ms) {
            /* link loss without a 0x0005 event: teardown with reason 0x08 */
            link->state = SSAP_LINK_IDLE;
            link->timeout_bucket = 0;
            link->first_tick_ms = 0;
            link->last_activity_ms = 0;
            if (link->on_disconnected)
                link->on_disconnected(0x08, link->ctx);
        }
        break;
    default:
        break;
    }
}
