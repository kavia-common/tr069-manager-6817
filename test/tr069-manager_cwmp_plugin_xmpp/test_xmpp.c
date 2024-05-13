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
#include "test_xmpp.h"

static const char* odl_defs = "../../src/dmdeviceadapter/amx/cwmp_plugin/odl/cwmp_plugin-definition.odl";
static const char* xmpp_odl = "./test_data/xmpp.odl";

void __wrap_cwmp_plugin_transfer_init() {
    //Does nothing
}

int test_xmpp_setup(UNUSED void** state) {
    amxut_bus_setup(state);

    /* Increase level only for debugging */
    sahTraceSetLevel(0);
    sahTraceAddZone(200, "TEST");
    sahTraceAddZone(200, ME);

    amxut_resolve_function("cwmp_plugin_main", _cwmp_plugin_main);
    amxut_resolve_function("ManagementServer.sendInformMessage", _sendInformMessage);
    amxut_resolve_function("connreq_xmpp_connection_changed", _connreq_xmpp_connection_changed);

    amxut_dm_load_odl(odl_defs);
    amxut_dm_load_odl(xmpp_odl);

    prestate_t* prestate = (prestate_t*) *state;

    if(prestate == NULL) {
        SAH_TRACEZ_INFO("TEST", "prestate is NULL");
    } else {
        if((prestate->default_odl != NULL) && (*prestate->default_odl != 0)) {
            SAH_TRACEZ_INFO("TEST", "Loading odl: %s", prestate->default_odl);
            amxut_dm_load_odl(prestate->default_odl);
        }

        if((prestate->xmpp_default_odl != NULL) && (*prestate->xmpp_default_odl != 0)) {
            SAH_TRACEZ_INFO("TEST", "Loading odl: %s", prestate->xmpp_default_odl);
            amxut_dm_load_odl(prestate->xmpp_default_odl);
        }
    }

    assert_success(_cwmp_plugin_main(AMXO_START, amxut_bus_dm(), amxut_bus_parser()));

    assert_non_null(amxb_be_who_has("ManagementServer."));
    assert_non_null(amxb_be_who_has("Device.XMPP."));

    amxp_sigmngr_add_signal(&(amxut_bus_dm()->sigmngr), "SendInformMessage!");
    amxut_timer_go_to_future_ms(1);
    return 0;
}

int test_xmpp_teardown(UNUSED void** state) {
    amxp_signal_t* sig = amxp_sigmngr_find_signal(&(amxut_bus_dm()->sigmngr), "SendInformMessage!");
    amxp_sigmngr_remove_signal(&(amxut_bus_dm()->sigmngr), "SendInformMessage!");
    amxp_signal_delete(&sig);
    assert_success(_cwmp_plugin_main(AMXO_STOP, amxut_bus_dm(), amxut_bus_parser()));
    return amxut_bus_teardown(state);
}

void test_xmpp_start_stop_default(UNUSED void** state) {
    amxut_dm_param_equals(csv_string_t, "ManagementServer.", "SupportedConnReqMethods", "HTTP,XMPP");
    amxut_dm_param_equals(cstring_t, "ManagementServer.", "ConnReqXMPPConnection", "Device.XMPP.Connection.1.");
    amxut_dm_param_equals(csv_string_t, "ManagementServer.", "ConnReqAllowedJabberIDs", "");
    amxut_dm_param_equals(cstring_t, "ManagementServer.", "ConnReqJabberID", "MyJabberID1");
}

static void send_inform_message_cb(const char* const sig_name,
                                   const amxc_var_t* const data,
                                   UNUSED void* const priv) {
    const char* events = GETP_CHAR(data, "data.events");
    bool immediately = GETP_BOOL(data, "data.immediately");
    const char* source = GETP_CHAR(data, "data.source");

    assert_string_equal(sig_name, "SendInformMessage!");
    assert_string_equal(events, "6 CONNECTION REQUEST");
    assert_true(immediately);
    assert_string_equal(source, "XMPP");
}

