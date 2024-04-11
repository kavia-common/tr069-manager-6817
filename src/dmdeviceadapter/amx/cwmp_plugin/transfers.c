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

static void transfers_update_transfercomplete(amxd_object_t* transfer_obj,
                                              amxc_ts_t* start_time,
                                              amxc_ts_t* complete_time,
                                              uint32_t* fault_code,
                                              const char* fault_string);

void transfers_firmwareupgrade_done(amxd_object_t* obj, uint32_t fault_code, const char* fault_string) {
    amxd_object_t* timewindow_obj = NULL;
    timewindow_mode_t timewindow_mode = timewindow_mode_undefined;
    transfer_info_t* info = NULL;

    when_null_trace(obj, stop, ERROR, "Invalid arg(s)");

    info = obj->priv;
    when_null_trace(info, stop, ERROR, "Invalid info");

    when_true_trace((info->lastTimeWindow == 0), stop, INFO, "Invalid last TimeWindow index");

    timewindow_obj = amxd_object_findf(obj, ".TimeWindowList.%d.", info->lastTimeWindow);
    timewindow_mode = transfers_timewindow_get_mode(timewindow_obj);

    if((timewindow_mode == timewindow_mode_immediately) || (timewindow_mode == timewindow_mode_at_any_time)) {
        if(info->withinTimeWindow) {
            transfers_update_transfercomplete(obj, NULL, NULL, &fault_code, fault_string);
            if(fault_code == 0) {
                transfers_transfer_done(obj);
            } else {
                transfers_set_status(obj, transfer_status_failed);
            }
        } else {
            SAH_TRACEZ_WARNING(ME, "Transfer done outside timewindow(s)");
        }
    }

stop:
    return;
}

transfer_status_t transfers_get_status(amxd_object_t* obj) {
    amxd_object_t* tc_obj = NULL;
    transfer_status_t transfer_status = transfer_status_undefined;
    char* value = NULL;

    when_null_trace(obj, stop, ERROR, "Invalid arg(s)");

    tc_obj = amxd_object_findf(obj, ".TransferComplete.");

    when_null_trace(tc_obj, stop, ERROR, "Failed to get TransferComplete obj");

    value = amxd_object_get_value(cstring_t, tc_obj, "Status", NULL);

    when_str_empty_trace(value, stop, ERROR, "Failed to get TimeWindow's Status");

    if(strcmp(value, "Idle") == 0) {
        transfer_status = transfer_status_idle;
    } else if(strcmp(value, "In Progress") == 0) {
        transfer_status = transfer_status_in_progress;
    } else if(strcmp(value, "Failed") == 0) {
        transfer_status = transfer_status_failed;
    } else if(strcmp(value, "Done") == 0) {
        transfer_status = transfer_status_done;
    } else {
        SAH_TRACEZ_ERROR(ME, "Unkown Status %s", value);
    }

stop:
    free(value);
    return transfer_status;
}

void transfers_set_status(amxd_object_t* obj, transfer_status_t status) {
    const char* value = NULL;
    amxd_object_t* tc_obj = NULL;
    amxd_trans_t trans;
    amxd_trans_init(&trans);

    when_null_trace(obj, stop, ERROR, "Invalid arg(s)");
    tc_obj = amxd_object_findf(obj, ".TransferComplete.");

    when_null_trace(tc_obj, stop, ERROR, "Failed to get TransferComplete obj");

    amxd_trans_select_object(&trans, tc_obj);

    switch(status) {
    case transfer_status_idle:
        value = "Idle";
        break;

    case transfer_status_in_progress:
        value = "In Progress";
        break;

    case transfer_status_failed:
        value = "Failed";
        break;

    case transfer_status_done:
        value = "Done";
        break;

    default:
        SAH_TRACEZ_ERROR(ME, "Unkown Status %d", status);
        break;
    }

    when_null_trace(value, stop, ERROR, "Invalid Status's value");

    amxd_trans_set_value(cstring_t, &trans, "Status", value);

    when_failed_trace(amxd_trans_apply(&trans, cwmp_plugin_get_dm()),
                      stop, ERROR, "Failed to set TransferComplete's Status");

stop:
    amxd_trans_clean(&trans);
    return;
}

