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

#include "test_schedule_download.h"

#include <dmengine/DM_ENG_Type.h>
#include <dmengine/DM_ENG_EntityType.h>
#include <dmengine/DM_ENG_RPCInterface.h>

#include "../../src/dmdeviceadapter/amx/adapter/DM_AmxScheduleDownload.h"

#define ME "TestZ"

static const char* device_odl = "./test_data/device-definition.odl";


typedef struct _rpc_args_t {
    const char* commandKey;
    const char* fileType;
    const char* url;
    const char* username;
    const char* password;
    uint32_t fileSize;
    const char* targetFileName;
    TimeWindowStruct* timeWindowList[TIMEWINDOWLIST_MAX_LENGTH];
} rpc_args_t;


static TimeWindowStruct timeWindow1 = {
    .windowStart = 3600,
    .windowEnd = 4200,
    .windowMode = TIMEWINDOWSTRUCT_WINDOWMODE_IMMEDIATELY,
    .userMessage = "",
    .maxRetries = 0
};

static TimeWindowStruct timeWindow2 = {
    .windowStart = 4200,
    .windowEnd = 5000,
    .windowMode = TIMEWINDOWSTRUCT_WINDOWMODE_IMMEDIATELY,
    .userMessage = "",
    .maxRetries = 0
};

static rpc_args_t rpc_arg = {
    .commandKey = "MyCommandKey",
    .fileType = FILETYPE_FIRMWARE_UPGRADE_IMAGE,
    .url = "https://ybt.net/image.swu",
    .username = "MyUsername",
    .password = "MyPassword",
    .fileSize = 0,
    .timeWindowList = {
        &timeWindow1,
        &timeWindow2
    }
};

DM_ENG_NotificationStatus __wrap_DM_ENG_NotificationInterface_timerStart(const char* name, int waitTime, int intervalTime, timerHandler handler);

DM_ENG_NotificationStatus __wrap_DM_ENG_NotificationInterface_timerStart(const char* name, int waitTime, int intervalTime, timerHandler handler) {
    amxut_timer_go_to_future_ms(waitTime);
    handler((char*) name);
    return DM_ENG_COMPLETED;
}

int __wrap_DM_ENG_InformMessageScheduler_SendTransferComplete(bool autonomous, const char* announceURL, const char* transferURL,
                                                              const char* fileType, unsigned int fileSize, const char* targetFileName,
                                                              bool isDownload, bool isScheduleDownload,
                                                              const char* commandKey, uint32_t faultCode, const char* faultString,
                                                              time_t startTime, time_t completeTime, const char* path);

int __wrap_DM_ENG_InformMessageScheduler_SendTransferComplete(bool autonomous, const char* announceURL, const char* transferURL,
                                                              const char* fileType, unsigned int fileSize, const char* targetFileName,
                                                              bool isDownload, bool isScheduleDownload,
                                                              const char* commandKey, uint32_t faultCode, const char* faultString,
                                                              time_t startTime, time_t completeTime, const char* path) {
    check_expected(autonomous);
    check_expected(announceURL);
    check_expected(transferURL);
    check_expected(fileType);
    check_expected(fileSize);
    check_expected(targetFileName);
    check_expected(isDownload);

    check_expected(isScheduleDownload);
    check_expected(commandKey);
    check_expected(faultCode);
    check_expected(faultString);
    check_expected(startTime);
    check_expected(completeTime);
    check_expected(path);

    return mock_type(int);
}


