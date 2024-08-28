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
#include "transfers.h"
#include "smm.h"
#include <gmap/gmap_query.h>

#include "test_manageabledevice.h"

static const char* testname = NULL;
static const char* odl_defs = "../../src/dmdeviceadapter/amx/cwmp_plugin/odl/cwmp_plugin-definition.odl";
static const char* hosts_odl = "./test_data/hosts.odl";
static const char* hosts_default_odl = "./test_data/hosts-default.odl";
static const char* devices_odl = "./test_data/devices.odl";
static const char* devices_default_odl = "./test_data/devices-default.odl";

static gmap_query_t* gmap_query = NULL;

static bool gmap_failure = false;

gmap_query_t* __wrap_gmap_query_open(const char* expression, const char* name, gmap_query_cb_t fn) {
    if(gmap_failure) {
        return NULL;
    }
    gmap_query = (gmap_query_t*) mock();
    check_expected(expression);

    if(gmap_query) {
        gmap_query->expression = expression;
        gmap_query->name = name;
        gmap_query->fn = fn;
    }

    printf("testname:%s\n", testname);
    fflush(stdout);

    /* call the callback function with the default device data with matching expression */
    amxb_bus_ctx_t* bus = amxut_bus_ctx();
    amxc_var_t res;
    amxc_var_init(&res);
    amxb_get(bus, "Devices.Device.*.", -1, &res, 100);
    amxc_var_for_each(device, GETI_ARG(&res, 0)) {
        fn(NULL, GET_CHAR(device, "Key"), device, gmap_query_expression_start_matching);
    }
    amxc_var_clean(&res);

    return gmap_query;
}

void __wrap_gmap_query_close(gmap_query_t* query) {
    check_expected(query);
}

static void trigger_gmap_expression_stop_matching() {
    amxb_bus_ctx_t* bus = amxut_bus_ctx();
    amxc_var_t res;
    amxc_var_init(&res);
    amxb_get(bus, "Devices.Device.*.", -1, &res, 100);
    amxc_var_for_each(device, GETI_ARG(&res, 0)) {
        gmap_query->fn(NULL, GET_CHAR(device, "Key"), device, gmap_query_expression_stop_matching);
    }
    amxc_var_clean(&res);
}

static void dump_datamodel(const char* expression) {
    amxb_bus_ctx_t* bus = amxut_bus_ctx();
    int status = -1;
    amxc_var_t res;

    amxc_var_init(&res);
    printf("Dumping data model '%s' from 0x%lx:", expression, (uintptr_t) bus);
    fflush(stdout);
    status = amxb_get(bus, expression, 100, &res, 100);
    printf("%d\n", status);
    fflush(stdout);

    amxc_var_dump(&res, STDOUT_FILENO);
    amxc_var_clean(&res);
}

static void update_host_parameter(const char* hostpath, const char* name, const char* value) {
    amxd_trans_t trans;
    amxd_trans_init(&trans);
    amxd_trans_select_pathf(&trans, hostpath);
    amxd_trans_set_cstring_t(&trans, name, value);
    assert_int_equal(amxd_status_ok, amxd_trans_apply(&trans, amxut_bus_dm()));
    amxd_trans_clean(&trans);
}

int test_manageabledevice_setup(UNUSED void** state) {

    amxut_bus_setup(state);
    cwmp_plugin_set_app(amxut_bus_dm(), amxut_bus_parser());
    sahTraceOpen("cwmp_plugin", TRACE_TYPE_STDOUT);
    sahTraceSetLevel(500);
    sahTraceAddZone(500, ME);
    amxut_resolve_function("updateConnectionRequestURL", _updateConnectionRequestURL);
    amxut_resolve_function("AddTransfer", _AddTransfer);
    amxut_resolve_function("addScheduleDownload", _addScheduleDownload);
    amxut_resolve_function("sendInformMessage", _sendInformMessage);
    amxut_resolve_function("changeDUState", _changeDUState);
    amxut_resolve_function("connreq_xmpp_connection_changed", _connreq_xmpp_connection_changed);
    amxut_resolve_function("schedule_download_transfer_failed", _schedule_download_transfer_failed);
    amxut_resolve_function("scheduleDownload_del_inst", _scheduleDownload_del_inst);
    amxut_resolve_function("timewindow_started", _timewindow_started);
    amxut_resolve_function("timewindow_ended", _timewindow_ended);
    amxut_resolve_function("scheduleDownload_added", _scheduleDownload_added);
    amxut_resolve_function("dscc_op_done", _dscc_op_done);
    amxut_resolve_function("app_stopped", _app_stopped);

    amxut_dm_load_odl(odl_defs);
    amxut_dm_load_odl(devices_odl);
    amxut_dm_load_odl(devices_default_odl);
    amxut_dm_load_odl(hosts_odl);
    amxut_dm_load_odl(hosts_default_odl);
    assert_non_null(amxb_be_who_has("ManagementServer."));
    assert_non_null(amxb_be_who_has("Hosts.Host."));
    assert_non_null(amxb_be_who_has("Devices.Device."));
    return 0;
}