static bool transfer_is_done(amxd_object_t* obj) {
    return (transfer_status_done == transfers_get_status(obj)) ? true : false;
}

void transfers_start_transfer(amxd_object_t* obj) {
    char* fileType = NULL;

    when_null_trace(obj, stop, ERROR, "Invalid arg(s)");

    fileType = amxd_object_get_cstring_t(obj, "FileType", NULL);

    when_str_empty_trace(fileType, stop, ERROR, "Invalid FileType");

    if(strcmp(fileType, FILETYPE_1_FIRMWARE_UPGRADE_IMAGE) == 0) {
        transfers_set_status(obj, transfer_status_in_progress);
        if(transfers_firmware_do_firmwareupgrade(obj) != 0) {
            uint32_t fc = FAULT_CODE_INTERNAL_ERROR;
            const char* fs = "Failed to perform upgrade";
            transfers_update_transfercomplete(obj, NULL, NULL, &fc, fs);
            transfers_set_status(obj, transfer_status_failed);
        }
    } else {
        SAH_TRACEZ_INFO(ME, "Unsupported FileType [%s]", fileType);
    }

stop:
    free(fileType);
    return;
}

void transfers_handle_transfer(amxd_object_t* obj) {
    transfer_info_t* info = NULL;
    amxd_object_t* timewindow_obj = NULL;
    timewindow_mode_t timewindow_mode = timewindow_mode_undefined;
    char* inst_path = NULL;

    when_null_trace(obj, stop, ERROR, "Invalid arg(s)");

    inst_path = amxd_object_get_path(obj, AMXD_OBJECT_INDEXED);

    when_true_trace(transfer_is_done(obj), stop, INFO, "Transfer done [%s]", inst_path);

    info = obj->priv;
    when_null_trace(info, stop, ERROR, "Invalid info");

    when_true_trace((info->lastTimeWindow == 0), stop, INFO, "Transfer will be handled later");

    SAH_TRACEZ_INFO(ME, "Handling %s Transfer", inst_path);
    SAH_TRACEZ_INFO(ME, "Within TimeWindow [%s]", info->withinTimeWindow ? "true" : "false");
    SAH_TRACEZ_INFO(ME, "Last TimeWindow [%d]", info->lastTimeWindow);

    timewindow_obj = amxd_object_findf(obj, ".TimeWindowList.%d.", info->lastTimeWindow);
    timewindow_mode = transfers_timewindow_get_mode(timewindow_obj);

    if((timewindow_mode == timewindow_mode_immediately) || (timewindow_mode == timewindow_mode_at_any_time)) {
        if(info->withinTimeWindow) {
            transfers_start_transfer(obj);
        } else {
            if(info->numberOfTimeWindow == info->lastTimeWindow) {
                uint32_t fc = FAULT_CODE_INTERNAL_ERROR;
                const char* fs = "TimeWindow(s) ended";
                transfers_update_transfercomplete(obj, NULL, NULL, &fc, fs);
                transfers_transfer_done(obj);
            }
        }
    }

stop:
    free(inst_path);
    return;
}

void transfers_transfer_add_info(amxd_object_t* obj) {
    when_null_trace(obj, stop, ERROR, "invalid arg(s)");
    when_not_null_trace(obj->priv, stop, ERROR, "obj has already info");

    transfer_info_t* info = (transfer_info_t*) calloc(1, sizeof(transfer_info_t));
    info->transfer_obj = obj;
    obj->priv = info;
    info->lastTimeWindow = 0;
    info->withinTimeWindow = false;

    info->numberOfTimeWindow = amxd_object_get_instance_count((amxd_object_findf(obj, ".TimeWindowList.")));
    when_true_trace((info->numberOfTimeWindow == 0), stop, ERROR, "Invalid number of TimeWindow(s)");

    for(uint32_t i = 1; i <= info->numberOfTimeWindow; i++) {
        timewindow_status_t timewindow_status = timewindow_status_undefined;
        amxd_object_t* inst = amxd_object_findf(obj, ".TimeWindowList.%d.", i);
        when_null_trace(inst, stop, ERROR, "Failed to get TimeWindow %d", i);
        timewindow_status = transfers_timewindow_get_status(inst);
        switch(timewindow_status) {
        case timewindow_status_started:
            info->lastTimeWindow = i;
            info->withinTimeWindow = true;
            break;

        case timewindow_status_ended:
            info->lastTimeWindow = i;
            info->withinTimeWindow = false;
            break;

        default:
            break;
        }
    }

    transfers_timewindow_add_info(obj);
stop:
    return;
}

