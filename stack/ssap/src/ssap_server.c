/*
 * ssap_server.c — SSAP server core implementation.
 *
 * Service table + dispatch of EXCHANGE_INFO / FIND_STRUCTURE / READ /
 * WRITE_CMD / WRITE_REQ, per ssaps_server.c model (Apache-2.0).
 */

#include "ssap_server.h"
#include "ssap_codec.h"
#include <string.h>

void ssap_server_init(ssap_server_t *srv, int (*send_frame)(const uint8_t *, size_t))
{
    memset(srv, 0, sizeof(*srv));
    srv->send_frame = send_frame;
    srv->next_handle = SSAP_HANDLE_START;
    srv->mtu = SSAP_MTU_DEFAULT;
    srv->version = SSAP_VERSION_1_3;
}

uint16_t ssap_server_add_service(ssap_server_t *srv, uint16_t uuid16, uint8_t is_primary)
{
    if (srv->service_count >= 8)
        return 0;
    ssap_service_t *svc = &srv->services[srv->service_count];
    memset(svc, 0, sizeof(*svc));
    svc->uuid16 = uuid16;
    svc->is_primary = is_primary;
    svc->start_handle = srv->next_handle;
    srv->next_handle += 2;
    svc->end_handle = srv->next_handle - 1;
    srv->service_count++;
    return svc->start_handle;
}

uint16_t ssap_server_add_property(ssap_server_t *srv, uint16_t svc_handle,
                                  uint16_t uuid16, uint32_t operation,
                                  uint8_t permission, ssap_read_cb rc, ssap_write_cb wc)
{
    ssap_service_t *svc = NULL;
    for (uint8_t i = 0; i < srv->service_count; i++) {
        if (srv->services[i].start_handle == svc_handle) {
            svc = &srv->services[i];
            break;
        }
    }
    if (!svc || svc->property_count >= SSAP_MAX_PROPERTIES)
        return 0;
    ssap_property_t *p = &svc->properties[svc->property_count];
    p->handle = srv->next_handle++;
    p->uuid16 = uuid16;
    p->type = SSAP_ITEM_PROPERTY;
    p->operation = operation;
    p->permission = permission;
    p->read_cb = rc;
    p->write_cb = wc;
    svc->property_count++;
    svc->end_handle = p->handle;
    return p->handle;
}

static ssap_property_t *find_property(ssap_server_t *srv, uint16_t handle)
{
    for (uint8_t i = 0; i < srv->service_count; i++) {
        ssap_service_t *svc = &srv->services[i];
        for (uint8_t j = 0; j < svc->property_count; j++) {
            if (svc->properties[j].handle == handle)
                return &svc->properties[j];
        }
    }
    return NULL;
}

