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
    /* SERVICE_CHANGE notification on handle 0x000E: {start u16}{end u16} */
    if (srv->send_frame) {
        uint8_t svc_evt[8];
        size_t n = 0;
        svc_evt[n++] = SSAP_MSG_VALUE_NTF;
        svc_evt[n++] = SSAP_CTRL_NO_FRAG;
        svc_evt[n++] = (uint8_t)(SSAP_HANDLE_SERVICE_CHANGE & 0xFF);
        svc_evt[n++] = (uint8_t)(SSAP_HANDLE_SERVICE_CHANGE >> 8);
        svc_evt[n++] = (uint8_t)(svc->start_handle & 0xFF);
        svc_evt[n++] = (uint8_t)(svc->start_handle >> 8);
        svc_evt[n++] = (uint8_t)(svc->end_handle & 0xFF);
        svc_evt[n++] = (uint8_t)(svc->end_handle >> 8);
        srv->send_frame(svc_evt, n);
    }
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
    /* auto-CCCD: if property supports notify/indicate, add CCCD descriptor */
    if (operation & (SSAP_OP_NOTIFY | SSAP_OP_INDICATE)) {
        p->desc_count = 1;
        p->desc_type = 0x02; /* CLIENT_CONFIG (CCCD) per ssap_type.h */
    }
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

/* Find property whose CCCD descriptor is at the given handle (= prop_handle+1) */
static ssap_property_t *find_by_cccd(ssap_server_t *srv, uint16_t handle)
{
    for (uint8_t i = 0; i < srv->service_count; i++) {
        ssap_service_t *svc = &srv->services[i];
        for (uint8_t j = 0; j < svc->property_count; j++) {
            if (svc->properties[j].handle + 1 == handle)
                return &svc->properties[j];
        }
    }
    return NULL;
}