void transfers_transfer_del_info(amxd_object_t* obj) {
    transfer_info_t* info = NULL;

    when_null_trace(obj, stop, ERROR, "invalid arg(s)");

    info = (transfer_info_t*) obj->priv;
    when_null_trace(info, stop, ERROR, "invalid arg(s)");

    info->transfer_obj = NULL;
    free(info);

    transfers_timewindow_del_info(obj);
stop:
    return;
}

static void transfers_update_transfercomplete(amxd_object_t* obj,
                                              amxc_ts_t* start_time,
                                              amxc_ts_t* complete_time,
                                              uint32_t* fault_code,
                                              const char* fault_string) {
    amxd_object_t* tc_obj = NULL;
    amxd_trans_t trans;
    amxd_trans_init(&trans);
    when_null_trace(obj, stop, ERROR, "Invalid arg(s)");

    tc_obj = amxd_object_findf(obj, ".TransferComplete.");
    when_null_trace(tc_obj, stop, ERROR, "Failed to get TransferComplete obj");

    amxd_trans_select_object(&trans, tc_obj);
    amxd_trans_set_attr(&trans, amxd_tattr_change_ro, true);
    if(start_time != NULL) {
        amxd_trans_set_value(amxc_ts_t, &trans, "StartTime", start_time);
    }
    if(complete_time != NULL) {
        amxd_trans_set_value(amxc_ts_t, &trans, "CompleteTime", complete_time);
    }
    if(fault_code != NULL) {
        amxd_trans_set_value(uint32_t, &trans, "FaultCode", *fault_code);
    }

    if(fault_string != NULL) {
        amxd_trans_set_value(cstring_t, &trans, "FaultString", fault_string);
    }

    amxd_trans_apply(&trans, cwmp_plugin_get_dm());

stop:
    amxd_trans_clean(&trans);
}

static void handle_pending_upgrade(amxd_object_t* obj) {
    const char* status = NULL;
    const char* bootFailureLog = NULL;
    const char* fs = "";
    uint32_t fc = 0;
    amxc_var_t ret;
    amxc_var_init(&ret);

    when_null_trace(obj, stop, ERROR, "Invalid arg(s)");

    SAH_TRACEZ_INFO(ME, "Handling pending upgrade");

    amxb_get(amxb_be_who_has("DeviceInfo."), "DeviceInfo.FirmwareImage.[active].", 0, &ret, 5);

    status = GETP_CHAR(&ret, "0.0.Status");
    bootFailureLog = GETP_CHAR(&ret, "0.0.BootFailureLog");

    if(status == NULL) {
        SAH_TRACEZ_ERROR(ME, "Failed to get FirmwareImage Status");
        fc = FAULT_CODE_INTERNAL_ERROR;
        fs = "Internal failure";
    } else if(strcmp(status, "Active") != 0) {
        SAH_TRACEZ_WARNING(ME, "FirmwareImage Info: Status [%s] - BootFailureLog [%s]", status, bootFailureLog ? bootFailureLog : "Null");
        fc = FAULT_CODE_INTERNAL_ERROR;
        fs = "Failed to apply firmware";
    }

    transfers_firmwareupgrade_done(obj, fc, fs);

stop:
    amxc_var_clean(&ret);
    return;
}

