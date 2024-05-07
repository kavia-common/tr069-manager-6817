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

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "cwmp_plugin.h"
#include <filetransfer/filetransfer.h>
#include <debug/sahtrace.h>
#include <debug/sahtrace_macros.h>
#include <amxd/amxd_dm.h>
#include <amxd/amxd_path.h>
#include <amxd/amxd_object.h>
#include <amxc/amxc.h>
#include <amxc/amxc_macros.h>
#include <stdlib.h>
#include <sys/types.h>

#define TRANSFER_ENTRY_PATH "ManagementServer.ACSTransfers.ACSTransfer."
#define TRANSFER_ENTRY_PATH_FMT TRANSFER_ENTRY_PATH "%d."

#define DEVICE_LOCALAGENT_PATH "Device.LocalAgent."
#define DM_FILTER_TRANSFER_COMPLETE  "notification in ['TransferComplete!']"

#define DL_FW_UPGRADE_SCRIPT "tr069_1_fw_upgrade"
#define DL_WEB_CONTENT_SCRIPT "tr069_2_webcontent"
#define DL_CONF_FILE_SCRIPT "tr069_3_conf_apply"

#define UL_VENDOR_LOGS_SCRIPT "tr069_2_vendor_logs_prepare"
#define UL_VENDOR_CONF_SCRIPT "tr069_1_vendor_conf_prepare"

#define UNKNOWN_TIME "0001-01-01T00:00:00Z"
#define DEVICEINFO_FIRMWAREIMAGE "DeviceInfo.FirmwareImage.active."

static amxc_llist_t selftest_requests;
static amxc_llist_t upload_requests;

typedef struct {
    amxc_llist_it_t it;
    amxc_var_t* args;
    int index;
    amxc_ts_t time;
} upload_context_t;

void download_timer_cb(amxp_timer_t* timer, void* priv);
static int firmwareimage_del_subscription(amxd_object_t* transfer_obj);

static void selftest_context_free(amxc_llist_it_t* it) {
    amxb_request_t* req = amxc_container_of(it, amxb_request_t, it);
    amxc_var_t* args = (amxc_var_t* ) req->priv;
    amxc_var_delete(&args);
    amxb_close_request(&req);
}

static void upload_context_free(amxc_llist_it_t* it) {
    upload_context_t* upload = amxc_container_of(it, upload_context_t, it);
    amxc_var_delete(&upload->args);
    free(upload);
}

static int upload_context_new(upload_context_t** upload, int index, amxc_var_t* args) {
    int ret = -1;
    when_null(upload, stop);
    *upload = (upload_context_t*) calloc(1, sizeof(upload_context_t));
    when_null(*upload, stop);
    (*upload)->index = index;
    amxc_var_new(&(*upload)->args);
    amxc_var_copy((*upload)->args, args);
    amxc_ts_now(&(*upload)->time);
    ret = 0;
stop:
    return ret;
}

static void filetransfer_event_cb(int fd, UNUSED void* priv) {
    ftx_fd_event_handler(fd);
}

static int filetransfer_fdset_cb(int fd, bool add) {
    int retval = -1;
    if(add) {
        retval = amxo_connection_add(cwmp_plugin_get_parser(), fd, filetransfer_event_cb, NULL, AMXO_LISTEN, NULL);
    } else {
        retval = amxo_connection_remove(cwmp_plugin_get_parser(), fd);
    }
    SAH_TRACEZ_INFO(ME, "%s connection fd %d %s", add ? "add" : "remove", fd, (retval == 0) ? "success" : "failure");
    return retval;
}

static int proc_build_cb(amxc_array_t* cmd, amxc_var_t* settings) {
    int ret = -1;
    const char* script = GETP_CHAR(settings, "script");
    const char* fileName = GETP_CHAR(settings, "fileName");

    when_null(script, stop);
    when_null(fileName, stop);

    amxc_array_append_data(cmd, strdup(script));
    amxc_array_append_data(cmd, strdup(fileName));
    ret = 0;
stop:
    return ret;
}

static void start_timer_cb(amxp_timer_t* timer, void* priv) {
    ftx_request_t* filetransfer_request = (ftx_request_t*) priv;
    when_null(filetransfer_request, stop);
    ftx_request_send(filetransfer_request);
stop:
    amxp_timer_delete(&timer);
}

