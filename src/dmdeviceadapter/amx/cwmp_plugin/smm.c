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
#include "smm.h"

#define SOFTWAREMODULES "CWMP_SoftwareModules."

typedef enum _op_type_t
{
    op_type_none,
    op_type_install,
    op_type_update,
    op_type_uninstall,
} op_type_t;

typedef enum _op_status_t
{
    op_status_none,
    op_status_in_progress,
    op_status_done
} op_status_t;

typedef struct _op_info_t {
    amxd_object_t* op_obj;
    amxp_timer_t* expiration_timer;
} op_info_t;

static int op_add_result(amxd_object_t* obj, amxc_var_t* result);
static op_status_t op_status_get(amxd_object_t* obj);
static void op_status_set(amxd_object_t* obj, op_status_t status);
static void op_failed(amxd_object_t* obj);

static void op_expiration_timer_cb(amxp_timer_t* timer, void* priv) {
    (void) timer;
    char* obj_path = NULL;
    op_status_t status = op_status_none;

    op_info_t* info = (op_info_t*) priv;
    when_null(info, stop);
    when_null(info->op_obj, stop);

    status = op_status_get(info->op_obj);

    if(status == op_status_in_progress) {
        obj_path = amxd_object_get_path(info->op_obj, AMXD_OBJECT_INDEXED);
        when_str_empty(obj_path, stop);
        SAH_TRACEZ_INFO(ME, "[%s] expired", obj_path);
        op_failed(info->op_obj);
        op_status_set(info->op_obj, op_status_done);
    }

stop:
    free(obj_path);
    return;
}

static void op_add_info(amxd_object_t* obj) {
    op_info_t* info = NULL;
    op_status_t status = op_status_none;

    when_null(obj, stop);
    when_not_null((obj->priv), stop);

    info = (op_info_t*) calloc(1, sizeof(op_info_t));
    obj->priv = info;
    info->op_obj = obj;
    info->expiration_timer = NULL;

    status = op_status_get(obj);

    if(status == op_status_none) {
        amxp_timer_new(&info->expiration_timer, op_expiration_timer_cb, info);
    }

stop:
    return;
}

static void op_del_info(amxd_object_t* obj) {
    op_info_t* info = NULL;
    when_null(obj, stop);
    when_null((obj->priv), stop);

    info = (op_info_t*) obj->priv;
    info->op_obj = NULL;
    amxp_timer_delete(&(info->expiration_timer));
    free(info);
    obj->priv = NULL;
stop:
    return;
}

static void dscc_status_done(amxd_object_t* obj) {
    amxd_trans_t trans;
    amxd_trans_init(&trans);

    when_null(obj, stop);
    amxd_trans_select_object(&trans, obj);
    amxd_trans_set_value(cstring_t, &trans, "Status", "Done");
    when_failed_trace(amxd_trans_apply(&trans, cwmp_plugin_get_dm()),
                      stop, ERROR, "Failed to set Status");
stop:
    amxd_trans_clean(&trans);
    return;
}

static op_type_t op_type_get(amxd_object_t* obj) {
    char* value = NULL;
    op_type_t type = op_type_none;
    when_null(obj, stop);

    value = amxd_object_get_value(cstring_t, obj, "Type", NULL);

    when_str_empty(value, stop);

    if(strcmp(value, "Install") == 0) {
        type = op_type_install;
    } else if(strcmp(value, "Update") == 0) {
        type = op_type_update;
    } else if(strcmp(value, "Uninstall") == 0) {
        type = op_type_uninstall;
    } else {
        SAH_TRACEZ_ERROR(ME, "Unkown Type %s", value);
    }

stop:
    free(value);
    return type;
}

static op_status_t op_status_get(amxd_object_t* obj) {
    char* value = NULL;
    op_status_t status = op_status_none;
    when_null(obj, stop);

    value = amxd_object_get_value(cstring_t, obj, "Status", NULL);

    when_str_empty(value, stop);

    if(strcmp(value, "In Progress") == 0) {
        status = op_status_in_progress;
    } else if(strcmp(value, "Done") == 0) {
        status = op_status_done;
    } else if(strcmp(value, "") == 0) {
        status = op_status_none;
    } else {
        SAH_TRACEZ_ERROR(ME, "Unkown Status %s", value);
    }

stop:
    free(value);
    return status;
}

