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

static const char* odl_config = "./test_data/cwmp_plugin.odl";
static const char* odl_defs = "../../src/dmdeviceadapter/amx/cwmp_plugin/odl/cwmp_plugin-definition.odl";
static const char* deviceinfo_odl = "./test_data/deviceinfo.odl";

#define TRANSFER_ROOT_PATH "ManagementServer.ACSTransfers."
#define TRANSFER_ENTRY_PATH TRANSFER_ROOT_PATH "ACSTransfer."

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

    amxd_status_t status = amxd_status_ok;

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
exit:
    return status;
}

int test_download_setup(UNUSED void** state) {
    amxc_var_t path;

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

    amxut_resolve_function("DeviceInfo.FirmwareImage.Download", _deviceinfo_firmwareimage_download);

    amxut_dm_load_odl(odl_config);
    amxut_dm_load_odl(odl_defs);
    amxut_dm_load_odl(deviceinfo_odl);


    assert_non_null(amxb_be_who_has("ManagementServer."));
    assert_non_null(amxb_be_who_has("DeviceInfo."));

    amxut_bus_handle_events();
    return 0;
}

int test_download_teardown(UNUSED void** state) {
    return amxut_bus_teardown(state);
}

void test_download_failed_authentication_failure(UNUSED void** state) {
    amxd_object_t* tranfers = NULL;
    amxc_var_t args;
    amxc_var_t object;
    amxc_var_t ret;
    const char* initiator = "ACS";
    const char* commandkey = "1234";
    const char* fileType = FILETYPE_1_FIRMWARE_UPGRADE_IMAGE;
    const char* url = "http://test.com/image.swu";
    uint32_t fileSize = 0;
    const char* targetFileName = "";
    const char* username = "username";
    const char* password = "password";
    const char* successURL = "";
    const char* failureURL = "";
    uint32_t delayseconds = 0;
    amxc_ts_t ts_start_time;
    amxc_ts_t ts_complete_time;
    int32_t index = 0;

    amxd_status_t status = amxd_status_ok;

    tranfers = amxd_dm_findf(amxut_bus_dm(), TRANSFER_ROOT_PATH);
    assert_non_null(tranfers);

    amxc_var_init(&ret);
    amxc_var_init(&args);
    amxc_var_init(&object);
    amxc_ts_now(&ts_start_time);
    amxc_ts_now(&ts_complete_time);
    ts_complete_time.sec += 3600;

    SAH_TRACEZ_INFO(ME, "ADD New transfer %s", commandkey);

    amxc_var_set_type(&args, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &args, "Initiator", "ACS");
    amxc_var_add_key(cstring_t, &args, "CommandKey", commandkey);
    amxc_var_add_key(bool, &args, "IsDownload", true);
    amxc_var_add_key(cstring_t, &args, "Url", url);
    amxc_var_add_key(cstring_t, &args, "FileType", fileType);
    amxc_var_add_key(uint32_t, &args, "FileSize", fileSize);
    amxc_var_add_key(cstring_t, &args, "TargetFileName", targetFileName);
    amxc_var_add_key(cstring_t, &args, "SaveFileName", targetFileName);
    amxc_var_add_key(cstring_t, &args, "Username", username);
    amxc_var_add_key(cstring_t, &args, "Password", password);
    amxc_var_add_key(cstring_t, &args, "SuccessURL", successURL);
    amxc_var_add_key(cstring_t, &args, "FailureURL", failureURL);
    amxc_var_add_key(uint32_t, &args, "FaultCode", 0);
    amxc_var_add_key(uint32_t, &args, "DelaySeconds", delayseconds);
    amxc_var_add_key(amxc_ts_t, &args, "StartTime", &ts_start_time);
    amxc_var_add_key(amxc_ts_t, &args, "CompleteTime", &ts_complete_time);

    amxc_var_copy(&object, &args);
    assert_int_equal(amxb_add(amxut_bus_ctx(), TRANSFER_ENTRY_PATH, 0, NULL, &object, &ret, 2), 0);
    amxc_var_clean(&object);
    index = GET_INT32(GETI_ARG(&ret, 0), "index");
    amxut_bus_handle_events();

    amxut_dm_param_equals(cstring_t, " ManagementServer.ACSTransfers.ACSTransfer.1.", "Initiator", initiator);
    amxut_dm_param_equals(cstring_t, " ManagementServer.ACSTransfers.ACSTransfer.1.", "CommandKey", commandkey);
    amxut_dm_param_equals(cstring_t, " ManagementServer.ACSTransfers.ACSTransfer.1.", "FileType", fileType);
    amxut_dm_param_equals(cstring_t, " ManagementServer.ACSTransfers.ACSTransfer.1.", "Url", url);
    amxut_dm_param_equals(uint32_t, " ManagementServer.ACSTransfers.ACSTransfer.1.", "FileSize", fileSize);
    amxut_dm_param_equals(cstring_t, " ManagementServer.ACSTransfers.ACSTransfer.1.", "Username", username);
    amxut_dm_param_equals(cstring_t, " ManagementServer.ACSTransfers.ACSTransfer.1.", "Password", password);

    amxc_var_add_key(int32_t, &args, "index", index);

    status = amxd_object_invoke_function(tranfers, "AddTransfer", &args, &ret);
    amxc_var_clean(&args);
    amxc_var_clean(&ret);
    assert_int_equal(status, amxd_status_ok);
    amxut_bus_handle_events();
    amxut_dm_param_equals(cstring_t, " ManagementServer.ACSTransfers.ACSTransfer.1.", "Status", "Applying");
    amxut_dm_param_equals(bool, " ManagementServer.ACSTransfers.ACSTransfer.1.", "IsDownload", true);

    amxut_timer_go_to_future_ms(1);
    deviceinfo_firmwareimage_set("Downloading", "");
    amxut_bus_handle_events();

    amxut_timer_go_to_future_ms(1);
    deviceinfo_firmwareimage_set("DownloadFailed", "(67) Error");
    amxut_bus_handle_events();
    amxut_dm_param_equals(cstring_t, " ManagementServer.ACSTransfers.ACSTransfer.1.", "Status", "Finished");
    amxut_dm_param_equals(uint32_t, " ManagementServer.ACSTransfers.ACSTransfer.1.", "FaultCode", 9012);
    amxut_dm_param_equals(cstring_t, " ManagementServer.ACSTransfers.ACSTransfer.1.", "FaultString", "File transfer server authentication failure");
}