static amxd_status_t send_inform_message(const char* events, bool immediately, const char* source) {
    amxd_object_t* management_server_obj = NULL;
    amxc_var_t args;
    amxc_var_t ret;
    amxd_status_t status = amxd_status_ok;
    amxc_var_init(&args);
    amxc_var_init(&ret);
    management_server_obj = amxd_dm_findf(amxut_bus_dm(), "ManagementServer.");
    assert_non_null(management_server_obj);
    amxc_var_set_type(&args, AMXC_VAR_ID_HTABLE);
    if(events) {
        amxc_var_add_key(cstring_t, &args, "events", events);
    }
    amxc_var_add_key(bool, &args, "immediately", immediately);
    if(source) {
        amxc_var_add_key(cstring_t, &args, "source", source);
    }
    status = amxd_object_invoke_function(management_server_obj, "sendInformMessage", &args, &ret);
    amxc_var_clean(&args);
    amxc_var_clean(&ret);
    return status;
}

void test_xmpp_ok_rpc_call(UNUSED void** state) {
    assert_int_equal(amxp_slot_connect(&(amxut_bus_dm()->sigmngr), "SendInformMessage!", NULL, send_inform_message_cb, NULL), 0);
    assert_int_equal(send_inform_message("6 CONNECTION REQUEST", true, "XMPP"), amxd_status_ok);
    amxut_bus_handle_events();
}

void test_xmpp_nok_rpc_call(UNUSED void** state) {
    assert_int_equal(send_inform_message(NULL, true, "XMPP"), amxd_status_invalid_function_argument);
    assert_int_equal(send_inform_message("", true, "XMPP"), amxd_status_invalid_arg);
    assert_int_equal(send_inform_message("6 CONNECTION REQUEST", true, NULL), amxd_status_invalid_function_argument);
    assert_int_equal(send_inform_message("6 CONNECTION REQUEST", true, ""), amxd_status_invalid_arg);
}

void test_xmpp_connection_deleted(UNUSED void** state) {
    amxut_dm_param_equals(cstring_t, "ManagementServer.", "ConnReqXMPPConnection", "Device.XMPP.Connection.1.");
    assert_int_equal(amxb_del(amxb_be_who_has("Device."), "Device.XMPP.Connection.", 1, NULL, NULL, 5), 0);
    amxut_bus_handle_events();
    amxut_dm_param_equals(cstring_t, "ManagementServer.", "ConnReqXMPPConnection", "");
}

void test_xmpp_connection_disabled(UNUSED void** state) {
    amxut_dm_param_equals(cstring_t, "ManagementServer.", "ConnReqXMPPConnection", "Device.XMPP.Connection.1.");
    amxut_dm_param_equals(cstring_t, "ManagementServer.", "ConnReqJabberID", "MyJabberID1");
    amxut_dm_param_set_bool("Device.XMPP.Connection.1.", "Enable", false);
    amxut_bus_handle_events();
    amxut_dm_param_equals(cstring_t, "ManagementServer", "ConnReqJabberID", "");
}

void test_xmpp_connection_empty(UNUSED void** state) {
    amxut_dm_param_equals(cstring_t, "ManagementServer.", "ConnReqXMPPConnection", "Device.XMPP.Connection.1.");
    amxut_dm_param_equals(cstring_t, "ManagementServer.", "ConnReqJabberID", "MyJabberID1");
    amxut_dm_param_set_cstring_t("ManagementServer.", "ConnReqXMPPConnection", "");
    amxut_bus_handle_events();
    amxut_dm_param_equals(cstring_t, "ManagementServer", "ConnReqJabberID", "");
}


void test_xmpp_connection_jabber_id_changed(UNUSED void** state) {
    amxut_dm_param_equals(cstring_t, "ManagementServer.", "ConnReqXMPPConnection", "Device.XMPP.Connection.1.");
    amxut_dm_param_equals(cstring_t, "ManagementServer.", "ConnReqJabberID", "MyJabberID1");
    amxut_dm_param_set_cstring_t("Device.XMPP.Connection.1.", "JabberID", "MyNewJabberID1");
    amxut_bus_handle_events();
    amxut_dm_param_equals(cstring_t, "ManagementServer", "ConnReqJabberID", "MyNewJabberID1");
}

void test_xmpp_change_connection(UNUSED void** state) {
    amxut_dm_param_equals(cstring_t, "ManagementServer.", "ConnReqXMPPConnection", "Device.XMPP.Connection.1.");
    amxut_dm_param_set_cstring_t("ManagementServer.", "ConnReqXMPPConnection", "Device.XMPP.Connection.2.");
    amxut_bus_handle_events();
    amxut_dm_param_equals(cstring_t, "ManagementServer.", "ConnReqXMPPConnection", "Device.XMPP.Connection.2.");
    amxut_dm_param_equals(cstring_t, "ManagementServer.", "ConnReqJabberID", "MyJabberID2");
}