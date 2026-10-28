/* SPDX-License-Identifier: Apache-2.0 */
#include <vlib/vlib.h>
#include <vnet/vnet.h>
#include <vnet/feature/feature.h>
#include <tenantcount/tenantcount.h>

typedef struct
{
  u32 sw_if_index;
  u32 tenant;
} tenantcount_trace_t;

static u8 *
format_tenantcount_trace (u8 *s, va_list *args)
{
  CLIB_UNUSED (vlib_main_t * vm) = va_arg (*args, vlib_main_t *);
  CLIB_UNUSED (vlib_node_t * node) = va_arg (*args, vlib_node_t *);
  tenantcount_trace_t *t = va_arg (*args, tenantcount_trace_t *);

  if (t->tenant == TENANTCOUNT_UNASSIGNED)
    return format (s, "TENANTCOUNT: sw_if_index %u, no tenant", t->sw_if_index);
  return format (s, "TENANTCOUNT: sw_if_index %u, tenant %u", t->sw_if_index,
		 t->tenant);
}

static char *tenantcount_error_strings[] = {
#define _(sym, string) string,
  foreach_tenantcount_error
#undef _
};

/* Single-packet loop for clarity. A dual/quad loop with prefetch is the
 * natural performance exercise once this is correct (see README, milestone 3). */
VLIB_NODE_FN (tenantcount_node)
(vlib_main_t *vm, vlib_node_runtime_t *node, vlib_frame_t *frame)
{
  tenantcount_main_t *tcm = &tenantcount_main;
  u32 n_left_from, *from, *to_next, next_index;
  u32 thread_index = vm->thread_index;
  u32 n_counted = 0, n_unassigned = 0;

  from = vlib_frame_vector_args (frame);
  n_left_from = frame->n_vectors;
  next_index = node->cached_next_index;

  while (n_left_from > 0)
    {
      u32 n_left_to_next;

      vlib_get_next_frame (vm, node, next_index, to_next, n_left_to_next);

      while (n_left_from > 0 && n_left_to_next > 0)
	{
	  u32 bi0, next0, sw_if_index0, tenant0 = TENANTCOUNT_UNASSIGNED;
	  vlib_buffer_t *b0;

	  bi0 = from[0];
	  to_next[0] = bi0;
	  from += 1;
	  to_next += 1;
	  n_left_from -= 1;
	  n_left_to_next -= 1;

	  b0 = vlib_get_buffer (vm, bi0);
	  sw_if_index0 = vnet_buffer (b0)->sw_if_index[VLIB_RX];

	  if (sw_if_index0 < vec_len (tcm->tenant_by_sw_if_index))
	    tenant0 = tcm->tenant_by_sw_if_index[sw_if_index0];

	  if (tenant0 < TENANTCOUNT_MAX_TENANTS)
	    {
	      vlib_increment_combined_counter (
		&tcm->counters, thread_index, tenant0, 1,
		vlib_buffer_length_in_chain (vm, b0));
	      n_counted++;
	    }
	  else
	    n_unassigned++;

	  /* Hand the packet to the next feature on the arc. */
	  vnet_feature_next (&next0, b0);

	  if (PREDICT_FALSE ((node->flags & VLIB_NODE_FLAG_TRACE) &&
			     (b0->flags & VLIB_BUFFER_IS_TRACED)))
	    {
	      tenantcount_trace_t *t =
		vlib_add_trace (vm, node, b0, sizeof (*t));
	      t->sw_if_index = sw_if_index0;
	      t->tenant = tenant0;
	    }

	  vlib_validate_buffer_enqueue_x1 (vm, node, next_index, to_next,
					   n_left_to_next, bi0, next0);
	}
      vlib_put_next_frame (vm, node, next_index, n_left_to_next);
    }

  vlib_node_increment_counter (vm, tenantcount_node.index,
			       TENANTCOUNT_ERROR_COUNTED, n_counted);
  vlib_node_increment_counter (vm, tenantcount_node.index,
			       TENANTCOUNT_ERROR_UNASSIGNED, n_unassigned);
  return frame->n_vectors;
}

VLIB_REGISTER_NODE (tenantcount_node) = {
  .name = "tenantcount",
  .vector_size = sizeof (u32),
  .format_trace = format_tenantcount_trace,
  .type = VLIB_NODE_TYPE_INTERNAL,
  .n_errors = ARRAY_LEN (tenantcount_error_strings),
  .error_strings = tenantcount_error_strings,
  .n_next_nodes = TENANTCOUNT_N_NEXT,
  .next_nodes = {
    [TENANTCOUNT_NEXT_DROP] = "error-drop",
  },
};