static int start_filetransfer_request(ftx_request_t* request, uint32_t delay) {
    int ret = -1;
    amxp_timer_t* start_timer = NULL;

    amxp_timer_new(&start_timer, start_timer_cb, (void*) request);
    when_null(start_timer, stop);
    amxp_timer_start(start_timer, delay * 1000);
    ret = 0;
stop:
    return ret;
}

static void task_finished_cb(void* priv) {
    when_null(priv, stop);
stop:
    return;
}

static void task_clean_cb(void* priv) {
    when_null(priv, stop);
stop:
    return;
}

static int filetransfer_run_task(const char* fileName, const char* script) {
    amxp_proc_ctrl_t* proc = NULL;
    cwmp_proc_ctx_t* ctx = NULL;
    amxc_var_t settings;
    int ret = -1;

    amxc_var_init(&settings);
    amxp_proc_ctrl_new(&proc, proc_build_cb);
    when_null(proc, stop);
    cwmp_proc_ctx_new(&ctx, proc, task_finished_cb, task_clean_cb, NULL);
    when_null(ctx, stop);

    amxc_var_set_type(&settings, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &settings, "fileName", fileName);
    amxc_var_add_key(cstring_t, &settings, "script", script);

    SAH_TRACEZ_INFO(ME, "Filetransfer starting task [%s %s]", script, fileName);

    amxp_slot_connect(proc->proc->sigmngr, "stop", NULL, proc_finished_cb, (void*) ctx);
    ret = amxp_proc_ctrl_start(proc, 300 * 1000, &settings);
stop:
    if(ret != 0) {
        if(proc) {
            amxp_proc_ctrl_delete(&proc);
        }
    }
    amxc_var_clean(&settings);
    return ret;
}

static void filetransfer_download_finished(const char* path) {
    const char* fileType = NULL;
    const char* targetFileName = NULL;
    const char* script = NULL;
    amxd_object_t* transfer = amxd_dm_findf(cwmp_plugin_get_dm(), "%s", path);

    fileType = amxd_object_get_cstring_t(transfer, "FileType", NULL);
    targetFileName = amxd_object_get_cstring_t(transfer, "SaveFileName", NULL);

    when_null_trace(fileType, stop, ERROR, "filetransfer fileType is null?");
    when_null_trace(targetFileName, stop, ERROR, "targetFileName is null?");

    if(strstr(fileType, "2")) {
        script = DL_WEB_CONTENT_SCRIPT;
    } else if(strstr(fileType, "3")) {
        script = DL_CONF_FILE_SCRIPT;
    } else {
        SAH_TRACEZ_ERROR(ME, "filetransfer unknown file type [%s]", targetFileName);
        goto stop;
    }

    if(filetransfer_run_task(targetFileName, script) != 0) {
        SAH_TRACEZ_ERROR(ME, "Failed to start task [%s %s]", script, targetFileName);
    }
stop:
    return;
}

static void transfer_update_status(const char* path,
                                   const char* status,
                                   amxc_ts_t* start_time,
                                   amxc_ts_t* complete_time,
                                   uint32_t error_code,
                                   const char* error_string) {
    char time_start_str[64] = {0};
    char time_end_str[64] = {0};
    amxc_ts_t error_complete_time = {0, 0, 0};

    amxd_trans_t trans;
    amxd_trans_init(&trans);
    amxd_object_t* transfer = amxd_dm_findf(cwmp_plugin_get_dm(), "%s", path);
    when_null_trace(transfer, stop, ERROR, "transfer not found [%s]", path);

    amxc_ts_format(start_time, time_start_str, sizeof(time_start_str));
    amxc_ts_format(complete_time, time_end_str, sizeof(time_end_str));
    SAH_TRACEZ_INFO(ME, "Transfer [%s] status: %s, fault_code: %d, fault_string: %s, start time: %s, complete time: %s",
                    path, status, error_code, error_string, time_start_str, time_end_str);

    amxd_trans_select_object(&trans, transfer);
    amxd_trans_set_attr(&trans, amxd_tattr_change_ro, true);
    amxd_trans_set_value(cstring_t, &trans, "Status", status);
    amxd_trans_set_value(amxc_ts_t, &trans, "StartTime", start_time);
    amxd_trans_set_value(amxc_ts_t, &trans, "CompleteTime", error_code ? &error_complete_time : complete_time);
    amxd_trans_set_value(uint32_t, &trans, "FaultCode", error_code);
    amxd_trans_set_value(cstring_t, &trans, "FaultString", error_string);
    amxd_trans_apply(&trans, cwmp_plugin_get_dm());

stop:
    amxd_trans_clean(&trans);
}

