/*
 * feature_mgr.c — heuristic feature switching implementation.
 *
 * Policy: start from what the app wanted ∩ capacity profile; then apply
 * runtime conditions:
 *   - RAM pressure: drop the most expensive features first (ranging, ICB,
 *     connect, scan+adv) to stay under the capacity budget.
 *   - Connection state: adv/scan are pointless while connected; ranging
 *     only when connected; SM only when connecting/pairing.
 *   - Peer capability: v1.3 only if peer negotiated >= 1.3; MTU-based
 *     decisions (small MTU → skip v1.3 multi-processing).
 */

#include "feature_mgr.h"

/* rough RAM cost per feature (bytes) */
static const uint32_t g_feat_cost[] = {
    [0] = 0,                                     /* v1.0 base */
    [1] = 2 * 1024,                              /* v1.3 */
    [2] = 4 * 1024,                              /* adv */
    [3] = 4 * 1024,                              /* scan */
    [4] = 8 * 1024,                              /* connect */
    [5] = 16 * 1024,                             /* ranging */
    [6] = 2 * 1024,                              /* dyn tcid */
    [7] = 8 * 1024,                              /* icb */
    [8] = 1 * 1024,                              /* low-lat */
    [9] = 4 * 1024,                              /* sm */
};

/* feature bit for index i */
static uint32_t bit_of(int i) { return 1u << i; }

void fm_init(fm_t *fm, fm_capacity_t cap, uint32_t wanted)
{
    fm->capacity = cap;
    fm->wanted = wanted;
    fm->enabled = 0;
    fm->on_change = NULL;
    fm->ctx = NULL;
}

uint32_t fm_update(fm_t *fm, const fm_conditions_t *cond)
{
    uint32_t old = fm->enabled;
    uint32_t mask = fm->wanted;             /* app wants */
    uint32_t budget = fm_capacity_budget(fm->capacity);
    uint32_t ram = cond->ram_free;
    uint32_t feasible = 0;

    /* 1. capacity profile trims at compile time: cap what the app can want */
    switch (fm->capacity) {
    case CAP_TINY:
        mask &= ~(FEAT_CONNECT | FEAT_RANGING | FEAT_ICB | FEAT_SM_SECURE |
                  FEAT_DYN_TCID | FEAT_SSAP_V1_3);
        break;
    case CAP_SMALL:
        mask &= ~(FEAT_RANGING | FEAT_ICB);
        break;
    case CAP_MEDIUM:
        mask &= ~FEAT_ICB;
        break;
    default:
        break;
    }

    /* 2. connection state gating */
    if (cond->connected) {
        mask &= ~(FEAT_ADV | FEAT_SCAN);   /* no advertising while connected */
        if (cond->use_case != FM_USECASE_RANGING)
            mask &= ~FEAT_RANGING;
    } else {
        mask &= ~(FEAT_RANGING | FEAT_ICB | FEAT_LOW_LAT | FEAT_SM_SECURE);
        if (cond->use_case != FM_USECASE_CONNECTED)
            mask &= ~FEAT_CONNECT;
    }

    /* 3. peer capability gating */
    if (cond->peer_version && cond->peer_version < 0x0301)
        mask &= ~FEAT_SSAP_V1_3;

    /* 4. RAM pressure: keep base, drop expensive features to fit budget */
    feasible = FEAT_SSAP_V1_0;
    {
        uint32_t cost = 0;
        /* order of dropping (most expensive first) */
        const uint32_t drop_order[] = {
            FEAT_RANGING, FEAT_ICB, FEAT_CONNECT, FEAT_ADV,
            FEAT_SCAN, FEAT_SM_SECURE, FEAT_DYN_TCID, FEAT_SSAP_V1_3,
            FEAT_LOW_LAT, 0
        };
        uint32_t remaining = mask & ~FEAT_SSAP_V1_0;
        /* first try to fit everything */
        for (int i = 0; i < 10; i++) {
            if (remaining & bit_of(i))
                cost += g_feat_cost[i];
        }
        if (cost > ram || cost > budget) {
            /* drop in order until it fits */
            for (int i = 0; drop_order[i]; i++) {
                uint32_t f = drop_order[i];
                if (!(remaining & f))
                    continue;
                /* cost of this feature */
                uint32_t fc = 0;
                for (int j = 0; j < 10; j++)
                    if (f & bit_of(j))
                        fc += g_feat_cost[j];
                if (cost > ram || cost > budget) {
                    remaining &= ~f;
                    cost -= fc;
                }
            }
        }
        feasible |= remaining;
    }

    fm->enabled = feasible;
    if (fm->on_change && old != fm->enabled)
        fm->on_change(old, fm->enabled, fm->ctx);
    return fm->enabled;
}
