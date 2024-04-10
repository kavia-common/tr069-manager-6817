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

#include <stdint.h>
#include <debug/sahtrace.h>
#include <debug/sahtrace_macros.h>
#include <dmengine/DM_ENG_Fault.h>
#include <dmengine/DM_ENG_Type.h>
#include <dmengine/DM_ENG_InformMessageScheduler.h>
#include "DM_AmxScheduleDownload.h"
#include "DM_AmxCommon.h"

#define ME "DM_DA"

#define MANAGEMENTSERVER_TRANSFERS "ManagementServer.Transfers."
#define SCHEDULEDOWNLOAD_TRANSFERS MANAGEMENTSERVER_TRANSFERS "ScheduleDownload."
#define SCHEDULEDOWNLOAD_TRANSFERCOMPLETE SCHEDULEDOWNLOAD_TRANSFERS "[TransferComplete.Initiator == 'ACS']."
#define SCHEDULEDOWNLOAD_TRANSFERCOMPLETE_DONE SCHEDULEDOWNLOAD_TRANSFERS "[TransferComplete.Initiator == 'ACS' and TransferComplete.Status == 'Done']."

static amxb_bus_ctx_t* bus_ctx = NULL;

int DM_ENG_Device_DoScheduleDownload(const char* commandKey, const char* fileType,
                                     const char* url, const char* username, const char* password,
                                     uint32_t fileSize, const char* targetFileName,
                                     TimeWindowStruct* timeWindowList[TIMEWINDOWLIST_MAX_LENGTH]) {

    int retval = DM_ENG_FAULTCODE_9002;
    amxc_var_t* timeWindowListPtr = NULL;
    amxc_var_t* timeWindowPtr = NULL;
    amxc_var_t args;

    SAH_TRACEZ_IN(ME);

    amxc_var_init(&args);

    amxc_var_set_type(&args, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &args, "Initiator", "ACS");
    amxc_var_add_key(cstring_t, &args, "CommandKey", commandKey);
    amxc_var_add_key(cstring_t, &args, "FileType", fileType);
    amxc_var_add_key(cstring_t, &args, "URL", url);
    amxc_var_add_key(cstring_t, &args, "Username", username);
    amxc_var_add_key(cstring_t, &args, "Password", password);
    amxc_var_add_key(uint32_t, &args, "FileSize", fileSize);
    amxc_var_add_key(cstring_t, &args, "TargetFileName", targetFileName);

    timeWindowListPtr = amxc_var_add_new_key(&args, "TimeWindowList");
    amxc_var_set_type(timeWindowListPtr, AMXC_VAR_ID_LIST);
    for(size_t i = 0; i < 2; i++) {
        if(timeWindowList[i]) {
            timeWindowPtr = amxc_var_add_new(timeWindowListPtr);
            amxc_var_set_type(timeWindowPtr, AMXC_VAR_ID_HTABLE);
            amxc_var_add_key(uint32_t, timeWindowPtr, "WindowStart", timeWindowList[i]->windowStart);
            amxc_var_add_key(uint32_t, timeWindowPtr, "WindowEnd", timeWindowList[i]->windowEnd);
            amxc_var_add_key(cstring_t, timeWindowPtr, "WindowMode", timeWindowList[i]->windowMode);
            amxc_var_add_key(cstring_t, timeWindowPtr, "UserMessage", timeWindowList[i]->userMessage);
            amxc_var_add_key(int32_t, timeWindowPtr, "MaxRetries", timeWindowList[i]->maxRetries);
        }
    }

    when_failed_trace(amxb_call(bus_ctx, MANAGEMENTSERVER_TRANSFERS, "addScheduleDownload", &args, NULL, 5),
                      stop, ERROR, "Failed to add ScheduleDownload transfer");

    retval = 0;
stop:
    amxc_var_clean(&args);
    SAH_TRACEZ_OUT(ME);
    return retval;
}

static time_t amxc_tc_to_time_utc(const amxc_ts_t* tsp) {
    time_t retval = -1;
    struct tm tmp = {0};
    when_failed(amxc_ts_to_tm_utc(tsp, &tmp), exit);
    retval = mktime(&tmp);
exit:
    return retval;
}