void transfers_handle_in_progress_transfer(amxd_object_t* obj) {
    char* inst_path = NULL;
    char* status = NULL;
    char* fileType = NULL;
    amxd_object_t* tc_obj = NULL;
    when_null_trace(obj, stop, ERROR, "Invalid arg(s)");

    inst_path = amxd_object_get_path(obj, AMXD_OBJECT_INDEXED);
    when_str_empty_trace(inst_path, stop, ERROR, "Invalid instance path");
    SAH_TRACEZ_INFO(ME, "Handling in progress transfer [%s]", inst_path);

    fileType = amxd_object_get_value(cstring_t, obj, "FileType", NULL);
    when_str_empty_trace(fileType, stop, ERROR, "Invalid FileType");

    tc_obj = amxd_object_findf(obj, ".TransferComplete.");
    when_null_trace(tc_obj, stop, ERROR, "Failed to get TransferComplete object");
    status = amxd_object_get_value(cstring_t, tc_obj, "Status", NULL);

    when_str_empty_trace(status, stop, ERROR, "Invalid Status");

    SAH_TRACEZ_INFO(ME, "Status  [%s] - FileType [%s]", status, fileType);

    if((strcmp(status, "In Progress") == 0) && (strcmp(fileType, FILETYPE_1_FIRMWARE_UPGRADE_IMAGE) == 0)) {
        handle_pending_upgrade(obj);
    }

stop:
    free(status);
    free(fileType);
    free(inst_path);
    return;
}

void transfers_transfer_done(amxd_object_t* obj) {
    char* fileType = NULL;

    when_null_trace(obj, stop, ERROR, "Invalid arg(s)");

    fileType = amxd_object_get_cstring_t(obj, "FileType", NULL);

    when_str_empty_trace(fileType, stop, ERROR, "Invalid FileType");

    if(strcmp(fileType, FILETYPE_1_FIRMWARE_UPGRADE_IMAGE) == 0) {
        transfers_firmware_transfer_done(obj);
    } else {
        SAH_TRACEZ_INFO(ME, "Unsupported FileType [%s]", fileType);
    }

    transfers_set_status(obj, transfer_status_done);

stop:
    free(fileType);
    return;
}

void transfers_save(void) {
    amxd_object_t* obj = amxd_dm_findf(cwmp_plugin_get_dm(), "ManagementServer.Transfers.ScheduleDownload.");
    amxd_object_for_each(instance, it, obj) {
        amxd_object_t* inst = amxc_llist_it_get_data(it, amxd_object_t, it);
        transfers_timewindow_save(inst);
    }
}

static amxp_timer_t* deferred_init_timer = NULL;

static void deferred_init_timer_cb(UNUSED amxp_timer_t* timer, UNUSED void* priv) {
    SAH_TRACEZ_IN(ME);
    amxd_object_t* obj = amxd_dm_findf(cwmp_plugin_get_dm(), "ManagementServer.Transfers.ScheduleDownload.");
    amxd_object_for_each(instance, it, obj) {
        amxd_object_t* inst = amxc_llist_it_get_data(it, amxd_object_t, it);
        transfers_transfer_add_info(inst);
        transfers_handle_in_progress_transfer(inst);
        transfers_timewindow_start(inst);
    }
    SAH_TRACEZ_OUT(ME);
}

void transfers_init(void) {
    SAH_TRACEZ_IN(ME);
    when_failed_trace(amxp_timer_new(&deferred_init_timer, deferred_init_timer_cb, NULL), stop, ERROR, "Failed to create timer");
    when_failed_trace(amxp_timer_start(deferred_init_timer, 0), stop, ERROR, "Failed to start timer");
    transfers_firmware_init();
stop:
    SAH_TRACEZ_OUT(ME);
    return;
}

void transfers_clean(void) {
    SAH_TRACEZ_IN(ME);

    amxp_timer_delete(&deferred_init_timer);

    amxd_object_t* obj = amxd_dm_findf(cwmp_plugin_get_dm(), "ManagementServer.Transfers.ScheduleDownload.");
    amxd_object_for_each(instance, it, obj) {
        amxd_object_t* inst = amxc_llist_it_get_data(it, amxd_object_t, it);
        transfers_transfer_del_info(inst);
    }
    transfers_firmware_clean();
    SAH_TRACEZ_OUT(ME);
}