int test_manageabledevice_teardown(UNUSED void** state) {
    return amxut_bus_teardown(state);
}

void test_manageabledevice_gmap_failure(UNUSED void** state) {
    testname = __func__;
    gmap_query_t query = {0};
    amxd_object_t* template = NULL;

    gmap_failure = true;
    cwmp_plugin_manageabledevice_init();
    amxut_bus_handle_events();

    template = amxd_dm_findf(amxut_bus_dm(), "ManagementServer.ManageableDevice.");
    assert_non_null(template);
    assert_int_equal(amxd_object_get_instance_count(template), 0);

    // second failure
    amxut_timer_go_to_future_ms(10001);
    amxut_bus_handle_events();
    assert_int_equal(amxd_object_get_instance_count(template), 0);

    // no failure
    gmap_failure = false;
    expect_string(__wrap_gmap_query_open, expression, "manageable && .Active != 0");
    will_return(__wrap_gmap_query_open, &query);
    amxut_timer_go_to_future_ms(10001);
    amxut_bus_handle_events();

    expect_value(__wrap_gmap_query_close, query, &query);
    cwmp_plugin_manageabledevice_clean();
}

void test_manageabledevice_start_matching(UNUSED void** state) {
    testname = __func__;
    gmap_query_t query = {0};

    expect_string(__wrap_gmap_query_open, expression, "manageable && .Active != 0");
    will_return(__wrap_gmap_query_open, &query);
    cwmp_plugin_manageabledevice_init();

    amxut_bus_handle_events();

    amxut_dm_param_equals(cstring_t, "ManagementServer.ManageableDevice.1.", "Alias", "cpe-ManageableDevice-1");
    amxut_dm_param_equals(cstring_t, "ManagementServer.ManageableDevice.1.", "SerialNumber", "1.0W1719A0000115");
    amxut_dm_param_equals(cstring_t, "ManagementServer.ManageableDevice.1.", "Host", "Device.Hosts.Host.1.,");
    amxut_dm_param_equals(cstring_t, "ManagementServer.ManageableDevice.2.", "Alias", "cpe-ManageableDevice-2");
    amxut_dm_param_equals(cstring_t, "ManagementServer.ManageableDevice.2.", "SerialNumber", "1.0W1719A0000116");
    amxut_dm_param_equals(cstring_t, "ManagementServer.ManageableDevice.2.", "Host", "Device.Hosts.Host.2.,Device.Hosts.Host.3.,");

    expect_value(__wrap_gmap_query_close, query, &query);
    cwmp_plugin_manageabledevice_clean();
}

void test_manageabledevice_start_stop_matching(UNUSED void** state) {
    testname = __func__;
    gmap_query_t query = {0};
    amxd_object_t* template = NULL;

    expect_string(__wrap_gmap_query_open, expression, "manageable && .Active != 0");
    will_return(__wrap_gmap_query_open, &query);
    cwmp_plugin_manageabledevice_init();

    amxut_bus_handle_events();

    printf("before:\n");
    fflush(stdout);
    dump_datamodel("ManagementServer.ManageableDevice.");

    template = amxd_dm_findf(amxut_bus_dm(), "ManagementServer.ManageableDevice.");
    assert_non_null(template);
    assert_int_equal(amxd_object_get_instance_count(template), 2);

    trigger_gmap_expression_stop_matching();

    printf("after:\n");
    fflush(stdout);
    dump_datamodel("ManagementServer.ManageableDevice.");

    template = amxd_dm_findf(amxut_bus_dm(), "ManagementServer.ManageableDevice.");
    assert_non_null(template);
    assert_int_equal(amxd_object_get_instance_count(template), 0);

    expect_value(__wrap_gmap_query_close, query, &query);
    cwmp_plugin_manageabledevice_clean();
}

