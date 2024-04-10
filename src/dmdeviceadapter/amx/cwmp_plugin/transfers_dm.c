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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <amxc/amxc.h>
#include <amxp/amxp.h>
#include <amxd/amxd_object.h>
#include <amxd/amxd_dm.h>
#include <amxd/amxd_transaction.h>

#include <debug/sahtrace.h>
#include <debug/sahtrace_macros.h>

#include "cwmp_plugin.h"
#include "transfers.h"

/* Methods */

amxd_status_t _addScheduleDownload(amxd_object_t* obj,
                                   UNUSED amxd_function_t* func,
                                   amxc_var_t* args,
                                   amxc_var_t* ret) {

    amxd_status_t status = amxd_status_invalid_arg;

    const char* initiator = NULL;
    const char* commandKey = NULL;
    const char* fileType = NULL;
    const char* url = NULL;
    const char* username = NULL;
    const char* password = NULL;
    uint32_t fileSize = 0;
    const char* targetFileName = NULL;
    amxc_var_t* timeWindowList = NULL;
    amxc_var_t* timeWindow = NULL;


    when_null_trace(args, stop, ERROR, "Invalid arg(s)");

    // amxc_var_dump(args, STDOUT_FILENO);

    initiator = GET_CHAR(args, "Initiator");
    commandKey = GET_CHAR(args, "CommandKey");
    fileType = GET_CHAR(args, "FileType");
    url = GET_CHAR(args, "URL");
    username = GET_CHAR(args, "Username");
    password = GET_CHAR(args, "Password");
    fileSize = GET_UINT32(args, "FileSize");
    targetFileName = GET_CHAR(args, "TargetFileName");
    timeWindowList = GET_ARG(args, "TimeWindowList");

    amxd_object_t* scheduleDownloadObj = amxd_object_get(obj, "ScheduleDownload.");

    amxd_trans_t trans;
    amxd_trans_init(&trans);
    amxd_trans_select_object(&trans, scheduleDownloadObj);
    amxd_trans_add_inst(&trans, 0, NULL);

    amxd_trans_set_value(cstring_t, &trans, "CommandKey", commandKey);
    amxd_trans_set_value(cstring_t, &trans, "FileType", fileType);
    amxd_trans_set_value(cstring_t, &trans, "URL", url);
    amxd_trans_set_value(cstring_t, &trans, "Username", username);
    amxd_trans_set_value(cstring_t, &trans, "Password", password);
    amxd_trans_set_value(uint32_t, &trans, "FileSize", fileSize);
    amxd_trans_set_value(cstring_t, &trans, "TargetFileName", targetFileName);

    timeWindow = GETI_ARG(timeWindowList, 0);
    when_null_trace(timeWindow, stop, ERROR, "Invalid TimeWindowList");

    amxd_trans_select_pathf(&trans, ".TimeWindowList.");

    amxd_trans_add_inst(&trans, 1, NULL);
    amxd_trans_set_value(cstring_t, &trans, "Status", "Idle");
    amxd_trans_set_value(uint32_t, &trans, "WindowStart", GET_UINT32(timeWindow, "WindowStart"));
    amxd_trans_set_value(uint32_t, &trans, "WindowEnd", GET_UINT32(timeWindow, "WindowEnd"));
    amxd_trans_set_value(cstring_t, &trans, "WindowMode", GET_CHAR(timeWindow, "WindowMode"));
    amxd_trans_set_value(cstring_t, &trans, "UserMessage", GET_CHAR(timeWindow, "UserMessage"));
    amxd_trans_set_value(int32_t, &trans, "MaxRetries", GET_INT32(timeWindow, "MaxRetries"));
    amxd_trans_select_pathf(&trans, ".^");

    timeWindow = GETI_ARG(timeWindowList, 1);
    if(timeWindow) {
        amxd_trans_add_inst(&trans, 2, NULL);
        amxd_trans_set_value(cstring_t, &trans, "Status", "Idle");
        amxd_trans_set_value(uint32_t, &trans, "WindowStart", GET_UINT32(timeWindow, "WindowStart"));
        amxd_trans_set_value(uint32_t, &trans, "WindowEnd", GET_UINT32(timeWindow, "WindowEnd"));
        amxd_trans_set_value(cstring_t, &trans, "WindowMode", GET_CHAR(timeWindow, "WindowMode"));
        amxd_trans_set_value(cstring_t, &trans, "UserMessage", GET_CHAR(timeWindow, "UserMessage"));
        amxd_trans_set_value(int32_t, &trans, "MaxRetries", GET_INT32(timeWindow, "MaxRetries"));
        amxd_trans_select_pathf(&trans, ".^");
    }

    amxd_trans_select_pathf(&trans, ".^");
    amxd_trans_select_pathf(&trans, ".TransferComplete.");
    amxd_trans_set_attr(&trans, amxd_tattr_change_ro, true);
    amxd_trans_set_value(cstring_t, &trans, "Status", "Idle");
    amxd_trans_set_value(cstring_t, &trans, "TransferType", "ScheduleDownload");
    amxd_trans_set_value(cstring_t, &trans, "CommandKey", commandKey);
    amxd_trans_set_value(cstring_t, &trans, "Initiator", initiator);

    status = amxd_trans_apply(&trans, cwmp_plugin_get_dm());

    when_true_trace((status != amxd_status_ok), stop, ERROR, "Failed to create new instance [%d]", status);

    status = amxd_status_ok;
    amxc_var_set(uint32_t, ret, 0);

stop:
    amxd_trans_clean(&trans);
    return status;
}

/* Events */

