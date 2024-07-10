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
#include "test_scheduledownload.h"

static const char* odl_defs = "../../src/dmdeviceadapter/amx/cwmp_plugin/odl/cwmp_plugin-definition.odl";
static const char* deviceinfo_odl = "./test_data/deviceinfo.odl";

typedef struct _download_result_t {
    amxd_status_t amxd_status;
} download_result_t;

static download_result_t download_result = {0};

void __wrap_cwmp_plugin_transfer_init() {
    //Does nothing
}

void __wrap_start_cwmpd() {
    //Does nothing
}

void __wrap_stop_cwmpd() {
    //Does nothing
}

amxd_status_t _deviceinfo_firmwareimage_download(amxd_object_t* obj,
                                                 amxd_function_t* func,
                                                 amxc_var_t* args,
                                                 amxc_var_t* ret); /* TODO: move the *.h */


static void deviceinfo_firmwareimage_set(const char* status, const char* bootFailureLog) {
    amxd_object_t* obj = amxd_dm_findf(amxut_bus_dm(), "DeviceInfo.FirmwareImage.active.");
    amxd_trans_t trans;
    amxd_trans_init(&trans);
    amxd_trans_select_object(&trans, obj);
    amxd_trans_set_value(cstring_t, &trans, "Status", status);
    amxd_trans_set_value(cstring_t, &trans, "BootFailureLog", bootFailureLog);
    amxd_trans_apply(&trans, amxut_bus_dm());
    amxd_trans_clean(&trans);
}

amxd_status_t _deviceinfo_firmwareimage_download(amxd_object_t* obj,
                                                 amxd_function_t* func,
                                                 amxc_var_t* args,
                                                 UNUSED amxc_var_t* ret) {

    amxd_status_t status = amxd_status_unknown_error;

    if(args == NULL) {
        status = amxd_status_invalid_arg;
        goto exit;
    }

    const char* url = GET_CHAR(args, "URL");
    bool autoActivate = GET_BOOL(args, "AutoActivate");
    const char* username = GET_CHAR(args, "Username");
    const char* password = GET_CHAR(args, "Password");

    assert_true(autoActivate);

    SAH_TRACEZ_INFO(ME, "%s.%s(URL='%s', Username='%s', Password='%s')", amxd_object_get_name(obj, AMXD_OBJECT_INDEXED),
                    amxd_function_get_name(func),
                    url, username, password);
    status = download_result.amxd_status;

exit:
    return status;
}

int test_scheduledownload_setup(UNUSED void** state) {
    amxut_bus_setup(state);

    /* Increase level only for debugging */
    sahTraceSetLevel(500);
    sahTraceAddZone(500, "TestZ");
    sahTraceAddZone(500, ME);

    amxut_resolve_function("cwmp_plugin_main", _cwmp_plugin_main);
    amxut_resolve_function("scheduleDownload_added", _scheduleDownload_added);
    amxut_resolve_function("timewindow_started", _timewindow_started);
    amxut_resolve_function("timewindow_ended", _timewindow_ended);
    amxut_resolve_function("schedule_download_transfer_failed", _schedule_download_transfer_failed);
    amxut_resolve_function("app_stopped", _app_stopped);
    amxut_resolve_function("scheduleDownload_del_inst", _scheduleDownload_del_inst);
    amxut_resolve_function("ManagementServer.Transfers.addScheduleDownload", _addScheduleDownload);
    amxut_resolve_function("DeviceInfo.FirmwareImage.Download", _deviceinfo_firmwareimage_download);

    amxut_dm_load_odl(odl_defs);
    amxut_dm_load_odl(deviceinfo_odl);

    prestate_t* prestate = (prestate_t*) *state;

    if(prestate == NULL) {
        SAH_TRACEZ_INFO(ME, "prestate is NULL");
    } else {
        if((prestate->default_odl != NULL) && (*prestate->default_odl != 0)) {
            SAH_TRACEZ_INFO(ME, "Loading odl: %s", prestate->default_odl);
            amxut_dm_load_odl(prestate->default_odl);
        }

        if((prestate->deviceinfo_default_odl != NULL) && (*prestate->deviceinfo_default_odl != 0)) {
            SAH_TRACEZ_INFO(ME, "Loading odl: %s", prestate->deviceinfo_default_odl);
            amxut_dm_load_odl(prestate->deviceinfo_default_odl);
        }
    }

    assert_success(_cwmp_plugin_main(AMXO_START, amxut_bus_dm(), amxut_bus_parser()));

    assert_non_null(amxb_be_who_has("ManagementServer."));
    assert_non_null(amxb_be_who_has("DeviceInfo."));

    download_result.amxd_status = amxd_status_ok;

    amxut_timer_go_to_future_ms(1);
    amxut_bus_handle_events();
    return 0;
}