void test_manageabledevice_host_available_after_match(UNUSED void** state) {
    testname = __func__;
    gmap_query_t query = {0};
    amxd_object_t* template = NULL;

    expect_string(__wrap_gmap_query_open, expression, "manageable && .Active != 0");
    will_return(__wrap_gmap_query_open, &query);
    cwmp_plugin_manageabledevice_init();

    amxut_bus_handle_events();

    printf("before:\n");
    fflush(stdout);
    dump_datamodel("Devices.Device.");
    dump_datamodel("ManagementServer.ManageableDevice.");

    template = amxd_dm_findf(amxut_bus_dm(), "ManagementServer.ManageableDevice.");
    assert_non_null(template);
    assert_int_equal(amxd_object_get_instance_count(template), 2);

    // WHEN a host physical is updated matching the cached entry
    update_host_parameter("Hosts.Host.cpe-Host-4", "PhysAddress", "00:00:00:00:00:04");
    amxut_bus_handle_events();

    printf("after:\n");
    fflush(stdout);
    dump_datamodel("ManagementServer.ManageableDevice.");

    template = amxd_dm_findf(amxut_bus_dm(), "ManagementServer.ManageableDevice.");
    assert_non_null(template);
    assert_int_equal(amxd_object_get_instance_count(template), 3);

    amxut_dm_param_equals(cstring_t, "ManagementServer.ManageableDevice.3.", "Alias", "cpe-ManageableDevice-3");
    amxut_dm_param_equals(cstring_t, "ManagementServer.ManageableDevice.3.", "SerialNumber", "1.0W1719A0000117");
    amxut_dm_param_equals(cstring_t, "ManagementServer.ManageableDevice.3.", "Host", "Device.Hosts.Host.4.,");

    expect_value(__wrap_gmap_query_close, query, &query);
    cwmp_plugin_manageabledevice_clean();
}

void test_manageabledevice_multiple_hosts_available_after_match(UNUSED void** state) {
    testname = __func__;
    gmap_query_t query = {0};
    amxd_object_t* template = NULL;

    expect_string(__wrap_gmap_query_open, expression, "manageable && .Active != 0");
    will_return(__wrap_gmap_query_open, &query);
    cwmp_plugin_manageabledevice_init();

    amxut_bus_handle_events();

    printf("before:\n");
    fflush(stdout);
    dump_datamodel("Devices.Device.");
    dump_datamodel("ManagementServer.ManageableDevice.");

    template = amxd_dm_findf(amxut_bus_dm(), "ManagementServer.ManageableDevice.");
    assert_non_null(template);
    assert_int_equal(amxd_object_get_instance_count(template), 2);

    // update host with physical address matching a cached entry
    update_host_parameter("Hosts.Host.cpe-Host-4", "PhysAddress", "00:00:00:00:00:04");
    amxut_bus_handle_events();

    // update a second host with physical address matching the same device
    amxut_bus_handle_events();
    update_host_parameter("Hosts.Host.cpe-Host-5", "PhysAddress", "00:00:00:00:00:05");
    amxut_bus_handle_events();

    printf("after:\n");
    fflush(stdout);
    dump_datamodel("ManagementServer.ManageableDevice.");

    template = amxd_dm_findf(amxut_bus_dm(), "ManagementServer.ManageableDevice.");
    assert_non_null(template);
    assert_int_equal(amxd_object_get_instance_count(template), 3);

    amxut_dm_param_equals(cstring_t, "ManagementServer.ManageableDevice.3.", "Alias", "cpe-ManageableDevice-3");
    amxut_dm_param_equals(cstring_t, "ManagementServer.ManageableDevice.3.", "SerialNumber", "1.0W1719A0000117");
    amxut_dm_param_equals(cstring_t, "ManagementServer.ManageableDevice.3.", "Host", "Device.Hosts.Host.4.,Device.Hosts.Host.5.,");

    expect_value(__wrap_gmap_query_close, query, &query);
    cwmp_plugin_manageabledevice_clean();
}