static bool filetransfer_request_cb(ftx_request_t* req, void* userdata) {
    amxc_ts_t* ts_start;
    amxc_ts_t* ts_end;
    char time_start_str[64] = {0};
    char time_end_str[64] = {0};
    uint32_t error_code;
    ftx_request_type_t request_type;
    amxc_string_t status_path;
    int* index = NULL;

    when_null(req, stop);
    when_null(userdata, stop);

    amxc_string_init(&status_path, 0);
    index = (int*) userdata;
    error_code = (uint32_t) ftx_request_get_error_code(req);
    ts_start = ftx_request_get_start_time(req);
    ts_end = ftx_request_get_end_time(req);
    request_type = ftx_request_get_type(req);
    amxc_ts_format(ts_start, time_start_str, sizeof(time_start_str));
    amxc_ts_format(ts_end, time_end_str, sizeof(time_end_str));

    amxc_string_setf(&status_path, TRANSFER_ENTRY_PATH_FMT, *index);

    if((error_code == 0) && (request_type == ftx_request_type_download)) {
        filetransfer_download_finished(amxc_string_get(&status_path, 0));
    }

    transfer_update_status(amxc_string_get(&status_path, 0), "Finished", ts_start, ts_end, error_code, (const char*) ftx_request_get_error_reason(req));

    ftx_request_delete(&req);
    free(index);
    amxc_string_clean(&status_path);
stop:
    return true;
}

static int filetransfer_set_common_data(ftx_request_t* filetransfer_request, amxc_var_t* args) {
    int ret = -1;
    int* data = (int*) malloc(sizeof(int));
    when_null(data, stop);

    *data = GET_INT32(args, "index");
    ftx_request_set_data(filetransfer_request, (void*) data);
    ftx_request_set_url(filetransfer_request, GETP_CHAR(args, "Url"));
    ftx_request_set_credentials(filetransfer_request, ftx_authentication_any,
                                GETP_CHAR(args, "Username"), GETP_CHAR(args, "Password"));
    ftx_request_set_max_transfer_time(filetransfer_request, 3600);
    ret = 0;
stop:
    return ret;
}

static int vendorlogfile_get_index(const char* fileType) {
    int index = 0;

    amxc_llist_t string_list;
    amxc_llist_init(&string_list);
    amxc_string_t vendorString;
    amxc_string_init(&vendorString, 0);

    when_str_empty(fileType, stop);

    if(strstr(fileType, "2 Vendor Log File") || strstr(fileType, "4 Vendor Log File 1")) {
        index = 1;
    } else if(strstr(fileType, "4 Vendor Log File")) {
        amxc_string_appendf(&vendorString, "%s", fileType);
        amxc_string_split_word(&vendorString, &string_list, NULL);
        amxc_string_t* vendorIndex = amxc_string_get_from_llist(&string_list, 8);
        if(!amxc_string_is_numeric(vendorIndex)) {
            SAH_TRACEZ_ERROR(ME, "Failed to retrieve index from fileType [%s]", fileType);
            goto stop;
        }
        index = atoi(amxc_string_get(vendorIndex, 0));
    }
stop:
    amxc_llist_clean(&string_list, amxc_string_list_it_free);
    amxc_string_clean(&vendorString);
    return index;
}

static upload_context_t* filetransfer_upload_find(const char* transferurl) {
    upload_context_t* upload = NULL;
    amxc_llist_for_each(it, &upload_requests) {
        upload = amxc_container_of(it, upload_context_t, it);
        const char* url = GET_CHAR(upload->args, "URL");
        if(url && (0 == strcmp(url, transferurl))) {
            return upload;
        }
    }
    return NULL;
}