static void op_status_set(amxd_object_t* obj, op_status_t status) {
    const char* value = NULL;
    amxd_trans_t trans;
    amxd_trans_init(&trans);

    when_null(obj, stop);
    amxd_trans_select_object(&trans, obj);
    switch(status) {
    case op_status_in_progress:
        value = "In Progress";
        break;

    case op_status_done:
        value = "Done";
        break;

    case op_status_none:
        value = "";
        break;

    default:
        SAH_TRACEZ_ERROR(ME, "Unkown Status %d", status);
        break;
    }
    when_null_trace(value, stop, ERROR, "Invalid value");
    amxd_trans_set_value(cstring_t, &trans, "Status", value);
    when_failed_trace(amxd_trans_apply(&trans, cwmp_plugin_get_dm()),
                      stop, ERROR, "Failed to set Status");
stop:
    amxd_trans_clean(&trans);
    return;
}

static void op_id_set(amxd_object_t* obj, uint32_t value) {
    amxd_trans_t trans;
    amxd_trans_init(&trans);

    when_null(obj, stop);

    amxd_trans_select_object(&trans, obj);

    amxd_trans_set_value(uint32_t, &trans, "ID", value);

    when_failed_trace(amxd_trans_apply(&trans, cwmp_plugin_get_dm()),
                      stop, ERROR, "Failed to set ID");
stop:
    amxd_trans_clean(&trans);
    return;
}


static amxd_object_t* op_obj_get_by_id(uint32_t id) {
    amxd_object_t* obj = NULL;
    const char* obj_path = NULL;
    amxc_llist_t paths;
    amxc_llist_init(&paths);
    when_true((id == 0), stop);
    when_failed(amxd_dm_resolve_pathf(cwmp_plugin_get_dm(), &paths, "ManagementServer.SMM.DUStateChangeComplete.*.Operations.[ID==%u].", id), stop);
    obj_path = amxc_string_get(amxc_string_from_llist_it(amxc_llist_get_first(&paths)), 0);
    when_str_empty(obj_path, stop);
    obj = amxd_dm_findf(cwmp_plugin_get_dm(), obj_path, id);
stop:
    if(obj == NULL) {
        SAH_TRACEZ_ERROR(ME, "No operation found for id [%u]", id);
    }
    amxc_llist_clean(&paths, amxc_string_list_it_free);
    return obj;
}