int test_scheduledownload_teardown(UNUSED void** state) {
    amxp_sigmngr_trigger_signal(&(amxut_bus_dm()->sigmngr), "app:stop", NULL);
    assert_success(_cwmp_plugin_main(AMXO_STOP, amxut_bus_dm(), amxut_bus_parser()));
    return amxut_bus_teardown(state);
}

void test_scheduledownload_basic_start_stop(UNUSED void** state) {
}

void test_scheduledownload_start_stop_nok_pending_upgrade(UNUSED void** state) {
    amxut_dm_param_equals(cstring_t, "ManagementServer.Transfers.ScheduleDownload.1.TimeWindowList.1.", "Status", "Started");
    amxut_dm_param_equals(cstring_t, "ManagementServer.Transfers.ScheduleDownload.1.TimeWindowList.2.", "Status", "Idle");
    amxut_dm_param_equals(cstring_t, "ManagementServer.Transfers.ScheduleDownload.1.TransferComplete.", "Status", "Failed");
    amxut_dm_param_equals(uint32_t, "ManagementServer.Transfers.ScheduleDownload.1.TransferComplete.", "FaultCode", 9002);
    amxut_dm_param_equals(cstring_t, "ManagementServer.Transfers.ScheduleDownload.1.TransferComplete.", "FaultString", "Failed to apply firmware");
    amxut_timer_go_to_future_ms(300 * 1000);
    amxut_bus_handle_events();
    amxut_dm_param_equals(cstring_t, "ManagementServer.Transfers.ScheduleDownload.1.TimeWindowList.1.", "Status", "Ended");
    amxut_dm_param_equals(cstring_t, "ManagementServer.Transfers.ScheduleDownload.1.TimeWindowList.2.", "Status", "Ended");
}

void test_scheduledownload_start_stop_ok_pending_upgrade(UNUSED void** state) {
    amxut_dm_param_equals(cstring_t, "ManagementServer.Transfers.ScheduleDownload.1.TimeWindowList.1.", "Status", "Started");
    amxut_dm_param_equals(cstring_t, "ManagementServer.Transfers.ScheduleDownload.1.TimeWindowList.2.", "Status", "Idle");
    amxut_dm_param_equals(cstring_t, "ManagementServer.Transfers.ScheduleDownload.1.TransferComplete.", "Status", "Done");
    amxut_dm_param_equals(uint32_t, "ManagementServer.Transfers.ScheduleDownload.1.TransferComplete.", "FaultCode", 0);
    amxut_dm_param_equals(cstring_t, "ManagementServer.Transfers.ScheduleDownload.1.TransferComplete.", "FaultString", "");
    amxut_timer_go_to_future_ms(300 * 1000);
    amxut_bus_handle_events();
    amxut_dm_param_equals(cstring_t, "ManagementServer.Transfers.ScheduleDownload.1.TimeWindowList.1.", "Status", "Ended");
    amxut_dm_param_equals(cstring_t, "ManagementServer.Transfers.ScheduleDownload.1.TimeWindowList.2.", "Status", "Ended");
}