static void filetransfer_upload_complete_notification(UNUSED const char* const sig_name,
                                                      const amxc_var_t* const data,
                                                      UNUSED void* const priv) {
    const amxc_var_t* adata = NULL;
    const char* transferurl = NULL;
    upload_context_t* upload = NULL;
    const char* fault_string = NULL;
    uint32_t error_code = 0;
    amxc_string_t status_path;
    amxc_ts_t end_time;

    when_null(data, stop);
    amxc_string_init(&status_path, 0);
    when_true(amxc_llist_is_empty(&upload_requests), stop);

    adata = GET_ARG(data, "data");
    when_null(adata, stop);
    transferurl = GET_CHAR(adata, "TransferURL");
    when_str_empty(transferurl, stop);

    upload = filetransfer_upload_find(transferurl);
    when_null(upload, stop);

    amxc_ts_now(&end_time);
    error_code = GET_UINT32(adata, "FaultCode");
    fault_string = GET_CHAR(adata, "FaultString");

    amxc_string_setf(&status_path, TRANSFER_ENTRY_PATH_FMT, upload->index);
    transfer_update_status(amxc_string_get(&status_path, 0), "Finished", &upload->time, &end_time, error_code, fault_string);

    amxc_llist_it_take(&upload->it);
    upload_context_free(&upload->it);
stop:
    amxc_string_clean(&status_path);
    return;
}

static void filetransfer_upload_selftest_call_done(const amxb_bus_ctx_t* bus_ctx,
                                                   amxb_request_t* req,
                                                   int status,
                                                   void* priv) {
    amxc_var_t* args = (amxc_var_t*) priv;
    uint32_t transfer_index = GET_UINT32(args, "index");
    when_true_trace(transfer_index == 0, exit, ERROR, "Transfer index missing");

    int retval = -1;
    int vendorlogfile_index = 0;
    upload_context_t* upload = NULL;
    amxc_ts_t time = {0, 0, 0};
    amxc_var_t upload_args;
    amxc_string_t vendorlogfile_path;
    amxc_string_t status_path;

    amxc_var_init(&upload_args);
    amxc_string_init(&vendorlogfile_path, 0);
    amxc_string_init(&status_path, 0);

    when_true_trace(status != 0, stop, ERROR, "SelfTestDiagnostic failed");
    const char* filetype = GET_CHAR(args, "FileType");
    when_str_empty_trace(filetype, stop, ERROR, "Mandatory [FileType] is missing");
    vendorlogfile_index = vendorlogfile_get_index(filetype);
    when_true_trace(vendorlogfile_index == 0, stop, ERROR, "Failed to get index from [%s]", filetype);
    const char* url = GET_CHAR(args, "Url");
    when_str_empty_trace(url, stop, ERROR, "Mandatory [Url] is missing");
    const char* username = GET_CHAR(args, "Username");
    when_str_empty_trace(username, stop, ERROR, "Mandatory [Username] is missing");
    const char* password = GET_CHAR(args, "Password");
    when_str_empty_trace(password, stop, ERROR, "Mandatory [Password] is missing");

    amxc_var_set_type(&upload_args, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &upload_args, "URL", url);
    amxc_var_add_key(cstring_t, &upload_args, "Username", username);
    amxc_var_add_key(cstring_t, &upload_args, "Password", password);

    amxc_string_appendf(&vendorlogfile_path, "Device.DeviceInfo.VendorLogFile.%d", vendorlogfile_index);
    retval = amxb_call((amxb_bus_ctx_t* const) bus_ctx, amxc_string_get(&vendorlogfile_path, 0), "Upload", &upload_args, NULL, 5);
    when_failed_trace(retval, stop, ERROR, "Failed to Call %s.Upload()", amxc_string_get(&vendorlogfile_path, 0));

    upload_context_new(&upload, transfer_index, &upload_args);
    amxc_llist_append(&upload_requests, &upload->it);

stop:
    if(retval != 0) {
        amxc_ts_now(&time);
        amxc_string_setf(&status_path, TRANSFER_ENTRY_PATH_FMT, transfer_index);
        transfer_update_status(amxc_string_get(&status_path, 0), "Finished", &time, &time, FAULTCODE_INTERNAL_ERROR, "Internal error");
    }
    amxc_llist_it_take(&req->it);
    amxc_var_delete(&args);
    amxb_close_request(&req);
    amxc_string_clean(&status_path);
    amxc_string_clean(&vendorlogfile_path);
    amxc_var_clean(&upload_args);
exit:
    return;
}