static int handle_transfercomplete(const char* path) {
    const char* commandKey = NULL;
    uint32_t faultCode = 0;
    const char* faultString = NULL;
    time_t startTime = -1;
    time_t completeTime = -1;
    const char* transferType = NULL;
    bool isScheduleDownload = false;

    int retval = -1;
    amxc_var_t ret;
    amxc_var_init(&ret);

    when_str_empty_trace(path, stop, ERROR, "Invalid arg(s)");
    SAH_TRACEZ_INFO(ME, "Handling TransferComplete for %s", path);

    amxc_string_t tc_path;
    amxc_string_init(&tc_path, 0);
    amxc_string_appendf(&tc_path, "%sTransferComplete.", path);
    retval = amxb_get(bus_ctx, amxc_string_get(&tc_path, 0), 0, &ret, 5);
    when_failed_trace(retval, stop, ERROR, "Failed to get object [%d]", retval);

    when_true_trace(amxc_var_is_null(&ret), stop, ERROR, "Empty result");

    amxc_var_t* tmp = GETP_ARG(&ret, "0.0");

    commandKey = GET_CHAR(tmp, "CommandKey");
    when_null_trace(commandKey, stop, ERROR, "Failed to get CommandKey");
    faultCode = GET_UINT32(tmp, "FaultCode");
    faultString = GET_CHAR(tmp, "FaultString");
    when_null_trace(faultString, stop, ERROR, "Failed to get FaultString");
    startTime = amxc_tc_to_time_utc(amxc_var_constcast(amxc_ts_t, GET_ARG(tmp, "StartTime")));
    completeTime = amxc_tc_to_time_utc(amxc_var_constcast(amxc_ts_t, GET_ARG(tmp, "CompleteTime")));
    transferType = GET_CHAR(tmp, "TransferType");
    when_null_trace(transferType, stop, ERROR, "Failed to get TransferType");

    if(strcmp(transferType, "ScheduleDownload") == 0) {
        isScheduleDownload = true;
    } else {
        SAH_TRACEZ_INFO(ME, "Unsupported TransferType [%s]", transferType);
        goto stop;
    }

    retval = DM_ENG_InformMessageScheduler_SendTransferComplete(false, NULL, NULL, NULL, 0, NULL, false, isScheduleDownload,
                                                                commandKey, faultCode, faultString, startTime, completeTime, path);

    when_failed_trace(retval, stop, ERROR, "Failed to send TransferComplete for [%s]", path);

    retval = 0;
stop:
    amxc_var_clean(&ret);
    amxc_string_clean(&tc_path);
    return retval;
}

static void handle_transfercomplete_cb(char* path) {
    handle_transfercomplete((const char*) path);
}

void differ_transfercomplete_handling(const char* path) {
    when_str_empty_trace(path, stop, ERROR, "Invalid TransferComplete path");
    SAH_TRACEZ_INFO(ME, "Transfer %s is Done", path);
    DM_ENG_NotificationInterface_timerStart(path, 0, 0, handle_transfercomplete_cb);
stop:
    return;
}

void transfercomplete_notification(UNUSED const char* const sig_name,
                                   const amxc_var_t* const data,
                                   UNUSED void* const priv) {

    SAH_TRACEZ_IN(ME);
    const char* status = NULL;
    const char* obj_path = NULL;

    status = GETP_CHAR(data, "parameters.Status.to");
    obj_path = GETP_CHAR(data, "object");

    when_null_trace(status, stop, ERROR, "Failed to get the TransferComplete status");
    when_null_trace(obj_path, stop, ERROR, "Failed to get the TransferComplete path");

    if(strcmp(status, "Done") == 0) {
        amxc_string_t tmp;
        amxc_string_init(&tmp, 0);
        amxc_string_set(&tmp, obj_path);
        amxc_string_replace(&tmp, "TransferComplete.", "", 1);
        handle_transfercomplete(amxc_string_get(&tmp, 0));
        amxc_string_clean(&tmp);
    }

stop:
    SAH_TRACEZ_OUT(ME);
    return;
}

int DM_AmxScheduleDownload_init(amxb_bus_ctx_t* ctx) {
    int retval = -1;
    SAH_TRACEZ_IN(ME);

    amxc_var_t ret;
    amxc_var_init(&ret);

    when_null_trace(ctx, stop, ERROR, "Invalid bus ctx");

    bus_ctx = ctx;

    retval = amxb_get(bus_ctx, SCHEDULEDOWNLOAD_TRANSFERCOMPLETE_DONE, 0, &ret, 5);

    if(retval != amxd_status_ok) {
        SAH_TRACEZ_INFO(ME, "Failed to get TransferComplete");
    } else if(amxc_var_is_null(&ret) != true) {
        amxc_var_t* tmp = GETI_ARG(&ret, 0);
        amxc_var_for_each(element, tmp) {
            differ_transfercomplete_handling(amxc_var_key(element));
        }
    }

    retval = amxb_subscribe(bus_ctx, SCHEDULEDOWNLOAD_TRANSFERCOMPLETE,
                            "(notification == 'dm:object-changed') && (contains('parameters.Status'))",
                            transfercomplete_notification, NULL);

stop:
    amxc_var_clean(&ret);
    SAH_TRACEZ_OUT(ME);
    return retval;
}


int DM_AmxScheduleDownload_clean(void) {
    int retval = -1;
    SAH_TRACEZ_IN(ME);

    retval = amxb_unsubscribe(bus_ctx, SCHEDULEDOWNLOAD_TRANSFERCOMPLETE, transfercomplete_notification, NULL);

    SAH_TRACEZ_OUT(ME);
    return retval;
}

