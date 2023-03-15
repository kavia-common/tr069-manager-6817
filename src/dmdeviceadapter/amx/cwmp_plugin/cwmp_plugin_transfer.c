/****************************************************************************
**
** Copyright (c) 2021 SoftAtHome
**
** Redistribution and use in source and binary forms, with or
** without modification, are permitted provided that the following
** conditions are met:
**
** 1. Redistributions of source code must retain the above copyright
** notice, this list of conditions and the following disclaimer.
**
** 2. Redistributions in binary form must reproduce the above
** copyright notice, this list of conditions and the following
** disclaimer in the documentation and/or other materials provided
** with the distribution.
**
** Subject to the terms and conditions of this license, each
** copyright holder and contributor hereby grants to those receiving
** rights under this license a perpetual, worldwide, non-exclusive,
** no-charge, royalty-free, irrevocable (except for failure to
** satisfy the conditions of this license) patent license to make,
** have made, use, offer to sell, sell, import, and otherwise
** transfer this software, where such license applies only to those
** patent claims, already acquired or hereafter acquired, licensable
** by such copyright holder or contributor that are necessarily
** infringed by:
**
** (a) their Contribution(s) (the licensed copyrights of copyright
** holders and non-copyrightable additions of contributors, in
** source or binary form) alone; or
**
** (b) combination of their Contribution(s) with the work of
** authorship to which such Contribution(s) was added by such
** copyright holder or contributor, if, at the time the Contribution
** is added, such addition causes such combination to be necessarily
** infringed. The patent license shall not apply to any other
** combinations which include the Contribution.
**
** Except as expressly stated above, no rights or licenses from any
** copyright holder or contributor is granted under this license,
** whether expressly, by implication, estoppel or otherwise.
**
** DISCLAIMER
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
** CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
** INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
** MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
** DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
** CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
** USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
** AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
** ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
** POSSIBILITY OF SUCH DAMAGE.
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

#define DL_FW_UPGRADE_SCRIPT "tr069_1_fw_upgrade"
#define DL_WEB_CONTENT_SCRIPT "tr069_2_webcontent"
#define DL_CONF_FILE_SCRIPT "tr069_3_conf_apply"

#define UL_VENDOR_LOGS_SCRIPT "tr069_2_vendor_logs_prepare"
#define UL_VENDOR_CONF_SCRIPT "tr069_1_vendor_conf_prepare"

typedef struct {
    ftx_request_t* request;
    int32_t delay;
} filetransfer_context_t;

static int filetransfer_context_new(ftx_request_t* req,
                                    int delay,
                                    filetransfer_context_t** ctx) {
    int ret = -1;
    when_null(ctx, stop);
    *ctx = (filetransfer_context_t*) calloc(1, sizeof(filetransfer_context_t));
    when_null(*ctx, stop);
    (*ctx)->request = req;
    (*ctx)->delay = delay;
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
    filetransfer_context_t* ft_ctx = NULL;
    when_null(priv, stop);
    ft_ctx = (filetransfer_context_t*) priv;

stop:
    if(ft_ctx && ft_ctx->request) {
        SAH_TRACEZ_INFO(ME, "Starting file upload");
        ftx_request_t* request = ft_ctx->request;
        ft_ctx->request = NULL;
        start_filetransfer_request(request, ft_ctx->delay);
    }
}

static void task_clean_cb(void* priv) {
    if(priv) {
        free(priv);
        priv = NULL;
    }
}

static int filetransfer_run_task(const char* fileName, const char* script, filetransfer_context_t* priv) {
    amxp_proc_ctrl_t* proc = NULL;
    cwmp_proc_ctx_t* ctx = NULL;
    amxc_var_t settings;
    int ret = -1;

    amxc_var_init(&settings);
    amxp_proc_ctrl_new(&proc, proc_build_cb);
    when_null(proc, stop);
    cwmp_proc_ctx_new(&ctx, proc, task_finished_cb, task_clean_cb, (void*) priv);
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

    if(strstr(fileType, "1")) {
        script = DL_FW_UPGRADE_SCRIPT;
    } else if(strstr(fileType, "2")) {
        script = DL_WEB_CONTENT_SCRIPT;
    } else if(strstr(fileType, "3")) {
        script = DL_CONF_FILE_SCRIPT;
    } else {
        SAH_TRACEZ_ERROR(ME, "filetransfer unknown file type [%s]", targetFileName);
        goto stop;
    }

    if(filetransfer_run_task(targetFileName, script, NULL) != 0) {
        SAH_TRACEZ_ERROR(ME, "Failed to start task [%s %s]", script, targetFileName);
    }
stop:
    return;
}

static void transfer_update_status(const char* path,
                                   amxc_ts_t* start_time,
                                   amxc_ts_t* complete_time,
                                   uint32_t error_code) {
    amxd_trans_t trans;
    amxd_trans_init(&trans);
    amxd_object_t* transfer = amxd_dm_findf(cwmp_plugin_get_dm(), "%s", path);
    when_null_trace(transfer, stop, ERROR, "transfer not found [%s]", path);

    amxd_trans_select_object(&trans, transfer);
    amxd_trans_set_attr(&trans, amxd_tattr_change_ro, true);
    amxd_trans_set_value(cstring_t, &trans, "Status", "Finished");
    amxd_trans_set_value(amxc_ts_t, &trans, "StartTime", start_time);
    amxd_trans_set_value(amxc_ts_t, &trans, "CompleteTime", complete_time);
    amxd_trans_set_value(uint32_t, &trans, "FaultCode", error_code);
    amxd_trans_apply(&trans, cwmp_plugin_get_dm());

stop:
    amxd_trans_clean(&trans);
}

static bool filetransfer_request_cb(ftx_request_t* req, void* userdata) {
    amxc_ts_t* ts_start;
    amxc_ts_t* ts_end;
    char time_start_str[64] = {0};
    char time_end_str[64] = {0};
    ftx_error_code_t error_code;
    ftx_request_type_t request_type;
    amxc_string_t status_path;
    int* index = NULL;

    when_null(req, stop);
    when_null(userdata, stop);

    amxc_string_init(&status_path, 0);
    index = (int*) userdata;
    error_code = ftx_request_get_error_code(req);
    ts_start = ftx_request_get_start_time(req);
    ts_end = ftx_request_get_end_time(req);
    request_type = ftx_request_get_type(req);
    amxc_ts_format(ts_start, time_start_str, sizeof(time_start_str));
    amxc_ts_format(ts_end, time_end_str, sizeof(time_end_str));

    SAH_TRACEZ_INFO(ME, "Transfer [%d] status: %s, return code: %u, reason: %s, start time: %s, complete time: %s",
                    *index, (error_code == ftx_error_code_no_error) ? "Success" : "Failure",
                    error_code, ftx_request_get_error_reason(req), time_start_str, time_end_str);

    amxc_string_setf(&status_path, TRANSFER_ENTRY_PATH_FMT, *index);
    transfer_update_status(amxc_string_get(&status_path, 0), ts_start, ts_end, (uint32_t) error_code);

    if((error_code == ftx_error_code_no_error) && (request_type == ftx_request_type_download)) {
        filetransfer_download_finished(amxc_string_get(&status_path, 0));
    }

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

static void find_device_serial(char** device_serial) {
    const char* serial = NULL;
    amxc_var_t get;
    amxb_bus_ctx_t* bus_ctx = NULL;
    amxc_var_init(&get);
    when_null(device_serial, stop);

    bus_ctx = amxb_be_who_has("DeviceInfo.");
    when_null(bus_ctx, stop);
    amxb_get(bus_ctx, "DeviceInfo.SerialNumber", 0, &get, 1);

    if(!amxc_var_is_null(&get)) {
        serial = GETP_CHAR(&get, "0.'DeviceInfo.'.SerialNumber");
    }
    if(serial) {
        *device_serial = strdup(serial);
    } else {
        *device_serial = strdup("");
    }
stop:
    amxc_var_clean(&get);
}

static int filetransfer_upload_prepare_file(ftx_request_t* filetransfer_request,
                                            const char* file_name,
                                            const char* script,
                                            int32_t delay) {
    int ret = -1;
    filetransfer_context_t* context = NULL;
    when_null(file_name, stop);
    when_null(script, stop);

    filetransfer_context_new(filetransfer_request, delay, &context);
    when_null(context, stop);

    SAH_TRACEZ_INFO(ME, "Preparing file for upload [%s]", file_name);

    ftx_request_set_target_file(filetransfer_request, file_name);
    ret = filetransfer_run_task(file_name, script, context);

stop:
    if((ret != 0) && context) {
        SAH_TRACEZ_ERROR(ME, "Failed to start task [%s %s]", script, file_name);
        free(context);
    }
    return ret;
}

static void filetransfer_prepare_upload(ftx_request_t* filetransfer_request, amxc_var_t* args) {
    const char* fileType = GETP_CHAR(args, "FileType");
    amxc_string_t file_name;
    char* device_serial = NULL;
    int error = -1;
    const char* script = NULL;
    const char* targetFile = NULL;
    int delay = 0;

    amxc_string_init(&file_name, 0);
    delay = GET_INT32(args, "DelaySeconds");
    when_null(filetransfer_request, stop);
    when_null(fileType, stop);

    find_device_serial(&device_serial);
    when_null_trace(device_serial, stop, ERROR, "Failed to read device serial");

    if(strstr(fileType, "2")) {
        amxc_string_setf(&file_name, "/tmp/%s_vendor_logs.log", device_serial);
        script = UL_VENDOR_LOGS_SCRIPT;
    } else if(strstr(fileType, "1")) {
        amxc_string_setf(&file_name, "/tmp/%s_vendor_conf.odl", device_serial);
        script = UL_VENDOR_CONF_SCRIPT;
    } else {
        SAH_TRACEZ_ERROR(ME, "Not supported file type [%s]", fileType);
        goto stop;
    }

    error = filetransfer_upload_prepare_file(filetransfer_request,
                                             amxc_string_get(&file_name, 0),
                                             script,
                                             delay);

stop:
    if(error != 0) {
        SAH_TRACEZ_ERROR(ME, "file upload failed, file[%s]", targetFile ? targetFile : "NULL");
        ftx_request_delete(&filetransfer_request);
        filetransfer_request = NULL;
    }
    free(device_serial);
    amxc_string_clean(&file_name);
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

static int filetransfer_request_prepare(amxc_var_t* args) {
    int ret = -1;
    bool is_download = false;
    int delay = 0;
    ftx_request_t* filetransfer_request = NULL;
    is_download = GET_BOOL(args, "IsDownload");
    delay = GET_INT32(args, "DelaySeconds");

    ftx_request_new(&filetransfer_request,
                    is_download ? ftx_request_type_download : ftx_request_type_upload,
                    filetransfer_request_cb);

    when_null_trace(filetransfer_request, stop, ERROR, "NULL filetransfer request, abort!");
    when_failed(filetransfer_set_common_data(filetransfer_request, args), stop);

    if(is_download) {
        filetransfer_download_set_target_file(filetransfer_request, args);
        start_filetransfer_request(filetransfer_request, delay);
    } else {
        filetransfer_prepare_upload(filetransfer_request, args);
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
}