void _scheduleDownload_added(UNUSED const char* const sig_name,
                             const amxc_var_t* const data,
                             UNUSED void* const priv) {
    SAH_TRACEZ_IN(ME);
    uint32_t index = 0;
    amxd_object_t* obj = NULL;
    when_true_trace(amxc_var_is_null(data), stop, ERROR, "Invalid arg(s)");
    index = GET_UINT32(data, "index");
    obj = amxd_dm_findf(cwmp_plugin_get_dm(), "ManagementServer.Transfers.ScheduleDownload.%d.", index);
    transfers_transfer_add_info(obj);
    transfers_timewindow_start(obj);
    // transfers_handle_transfer(obj);
stop:
    SAH_TRACEZ_OUT(ME);
    return;
}

void _timewindow_started(UNUSED const char* const sig_name,
                         const amxc_var_t* const data,
                         UNUSED void* const priv) {
    const char* timewindow_path = NULL;
    amxd_object_t* obj = NULL;
    amxd_object_t* transfer_obj = NULL;
    transfer_info_t* info = NULL;
    // timewindow_mode_t timewindow_mode = timewindow_mode_undefined;
    // int32_t timewindow_maxretries = -1;
    when_true_trace(amxc_var_is_null(data), stop, ERROR, "Invalid arg(s)");
    // amxc_var_dump(data, STDOUT_FILENO);

    timewindow_path = GET_CHAR(data, "path");
    when_str_empty_trace(timewindow_path, stop, ERROR, "Invalid timewindow path");

    obj = amxd_dm_findf(cwmp_plugin_get_dm(), "%s", timewindow_path);
    transfer_obj = amxd_object_findf(obj, ".^.^.");

    info = transfer_obj->priv;
    when_null_trace(info, stop, ERROR, "Invalid info");
    info->withinTimeWindow = true;
    info->lastTimeWindow = amxd_object_get_index(obj);

    transfers_handle_transfer(transfer_obj);

stop:
    return;
}

void _timewindow_ended(UNUSED const char* const sig_name,
                       const amxc_var_t* const data,
                       UNUSED void* const priv) {
    const char* timewindow_path = NULL;
    amxd_object_t* obj = NULL;
    amxd_object_t* transfer_obj = NULL;
    transfer_info_t* info = NULL;

    when_true_trace(amxc_var_is_null(data), stop, ERROR, "Invalid arg(s)");
    // amxc_var_dump(data, STDOUT_FILENO);

    timewindow_path = GET_CHAR(data, "path");
    when_str_empty_trace(timewindow_path, stop, ERROR, "Invalid timewindow path");

    obj = amxd_dm_findf(cwmp_plugin_get_dm(), "%s", timewindow_path);
    transfer_obj = amxd_object_findf(obj, ".^.^.");

    info = transfer_obj->priv;
    when_null_trace(info, stop, ERROR, "Invalid info");
    info->withinTimeWindow = false;

    transfers_handle_transfer(transfer_obj);

stop:
    return;
}

void _schedule_download_transfer_failed(UNUSED const char* const sig_name,
                                        const amxc_var_t* const data,
                                        UNUSED void* const priv) {
    const char* tc_path = NULL;
    amxd_object_t* obj = NULL;
    amxd_object_t* transfer_obj = NULL;
    transfer_info_t* info = NULL;
    amxd_object_t* timewindow_obj = NULL;
    timewindow_mode_t timewindow_mode = timewindow_mode_undefined;

    when_true_trace(amxc_var_is_null(data), stop, ERROR, "Invalid arg(s)");
    // amxc_var_dump(data, STDOUT_FILENO);

    tc_path = GET_CHAR(data, "path");
    when_str_empty_trace(tc_path, stop, ERROR, "Invalid TransferComplete path");

    obj = amxd_dm_findf(cwmp_plugin_get_dm(), "%s", tc_path);
    transfer_obj = amxd_object_findf(obj, ".^.");

    info = transfer_obj->priv;
    when_null_trace(info, stop, ERROR, "Invalid info");

    timewindow_obj = amxd_object_findf(transfer_obj, ".TimeWindowList.%d.", info->lastTimeWindow);
    when_null_trace(timewindow_obj, stop, ERROR, "Invalid TimeWindow obj");
    timewindow_mode = transfers_timewindow_get_mode(timewindow_obj);

    if((timewindow_mode == timewindow_mode_immediately) || (timewindow_mode == timewindow_mode_at_any_time)) {
        if(info->withinTimeWindow) {
            int32_t timewindow_maxretries = -1;
            timewindow_maxretries = transfers_timewindow_get_maxretries(timewindow_obj);
            if(timewindow_maxretries <= 0) {
                if(info->numberOfTimeWindow == info->lastTimeWindow) {
                    transfers_transfer_done(transfer_obj);
                }
            } else {
                timewindow_maxretries--;
                transfers_timewindow_set_maxretries(timewindow_obj, timewindow_maxretries);
                transfers_start_transfer(transfer_obj);
            }
        } else {
            /* Ignore */
            SAH_TRACEZ_WARNING(ME, "Transfer failed outside timewindow(s)");
        }
    }

stop:
    return;
}

void _app_stopped(UNUSED const char* const sig_name,
                  UNUSED const amxc_var_t* const data,
                  UNUSED void* const priv) {
    transfers_save();
}

/* Actions */

amxd_status_t _scheduleDownload_del_inst(amxd_object_t* object,
                                         UNUSED amxd_param_t* param,
                                         UNUSED amxd_action_t reason,
                                         const amxc_var_t* const args,
                                         UNUSED amxc_var_t* const retval,
                                         UNUSED void* priv) {
    amxd_status_t status = amxd_status_unknown_error;
    amxd_object_t* obj = NULL;
    uint32_t index = 0;
    index = GET_UINT32(args, "index");
    obj = amxd_object_get_instance(object, NULL, index);
    transfers_transfer_del_info(obj);
    status = amxd_status_ok;
    return status;
}