static int op_run(amxd_object_t* obj) {
    int retval = -1;
    char* url = NULL;
    char* uuid = NULL;
    char* username = NULL;
    char* password = NULL;
    char* executionEnvRef = NULL;
    char* version = NULL;
    const char* operation_rpc = NULL;
    uint32_t operation_id = 0;

    amxc_var_t args;
    amxc_var_init(&args);
    amxc_var_t ret;
    amxc_var_init(&ret);

    when_null(obj, stop);
    op_type_t type = op_type_get(obj);
    when_null(obj, stop);

    if(type == op_type_install) {
        url = amxd_object_get_value(cstring_t, obj, "URL", NULL);
        when_null(url, stop);
        uuid = amxd_object_get_value(cstring_t, obj, "UUID", NULL);
        when_null(uuid, stop);
        username = amxd_object_get_value(cstring_t, obj, "Username", NULL);
        when_null(username, stop);
        password = amxd_object_get_value(cstring_t, obj, "Password", NULL);
        when_null(password, stop);
        executionEnvRef = amxd_object_get_value(cstring_t, obj, "ExecutionEnvRef", NULL);
        when_null(executionEnvRef, stop);
        operation_rpc = "InstallDU";
    } else if(type == op_type_update) {
        uuid = amxd_object_get_value(cstring_t, obj, "UUID", NULL);
        when_null(uuid, stop);
        version = amxd_object_get_value(cstring_t, obj, "Version", NULL);
        when_null(version, stop);
        url = amxd_object_get_value(cstring_t, obj, "URL", NULL);
        when_null(url, stop);
        username = amxd_object_get_value(cstring_t, obj, "Username", NULL);
        when_null(username, stop);
        password = amxd_object_get_value(cstring_t, obj, "Password", NULL);
        when_null(password, stop);
        operation_rpc = "UpdateDU";
    } else if(type == op_type_uninstall) {
        uuid = amxd_object_get_value(cstring_t, obj, "UUID", NULL);
        when_null(uuid, stop);
        version = amxd_object_get_value(cstring_t, obj, "Version", NULL);
        when_null(version, stop);
        executionEnvRef = amxd_object_get_value(cstring_t, obj, "ExecutionEnvRef", NULL);
        when_null(executionEnvRef, stop);
        operation_rpc = "UninstallDU";
    } else {
        SAH_TRACEZ_ERROR(ME, "Unkown operation");
    }

    when_str_empty(operation_rpc, stop);

    amxc_var_set_type(&args, AMXC_VAR_ID_HTABLE);
    if(url != NULL) {
        amxc_var_add_key(cstring_t, &args, "URL", url);
    }
    if(uuid != NULL) {
        amxc_var_add_key(cstring_t, &args, "UUID", uuid);
    }
    if(username != NULL) {
        amxc_var_add_key(cstring_t, &args, "Username", username);
    }
    if(password != NULL) {
        amxc_var_add_key(cstring_t, &args, "Password", password);
    }
    if(executionEnvRef != NULL) {
        amxc_var_add_key(cstring_t, &args, "ExecutionEnvRef", executionEnvRef);
    }
    if(version != NULL) {
        amxc_var_add_key(cstring_t, &args, "Version", version);
    }

    retval = amxb_call(amxb_be_who_has(SOFTWAREMODULES), SOFTWAREMODULES, operation_rpc, &args, &ret, 5);
    when_failed_trace(retval, stop, ERROR, "%s call failed (%d)", operation_rpc, retval);
    operation_id = GETP_UINT32(&ret, "1.OperationId");
    when_true_trace((operation_id == 0), stop, ERROR, "Invalid OperationID");
    op_id_set(obj, operation_id);
    op_status_set(obj, op_status_in_progress);
    retval = 0;
stop:
    free(url);
    free(uuid);
    free(username);
    free(password);
    free(executionEnvRef);
    free(version);
    amxc_var_clean(&args);
    amxc_var_clean(&ret);
    return retval;
}

static void op_failed(amxd_object_t* obj) {
    SAH_TRACEZ_INFO(ME, "Operation failed");
    amxc_var_t data;
    amxc_var_init(&data);
    amxc_var_set_type(&data, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &data, "UUID", "");
    amxc_var_add_key(cstring_t, &data, "DeploymentUnitRef", "");
    amxc_var_add_key(cstring_t, &data, "Version", "");
    amxc_var_add_key(cstring_t, &data, "CurrentState", "");
    amxc_var_add_key(bool, &data, "Resolved", true);
    amxc_var_add_key(cstring_t, &data, "ExecutionUnitRefList", "");
    amxc_var_add_key(amxc_ts_t, &data, "StartTime", NULL);
    amxc_var_add_key(amxc_ts_t, &data, "CompleteTime", NULL);
    amxc_var_add_key(uint32_t, &data, "FaultCode", 9002);
    amxc_var_add_key(cstring_t, &data, "FaultString", "Internal error");
    op_add_result(obj, &data);
    op_status_set(obj, op_status_done);
    amxc_var_clean(&data);
}

static void dscc_new(amxd_object_t* obj) {
    char* obj_path = NULL;
    when_null(obj, stop);
    obj_path = amxd_object_get_path(obj, AMXD_OBJECT_NAMED);
    when_str_empty(obj_path, stop);
    SAH_TRACEZ_INFO(ME, "Handling new: %s", obj_path);
stop:
    free(obj_path);
    return;
}

static void op_expiration_timer_start(amxd_object_t* obj) {
    op_info_t* info = NULL;
    when_null(obj, stop);
    info = (op_info_t*) obj->priv;
    when_null(info, stop);
    amxp_timer_start(info->expiration_timer, 3600 * 1000);
stop:
    return;
}

