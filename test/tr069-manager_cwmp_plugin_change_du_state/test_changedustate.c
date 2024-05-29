/****************************************************************************
**
** SPDX-License-Identifier: BSD-2-Clause-Patent
**
** SPDX-FileCopyrightText: Copyright (c) 2024 SoftAtHome
**
** Redistribution and use in source and binary forms, with or without modification,
** are permitted provided that the following conditions are met:
**
** 1. Redistributions of source code must retain the above copyright notice,
** this list of conditions and the following disclaimer.
**
** 2. Redistributions in binary form must reproduce the above copyright notice,
** this list of conditions and the following disclaimer in the documentation
** and/or other materials provided with the distribution.
**
** Subject to the terms and conditions of this license, each copyright holder
** and contributor hereby grants to those receiving rights under this license
** a perpetual, worldwide, non-exclusive, no-charge, royalty-free, irrevocable
** (except for failure to satisfy the conditions of this license) patent license
** to make, have made, use, offer to sell, sell, import, and otherwise transfer
** this software, where such license applies only to those patent claims, already
** acquired or hereafter acquired, licensable by such copyright holder or contributor
** that are necessarily infringed by:
**
** (a) their Contribution(s) (the licensed copyrights of copyright holders and
** non-copyrightable additions of contributors, in source or binary form) alone;
** or
**
** (b) combination of their Contribution(s) with the work of authorship to which
** such Contribution(s) was added by such copyright holder or contributor, if,
** at the time the Contribution is added, such addition causes such combination
** to be necessarily infringed. The patent license shall not apply to any other
** combinations which include the Contribution.
**
** Except as expressly stated above, no rights or licenses from any copyright
** holder or contributor is granted under this license, whether expressly, by
** implication, estoppel or otherwise.
**
** DISCLAIMER
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
** AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE
** LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
** SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
** CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
** OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
** USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <setjmp.h>
#include <stdarg.h>
#include <cmocka.h>

#include <amxc/amxc_macros.h>
#include <amxc/amxc.h>
#include <amxp/amxp.h>

#include <amxd/amxd_dm.h>
#include <amxd/amxd_object.h>
#include <amxd/amxd_transaction.h>

#include <amxo/amxo.h>

#include <amxut/amxut_bus.h>
#include <amxut/amxut_dm.h>
#include <amxut/amxut_macros.h>
#include <amxut/amxut_sahtrace.h>
#include <amxut/amxut_timer.h>
#include <amxut/amxut_util.h>

#include <debug/sahtrace.h>

#include "cwmp_plugin.h"
#include "smm.h"
#include "test_changedustate.h"

static const char* odl_defs = "../../src/dmdeviceadapter/amx/cwmp_plugin/odl/cwmp_plugin-definition.odl";

void __wrap_cwmp_plugin_transfer_init() {
    //Does nothing
}

int test_changedustate_setup(UNUSED void** state) {
    amxut_bus_setup(state);

    /* Increase level only for debugging */
    sahTraceSetLevel(500);
    sahTraceAddZone(500, "TestZ");
    sahTraceAddZone(500, ME);

    amxut_resolve_function("cwmp_plugin_main", _cwmp_plugin_main);
    amxut_resolve_function("ManagementServer.SMM.changeDUState", _changeDUState);

    amxut_dm_load_odl(odl_defs);

    const char* default_odl = (const char*) *state;

    if((default_odl != NULL) && (*default_odl != 0)) {
        SAH_TRACEZ_INFO(ME, "Loading default: %s", default_odl);
        amxut_dm_load_odl(default_odl);
    }

    assert_success(_cwmp_plugin_main(AMXO_START, amxut_bus_dm(), amxut_bus_parser()));

    assert_non_null(amxb_be_who_has("ManagementServer."));

    amxut_bus_handle_events();
    return 0;
}

int test_changedustate_teardown(UNUSED void** state) {
    assert_success(_cwmp_plugin_main(AMXO_STOP, amxut_bus_dm(), amxut_bus_parser()));
    return amxut_bus_teardown(state);
}

