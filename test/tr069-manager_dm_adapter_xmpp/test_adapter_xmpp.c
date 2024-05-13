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
#include <amxd/amxd_object_event.h>

#include <amxo/amxo.h>

#include <amxut/amxut_bus.h>
#include <amxut/amxut_dm.h>
#include <amxut/amxut_macros.h>
#include <amxut/amxut_sahtrace.h>
#include <amxut/amxut_timer.h>
#include <amxut/amxut_util.h>

#include <debug/sahtrace.h>
#include <debug/sahtrace_macros.h>

#include "test_adapter_xmpp.h"

#include <dmengine/DM_ENG_Type.h>
#include <dmengine/DM_ENG_EntityType.h>
#include <dmengine/DM_ENG_RPCInterface.h>

#include "../../src/dmdeviceadapter/amx/adapter/DM_AmxRPCInterface.h"

static const char* device_odl = "./test_data/device-definition.odl";

int __wrap_DM_ENG_InformMessageScheduler_SendEvents(const char* events, bool immediately, const char* source);

int __wrap_DM_ENG_InformMessageScheduler_SendEvents(const char* events, bool immediately, const char* source) {
    check_expected(events);
    check_expected(immediately);
    check_expected(source);
    return mock_type(int);
}


static void set_DM_ENG_InformMessageScheduler_SendEvents_expect_and_return(const char* events, bool immediately, const char* source, int retval) {
    expect_string(__wrap_DM_ENG_InformMessageScheduler_SendEvents, events, events);
    expect_value(__wrap_DM_ENG_InformMessageScheduler_SendEvents, immediately, immediately);
    expect_string(__wrap_DM_ENG_InformMessageScheduler_SendEvents, source, source);
    will_return(__wrap_DM_ENG_InformMessageScheduler_SendEvents, retval);
}

amxd_status_t _SendInformMessage(amxd_object_t* object,
                                 UNUSED amxd_function_t* func,
                                 amxc_var_t* args,
                                 UNUSED amxc_var_t* ret);

amxd_status_t _SendInformMessage(amxd_object_t* object,
                                 UNUSED amxd_function_t* func,
                                 amxc_var_t* args,
                                 UNUSED amxc_var_t* ret) {
    amxc_var_t data;

    amxc_var_init(&data);
    amxc_var_set_type(&data, AMXC_VAR_ID_HTABLE);
    amxc_var_set_key(&data, "data", args, AMXC_VAR_FLAG_COPY);

    amxd_object_send_signal(object, "SendInformMessage!", &data, false);

    amxc_var_clean(&data);
    return amxd_status_ok;
}

int test_adapter_xmpp_setup(void** state) {
    amxut_bus_setup(state);
    sahTraceSetLevel(500);
    sahTraceAddZone(500, "TEST");
    sahTraceAddZone(500, "DM_DA");

    amxut_dm_load_odl(device_odl);

    assert_non_null(amxb_be_who_has("ManagementServer."));
    amxp_sigmngr_add_signal(&(amxut_bus_dm()->sigmngr), "SendInformMessage!");

    amxut_bus_handle_events();

    return 0;
}

int test_adapter_xmpp_teardown(void** state) {
    amxp_signal_t* sig = amxp_sigmngr_find_signal(&(amxut_bus_dm()->sigmngr), "SendInformMessage!");
    amxp_sigmngr_remove_signal(&(amxut_bus_dm()->sigmngr), "SendInformMessage!");
    amxp_signal_delete(&sig);
    return amxut_bus_teardown(state);
}

void test_adapter_xmpp_start_stop(UNUSED void** state) {
    assert_int_equal(DM_AmxRPCInterface_init(amxb_be_who_has("ManagementServer.")), 0);

    assert_int_equal(DM_AmxRPCInterface_clean(), 0);
}

void test_adapter_xmpp_send_inform_message(UNUSED void** state) {

    int retval = 0;

    assert_int_equal(DM_AmxRPCInterface_init(amxb_be_who_has("ManagementServer.")), 0);

    set_DM_ENG_InformMessageScheduler_SendEvents_expect_and_return("6 CONNECTION REQUEST", true, "XMPP", 0);

    amxd_object_t* obj = amxd_dm_findf(amxut_bus_dm(), "ManagementServer.");
    amxc_var_t args;

    amxc_var_init(&args);
    amxc_var_set_type(&args, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &args, "events", "6 CONNECTION REQUEST");
    amxc_var_add_key(bool, &args, "immediately", true);
    amxc_var_add_key(cstring_t, &args, "source", "XMPP");
    retval = _SendInformMessage(obj, NULL, &args, NULL);
    amxc_var_clean(&args);
    assert_int_equal(retval, 0);

    amxut_bus_handle_events();

    assert_int_equal(DM_AmxRPCInterface_clean(), 0);
}