void test_scheduledownload_ok_rpc_call(UNUSED void** state) {
    amxd_object_t* tranfers = NULL;
    amxc_var_t args;
    amxc_var_t ret;
    const char* url = "http://ybt.com/image.swu";
    const char* username = "neymar";
    const char* password = "n3ymar";

    amxd_status_t status = amxd_status_ok;

    tranfers = amxd_dm_findf(amxut_bus_dm(), "ManagementServer.Transfers.");

    assert_non_null(tranfers);

    amxc_var_init(&args);
    amxc_var_init(&ret);

    amxc_var_set_type(&args, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &args, "Initiator", "ACS");
    amxc_var_add_key(cstring_t, &args, "CommandKey", "1234");
    amxc_var_add_key(cstring_t, &args, "FileType", FILETYPE_1_FIRMWARE_UPGRADE_IMAGE);
    amxc_var_add_key(cstring_t, &args, "URL", url);
    amxc_var_add_key(uint32_t, &args, "FileSize", 0);

    amxc_var_add_key(cstring_t, &args, "Username", username);
    amxc_var_add_key(cstring_t, &args, "Password", password);

    amxc_var_t* timeWindowList = amxc_var_add_new_key(&args, "TimeWindowList");
    amxc_var_set_type(timeWindowList, AMXC_VAR_ID_LIST);

    amxc_var_t* timeWindow1 = amxc_var_add_new(timeWindowList);
    amxc_var_set_type(timeWindow1, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(uint32_t, timeWindow1, "WindowStart", 100);
    amxc_var_add_key(uint32_t, timeWindow1, "WindowEnd", 200);
    amxc_var_add_key(cstring_t, timeWindow1, "WindowMode", WINDOWMODE_2_IMMEDIATELY);
    amxc_var_add_key(cstring_t, timeWindow1, "UserMessage", "");
    amxc_var_add_key(int32_t, timeWindow1, "MaxRetries", 0);

    download_result.amxd_status = amxd_status_ok;

    status = amxd_object_invoke_function(tranfers, "addScheduleDownload", &args, &ret);

    amxc_var_clean(&args);
    amxc_var_clean(&ret);

    assert_int_equal(status, amxd_status_ok);



    amxut_timer_go_to_future_ms(100 * 1000);
    amxut_bus_handle_events();
    amxut_dm_param_equals(cstring_t, "ManagementServer.Transfers.ScheduleDownload.1.TimeWindowList.1.", "Status", "Started");
    amxut_dm_param_equals(cstring_t, "ManagementServer.Transfers.ScheduleDownload.1.TransferComplete.", "Status", "In Progress");

    deviceinfo_firmwareimage_set("Downloading", "");
    deviceinfo_firmwareimage_set("Validating", "");
    amxut_bus_handle_events();
    amxut_dm_param_equals(cstring_t, "ManagementServer.Transfers.ScheduleDownload.1.TransferComplete.", "Status", "In Progress");
    /* a reboot is expected now to continue the firmware upgrade */
}

void test_scheduledownload_nok_1_rpc_call(UNUSED void** state) {
    amxd_object_t* tranfers = NULL;
    amxc_var_t args;
    amxc_var_t ret;
    const char* url = "http://ybt.com/image.swu";
    const char* username = "messi";
    const char* password = "m3ssi";

    amxd_status_t status = amxd_status_ok;

    tranfers = amxd_dm_findf(amxut_bus_dm(), "ManagementServer.Transfers.");

    assert_non_null(tranfers);


    amxc_var_init(&args);
    amxc_var_init(&ret);

    amxc_var_set_type(&args, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &args, "Initiator", "ACS");
    amxc_var_add_key(cstring_t, &args, "CommandKey", "1234");
    amxc_var_add_key(cstring_t, &args, "FileType", FILETYPE_1_FIRMWARE_UPGRADE_IMAGE);
    amxc_var_add_key(cstring_t, &args, "URL", url);
    amxc_var_add_key(uint32_t, &args, "FileSize", 0);

    amxc_var_add_key(cstring_t, &args, "Username", username);
    amxc_var_add_key(cstring_t, &args, "Password", password);

    amxc_var_t* timeWindowList = amxc_var_add_new_key(&args, "TimeWindowList");
    amxc_var_set_type(timeWindowList, AMXC_VAR_ID_LIST);

    amxc_var_t* timeWindow1 = amxc_var_add_new(timeWindowList);
    amxc_var_set_type(timeWindow1, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(uint32_t, timeWindow1, "WindowStart", 100);
    amxc_var_add_key(uint32_t, timeWindow1, "WindowEnd", 200);
    amxc_var_add_key(cstring_t, timeWindow1, "WindowMode", WINDOWMODE_2_IMMEDIATELY);
    amxc_var_add_key(cstring_t, timeWindow1, "UserMessage", "");
    amxc_var_add_key(int32_t, timeWindow1, "MaxRetries", 0);

    download_result.amxd_status = amxd_status_unknown_error;

    status = amxd_object_invoke_function(tranfers, "addScheduleDownload", &args, &ret);

    amxc_var_clean(&args);
    amxc_var_clean(&ret);

    assert_int_equal(status, amxd_status_ok);

    amxut_timer_go_to_future_ms(100 * 1000);
    amxut_bus_handle_events();
    amxut_dm_param_equals(cstring_t, "ManagementServer.Transfers.ScheduleDownload.1.TimeWindowList.1.", "Status", "Started");
    amxut_dm_param_equals(cstring_t, "ManagementServer.Transfers.ScheduleDownload.1.TransferComplete.", "Status", "Done");

    amxut_timer_go_to_future_ms(100 * 1000);
    amxut_bus_handle_events();
    amxut_dm_param_equals(cstring_t, "ManagementServer.Transfers.ScheduleDownload.1.TimeWindowList.1.", "Status", "Ended");
}

void test_scheduledownload_nok_2_rpc_call(UNUSED void** state) {
    amxd_object_t* tranfers = NULL;
    amxc_var_t args;
    amxc_var_t ret;
    const char* url = "http://ybt.com/image.swu";
    const char* username = "messi";
    const char* password = "m3ssi";

    amxd_status_t status = amxd_status_ok;

    tranfers = amxd_dm_findf(amxut_bus_dm(), "ManagementServer.Transfers.");

    assert_non_null(tranfers);


    amxc_var_init(&args);
    amxc_var_init(&ret);

    amxc_var_set_type(&args, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &args, "Initiator", "ACS");
    amxc_var_add_key(cstring_t, &args, "CommandKey", "1234");
    amxc_var_add_key(cstring_t, &args, "FileType", FILETYPE_1_FIRMWARE_UPGRADE_IMAGE);
    amxc_var_add_key(cstring_t, &args, "URL", url);
    amxc_var_add_key(uint32_t, &args, "FileSize", 0);

    amxc_var_add_key(cstring_t, &args, "Username", username);
    amxc_var_add_key(cstring_t, &args, "Password", password);

    amxc_var_t* timeWindowList = amxc_var_add_new_key(&args, "TimeWindowList");
    amxc_var_set_type(timeWindowList, AMXC_VAR_ID_LIST);

    amxc_var_t* timeWindow1 = amxc_var_add_new(timeWindowList);
    amxc_var_set_type(timeWindow1, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(uint32_t, timeWindow1, "WindowStart", 100);
    amxc_var_add_key(uint32_t, timeWindow1, "WindowEnd", 200);
    amxc_var_add_key(cstring_t, timeWindow1, "WindowMode", WINDOWMODE_2_IMMEDIATELY);
    amxc_var_add_key(cstring_t, timeWindow1, "UserMessage", "");
    amxc_var_add_key(int32_t, timeWindow1, "MaxRetries", 2);

    download_result.amxd_status = amxd_status_unknown_error;

    status = amxd_object_invoke_function(tranfers, "addScheduleDownload", &args, &ret);

    amxc_var_clean(&args);
    amxc_var_clean(&ret);

    assert_int_equal(status, amxd_status_ok);

    amxut_timer_go_to_future_ms(100 * 1000);
    amxut_bus_handle_events();
    amxut_dm_param_equals(cstring_t, "ManagementServer.Transfers.ScheduleDownload.1.TimeWindowList.1.", "Status", "Started");
    amxut_dm_param_equals(cstring_t, "ManagementServer.Transfers.ScheduleDownload.1.TransferComplete.", "Status", "Done");
    amxut_dm_param_equals(int32_t, "ManagementServer.Transfers.ScheduleDownload.1.TimeWindowList.1.", "MaxRetries", 0);

    amxut_timer_go_to_future_ms(100 * 1000);
    amxut_bus_handle_events();
    amxut_dm_param_equals(cstring_t, "ManagementServer.Transfers.ScheduleDownload.1.TimeWindowList.1.", "Status", "Ended");
}

void test_scheduledownload_nok_3_rpc_call(UNUSED void** state) {
    amxd_object_t* tranfers = NULL;
    amxc_var_t args;
    amxc_var_t ret;
    const char* url = "http://ybt.com/image.swu";
    const char* username = "messi";
    const char* password = "m3ssi";

    amxd_status_t status = amxd_status_ok;

    tranfers = amxd_dm_findf(amxut_bus_dm(), "ManagementServer.Transfers.");

    assert_non_null(tranfers);


    amxc_var_init(&args);
    amxc_var_init(&ret);

    amxc_var_set_type(&args, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &args, "Initiator", "ACS");
    amxc_var_add_key(cstring_t, &args, "CommandKey", "1234");
    amxc_var_add_key(cstring_t, &args, "FileType", FILETYPE_1_FIRMWARE_UPGRADE_IMAGE);
    amxc_var_add_key(cstring_t, &args, "URL", url);
    amxc_var_add_key(uint32_t, &args, "FileSize", 0);

    amxc_var_add_key(cstring_t, &args, "Username", username);
    amxc_var_add_key(cstring_t, &args, "Password", password);

    amxc_var_t* timeWindowList = amxc_var_add_new_key(&args, "TimeWindowList");
    amxc_var_set_type(timeWindowList, AMXC_VAR_ID_LIST);

    amxc_var_t* timeWindow1 = amxc_var_add_new(timeWindowList);
    amxc_var_set_type(timeWindow1, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(uint32_t, timeWindow1, "WindowStart", 100);
    amxc_var_add_key(uint32_t, timeWindow1, "WindowEnd", 200);
    amxc_var_add_key(cstring_t, timeWindow1, "WindowMode", WINDOWMODE_1_AT_ANY_TIME);
    amxc_var_add_key(cstring_t, timeWindow1, "UserMessage", "");
    amxc_var_add_key(int32_t, timeWindow1, "MaxRetries", 4);

    download_result.amxd_status = amxd_status_ok;

    status = amxd_object_invoke_function(tranfers, "addScheduleDownload", &args, &ret);

    amxc_var_clean(&args);
    amxc_var_clean(&ret);

    assert_int_equal(status, amxd_status_ok);

    amxut_timer_go_to_future_ms(100 * 1000);
    amxut_bus_handle_events();
    amxut_dm_param_equals(cstring_t, "ManagementServer.Transfers.ScheduleDownload.1.TimeWindowList.1.", "Status", "Started");
    amxut_dm_param_equals(cstring_t, "ManagementServer.Transfers.ScheduleDownload.1.TransferComplete.", "Status", "In Progress");
    amxut_dm_param_equals(int32_t, "ManagementServer.Transfers.ScheduleDownload.1.TimeWindowList.1.", "MaxRetries", 4);

    deviceinfo_firmwareimage_set("Downloading", "");
    deviceinfo_firmwareimage_set("DownloadFailed", "");
    amxut_bus_handle_events();

    /* 1st try */
    amxut_dm_param_equals(cstring_t, "ManagementServer.Transfers.ScheduleDownload.1.TransferComplete.", "Status", "In Progress");
    amxut_dm_param_equals(int32_t, "ManagementServer.Transfers.ScheduleDownload.1.TimeWindowList.1.", "MaxRetries", 3);

    deviceinfo_firmwareimage_set("Downloading", "");
    deviceinfo_firmwareimage_set("ValidationFailed", "");
    amxut_bus_handle_events();

    /* 2nd try */
    amxut_dm_param_equals(cstring_t, "ManagementServer.Transfers.ScheduleDownload.1.TransferComplete.", "Status", "In Progress");
    amxut_dm_param_equals(int32_t, "ManagementServer.Transfers.ScheduleDownload.1.TimeWindowList.1.", "MaxRetries", 2);

    deviceinfo_firmwareimage_set("Downloading", "");
    deviceinfo_firmwareimage_set("InstallationFailed", "");
    amxut_bus_handle_events();

    /* 3th try */
    amxut_dm_param_equals(cstring_t, "ManagementServer.Transfers.ScheduleDownload.1.TransferComplete.", "Status", "In Progress");
    amxut_dm_param_equals(int32_t, "ManagementServer.Transfers.ScheduleDownload.1.TimeWindowList.1.", "MaxRetries", 1);

    deviceinfo_firmwareimage_set("Downloading", "");
    deviceinfo_firmwareimage_set("ActivationFailed", "");
    amxut_bus_handle_events();

    /* 4th try */
    amxut_dm_param_equals(cstring_t, "ManagementServer.Transfers.ScheduleDownload.1.TransferComplete.", "Status", "In Progress");
    amxut_dm_param_equals(int32_t, "ManagementServer.Transfers.ScheduleDownload.1.TimeWindowList.1.", "MaxRetries", 0);

    deviceinfo_firmwareimage_set("Downloading", "");
    deviceinfo_firmwareimage_set("ValidationFailed", "");
    amxut_bus_handle_events();

    amxut_dm_param_equals(cstring_t, "ManagementServer.Transfers.ScheduleDownload.1.TransferComplete.", "Status", "Done");
    amxut_dm_param_equals(int32_t, "ManagementServer.Transfers.ScheduleDownload.1.TimeWindowList.1.", "MaxRetries", 0);

    amxut_timer_go_to_future_ms(100 * 1000);
    amxut_bus_handle_events();
    amxut_dm_param_equals(cstring_t, "ManagementServer.Transfers.ScheduleDownload.1.TimeWindowList.1.", "Status", "Ended");
}

void test_scheduledownload_nok_4_rpc_call(UNUSED void** state) {
    amxd_object_t* tranfers = NULL;
    amxc_var_t args;
    amxc_var_t ret;
    const char* url = "toto://ybt.com/image.swu";
    const char* username = "iniesta";
    const char* password = "ini3sta";

    amxd_status_t status = amxd_status_ok;

    tranfers = amxd_dm_findf(amxut_bus_dm(), "ManagementServer.Transfers.");

    assert_non_null(tranfers);


    amxc_var_init(&args);
    amxc_var_init(&ret);

    amxc_var_set_type(&args, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &args, "Initiator", "ACS");
    amxc_var_add_key(cstring_t, &args, "CommandKey", "1234");
    amxc_var_add_key(cstring_t, &args, "FileType", FILETYPE_1_FIRMWARE_UPGRADE_IMAGE);
    amxc_var_add_key(cstring_t, &args, "URL", url);
    amxc_var_add_key(uint32_t, &args, "FileSize", 0);

    amxc_var_add_key(cstring_t, &args, "Username", username);
    amxc_var_add_key(cstring_t, &args, "Password", password);

    amxc_var_t* timeWindowList = amxc_var_add_new_key(&args, "TimeWindowList");
    amxc_var_set_type(timeWindowList, AMXC_VAR_ID_LIST);

    amxc_var_t* timeWindow1 = amxc_var_add_new(timeWindowList);
    amxc_var_set_type(timeWindow1, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(uint32_t, timeWindow1, "WindowStart", 100);
    amxc_var_add_key(uint32_t, timeWindow1, "WindowEnd", 200);
    amxc_var_add_key(cstring_t, timeWindow1, "WindowMode", WINDOWMODE_2_IMMEDIATELY);
    amxc_var_add_key(cstring_t, timeWindow1, "UserMessage", "");
    amxc_var_add_key(int32_t, timeWindow1, "MaxRetries", 0);

    amxc_var_t* timeWindow2 = amxc_var_add_new(timeWindowList);
    amxc_var_set_type(timeWindow2, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(uint32_t, timeWindow2, "WindowStart", 300);
    amxc_var_add_key(uint32_t, timeWindow2, "WindowEnd", 400);
    amxc_var_add_key(cstring_t, timeWindow2, "WindowMode", WINDOWMODE_2_IMMEDIATELY);
    amxc_var_add_key(cstring_t, timeWindow2, "UserMessage", "");
    amxc_var_add_key(int32_t, timeWindow2, "MaxRetries", 0);

    download_result.amxd_status = amxd_status_ok;

    status = amxd_object_invoke_function(tranfers, "addScheduleDownload", &args, &ret);

    amxc_var_clean(&args);
    amxc_var_clean(&ret);

    assert_int_equal(status, amxd_status_ok);

    amxut_timer_go_to_future_ms(100 * 1000);
    amxut_bus_handle_events();
    amxut_dm_param_equals(cstring_t, "ManagementServer.Transfers.ScheduleDownload.1.TimeWindowList.1.", "Status", "Started");
    amxut_dm_param_equals(cstring_t, "ManagementServer.Transfers.ScheduleDownload.1.TransferComplete.", "Status", "In Progress");

    deviceinfo_firmwareimage_set("Downloading", "");
    deviceinfo_firmwareimage_set("DownloadFailed", "Protocol not supported");
    amxut_bus_handle_events();

    amxut_dm_param_equals(cstring_t, "ManagementServer.Transfers.ScheduleDownload.1.TransferComplete.", "Status", "Failed");
    amxut_dm_param_equals(uint32_t, "ManagementServer.Transfers.ScheduleDownload.1.TransferComplete.", "FaultCode", 9013);
    amxut_dm_param_equals(cstring_t, "ManagementServer.Transfers.ScheduleDownload.1.TransferComplete.", "FaultString", "Protocol not supported");

    amxut_timer_go_to_future_ms(100 * 1000);
    amxut_bus_handle_events();

    amxut_dm_param_equals(cstring_t, "ManagementServer.Transfers.ScheduleDownload.1.TimeWindowList.1.", "Status", "Ended");

    amxut_timer_go_to_future_ms(100 * 1000);
    amxut_bus_handle_events();

    amxut_dm_param_equals(cstring_t, "ManagementServer.Transfers.ScheduleDownload.1.TimeWindowList.2.", "Status", "Started");
    amxut_dm_param_equals(cstring_t, "ManagementServer.Transfers.ScheduleDownload.1.TransferComplete.", "Status", "In Progress");

    deviceinfo_firmwareimage_set("Downloading", "");
    deviceinfo_firmwareimage_set("DownloadFailed", "Protocol not supported");
    amxut_bus_handle_events();

    amxut_dm_param_equals(cstring_t, "ManagementServer.Transfers.ScheduleDownload.1.TransferComplete.", "Status", "Done");
    amxut_dm_param_equals(uint32_t, "ManagementServer.Transfers.ScheduleDownload.1.TransferComplete.", "FaultCode", 9013);
    amxut_dm_param_equals(cstring_t, "ManagementServer.Transfers.ScheduleDownload.1.TransferComplete.", "FaultString", "Protocol not supported");

    amxut_timer_go_to_future_ms(100 * 1000);
    amxut_bus_handle_events();

    amxut_dm_param_equals(cstring_t, "ManagementServer.Transfers.ScheduleDownload.1.TimeWindowList.2.", "Status", "Ended");
}

void test_scheduledownload_nok_ok_rpc_call(UNUSED void** state) {
    amxd_object_t* tranfers = NULL;
    amxc_var_t args;
    amxc_var_t ret;
    const char* url = "http://ybt.com/image.swu";
    const char* username = "iniesta";
    const char* password = "ini3sta";

    amxd_status_t status = amxd_status_ok;

    tranfers = amxd_dm_findf(amxut_bus_dm(), "ManagementServer.Transfers.");

    assert_non_null(tranfers);


    amxc_var_init(&args);
    amxc_var_init(&ret);

    amxc_var_set_type(&args, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &args, "Initiator", "ACS");
    amxc_var_add_key(cstring_t, &args, "CommandKey", "1234");
    amxc_var_add_key(cstring_t, &args, "FileType", FILETYPE_1_FIRMWARE_UPGRADE_IMAGE);
    amxc_var_add_key(cstring_t, &args, "URL", url);
    amxc_var_add_key(uint32_t, &args, "FileSize", 0);

    amxc_var_add_key(cstring_t, &args, "Username", username);
    amxc_var_add_key(cstring_t, &args, "Password", password);

    amxc_var_t* timeWindowList = amxc_var_add_new_key(&args, "TimeWindowList");
    amxc_var_set_type(timeWindowList, AMXC_VAR_ID_LIST);

    amxc_var_t* timeWindow1 = amxc_var_add_new(timeWindowList);
    amxc_var_set_type(timeWindow1, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(uint32_t, timeWindow1, "WindowStart", 100);
    amxc_var_add_key(uint32_t, timeWindow1, "WindowEnd", 200);
    amxc_var_add_key(cstring_t, timeWindow1, "WindowMode", WINDOWMODE_2_IMMEDIATELY);
    amxc_var_add_key(cstring_t, timeWindow1, "UserMessage", "");
    amxc_var_add_key(int32_t, timeWindow1, "MaxRetries", 0);

    amxc_var_t* timeWindow2 = amxc_var_add_new(timeWindowList);
    amxc_var_set_type(timeWindow2, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(uint32_t, timeWindow2, "WindowStart", 300);
    amxc_var_add_key(uint32_t, timeWindow2, "WindowEnd", 400);
    amxc_var_add_key(cstring_t, timeWindow2, "WindowMode", WINDOWMODE_2_IMMEDIATELY);
    amxc_var_add_key(cstring_t, timeWindow2, "UserMessage", "");
    amxc_var_add_key(int32_t, timeWindow2, "MaxRetries", 0);

    download_result.amxd_status = amxd_status_ok;

    status = amxd_object_invoke_function(tranfers, "addScheduleDownload", &args, &ret);

    amxc_var_clean(&args);
    amxc_var_clean(&ret);

    assert_int_equal(status, amxd_status_ok);

    amxut_timer_go_to_future_ms(100 * 1000);
    amxut_bus_handle_events();

    amxut_dm_param_equals(cstring_t, "ManagementServer.Transfers.ScheduleDownload.1.TimeWindowList.1.", "Status", "Started");
    amxut_dm_param_equals(cstring_t, "ManagementServer.Transfers.ScheduleDownload.1.TransferComplete.", "Status", "In Progress");

    deviceinfo_firmwareimage_set("Downloading", "");
    deviceinfo_firmwareimage_set("DownloadFailed", "");
    amxut_bus_handle_events();

    amxut_dm_param_equals(cstring_t, "ManagementServer.Transfers.ScheduleDownload.1.TransferComplete.", "Status", "Failed");
    amxut_dm_param_equals(uint32_t, "ManagementServer.Transfers.ScheduleDownload.1.TransferComplete.", "FaultCode", 9010);
    amxut_dm_param_equals(cstring_t, "ManagementServer.Transfers.ScheduleDownload.1.TransferComplete.", "FaultString", "");

    amxut_timer_go_to_future_ms(100 * 1000);
    amxut_bus_handle_events();

    amxut_dm_param_equals(cstring_t, "ManagementServer.Transfers.ScheduleDownload.1.TimeWindowList.1.", "Status", "Ended");

    amxut_timer_go_to_future_ms(100 * 1000);
    amxut_bus_handle_events();

    amxut_dm_param_equals(cstring_t, "ManagementServer.Transfers.ScheduleDownload.1.TimeWindowList.2.", "Status", "Started");
    amxut_dm_param_equals(cstring_t, "ManagementServer.Transfers.ScheduleDownload.1.TransferComplete.", "Status", "In Progress");

    deviceinfo_firmwareimage_set("Downloading", "");
    deviceinfo_firmwareimage_set("Validating", "");
    amxut_bus_handle_events();

    amxut_dm_param_equals(cstring_t, "ManagementServer.Transfers.ScheduleDownload.1.TransferComplete.", "Status", "In Progress");
    /* a reboot is expected now to continue the firmware upgrade */
}