void test_changedustate_ok_rpc_call(UNUSED void** state) {
    amxd_object_t* smm = NULL;
    amxd_status_t status = amxd_status_ok;
    amxc_var_t* args = NULL;
    amxc_var_t ret;
    amxc_var_init(&ret);
    smm = amxd_dm_findf(amxut_bus_dm(), "ManagementServer.SMM.");
    assert_non_null(smm);
    args = amxut_util_read_json_from_file("./test_data/rpc_arg_01.json");
    status = amxd_object_invoke_function(smm, "changeDUState", args, &ret);
    amxc_var_delete(&args);
    amxc_var_clean(&ret);
    assert_int_equal(status, amxd_status_ok);
    amxut_bus_handle_events();

    amxut_dm_param_equals(cstring_t, "ManagementServer.SMM.DUStateChangeComplete.1.", "CommandKey", "240391");
    amxut_dm_param_equals(cstring_t, "ManagementServer.SMM.DUStateChangeComplete.1.", "Initiator", "ACS");
    amxut_dm_param_equals(cstring_t, "ManagementServer.SMM.DUStateChangeComplete.1.", "Status", "");

    amxut_dm_param_equals(cstring_t, "ManagementServer.SMM.DUStateChangeComplete.1.Operations.1.", "Status", "");
    amxut_dm_param_equals(cstring_t, "ManagementServer.SMM.DUStateChangeComplete.1.Operations.1.", "Type", "Install");
    amxut_dm_param_equals(cstring_t, "ManagementServer.SMM.DUStateChangeComplete.1.Operations.1.", "URL", "my_url.com");
    amxut_dm_param_equals(cstring_t, "ManagementServer.SMM.DUStateChangeComplete.1.Operations.1.", "Username", "my_username");
    amxut_dm_param_equals(cstring_t, "ManagementServer.SMM.DUStateChangeComplete.1.Operations.1.", "Password", "my_password");
    amxut_dm_param_equals(cstring_t, "ManagementServer.SMM.DUStateChangeComplete.1.Operations.1.", "UUID", "1234");
    amxut_dm_param_equals(cstring_t, "ManagementServer.SMM.DUStateChangeComplete.1.Operations.1.", "ExecutionEnvRef", "MyEnv");
    amxut_dm_param_equals(cstring_t, "ManagementServer.SMM.DUStateChangeComplete.1.Operations.1.", "Version", "");

    amxut_dm_param_equals(cstring_t, "ManagementServer.SMM.DUStateChangeComplete.1.Operations.2.", "Status", "");
    amxut_dm_param_equals(cstring_t, "ManagementServer.SMM.DUStateChangeComplete.1.Operations.2.", "Type", "Update");
    amxut_dm_param_equals(cstring_t, "ManagementServer.SMM.DUStateChangeComplete.1.Operations.2.", "URL", "my_url.com");
    amxut_dm_param_equals(cstring_t, "ManagementServer.SMM.DUStateChangeComplete.1.Operations.2.", "Username", "");
    amxut_dm_param_equals(cstring_t, "ManagementServer.SMM.DUStateChangeComplete.1.Operations.2.", "Password", "");
    amxut_dm_param_equals(cstring_t, "ManagementServer.SMM.DUStateChangeComplete.1.Operations.2.", "UUID", "1234");
    amxut_dm_param_equals(cstring_t, "ManagementServer.SMM.DUStateChangeComplete.1.Operations.2.", "ExecutionEnvRef", "");
    amxut_dm_param_equals(cstring_t, "ManagementServer.SMM.DUStateChangeComplete.1.Operations.2.", "Version", "v1.2");

    amxut_dm_param_equals(cstring_t, "ManagementServer.SMM.DUStateChangeComplete.1.Operations.3.", "Status", "");
    amxut_dm_param_equals(cstring_t, "ManagementServer.SMM.DUStateChangeComplete.1.Operations.3.", "Type", "Uninstall");
    amxut_dm_param_equals(cstring_t, "ManagementServer.SMM.DUStateChangeComplete.1.Operations.3.", "URL", "");
    amxut_dm_param_equals(cstring_t, "ManagementServer.SMM.DUStateChangeComplete.1.Operations.3.", "Username", "");
    amxut_dm_param_equals(cstring_t, "ManagementServer.SMM.DUStateChangeComplete.1.Operations.3.", "Password", "");
    amxut_dm_param_equals(cstring_t, "ManagementServer.SMM.DUStateChangeComplete.1.Operations.3.", "UUID", "1234");
    amxut_dm_param_equals(cstring_t, "ManagementServer.SMM.DUStateChangeComplete.1.Operations.3.", "ExecutionEnvRef", "MyEnv");
    amxut_dm_param_equals(cstring_t, "ManagementServer.SMM.DUStateChangeComplete.1.Operations.3.", "Version", "v1.2");
}


void test_dscc_instance_delete(UNUSED void** state) {
    amxc_var_t ret;
    amxc_var_init(&ret);
    assert_int_equal(amxb_del(amxut_bus_ctx(), "ManagementServer.SMM.DUStateChangeComplete.1.", 0, NULL, &ret, 3), 0);
    amxc_var_clean(&ret);
}