static void set_SendTransferComplete_expect_and_return(bool autonomous, const char* announceURL, const char* transferURL,
                                                       const char* fileType, unsigned int fileSize, const char* targetFileName,
                                                       bool isDownload, bool isScheduleDownload,
                                                       const char* commandKey, uint32_t faultCode, const char* faultString,
                                                       time_t startTime, time_t completeTime, const char* path, int retval) {
    expect_value(__wrap_DM_ENG_InformMessageScheduler_SendTransferComplete, autonomous, autonomous);
    expect_value(__wrap_DM_ENG_InformMessageScheduler_SendTransferComplete, announceURL, announceURL);
    expect_value(__wrap_DM_ENG_InformMessageScheduler_SendTransferComplete, transferURL, transferURL);
    expect_value(__wrap_DM_ENG_InformMessageScheduler_SendTransferComplete, fileType, fileType);
    expect_value(__wrap_DM_ENG_InformMessageScheduler_SendTransferComplete, fileSize, fileSize);
    expect_value(__wrap_DM_ENG_InformMessageScheduler_SendTransferComplete, targetFileName, targetFileName);
    expect_value(__wrap_DM_ENG_InformMessageScheduler_SendTransferComplete, isDownload, isDownload);

    expect_value(__wrap_DM_ENG_InformMessageScheduler_SendTransferComplete, isScheduleDownload, isScheduleDownload);
    expect_string(__wrap_DM_ENG_InformMessageScheduler_SendTransferComplete, commandKey, commandKey);
    expect_value(__wrap_DM_ENG_InformMessageScheduler_SendTransferComplete, faultCode, faultCode);
    expect_string(__wrap_DM_ENG_InformMessageScheduler_SendTransferComplete, faultString, faultString);
    expect_value(__wrap_DM_ENG_InformMessageScheduler_SendTransferComplete, startTime, startTime);
    expect_value(__wrap_DM_ENG_InformMessageScheduler_SendTransferComplete, completeTime, completeTime);
    expect_string(__wrap_DM_ENG_InformMessageScheduler_SendTransferComplete, path, path);

    will_return(__wrap_DM_ENG_InformMessageScheduler_SendTransferComplete, retval);
}

amxd_status_t _addScheduleDownload(amxd_object_t* obj,
                                   amxd_function_t* func,
                                   amxc_var_t* args,
                                   UNUSED amxc_var_t* ret);

amxd_status_t _addScheduleDownload(amxd_object_t* obj,
                                   amxd_function_t* func,
                                   amxc_var_t* args,
                                   UNUSED amxc_var_t* ret) {

    amxd_status_t status = amxd_status_invalid_arg;

    when_null(args, stop);

    const char* initiator = GET_CHAR(args, "Initiator");
    const char* commandKey = GET_CHAR(args, "CommandKey");
    const char* fileType = GET_CHAR(args, "FileType");
    const char* url = GET_CHAR(args, "URL");
    const char* username = GET_CHAR(args, "Username");
    const char* password = GET_CHAR(args, "Password");
    uint32_t fileSize = GET_UINT32(args, "FileSize");
    const char* targetFileName = GET_CHAR(args, "TargetFileName");

    when_null(initiator, stop);
    when_failed(strcmp(initiator, "ACS"), stop);

    if(rpc_arg.commandKey != NULL) {
        when_null(commandKey, stop);
        when_failed(strcmp(commandKey, rpc_arg.commandKey), stop);
    }

    if(rpc_arg.fileType != NULL) {
        when_null(fileType, stop);
        when_failed(strcmp(fileType, rpc_arg.fileType), stop);
    }

    if(rpc_arg.url != NULL) {
        when_null(url, stop);
        when_failed(strcmp(url, rpc_arg.url), stop);
    }

    if(rpc_arg.username != NULL) {
        when_null(username, stop);
        when_failed(strcmp(username, rpc_arg.username), stop);
    }

    if(rpc_arg.password != NULL) {
        when_null(password, stop);
        when_failed(strcmp(password, rpc_arg.password), stop);
    }

    when_false((rpc_arg.fileSize == fileSize), stop);

    if(rpc_arg.targetFileName != NULL) {
        when_null(targetFileName, stop);
        when_failed(strcmp(targetFileName, rpc_arg.targetFileName), stop);
    }

    /* TODO: add timewindowlist verification */

    SAH_TRACEZ_INFO(ME, "%s.%s() called", amxd_object_get_name(obj, AMXD_OBJECT_NAMED), amxd_function_get_name(func));

    status = amxd_status_ok;

stop:
    return status;
}