static void op_new(amxd_object_t* obj) {
    char* obj_path = NULL;
    when_null(obj, stop);
    obj_path = amxd_object_get_path(obj, AMXD_OBJECT_NAMED);
    when_str_empty(obj_path, stop);
    SAH_TRACEZ_INFO(ME, "Handling new: %s", obj_path);
    op_add_info(obj);
    if(op_run(obj) != 0) {
        op_failed(obj);
    } else {
        op_expiration_timer_start(obj);
    }
stop:
    free(obj_path);
    return;
}

void _dscc_added(UNUSED const char* const sig_name,
                 const amxc_var_t* const data,
                 UNUSED void* const priv) {
    SAH_TRACEZ_IN(ME);
    uint32_t index = 0;
    const char* path = NULL;
    amxd_object_t* obj = NULL;
    when_true_trace(amxc_var_is_null(data), stop, ERROR, "Invalid arg(s)");
    index = GET_UINT32(data, "index");
    when_true((index == 0), stop);
    path = GET_CHAR(data, "path");
    when_str_empty(path, stop);
    obj = amxd_dm_findf(cwmp_plugin_get_dm(), "%s.%d.", path, index);
    dscc_new(obj);
stop:
    SAH_TRACEZ_OUT(ME);
    return;
}

void _dscc_op_added(UNUSED const char* const sig_name,
                    const amxc_var_t* const data,
                    UNUSED void* const priv) {
    SAH_TRACEZ_IN(ME);
    uint32_t index = 0;
    const char* path = NULL;
    amxd_object_t* obj = NULL;
    when_true_trace(amxc_var_is_null(data), stop, ERROR, "Invalid arg(s)");
    index = GET_UINT32(data, "index");
    when_true((index == 0), stop);
    path = GET_CHAR(data, "path");
    when_str_empty(path, stop);
    obj = amxd_dm_findf(cwmp_plugin_get_dm(), "%s.%d.", path, index);
    op_new(obj);
stop:
    SAH_TRACEZ_OUT(ME);
    return;
}

static int op_add_result(amxd_object_t* obj, amxc_var_t* result) {
    int retval = -1;
    amxc_ts_t* startTime = NULL;
    amxc_ts_t* completeTime = NULL;
    amxd_trans_t trans;
    amxd_trans_init(&trans);
    when_null(obj, stop);
    when_true(amxc_var_is_null(result), stop);

    amxd_trans_select_object(&trans, obj);
    amxd_trans_select_pathf(&trans, ".Results.");
    amxd_trans_add_inst(&trans, 0, NULL);

    amxd_trans_set_value(cstring_t, &trans, "CurrentState", GET_CHAR(result, "CurrentState"));
    amxd_trans_set_value(cstring_t, &trans, "DeploymentUnitRef", GET_CHAR(result, "DeploymentUnitRef"));
    amxd_trans_set_value(cstring_t, &trans, "ExecutionUnitRefList", GET_CHAR(result, "ExecutionUnitRefList"));
    amxd_trans_set_value(uint32_t, &trans, "FaultCode", GET_UINT32(result, "FaultCode"));
    amxd_trans_set_value(cstring_t, &trans, "FaultString", GET_CHAR(result, "FaultString"));
    amxd_trans_set_value(bool, &trans, "Resolved", GET_BOOL(result, "Resolved"));
    startTime = amxc_var_dyncast(amxc_ts_t, GET_ARG(result, "StartTime"));
    amxd_trans_set_value(amxc_ts_t, &trans, "StartTime", startTime);
    completeTime = amxc_var_dyncast(amxc_ts_t, GET_ARG(result, "CompleteTime"));
    amxd_trans_set_value(amxc_ts_t, &trans, "CompleteTime", completeTime);
    amxd_trans_set_value(cstring_t, &trans, "UUID", GET_CHAR(result, "UUID"));
    amxd_trans_set_value(cstring_t, &trans, "Version", GET_CHAR(result, "Version"));
    retval = amxd_trans_apply(&trans, cwmp_plugin_get_dm());
    when_failed_trace(retval, stop, ERROR, "Failed to create new instance [%d]", retval);
    retval = 0;
stop:
    free(startTime);
    free(completeTime);
    amxd_trans_clean(&trans);
    return retval;
}