static void filetransfer_upload_prepare(amxc_var_t* args) {
    amxb_bus_ctx_t* const bus_ctx = get_bus_ctx("Device.");
    when_null_trace(bus_ctx, stop, ERROR, "Could not find the context of [Device]");
    const char* filetype = GET_CHAR(args, "FileType");
    when_str_empty(filetype, stop);

    if(strstr(filetype, "2 Vendor Log File") || strstr(filetype, "4 Vendor Log File 1")) {
        amxb_request_t* req = amxb_async_call(bus_ctx, "Device.", "SelfTestDiagnostics", NULL, filetransfer_upload_selftest_call_done, args);
        when_null_trace(req, stop, ERROR, "file upload failed : Failed to call Device.SelfTestDiagnostics()");
        amxc_llist_append(&selftest_requests, &req->it);
    } else if(strstr(filetype, "4 Vendor Log File")) {
        filetransfer_upload_selftest_call_done(bus_ctx, NULL, 0, args);
    } else {
        SAH_TRACEZ_ERROR(ME, "Not supported file type [%s]", filetype);
        amxc_var_delete(&args);
        goto stop;
    }
stop:
    return;
}

static void filetransfer_download_set_target_file(ftx_request_t* filetransfer_request, amxc_var_t* args) {
    const char* targetFileName = GETP_CHAR(args, "TargetFileName");
    const char* cmdKey = GETP_CHAR(args, "CommandKey");
    amxd_object_t* transfer = amxd_dm_findf(cwmp_plugin_get_dm(), TRANSFER_ENTRY_PATH "[CommandKey == '%s'].", cmdKey);
    amxc_string_t file_name;
    amxc_string_init(&file_name, 0);

    when_null_trace(transfer, stop, ERROR, "failed to find transfer with key [%s]", cmdKey);

    if(targetFileName && *targetFileName) {
        amxc_string_setf(&file_name, "/tmp/%s", targetFileName);
    } else {
        amxc_string_setf(&file_name, "/tmp/dl_%s", cmdKey);
    }

    ftx_request_set_target_file(filetransfer_request, amxc_string_get(&file_name, 0));
    amxd_object_set_cstring_t(transfer, "SaveFileName", amxc_string_get(&file_name, 0));
stop:
    amxc_string_clean(&file_name);
}

static void update_transfer_obj(amxd_object_t* transfer_obj,
                                const char* status,
                                amxc_ts_t* start_time,
                                amxc_ts_t* complete_time,
                                uint32_t* fault_code) {
    amxd_trans_t trans;
    amxd_trans_init(&trans);
    when_null_trace(transfer_obj, stop, ERROR, "Invalid arg(s)");

    amxd_trans_select_object(&trans, transfer_obj);
    amxd_trans_set_attr(&trans, amxd_tattr_change_ro, true);
    if(status != NULL) {
        amxd_trans_set_value(cstring_t, &trans, "Status", status);
    }
    if(start_time != NULL) {
        amxd_trans_set_value(amxc_ts_t, &trans, "StartTime", start_time);
    }
    if(complete_time != NULL) {
        amxd_trans_set_value(amxc_ts_t, &trans, "CompleteTime", complete_time);
    }
    if(fault_code != NULL) {
        amxd_trans_set_value(uint32_t, &trans, "FaultCode", *fault_code);
    }
    amxd_trans_apply(&trans, cwmp_plugin_get_dm());

stop:
    amxd_trans_clean(&trans);
}

static amxd_object_t* get_transfer_obj_by_index(uint32_t index) {
    amxd_object_t* transfer = NULL;
    amxc_string_t transfer_path;
    amxc_string_init(&transfer_path, 0);
    amxc_string_setf(&transfer_path, TRANSFER_ENTRY_PATH_FMT, index);
    transfer = amxd_dm_findf(cwmp_plugin_get_dm(), "%s", amxc_string_get(&transfer_path, 0));

    amxc_string_clean(&transfer_path);
    if(transfer == NULL) {
        SAH_TRACEZ_ERROR(ME, "Failed to get transfer object with index %d", index);
    }
    return transfer;
}