int test_schedule_download_setup(void** state) {
    amxut_bus_setup(state);
    sahTraceSetLevel(500);
    sahTraceAddZone(500, ME);
    sahTraceAddZone(500, "DM_DA");

    amxut_resolve_function("ManagementServer.Transfers.addScheduleDownload", _addScheduleDownload);

    amxut_dm_load_odl(device_odl);

    const char* default_odl = (const char*) *state;

    if((default_odl != NULL) && (*default_odl != 0)) {
        SAH_TRACEZ_INFO(ME, "Loading default: %s", default_odl);
        amxut_dm_load_odl(default_odl);
    }

    assert_non_null(amxb_be_who_has("ManagementServer."));

    amxut_bus_handle_events();

    return 0;
}

int test_schedule_download_teardown(void** state) {
    return amxut_bus_teardown(state);
}

void test_schedule_download_start_stop_1(UNUSED void** state) {
    assert_int_equal(DM_AmxScheduleDownload_init(amxb_be_who_has("ManagementServer.")), 0);

    assert_int_equal(DM_AmxScheduleDownload_clean(), 0);
}

void test_schedule_download_rpc(UNUSED void** state) {

    int retval = 0;

    assert_int_equal(DM_AmxScheduleDownload_init(amxb_be_who_has("ManagementServer.")), 0);

    retval = DM_ENG_Device_DoScheduleDownload(rpc_arg.commandKey, rpc_arg.fileType, rpc_arg.url, rpc_arg.username,
                                              rpc_arg.password, rpc_arg.fileSize, rpc_arg.targetFileName, rpc_arg.timeWindowList);

    assert_int_equal(retval, 0);

    assert_int_equal(DM_AmxScheduleDownload_clean(), 0);
}

void test_schedule_download_notification(UNUSED void** state) {

    struct tm tmp = {0};
    bool isScheduleDownload = true;
    const char* commandKey = "1234";
    uint32_t faultCode = 0;
    const char* faultString = "";
    time_t startTime = -1;
    time_t completeTime = -1;

    assert_non_null(strptime("0001-01-01T00:00:00Z", "%Y-%m-%dT%H:%M:%SZ", &tmp));

    startTime = mktime(&tmp);
    completeTime = mktime(&tmp);

    assert_false(((startTime == -1) || (completeTime == -1)));
    const char* path = "ManagementServer.Transfers.ScheduleDownload.1.";

    set_SendTransferComplete_expect_and_return(false, NULL, NULL, NULL, 0, NULL, false, isScheduleDownload,
                                               commandKey, faultCode, faultString, startTime, completeTime, path, 0);

    assert_int_equal(DM_AmxScheduleDownload_init(amxb_be_who_has("ManagementServer.")), 0);

    amxut_dm_param_equals_cstring_t("ManagementServer.Transfers.ScheduleDownload.1.TransferComplete.", "Status", "");
    amxut_dm_param_set_cstring_t("ManagementServer.Transfers.ScheduleDownload.1.TransferComplete.", "Status", "Done");
    amxut_dm_param_equals_cstring_t("ManagementServer.Transfers.ScheduleDownload.1.TransferComplete.", "Status", "Done");

    amxut_bus_handle_events();

    assert_int_equal(DM_AmxScheduleDownload_clean(), 0);
}

void test_schedule_download_start_stop_2(UNUSED void** state) {

    struct tm tmp = {0};
    bool isScheduleDownload = true;
    const char* commandKey = "5678";
    uint32_t faultCode = 0;
    const char* faultString = "";
    time_t startTime = -1;
    time_t completeTime = -1;

    assert_non_null(strptime("0001-01-01T00:00:00Z", "%Y-%m-%dT%H:%M:%SZ", &tmp));

    startTime = mktime(&tmp);
    completeTime = mktime(&tmp);

    assert_false(((startTime == -1) || (completeTime == -1)));

    const char* path = "ManagementServer.Transfers.ScheduleDownload.2.";

    set_SendTransferComplete_expect_and_return(false, NULL, NULL, NULL, 0, NULL, false, isScheduleDownload,
                                               commandKey, faultCode, faultString, startTime, completeTime, path, 0);

    assert_int_equal(DM_AmxScheduleDownload_init(amxb_be_who_has("ManagementServer.")), 0);

    amxut_timer_go_to_future_ms(0);
    amxut_bus_handle_events();

    assert_int_equal(DM_AmxScheduleDownload_clean(), 0);
}