/* Find first property with matching uuid16 in handle range [start, end] */
static ssap_property_t *find_by_uuid(ssap_server_t *srv, uint16_t uuid16,
                                     uint16_t start, uint16_t end)
{
    for (uint8_t i = 0; i < srv->service_count; i++) {
        ssap_service_t *svc = &srv->services[i];
        for (uint8_t j = 0; j < svc->property_count; j++) {
            ssap_property_t *p = &svc->properties[j];
            if (p->uuid16 == uuid16 && p->handle >= start && p->handle <= end)
                return p;
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
        if (ver)
            srv->version = (ver < srv->version) ? ver : srv->version; /* min(peer, local) */
        /* advertise capabilities: reliable(3) + multiProcessing(5) for v1.3+ */
        uint8_t rsp_ctrl = 0x03; /* mtu + version */
        if (srv->version >= SSAP_VERSION_1_3)
            rsp_ctrl |= 0x28; /* bit3=reliable | bit5=multiProcessing */
        rsp_len = ssap_encode_exchange_info(rsp, sizeof(rsp),
                                            SSAP_MSG_EXCHANGE_INFO_RSP, rsp_ctrl,
                                            srv->mtu, srv->version);
        return srv->send_frame(rsp, rsp_len);
    }
    case SSAP_MSG_FIND_STRUCTURE_REQ: {
        /* findType in ctrl low 3 bits; [start u16][end u16][uuid?] in payload */
        uint8_t find_type = ctrl & 0x07;
        if (len < 6)
            return -1;
        uint16_t start_h = (uint16_t)(pdu[2] | ((uint16_t)pdu[3] << 8));
        uint16_t end_h   = (uint16_t)(pdu[4] | ((uint16_t)pdu[5] << 8));
        uint8_t is_v10 = (srv->version < SSAP_VERSION_1_3);
        if (find_type == SSAP_FIND_PRIMARY_SERVICE) {
            for (uint8_t i = 0; i < srv->service_count; i++) {
                ssap_service_t *svc = &srv->services[i];
                if (!svc->is_primary || svc->start_handle < start_h || svc->start_handle > end_h)
                    continue;
                /* memberValue = member-presence bitmap (ssap_type.h):
                 * REFERENCE=0x01, PROPERTY=0x02, METHOD=0x04, EVENT=0x08 */
                uint8_t member = (svc->property_count > 0) ? 0x02 : 0x00;
                size_t n = 0;
                rsp[n++] = SSAP_MSG_FIND_STRUCTURE_RSP;
                rsp[n++] = SSAP_CTRL_NO_FRAG;
                rsp[n++] = (uint8_t)(svc->start_handle & 0xFF);
                rsp[n++] = (uint8_t)(svc->start_handle >> 8);
                rsp[n++] = (uint8_t)(svc->end_handle & 0xFF);
                rsp[n++] = (uint8_t)(svc->end_handle >> 8);
                if (is_v10) {
                    /* v1.0 member: [start][end][member] — no uuid */
                    rsp[n++] = member;
                } else {
                    /* v1.3 member: [start][end][uuid 2/16][member] */
                    rsp[n++] = (uint8_t)(svc->uuid16 & 0xFF);
                    rsp[n++] = (uint8_t)(svc->uuid16 >> 8);
                    rsp[n++] = member;
                }
                srv->send_frame(rsp, n);
            }
            return 0;
        }
        if (find_type == SSAP_FIND_PROPERTY) {
            for (uint8_t i = 0; i < srv->service_count; i++) {
                ssap_service_t *svc = &srv->services[i];
                for (uint8_t j = 0; j < svc->property_count; j++) {
                    ssap_property_t *p = &svc->properties[j];
                    if (p->handle < start_h || p->handle > end_h)
                        continue;
                    size_t n = 0;
                    rsp[n++] = SSAP_MSG_FIND_STRUCTURE_RSP;
                    rsp[n++] = SSAP_CTRL_NO_FRAG;
                    rsp[n++] = (uint8_t)(p->handle & 0xFF);
                    rsp[n++] = (uint8_t)(p->handle >> 8);
                    if (!is_v10) {
                        /* v1.3 property member: [handle][uuid][op u32][descCount] */
                        rsp[n++] = (uint8_t)(p->uuid16 & 0xFF);
                        rsp[n++] = (uint8_t)(p->uuid16 >> 8);
                    }
                    /* v1.0: [handle][op u32][descCount] — no uuid */
                    rsp[n++] = (uint8_t)(p->operation & 0xFF);
                    rsp[n++] = (uint8_t)((p->operation >> 8) & 0xFF);
                    rsp[n++] = (uint8_t)((p->operation >> 16) & 0xFF);
                    rsp[n++] = (uint8_t)((p->operation >> 24) & 0xFF);
                    rsp[n++] = p->desc_count; /* descriptor count */
                    if (p->desc_count > 0) {
                        rsp[n++] = p->desc_type; /* e.g. 0x02 = CLIENT_CONFIG / CCCD */
                    }
                    srv->send_frame(rsp, n);
                }
            }
            return 0;
        }
        return 0;
    }
    case SSAP_MSG_FIND_STRUCTURE_BY_UUID_REQ: {
        /* same struct as FIND_STRUCTURE_REQ but uuid is mandatory;
         * find property matching uuid within handle range */
        if (len < 7)
            return -1;
        uint8_t uuidType = (ctrl >> 3) & 0x03; /* itemType bits */
        uint16_t start_h = (uint16_t)(pdu[2] | ((uint16_t)pdu[3] << 8));
        uint16_t end_h   = (uint16_t)(pdu[4] | ((uint16_t)pdu[5] << 8));
        uint8_t uuid_size = uuidType ? SSAP_UUID128_LEN : SSAP_UUID16_LEN;
        if (len < 6 + uuid_size)
            return -1;
        uint16_t uuid16 = 0;
        if (!uuidType)
            uuid16 = (uint16_t)(pdu[6] | ((uint16_t)pdu[7] << 8));
        /* search properties matching uuid in range */
        uint8_t found = 0;
        for (uint8_t i = 0; i < srv->service_count; i++) {
            ssap_service_t *svc = &srv->services[i];
            for (uint8_t j = 0; j < svc->property_count; j++) {
                ssap_property_t *p = &svc->properties[j];
                if (p->uuid16 != uuid16 || p->handle < start_h || p->handle > end_h)
                    continue;
                size_t n = 0;
                rsp[n++] = SSAP_MSG_FIND_STRUCTURE_BY_UUID_RSP;
                rsp[n++] = SSAP_CTRL_NO_FRAG;
                rsp[n++] = (uint8_t)(p->handle & 0xFF);
                rsp[n++] = (uint8_t)(p->handle >> 8);
                uint8_t is_v10 = (srv->version < SSAP_VERSION_1_3);
                if (!is_v10) {
                    rsp[n++] = (uint8_t)(p->uuid16 & 0xFF);
                    rsp[n++] = (uint8_t)(p->uuid16 >> 8);
                }
                rsp[n++] = (uint8_t)(p->operation & 0xFF);
                rsp[n++] = (uint8_t)((p->operation >> 8) & 0xFF);
                rsp[n++] = (uint8_t)((p->operation >> 16) & 0xFF);
                rsp[n++] = (uint8_t)((p->operation >> 24) & 0xFF);
                rsp[n++] = p->desc_count;
                if (p->desc_count > 0)
                    rsp[n++] = p->desc_type;
                srv->send_frame(rsp, n);
                found = 1;
            }
        }
        if (!found) {
            rsp[0] = SSAP_MSG_FIND_STRUCTURE_BY_UUID_RSP;
            rsp[1] = 0x08 | SSAP_CTRL_NO_FRAG; /* error bit */
            rsp[2] = (uint8_t)(SSAP_ERRCODE_ITEM_INEXIST & 0xFF);
            rsp[3] = (uint8_t)(SSAP_ERRCODE_ITEM_INEXIST >> 8);
            srv->send_frame(rsp, 4);
        }
        return 0;
    }
    case SSAP_MSG_READ_REQ: {
        /* multi-item: [{handle u16}{type u8}]... */
        if (len < 5)
            return -1;
        uint8_t item_count = (uint8_t)((len - 2) / 3);
        if (item_count == 0)
            return -1;
        rsp[0] = SSAP_MSG_READ_RSP;
        if (item_count == 1) {
            /* single value: [msgCode][ctrl multi=0][value...] — no handle/len prefix */
            uint16_t handle = (uint16_t)(pdu[2] | ((uint16_t)pdu[3] << 8));
            ssap_property_t *p = find_property(srv, handle);
            uint8_t value[SSAP_MAX_VALUE_LEN];
            uint16_t vlen = 0;
            int ok = (p && p->read_cb) ? p->read_cb(handle, value, &vlen, sizeof(value)) : -1;
            if (ok != 0) {
                uint8_t err = p ? SSAP_ERRCODE_FORBID_READ : SSAP_ERRCODE_INVALID_HANDLE;
                rsp[1] = SSAP_CTRL_NO_FRAG | 0x08;
                rsp[2] = (uint8_t)(err & 0xFF);
                rsp[3] = (uint8_t)(err >> 8);
                return srv->send_frame(rsp, 4);
            }
            rsp[1] = SSAP_CTRL_NO_FRAG;
            size_t n = 2;
            if (vlen && n + vlen <= sizeof(rsp)) {
                memcpy(rsp + n, value, vlen);
                n += vlen;
            }
            return srv->send_frame(rsp, n);
        }
        /* multi-value: ctrl.multi=1, items [{length:15|success:1}{value...}]... */
        rsp[1] = SSAP_CTRL_NO_FRAG | 0x04; /* multi bit set */
        size_t n = 2;
        for (uint8_t i = 0; i < item_count && n + 4 <= sizeof(rsp); i++) {
            uint16_t off = 2 + i * 3;
            uint16_t handle = (uint16_t)(pdu[off] | ((uint16_t)pdu[off + 1] << 8));
            ssap_property_t *p = find_property(srv, handle);
            uint8_t value[SSAP_MAX_VALUE_LEN];
            uint16_t vlen = 0;
            int ok = (p && p->read_cb) ? p->read_cb(handle, value, &vlen, sizeof(value)) : -1;
            if (ok != 0) {
                uint16_t item_hdr = (uint8_t)(SSAP_ERRCODE_INVALID_HANDLE & 0x7F); /* length=err, success=0 */
                rsp[n++] = (uint8_t)(item_hdr & 0xFF);
                rsp[n++] = (uint8_t)(item_hdr >> 8);
            } else {
                uint16_t item_hdr = (uint16_t)((vlen & 0x7FFF) | 0x8000); /* length=vlen, success=1 */
                rsp[n++] = (uint8_t)(item_hdr & 0xFF);
                rsp[n++] = (uint8_t)(item_hdr >> 8);
                if (vlen && n + vlen <= sizeof(rsp)) {
                    memcpy(rsp + n, value, vlen);
                    n += vlen;
                }
            }
        }
        return srv->send_frame(rsp, n);
    }
    case SSAP_MSG_READ_BY_UUID_REQ: {
        /* [msgCode][ctrl:uuidType:1][start u16][end u16][dataType u8][uuid 2/16] */
        if (len < 7)
            return -1;
        uint8_t uuidType = ctrl & 0x01;
        uint16_t start_h = (uint16_t)(pdu[2] | ((uint16_t)pdu[3] << 8));
        uint16_t end_h   = (uint16_t)(pdu[4] | ((uint16_t)pdu[5] << 8));
        uint8_t dataType = pdu[6];
        (void)dataType;
        uint8_t uuid_size = uuidType ? SSAP_UUID128_LEN : SSAP_UUID16_LEN;
        if (len < 7 + uuid_size)
            return -1;
        uint16_t uuid16 = 0;
        if (!uuidType)
            uuid16 = (uint16_t)(pdu[7] | ((uint16_t)pdu[8] << 8));
        ssap_property_t *p = find_by_uuid(srv, uuid16, start_h, end_h);
        size_t n = 0;
        rsp[n++] = SSAP_MSG_READ_BY_UUID_RSP;
        if (!p) {
            /* no matching property: error response ctrl.error=1 */
            rsp[n++] = 0x08 | SSAP_CTRL_NO_FRAG; /* error bit set */
            rsp[n++] = (uint8_t)(SSAP_ERRCODE_ITEM_INEXIST & 0xFF);
            rsp[n++] = (uint8_t)(SSAP_ERRCODE_ITEM_INEXIST >> 8);
            return srv->send_frame(rsp, n);
        }
        uint8_t value[SSAP_MAX_VALUE_LEN];
        uint16_t vlen = 0;
        int ok = (p->read_cb) ? p->read_cb(p->handle, value, &vlen, sizeof(value)) : -1;
        if (ok != 0) {
            rsp[n++] = 0x08 | SSAP_CTRL_NO_FRAG;
            rsp[n++] = (uint8_t)(SSAP_ERRCODE_FORBID_READ & 0xFF);
            rsp[n++] = (uint8_t)(SSAP_ERRCODE_FORBID_READ >> 8);
            return srv->send_frame(rsp, n);
        }
        /* single value: [msgCode][ctrl][handle u16][value...] */
        rsp[n++] = SSAP_CTRL_NO_FRAG;
        rsp[n++] = (uint8_t)(p->handle & 0xFF);
        rsp[n++] = (uint8_t)(p->handle >> 8);
        if (vlen && n + vlen <= sizeof(rsp)) {
            memcpy(rsp + n, value, vlen);
            n += vlen;
        }
        return srv->send_frame(rsp, n);
    }
    case SSAP_MSG_WRITE_CMD:
    case SSAP_MSG_WRITE_REQ: {
        if (len < 5)
            return -1;
        uint8_t multi = (ctrl >> 2) & 0x01;
        if (!multi) {
            /* single-item: [handle u16][type u8][value...] */
            uint16_t handle = (uint16_t)(pdu[2] | ((uint16_t)pdu[3] << 8));
            uint8_t type = pdu[4];
            (void)type;
            /* CCCD descriptor write: client writes to prop_handle+1 */
            ssap_property_t *cccd_prop = find_by_cccd(srv, handle);
            if (cccd_prop) {
                uint16_t val = (len >= 7) ? (uint16_t)(pdu[5] | ((uint16_t)pdu[6] << 8)) : 0;
                cccd_prop->cccd_value = (val == 0x0001) ? 1 : (val == 0x0002) ? 2 : 0;
                if (opcode == SSAP_MSG_WRITE_CMD)
                    return 0;
                size_t n = ssap_encode_write_rsp(rsp, sizeof(rsp), handle, 0x00, 0);
                return srv->send_frame(rsp, n);
            }
            ssap_property_t *p = find_property(srv, handle);
            int ok = (p && p->write_cb) ? p->write_cb(handle, pdu + 5, (uint16_t)(len - 5)) : -1;
            if (opcode == SSAP_MSG_WRITE_CMD)
                return 0;
            uint8_t result = ok == 0 ? 0x00 : 0x01;
            uint8_t err = p ? SSAP_ERRCODE_FORBID_WRITE : SSAP_ERRCODE_INVALID_HANDLE;
            size_t n = ssap_encode_write_rsp(rsp, sizeof(rsp), handle, result, err);
            return srv->send_frame(rsp, n);
        }
        /* multi-item: [{handle u16}{subItemCount u8}{subItem}...]...
         * subItem = [type u8][len u16 LE][value len bytes] */
        uint8_t errorNum = 0;
        uint8_t error_items[128]; /* max errors */
        size_t ei = 0;
        size_t off = 2; /* skip msgCode+ctrl */
        while (off + 3 <= len) {
            uint16_t handle = (uint16_t)(pdu[off] | ((uint16_t)pdu[off + 1] << 8));
            uint8_t subCount = pdu[off + 2];
            off += 3;
            for (uint8_t s = 0; s < subCount && off + 3 <= len; s++) {
                uint8_t subType = pdu[off++];
                uint16_t subLen = (uint16_t)(pdu[off] | ((uint16_t)pdu[off + 1] << 8));
                off += 2;
                if (off + subLen > len)
                    break;
                ssap_property_t *cccd_prop = find_by_cccd(srv, handle);
                if (cccd_prop) {
                    uint16_t val = (subLen >= 2) ? (uint16_t)(pdu[off] | ((uint16_t)pdu[off + 1] << 8)) : 0;
                    cccd_prop->cccd_value = (val == 0x0001) ? 1 : (val == 0x0002) ? 2 : 0;
                } else {
                    ssap_property_t *p = find_property(srv, handle);
                    int ok = (p && p->write_cb) ? p->write_cb(handle, pdu + off, subLen) : -1;
                    if (ok != 0 && ei + 3 < sizeof(error_items)) {
                        error_items[ei++] = (uint8_t)(handle & 0xFF);
                        error_items[ei++] = (uint8_t)(handle >> 8);
                        error_items[ei++] = p ? SSAP_ERRCODE_FORBID_WRITE : SSAP_ERRCODE_INVALID_HANDLE;
                        errorNum++;
                    }
                }
                off += subLen;
            }
        }
        if (opcode == SSAP_MSG_WRITE_CMD)
            return 0;
        /* WRITE_RSP multi: [ctrl: result:2][errorNum u8][{handle u16}{errorCode u8}]... */
        uint8_t result = (errorNum > 0) ? 0x01 : 0x00;
        rsp[0] = SSAP_MSG_WRITE_RSP;
        rsp[1] = result;
        rsp[2] = errorNum;
        size_t n = 3;
        if (ei > 0 && n + ei <= sizeof(rsp)) {
            memcpy(rsp + n, error_items, ei);
            n += ei;
        }
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
    /* CCCD gating: check if notify/indicate is enabled for this property */
    ssap_property_t *p = find_property(srv, handle);
    if (p) {
        if (indicate && p->cccd_value < 2)
            return -1; /* indicate requires CCCD=2 */
        if (!indicate && p->cccd_value < 1)
            return -1; /* notify requires CCCD>=1 */
    }
    uint8_t pdu[SSAP_MAX_VALUE_LEN + 8];
    size_t n = ssap_encode_value(pdu, sizeof(pdu),
                                 indicate ? SSAP_MSG_VALUE_IND : SSAP_MSG_VALUE_NTF,
                                 SSAP_CTRL_NO_FRAG, 0, handle, value, len);
    return n ? srv->send_frame(pdu, n) : -1;
}

void ssap_server_apply_config(ssap_server_t *srv, const ssap_server_config_t *cfg)
{
    if (!srv || !cfg)
        return;
    /* v1.3 features gated by the feature manager */
    if (!(cfg->enabled_features & (1u << 1))) { /* FEAT_SSAP_V1_3 */
        srv->version = SSAP_VERSION_1_0;
    } else if (srv->version < SSAP_VERSION_1_3) {
        srv->version = SSAP_VERSION_1_3;
    }
    if (cfg->max_mtu && cfg->max_mtu < srv->mtu)
        srv->mtu = cfg->max_mtu;
}