static void firmwareimage_notification(UNUSED const char* const sig_name,
                                       const amxc_var_t* const data,
                                       void* const priv) {


    amxd_object_t* transfer_obj = NULL;
    const char* status = NULL;
    const char* bootFailureLog = NULL;
    uint32_t fault_code = 0;

    when_null_trace(priv, stop, ERROR, "Invalid arg(s)");

    transfer_obj = (amxd_object_t*) priv;

    status = GETP_CHAR(data, "parameters.Status.to");
    bootFailureLog = GETP_CHAR(data, "parameters.BootFailureLog.to");

    when_null_trace(status, stop, ERROR, "Failed to get the firmware upgrade status");

    if(bootFailureLog == NULL) {
        bootFailureLog = "";
    }

    SAH_TRACEZ_INFO(ME, "Firmware upgrade status: %s, bootFailureLog %s", status, bootFailureLog);

    if((strcmp(status, "DownloadFailed") == 0) ||
       (strcmp(status, "ValidationFailed") == 0) ||
       (strcmp(status, "InstallationFailed") == 0) ||
       (strcmp(status, "ActivationFailed") == 0)) {
        amxc_ts_t tsp;
        fault_code = 9010;
        if(strcmp(bootFailureLog, "Protocol not supported") == 0) {
            fault_code = 9013;
        }
        amxc_ts_parse(&tsp, UNKNOWN_TIME, strlen(UNKNOWN_TIME));
        update_transfer_obj(transfer_obj, "Finished", NULL, &tsp, &fault_code);
        firmwareimage_del_subscription(transfer_obj);
    } else if(strcmp(status, "Downloading") == 0) {
        update_transfer_obj(transfer_obj, "Transferring", NULL, NULL, NULL);
    } else if(strcmp(status, "Validating") == 0) {
        update_transfer_obj(transfer_obj, "Applying", NULL, NULL, NULL);
        /* expecting a reboot now or an error */
    }
stop:
    return;
}

static int firmwareimage_add_subscription(amxd_object_t* transfer_obj) {
    int retval = -1;

    if(transfer_obj != NULL) {
        retval = amxb_subscribe(amxb_be_who_has("DeviceInfo."), DEVICEINFO_FIRMWAREIMAGE, "(notification == 'dm:object-changed') && \
                                (contains('parameters.Status'))", firmwareimage_notification, transfer_obj);
    }

    if(retval != 0) {
        SAH_TRACEZ_ERROR(ME, "Failed to add subscription to [%s]", DEVICEINFO_FIRMWAREIMAGE);
    }
    return retval;
}

static int firmwareimage_del_subscription(amxd_object_t* transfer_obj) {
    int retval = -1;

    if(transfer_obj != NULL) {
        retval = amxb_unsubscribe(amxb_be_who_has("DeviceInfo."), DEVICEINFO_FIRMWAREIMAGE, firmwareimage_notification, transfer_obj);
    }

    if(retval != 0) {
        SAH_TRACEZ_ERROR(ME, "Failed to delete subscription from [%s]", DEVICEINFO_FIRMWAREIMAGE);
    }
    return retval;
}

static int firmwareimage_download(amxd_object_t* transfer_obj) {
    int retval = -1;
    const char* url = NULL;
    const char* username = NULL;
    const char* password = NULL;
    amxc_var_t args;
    amxc_var_init(&args);

    when_null(transfer_obj, stop);

    url = amxd_object_get_cstring_t(transfer_obj, "Url", NULL);
    when_null_trace(url, stop, ERROR, "Failed to get URL");
    username = amxd_object_get_cstring_t(transfer_obj, "Username", NULL);
    when_null_trace(username, stop, ERROR, "Failed to get Username");
    password = amxd_object_get_cstring_t(transfer_obj, "Password", NULL);
    when_null_trace(password, stop, ERROR, "Failed to get Password");

    amxc_var_set_type(&args, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &args, "URL", url);
    amxc_var_add_key(bool, &args, "AutoActivate", true);
    amxc_var_add_key(cstring_t, &args, "Username", username);
    amxc_var_add_key(cstring_t, &args, "Password", password);

    retval = amxb_call(amxb_be_who_has("DeviceInfo."), DEVICEINFO_FIRMWAREIMAGE, "Download", &args, NULL, 5);

    when_failed_trace(retval, stop, ERROR, "Failed to Call %sDownload()", DEVICEINFO_FIRMWAREIMAGE);

stop:
    amxc_var_clean(&args);
    return retval;
}

static int firmware_upgrade(amxd_object_t* transfer_obj) {
    int retval = -1;
    when_null_trace(transfer_obj, stop, ERROR, "Invalid arg(s)");
    when_failed(firmwareimage_add_subscription(transfer_obj), stop);
    when_failed(firmwareimage_download(transfer_obj), stop);
    retval = 0;
stop:
    if(retval != 0) {
        SAH_TRACEZ_ERROR(ME, "Failed to perform firmware upgrade");
    }
    return retval;
}

