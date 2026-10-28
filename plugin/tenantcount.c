/* SPDX-License-Identifier: Apache-2.0
 *
 * tenantcount: per-tenant packet/byte counters on the ip4-unicast feature arc.
 * An interface is mapped to a tenant id; every IPv4 packet received on it is
 * counted against that tenant, then passed on untouched.
 *
 * CLI:
 *   tenantcount assign <interface> tenant <id> [del]
 *   show tenantcount
 *   clear tenantcount
 */
#include <vlib/vlib.h>
#include <vnet/vnet.h>
#include <vnet/plugin/plugin.h>
#include <vpp/app/version.h>
#include <tenantcount/tenantcount.h>

tenantcount_main_t tenantcount_main;

int
tenantcount_assign (u32 sw_if_index, u32 tenant, int is_del)
{
  tenantcount_main_t *tcm = &tenantcount_main;
  u32 prev;

  if (pool_is_free_index (tcm->vnet_main->interface_main.sw_interfaces,
			  sw_if_index))
    return VNET_API_ERROR_INVALID_SW_IF_INDEX;

  if (!is_del && tenant >= TENANTCOUNT_MAX_TENANTS)
    return VNET_API_ERROR_INVALID_VALUE;

  vec_validate_init_empty (tcm->tenant_by_sw_if_index, sw_if_index,
			   TENANTCOUNT_UNASSIGNED);
  prev = tcm->tenant_by_sw_if_index[sw_if_index];

  if (is_del)
    {
      if (prev == TENANTCOUNT_UNASSIGNED)
	return VNET_API_ERROR_NO_SUCH_ENTRY;
      tcm->tenant_by_sw_if_index[sw_if_index] = TENANTCOUNT_UNASSIGNED;
      vnet_feature_enable_disable ("ip4-unicast", "tenantcount", sw_if_index,
				   0 /* disable */, 0, 0);
      return 0;
    }

  tcm->tenant_by_sw_if_index[sw_if_index] = tenant;
  /* Only enable the feature once per interface (re-assign just remaps). */
  if (prev == TENANTCOUNT_UNASSIGNED)
    vnet_feature_enable_disable ("ip4-unicast", "tenantcount", sw_if_index,
				 1 /* enable */, 0, 0);
  return 0;
}

static clib_error_t *
tenantcount_assign_command_fn (vlib_main_t *vm, unformat_input_t *input,
			       vlib_cli_command_t *cmd)
{
  tenantcount_main_t *tcm = &tenantcount_main;
  unformat_input_t _line_input, *line_input = &_line_input;
  u32 sw_if_index = ~0, tenant = ~0;
  int is_del = 0, rv;
  clib_error_t *error = 0;

  if (!unformat_user (input, unformat_line_input, line_input))
    return 0;

  while (unformat_check_input (line_input) != UNFORMAT_END_OF_INPUT)
    {
      if (unformat (line_input, "%U", unformat_vnet_sw_interface,
		    tcm->vnet_main, &sw_if_index))
	;
      else if (unformat (line_input, "tenant %u", &tenant))
	;
      else if (unformat (line_input, "del"))
	is_del = 1;
      else
	{
	  error = clib_error_return (0, "unknown input `%U'",
				     format_unformat_error, line_input);
	  goto done;
	}
    }

  if (sw_if_index == ~0)
    {
      error = clib_error_return (0, "interface required");
      goto done;
    }
  if (!is_del && tenant == ~0)
    {
      error = clib_error_return (0, "tenant <id> required");
      goto done;
    }

  rv = tenantcount_assign (sw_if_index, tenant, is_del);
  if (rv)
    error = clib_error_return (0, "tenantcount_assign failed: %d", rv);

done:
  unformat_free (line_input);
  return error;
}

VLIB_CLI_COMMAND (tenantcount_assign_command, static) = {
  .path = "tenantcount assign",
  .short_help = "tenantcount assign <interface> tenant <id> [del]",
  .function = tenantcount_assign_command_fn,
};

static clib_error_t *
tenantcount_show_command_fn (vlib_main_t *vm, unformat_input_t *input,
			     vlib_cli_command_t *cmd)
{
  tenantcount_main_t *tcm = &tenantcount_main;
  vnet_main_t *vnm = tcm->vnet_main;
  vlib_counter_t c;
  u32 i;

  vlib_cli_output (vm, "Interface assignments:");
  vec_foreach_index (i, tcm->tenant_by_sw_if_index)
    {
      if (tcm->tenant_by_sw_if_index[i] == TENANTCOUNT_UNASSIGNED)
	continue;
      vlib_cli_output (vm, "  %U -> tenant %u", format_vnet_sw_if_index_name,
		       vnm, i, tcm->tenant_by_sw_if_index[i]);
    }

  vlib_cli_output (vm, "Per-tenant counters:");
  for (i = 0; i < TENANTCOUNT_MAX_TENANTS; i++)
    {
      vlib_get_combined_counter (&tcm->counters, i, &c);
      if (c.packets == 0 && c.bytes == 0)
	continue;
      vlib_cli_output (vm, "  tenant %u: %llu packets, %llu bytes", i,
		       (unsigned long long) c.packets,
		       (unsigned long long) c.bytes);
    }
  return 0;
}

VLIB_CLI_COMMAND (tenantcount_show_command, static) = {
  .path = "show tenantcount",
  .short_help = "show tenantcount",
  .function = tenantcount_show_command_fn,
};

static clib_error_t *
tenantcount_clear_command_fn (vlib_main_t *vm, unformat_input_t *input,
			      vlib_cli_command_t *cmd)
{
  vlib_clear_combined_counters (&tenantcount_main.counters);
  return 0;
}

VLIB_CLI_COMMAND (tenantcount_clear_command, static) = {
  .path = "clear tenantcount",
  .short_help = "clear tenantcount",
  .function = tenantcount_clear_command_fn,
};

/* Run on ip4-unicast, before the FIB lookup. */
VNET_FEATURE_INIT (tenantcount_feature, static) = {
  .arc_name = "ip4-unicast",
  .node_name = "tenantcount",
  .runs_before = VNET_FEATURES ("ip4-lookup"),
};

static clib_error_t *
tenantcount_init (vlib_main_t *vm)
{
  tenantcount_main_t *tcm = &tenantcount_main;

  tcm->vlib_main = vm;
  tcm->vnet_main = vnet_get_main ();

  tcm->counters.name = "tenantcount";
  vlib_validate_combined_counter (&tcm->counters,
				  TENANTCOUNT_MAX_TENANTS - 1);
  return 0;
}

VLIB_INIT_FUNCTION (tenantcount_init);

VLIB_PLUGIN_REGISTER () = {
  .version = VPP_BUILD_VER,
  .description = "Per-tenant packet counters (portfolio example)",
};