int ssap_server_dispatch(ssap_server_t *srv, const uint8_t *pdu, size_t len)
{
    if (len < 2 || !srv->send_frame)
        return -1;
    uint8_t opcode = ssap_opcode_of(pdu[0]);
    uint8_t ctrl = pdu[1];
    uint8_t rsp[512];
    size_t rsp_len;

    switch (opcode) {
    case SSAP_MSG_EXCHANGE_INFO_REQ: {
        uint16_t mtu = 0, ver = 0;
        ssap_decode_exchange_info(pdu, len, &mtu, &ver);
        if (mtu)
            srv->mtu = mtu > SSAP_MTU_MAX ? SSAP_MTU_MAX : mtu;
        rsp_len = ssap_encode_exchange_info(rsp, sizeof(rsp),
                                            SSAP_MSG_EXCHANGE_INFO_RSP, 0x03,
                                            srv->mtu, srv->version);
        return srv->send_frame(rsp, rsp_len);
    }
    case SSAP_MSG_FIND_STRUCTURE_REQ: {
        /* respond with service info: findType in ctrl low 3 bits */
        uint8_t find_type = ctrl & 0x07;
        if (find_type == SSAP_FIND_PRIMARY_SERVICE) {
            for (uint8_t i = 0; i < srv->service_count; i++) {
                ssap_service_t *svc = &srv->services[i];
                if (!svc->is_primary)
                    continue;
                /* minimal response: [msgCode][ctrl][handle u16][type u8][uuid16] */
                size_t n = 0;
                rsp[n++] = SSAP_MSG_FIND_STRUCTURE_RSP;
                rsp[n++] = SSAP_CTRL_NO_FRAG;
                rsp[n++] = (uint8_t)(svc->start_handle & 0xFF);
                rsp[n++] = (uint8_t)(svc->start_handle >> 8);
                rsp[n++] = SSAP_ITEM_PRIMARY_SERVICE;
                rsp[n++] = (uint8_t)(svc->uuid16 & 0xFF);
                rsp[n++] = (uint8_t)(svc->uuid16 >> 8);
                /* operation bits for the service */
                rsp[n++] = 0x01; /* read */
                rsp[n++] = 0x00; /* descriptor count */
                srv->send_frame(rsp, n);
            }
            return 0;
        }
        if (find_type == SSAP_FIND_PROPERTY) {
            for (uint8_t i = 0; i < srv->service_count; i++) {
                ssap_service_t *svc = &srv->services[i];
                for (uint8_t j = 0; j < svc->property_count; j++) {
                    ssap_property_t *p = &svc->properties[j];
                    if (p->handle < ctrl) /* startHandle in payload; simplified */
                        continue;
                    size_t n = 0;
                    rsp[n++] = SSAP_MSG_FIND_STRUCTURE_RSP;
                    rsp[n++] = SSAP_CTRL_NO_FRAG;
                    rsp[n++] = (uint8_t)(p->handle & 0xFF);
                    rsp[n++] = (uint8_t)(p->handle >> 8);
                    rsp[n++] = SSAP_ITEM_PROPERTY;
                    rsp[n++] = (uint8_t)(p->uuid16 & 0xFF);
                    rsp[n++] = (uint8_t)(p->uuid16 >> 8);
                    rsp[n++] = (uint8_t)(p->operation & 0xFF);
                    rsp[n++] = 0x00;
                    srv->send_frame(rsp, n);
                }
            }
            return 0;
        }
        return 0;
    }
    case SSAP_MSG_READ_REQ: {
        /* items: [handle u16][type u8] */
        if (len < 5)
            return -1;
        uint16_t handle = (uint16_t)(pdu[2] | ((uint16_t)pdu[3] << 8));
        ssap_property_t *p = find_property(srv, handle);
        uint8_t value[SSAP_MAX_VALUE_LEN];
        uint16_t vlen = 0;
        int ok = (p && p->read_cb) ? p->read_cb(handle, value, &vlen, sizeof(value)) : -1;
        size_t n = 0;
        rsp[n++] = SSAP_MSG_READ_RSP;
        rsp[n++] = SSAP_CTRL_NO_FRAG;
        if (ok == 0) {
            rsp[n++] = (uint8_t)(handle & 0xFF);
            rsp[n++] = (uint8_t)(handle >> 8);
            rsp[n++] = (uint8_t)(vlen & 0xFF);
            rsp[n++] = (uint8_t)(vlen >> 8);
            if (vlen && n + vlen <= sizeof(rsp)) {
                memcpy(rsp + n, value, vlen);
                n += vlen;
            }
        } else {
            rsp[n++] = 0x00;
            rsp[n++] = 0x00;
            rsp[n++] = 0x01; /* error */
        }
        return srv->send_frame(rsp, n);
    }
    case SSAP_MSG_WRITE_CMD:
    case SSAP_MSG_WRITE_REQ: {
        if (len < 5)
            return -1;
        uint16_t handle = (uint16_t)(pdu[2] | ((uint16_t)pdu[3] << 8));
        uint8_t type = pdu[4];
        (void)type;
        ssap_property_t *p = find_property(srv, handle);
        int ok = (p && p->write_cb) ? p->write_cb(handle, pdu + 5, (uint16_t)(len - 5)) : -1;
        if (opcode == SSAP_MSG_WRITE_CMD)
            return 0; /* no response for write command */
        size_t n = ssap_encode_error_rsp(rsp, sizeof(rsp), SSAP_MSG_WRITE_REQ,
                                         handle, ok == 0 ? 0x00 : 0x0F);
        return srv->send_frame(rsp, n);
    }
    default:
        return -1;
    }
}

int ssap_server_notify(ssap_server_t *srv, uint16_t handle,
                       const uint8_t *value, uint16_t len, uint8_t indicate)
{
    if (!srv->send_frame)
        return -1;
    uint8_t pdu[SSAP_MAX_VALUE_LEN + 8];
    size_t n = ssap_encode_value(pdu, sizeof(pdu),
                                 indicate ? SSAP_MSG_VALUE_IND : SSAP_MSG_VALUE_NTF,
                                 SSAP_CTRL_NO_FRAG, 0, handle, value, len);
    return n ? srv->send_frame(pdu, n) : -1;
}