void download_timer_cb(amxp_timer_t* timer, void* priv) {
    when_null_trace(priv, stop, ERROR, "Invalid arg(s)")
    amxd_object_t* transfer_obj = (amxd_object_t*) priv;
    if(firmware_upgrade(transfer_obj) != 0) {
        uint32_t fault_code = 9002;
        update_transfer_obj(transfer_obj, "Finished", NULL, NULL, &fault_code);
        firmwareimage_del_subscription(transfer_obj);
    }
stop:
    amxp_timer_delete(&timer);
}

static void filetransfer_upload_cb(amxp_timer_t* timer, void* priv) {
    amxc_var_t* args = (amxc_var_t* ) priv;
    filetransfer_upload_prepare(args);
    amxp_timer_delete(&timer);
}

static void filetransfer_upload_start(amxc_var_t* args) {
    bool error = true;
    amxp_timer_t* upload_delay_timer = NULL;
    amxc_var_t* data = NULL;
    uint32_t delaySeconds = 0;
    when_null(args, stop);

    delaySeconds = GET_INT32(args, "DelaySeconds");
    amxc_var_new(&data);
    amxc_var_copy(data, args);
    amxp_timer_new(&upload_delay_timer, filetransfer_upload_cb, (void*) data);
    when_null(upload_delay_timer, stop);
    amxp_timer_start(upload_delay_timer, delaySeconds * 1000);
    error = false;
stop:
    if(error) {
        amxc_var_delete(&data);
    }
}

static int filetransfer_request_prepare(amxc_var_t* args) {
    int ret = -1;
    bool is_download = GET_BOOL(args, "IsDownload");
    uint32_t delaySeconds = GET_INT32(args, "DelaySeconds");
    const char* fileType = GET_CHAR(args, "FileType");

    if(is_download) {
        if(fileType && (strcmp(fileType, "1 Firmware Upgrade Image") == 0)) {
            amxp_timer_t* download_timer = NULL;
            uint32_t transfer_index = GET_UINT32(args, "index");
            amxd_object_t* transfer_obj = get_transfer_obj_by_index(transfer_index);
            when_null(transfer_obj, stop);
            when_failed_trace(amxp_timer_new(&download_timer, download_timer_cb, transfer_obj),
                              stop, ERROR, "Failed to create timer");
            when_failed_trace(amxp_timer_start(download_timer, delaySeconds * 1000),
                              stop, ERROR, "Failed to start timer");
            SAH_TRACEZ_INFO(ME, "Transfer [%d]: Initiate the Download [%s] within [%d] seconds", transfer_index, fileType, delaySeconds);
            ret = 0;
            goto stop;
        }
        ftx_request_t* filetransfer_request = NULL;
        ftx_request_new(&filetransfer_request, ftx_request_type_download, filetransfer_request_cb);

        when_null_trace(filetransfer_request, stop, ERROR, "NULL filetransfer request, abort!");
        when_failed(filetransfer_set_common_data(filetransfer_request, args), stop);

        filetransfer_download_set_target_file(filetransfer_request, args);
        start_filetransfer_request(filetransfer_request, delaySeconds);
    } else {
        filetransfer_upload_start(args);
    }
    ret = 0;
stop:
    return ret;
}

amxd_status_t _AddTransfer(UNUSED amxd_object_t* object,
                           UNUSED amxd_function_t* func,
                           amxc_var_t* args,
                           amxc_var_t* ret) {
    bool rv = false;
    when_null(args, stop);
    when_null(ret, stop);
    when_failed(filetransfer_request_prepare(args), stop);
    rv = true;

stop:
    amxc_var_set(bool, ret, rv);
    return amxd_status_ok;
}

void cwmp_plugin_transfer_init(void) {
    ftx_init(filetransfer_fdset_cb);
    int retval = cwmp_plugin_add_subscription(DEVICE_LOCALAGENT_PATH, DM_FILTER_TRANSFER_COMPLETE, filetransfer_upload_complete_notification);
    when_failed_trace(retval, stop, ERROR, "Could not create LocalAgent subscription");
stop:
    return;
}

void cwmp_plugin_transfer_clean(void) {
    amxc_llist_clean(&selftest_requests, selftest_context_free);
    amxc_llist_clean(&upload_requests, upload_context_free);
    cwmp_plugin_del_subscription(DEVICE_LOCALAGENT_PATH, filetransfer_upload_complete_notification);
}
