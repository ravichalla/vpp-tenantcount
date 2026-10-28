/* SPDX-License-Identifier: Apache-2.0 */
#ifndef __included_tenantcount_h__
#define __included_tenantcount_h__

#include <vnet/vnet.h>
#include <vnet/feature/feature.h>
#include <vlib/vlib.h>
#include <vlib/counter.h>

#define TENANTCOUNT_MAX_TENANTS 256
#define TENANTCOUNT_UNASSIGNED  (~0u)

typedef struct
{
  /* sw_if_index -> tenant id (TENANTCOUNT_UNASSIGNED if none) */
  u32 *tenant_by_sw_if_index;

  /* per-tenant packets/bytes, per-thread safe (VPP combined counter) */
  vlib_combined_counter_main_t counters;

  vlib_main_t *vlib_main;
  vnet_main_t *vnet_main;
} tenantcount_main_t;

extern tenantcount_main_t tenantcount_main;
extern vlib_node_registration_t tenantcount_node;

#define foreach_tenantcount_error                                             \
  _ (COUNTED, "packets counted for a tenant")                                 \
  _ (UNASSIGNED, "packets on interface with no tenant")

typedef enum
{
#define _(sym, str) TENANTCOUNT_ERROR_##sym,
  foreach_tenantcount_error
#undef _
    TENANTCOUNT_N_ERROR,
} tenantcount_error_t;

typedef enum
{
  TENANTCOUNT_NEXT_DROP,
  TENANTCOUNT_N_NEXT,
} tenantcount_next_t;

/* Assign (or unassign) an interface to a tenant. Returns 0 or a vnet error. */
int tenantcount_assign (u32 sw_if_index, u32 tenant, int is_del);

#endif /* __included_tenantcount_h__ */
