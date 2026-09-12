/*
 * feature_mgr.h — heuristic feature switching for resource-constrained hosts.
 *
 * For a small board (limited flash/RAM), enable/disable protocol features
 * adaptively based on:
 *   - available RAM (runtime pressure)
 *   - compile-time flash budget (capacity profile)
 *   - connection state (idle vs connected)
 *   - peer capability (negotiated SSAP version/MTU)
 *   - use-case (advertiser / scanner / connected / ranging)
 *
 * Design: features are bitflags with an estimated RAM cost and a set of
 * "enablers" (conditions that must hold). A cheap heuristic evaluates the
 * full set and flips bits; consumers poll or get notified.
 */

#ifndef FEATURE_MGR_H
#define FEATURE_MGR_H

#include <stdint.h>
#include <stddef.h>

/* ---- feature bits ---- */
enum {
    FEAT_SSAP_V1_0 = 1u << 0,   /* base SSAP (always on if stack present) */
    FEAT_SSAP_V1_3 = 1u << 1,   /* v1.3: multi-processing, fragmentation (RAM: +2KB) */
    FEAT_ADV       = 1u << 2,   /* advertising (RAM: +4KB) */
    FEAT_SCAN      = 1u << 3,   /* scanning (RAM: +4KB) */
    FEAT_CONNECT   = 1u << 4,   /* connection establishment (RAM: +8KB) */
    FEAT_RANGING   = 1u << 5,   /* channel sounding / measure (RAM: +16KB, IQ buffers) */
    FEAT_DYN_TCID  = 1u << 6,   /* dynamic unicast channels 0x80-0xDF (RAM: +2KB) */
    FEAT_ICB       = 1u << 7,   /* isochronous data path (RAM: +8KB) */
    FEAT_LOW_LAT   = 1u << 8,   /* ACB subrate / low-latency (RAM: +1KB) */
    FEAT_SM_SECURE = 1u << 9,   /* security manager / pairing (RAM: +4KB) */
};

/* ---- capacity profiles (compile-time) ---- */
typedef enum {
    CAP_TINY = 0,   /* ~32KB RAM budget: base + adv or scan, no connect */
    CAP_SMALL,      /* ~64KB: + connect + v1.3, no ranging/ICB */
    CAP_MEDIUM,     /* ~128KB: + ranging + dyn tcid, low-lat */
    CAP_FULL,       /* ~256KB+: everything */
} fm_capacity_t;

/* ---- runtime conditions fed by the stack ---- */
typedef struct {
    size_t ram_free;        /* bytes available (from sysinfo/mallinfo) */
    int    connected;       /* ACB link up */
    uint16_t peer_version;  /* SSAP version negotiated (0x0101..0x0301), 0 if none */
    uint16_t peer_mtu;      /* negotiated MTU, 0 if none */
    uint8_t  use_case;      /* FM_USECASE_* */
} fm_conditions_t;

typedef enum {
    FM_USECASE_IDLE = 0,
    FM_USECASE_ADVERTISER,
    FM_USECASE_SCANNER,
    FM_USECASE_CONNECTED,
    FM_USECASE_RANGING,
} fm_usecase_t;

typedef struct {
    fm_capacity_t capacity;
    uint32_t enabled;       /* current feature set */
    uint32_t wanted;        /* what the app asked for (mask) */
    /* optional notify when the feature set changes */
    void (*on_change)(uint32_t old, uint32_t now, void *ctx);
    void *ctx;
} fm_t;

void fm_init(fm_t *fm, fm_capacity_t cap, uint32_t wanted);
/* Re-evaluate the feature set from current conditions. Returns new mask. */
uint32_t fm_update(fm_t *fm, const fm_conditions_t *cond);

/* ---- helpers ---- */
static inline int fm_has(const fm_t *fm, uint32_t bit) { return (fm->enabled & bit) != 0; }
static inline uint32_t fm_capacity_budget(fm_capacity_t cap)
{
    switch (cap) {
    case CAP_TINY:   return 32 * 1024;
    case CAP_SMALL:  return 64 * 1024;
    case CAP_MEDIUM: return 128 * 1024;
    default:         return 256 * 1024;
    }
}

#endif /* FEATURE_MGR_H */