static void op_handle_results(UNUSED const char* const sig_name,
                              const amxc_var_t* const data,
                              UNUSED void* const priv) {
    amxd_object_t* obj = NULL;
    SAH_TRACEZ_IN(ME);
    when_true(amxc_var_is_null(data), stop);
    obj = op_obj_get_by_id(GET_UINT32(data, "OperationId"));
    when_null(obj, stop);

    if(op_status_get(obj) == op_status_in_progress) {
        op_add_result(obj, GETP_ARG(data, "OpResultStruct"));
        op_status_set(obj, op_status_done);
    }
stop:
    SAH_TRACEZ_OUT(ME);
}


static int count_objects(UNUSED amxd_object_t* templ, UNUSED amxd_object_t* obj, void* priv) {
    int* count = (int*) priv;
    *count = *count + 1;
    return 1;
}

void _dscc_op_done(UNUSED const char* const sig_name,
                   const amxc_var_t* const data,
                   UNUSED void* const priv) {
    const char* path = NULL;
    amxd_object_t* obj = NULL;
    amxd_object_t* dscc_obj = NULL;
    uint32_t number_of_operations = 0;
    uint32_t count = 0;

    path = GET_CHAR(data, "path");
    when_str_empty(path, stop);
    obj = amxd_dm_findf(cwmp_plugin_get_dm(), "%s", path);
    when_null(obj, stop);
    op_del_info(obj);
    dscc_obj = amxd_object_findf(obj, "^.^");
    when_null(dscc_obj, stop);

    number_of_operations = amxd_object_get_value(uint32_t, dscc_obj, "NumberOfOperations", NULL);
    when_true((number_of_operations == 0), stop);

    amxd_object_for_all(dscc_obj, ".Operations.[Status=='Done'].", count_objects, &count);

    if(count == number_of_operations) {
        SAH_TRACEZ_INFO(ME, "Operation(s) done");
        dscc_status_done(dscc_obj);
    }

stop:
    return;
}

static void op_set_all_to_done(void) {
    /**
     * TODO: op_set_all_to_done() should be reworked
     * OperationIds aren't persistent, this means we can't expect a result for operations in progress after a restart.
     * That's why all those operations are set to done.
     * In addition to that timing isn't handled correctly.
     */
    amxd_object_t* obj = NULL;
    const char* obj_path = NULL;
    amxc_llist_t paths;
    amxc_llist_init(&paths);
    SAH_TRACEZ_IN(ME);
    when_failed(amxd_dm_resolve_pathf(cwmp_plugin_get_dm(), &paths, "ManagementServer.SMM.DUStateChangeComplete.*.Operations.[Status!='Done']."), stop);
    amxc_llist_for_each(it, &paths) {
        obj_path = amxc_string_get(amxc_string_from_llist_it(it), 0);
        obj = amxd_dm_findf(cwmp_plugin_get_dm(), "%s", obj_path);
        op_failed(obj);
    }
stop:
    amxc_llist_clean(&paths, amxc_string_list_it_free);
    SAH_TRACEZ_OUT(ME);
}

void smm_differed_init(void) {
    op_set_all_to_done();
}

int smm_init(void) {
    int retval = -1;
    SAH_TRACEZ_IN(ME);

    amxc_var_t ret;
    amxc_var_init(&ret);

    retval = amxb_subscribe(amxb_be_who_has(SOFTWAREMODULES), SOFTWAREMODULES, "notification == 'OpResultDU!'",
                            op_handle_results, NULL);

    when_failed_trace(retval, stop, ERROR, "Failed to create subscription [%d]", retval);
    retval = 0;
stop:
    amxc_var_clean(&ret);
    SAH_TRACEZ_OUT(ME);
    return retval;
}

int smm_clean(void) {
    int retval = -1;
    SAH_TRACEZ_IN(ME);

    retval = 0;
    SAH_TRACEZ_OUT(ME);
    return retval;
}