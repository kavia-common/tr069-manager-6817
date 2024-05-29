/****************************************************************************
**
** SPDX-License-Identifier: BSD-2-Clause-Patent
**
** SPDX-FileCopyrightText: Copyright (c) 2023 SoftAtHome
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

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <setjmp.h>
#include <stdarg.h>
#include <cmocka.h>
#include <string.h>
#include <time.h>

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
#include <debug/sahtrace_macros.h>

#include "test_change_du_state.h"

#include <dmengine/DM_ENG_Type.h>
#include <dmengine/DM_ENG_EntityType.h>
#include <dmengine/DM_ENG_RPCInterface.h>

#include "../../src/dmdeviceadapter/amx/adapter/DM_AmxChangeDUState.h"

#define ME "TestZ"

static struct rpc_arg_t {
    const char* commandKey;
    OperationStruct operations;
} rpc_arg;

static const char* definition_odl = "./test_data/smm-definition.odl";

amxd_status_t _changeDUState(amxd_object_t* object,
                             amxd_function_t* func,
                             amxc_var_t* args,
                             amxc_var_t* ret);

DM_ENG_NotificationStatus __wrap_DM_ENG_NotificationInterface_timerStart(const char* name, int waitTime, int intervalTime, timerHandler handler);

amxd_status_t _changeDUState(amxd_object_t* object,
                             amxd_function_t* func,
                             amxc_var_t* args,
                             amxc_var_t* ret) {

    amxd_status_t status = amxd_status_invalid_arg;

    when_null(args, stop);

    const char* initiator = GET_CHAR(args, "Initiator");
    const char* commandKey = GET_CHAR(args, "CommandKey");
    amxc_var_t* operations = GET_ARG(args, "Operations");

    assert_non_null(initiator);
    assert_string_equal(initiator, "ACS");

    if(rpc_arg.commandKey != NULL) {
        assert_non_null(commandKey);
        assert_string_equal(commandKey, rpc_arg.commandKey);
    }

    if(rpc_arg.operations.type == 1) {
        if(rpc_arg.operations.data.install.uuid != NULL) {
            const char* uuid = GETP_CHAR(operations, "0.UUID");
            assert_non_null(uuid);
            assert_string_equal(uuid, rpc_arg.operations.data.install.uuid);
        }

        if(rpc_arg.operations.data.install.url != NULL) {
            const char* url = GETP_CHAR(operations, "0.URL");
            assert_non_null(url);
            assert_string_equal(url, rpc_arg.operations.data.install.url);
        }

        if(rpc_arg.operations.data.install.username != NULL) {
            const char* username = GETP_CHAR(operations, "0.Username");
            assert_non_null(username);
            assert_string_equal(username, rpc_arg.operations.data.install.username);
        }

        if(rpc_arg.operations.data.install.password != NULL) {
            const char* password = GETP_CHAR(operations, "0.Password");
            assert_non_null(password);
            assert_string_equal(password, rpc_arg.operations.data.install.password);
        }

        if(rpc_arg.operations.data.install.executionEnvRef != NULL) {
            const char* executionEnvRef = GETP_CHAR(operations, "0.ExecutionEnvRef");
            assert_non_null(executionEnvRef);
            assert_string_equal(executionEnvRef, rpc_arg.operations.data.install.executionEnvRef);
        }
    } else if(rpc_arg.operations.type == 2) {
        /* TODO: same for update */
    } else if(rpc_arg.operations.type == 3) {
        /* TODO: same for uninstall */
    }

    status = amxd_status_ok;

stop:
    return status;
}

DM_ENG_NotificationStatus __wrap_DM_ENG_NotificationInterface_timerStart(const char* name, int waitTime, int intervalTime, timerHandler handler) {
    amxut_timer_go_to_future_ms(waitTime);
    handler((char*) name);
    return DM_ENG_COMPLETED;
}


int __wrap_DM_ENG_InformMessageScheduler_SendDUStateChangeComplete(amxc_var_t* results, const char* commandKey, const char* path);

int __wrap_DM_ENG_InformMessageScheduler_SendDUStateChangeComplete(amxc_var_t* results, const char* commandKey, const char* path) {
    amxc_var_dump(results, STDOUT_FILENO);
    check_expected(commandKey);
    check_expected(path);
    return mock_type(int);
}


static void set_SendDUStateChangeComplete_expect_and_return(amxc_var_t* results, const char* commandKey, const char* path, int retval) {
    (void) results; /* TODO: */
    expect_string(__wrap_DM_ENG_InformMessageScheduler_SendDUStateChangeComplete, commandKey, commandKey);
    expect_string(__wrap_DM_ENG_InformMessageScheduler_SendDUStateChangeComplete, path, path);

    will_return(__wrap_DM_ENG_InformMessageScheduler_SendDUStateChangeComplete, retval);
}

int test_change_du_state_setup(void** state) {
    amxut_bus_setup(state);
    sahTraceSetLevel(500);
    sahTraceAddZone(500, ME);
    sahTraceAddZone(500, "DM_DA");

    amxut_resolve_function("ManagementServer.SMM.changeDUState", _changeDUState);

    amxut_dm_load_odl(definition_odl);

    const char* default_odl = (const char*) *state;

    if((default_odl != NULL) && (*default_odl != 0)) {
        SAH_TRACEZ_INFO(ME, "Loading default: %s", default_odl);
        amxut_dm_load_odl(default_odl);
    }

    assert_non_null(amxb_be_who_has("ManagementServer."));

    amxut_bus_handle_events();

    return 0;
}

int test_change_du_state_teardown(void** state) {
    return amxut_bus_teardown(state);
}

void test_change_du_state_rpc(UNUSED void** state) {
    int retval = 0;
    rpc_arg.commandKey = "2403";
    rpc_arg.operations.type = 1;
    rpc_arg.operations.data.install.uuid = "";
    rpc_arg.operations.data.install.url = "MyURL.com";
    rpc_arg.operations.data.install.username = "";
    rpc_arg.operations.data.install.password = "";
    rpc_arg.operations.data.install.executionEnvRef = "MyEnvRef";
    assert_int_equal(DM_AmxChangeDUState_init(amxb_be_who_has("ManagementServer.")), 0);
    retval = DM_ENG_Device_DoChangeDUState(&(rpc_arg.operations), rpc_arg.commandKey);
    assert_int_equal(retval, 0);
    assert_int_equal(DM_AmxChangeDUState_clean(), 0);
}

void test_change_du_state_notification(UNUSED void** state) {

    const char* commandKey = "2403";
    const char* path = "ManagementServer.SMM.DUStateChangeComplete.1.";
    set_SendDUStateChangeComplete_expect_and_return(NULL, commandKey, path, 0);
    assert_int_equal(DM_AmxChangeDUState_init(amxb_be_who_has("ManagementServer.")), 0);
    amxut_dm_param_equals_cstring_t("ManagementServer.SMM.DUStateChangeComplete.1.", "Status", "");
    amxut_dm_param_set_cstring_t("ManagementServer.SMM.DUStateChangeComplete.1.", "Status", "Done");
    amxut_dm_param_equals_cstring_t("ManagementServer.SMM.DUStateChangeComplete.1.", "Status", "Done");
    amxut_bus_handle_events();
    assert_int_equal(DM_AmxChangeDUState_clean(), 0);
}

void test_change_du_state_start_stop(UNUSED void** state) {
    const char* commandKey = "3103";
    const char* path = "ManagementServer.SMM.DUStateChangeComplete.2.";
    set_SendDUStateChangeComplete_expect_and_return(NULL, commandKey, path, 0);
    assert_int_equal(DM_AmxChangeDUState_init(amxb_be_who_has("ManagementServer.")), 0);
    amxut_timer_go_to_future_ms(0);
    amxut_bus_handle_events();
    assert_int_equal(DM_AmxChangeDUState_clean(), 0);
}
