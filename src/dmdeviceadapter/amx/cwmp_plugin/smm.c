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
#define SOFTWAREMODULES_SIGNAL_REGEX "^wait:CWMP_SoftwareModules\\.$"


typedef enum _op_status_t
{
    op_status_none,
    op_status_in_progress,
    op_status_done
} op_status_t;

static int op_add_result(amxd_object_t* obj, amxc_var_t* result);
static op_status_t op_status_get(amxd_object_t* obj);
static void op_status_set(amxd_object_t* obj, op_status_t status);

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

static amxd_object_t* op_obj_get_by_id(uint32_t id) {
    amxd_object_t* obj = NULL;
    const char* obj_path = NULL;
    amxc_llist_t paths;
    amxc_llist_init(&paths);
    when_true((id == 0), stop);
    when_failed(amxd_dm_resolve_pathf(cwmp_plugin_get_dm(), &paths, "ManagementServer.SMM.DUStateChangeComplete.*.Operations.[ID==%u].", id), stop);
    when_true(amxc_llist_is_empty(&paths), stop);
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

static int op_run(amxc_var_t* operation, uint32_t* operation_id) {
    int retval = -1;
    const char* url = NULL;
    const char* uuid = NULL;
    const char* username = NULL;
    const char* password = NULL;
    const char* executionEnvRef = NULL;
    const char* version = NULL;
    const char* rpc = NULL;
    const char* type = NULL;

    amxc_var_t args;
    amxc_var_init(&args);
    amxc_var_t ret;
    amxc_var_init(&ret);

    when_true(amxc_var_is_null(operation), stop);
    type = GET_CHAR(operation, "Type");

    if(strcmp(type, "Install") == 0) {
        url = GET_CHAR(operation, "URL");
        when_null(url, stop);
        uuid = GET_CHAR(operation, "UUID");
        when_null(uuid, stop);
        username = GET_CHAR(operation, "Username");
        when_null(username, stop);
        password = GET_CHAR(operation, "Password");
        when_null(password, stop);
        executionEnvRef = GET_CHAR(operation, "ExecutionEnvRef");
        when_null(executionEnvRef, stop);
        rpc = "InstallDU";
    } else if(strcmp(type, "Update") == 0) {
        uuid = GET_CHAR(operation, "UUID");
        when_null(uuid, stop);
        version = GET_CHAR(operation, "Version");
        when_null(version, stop);
        url = GET_CHAR(operation, "URL");
        when_null(url, stop);
        username = GET_CHAR(operation, "Username");
        when_null(username, stop);
        password = GET_CHAR(operation, "Password");
        when_null(password, stop);
        rpc = "UpdateDU";
    } else if(strcmp(type, "Uninstall") == 0) {
        uuid = GET_CHAR(operation, "UUID");
        when_null(uuid, stop);
        version = GET_CHAR(operation, "Version");
        when_null(version, stop);
        executionEnvRef = GET_CHAR(operation, "ExecutionEnvRef");
        when_null(executionEnvRef, stop);
        rpc = "UninstallDU";
    } else {
        SAH_TRACEZ_ERROR(ME, "Invalid operation type [%s]", type);
        goto stop;
    }

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

    retval = amxb_call(amxb_be_who_has(SOFTWAREMODULES), SOFTWAREMODULES, rpc, &args, &ret, 5);
    when_failed_trace(retval, stop, ERROR, "%s call failed (%d)", rpc, retval);
    *operation_id = GETP_UINT32(&ret, "1.OperationId");
    retval = 0;
stop:
    amxc_var_clean(&args);
    amxc_var_clean(&ret);
    return retval;
}

static int op_add_result(amxd_object_t* obj, amxc_var_t* result) {
    int retval = -1;
    amxc_ts_t* startTime = NULL;
    amxc_ts_t* completeTime = NULL;
    const char* currentState = NULL;
    const char* deploymentUnitRef = NULL;
    const char* executionUnitRefList = NULL;
    const char* faultString = NULL;
    const char* uuid = NULL;
    const char* version = NULL;
    amxd_trans_t trans;
    amxd_trans_init(&trans);
    when_null(obj, stop);
    when_true(amxc_var_is_null(result), stop);

    amxd_trans_select_object(&trans, obj);
    amxd_trans_select_pathf(&trans, ".Results.");
    amxd_trans_add_inst(&trans, 0, NULL);

    currentState = GET_CHAR(result, "CurrentState");
    if(currentState != NULL) {
        amxd_trans_set_value(cstring_t, &trans, "CurrentState", currentState);
    }
    deploymentUnitRef = GET_CHAR(result, "DeploymentUnitRef");
    if(deploymentUnitRef != NULL) {
        amxd_trans_set_value(cstring_t, &trans, "DeploymentUnitRef", deploymentUnitRef);
    }
    executionUnitRefList = GET_CHAR(result, "ExecutionUnitRefList");
    if(executionUnitRefList != NULL) {
        amxd_trans_set_value(cstring_t, &trans, "ExecutionUnitRefList", executionUnitRefList);
    }
    amxd_trans_set_value(uint32_t, &trans, "FaultCode", GET_UINT32(result, "FaultCode"));
    faultString = GET_CHAR(result, "FaultString");
    if(faultString != NULL) {
        amxd_trans_set_value(cstring_t, &trans, "FaultString", faultString);
    }
    amxd_trans_set_value(bool, &trans, "Resolved", GET_BOOL(result, "Resolved"));
    startTime = amxc_var_dyncast(amxc_ts_t, GET_ARG(result, "StartTime"));
    if(startTime != NULL) {

        amxd_trans_set_value(amxc_ts_t, &trans, "StartTime", startTime);
    }
    completeTime = amxc_var_dyncast(amxc_ts_t, GET_ARG(result, "CompleteTime"));
    if(completeTime != NULL) {
        amxd_trans_set_value(amxc_ts_t, &trans, "CompleteTime", completeTime);
    }
    uuid = GET_CHAR(result, "UUID");
    if(uuid != NULL) {
        amxd_trans_set_value(cstring_t, &trans, "UUID", uuid);
    }
    version = GET_CHAR(result, "Version");
    if(version != NULL) {
        amxd_trans_set_value(cstring_t, &trans, "Version", version);
    }
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
        when_failed(op_add_result(obj, GETP_ARG(data, "OpResultStruct")), stop);
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

amxd_status_t _changeDUState(amxd_object_t* obj,
                             UNUSED amxd_function_t* func,
                             amxc_var_t* args,
                             amxc_var_t* ret) {

    amxd_status_t status = amxd_status_invalid_arg;
    const char* initiator = NULL;
    const char* commandKey = NULL;
    amxc_var_t* operation = NULL;
    const char* operation_type = NULL;
    uint32_t operation_id = 0;
    amxd_trans_t trans;
    amxd_trans_init(&trans);

    when_null_trace(args, stop, ERROR, "Invalid arg(s)");
    initiator = GET_CHAR(args, "Initiator");
    commandKey = GET_CHAR(args, "CommandKey");
    operation = amxc_var_get_first(GET_ARG(args, "Operations"));
    when_true(amxc_var_is_null(operation), stop);
    when_failed_trace(op_run(operation, &operation_id), stop, ERROR, "Failed to perform operation");
    when_true_trace((operation_id == 0), stop, ERROR, "Invalid operation ID");

    amxd_trans_select_object(&trans, amxd_object_get(obj, "DUStateChangeComplete."));
    amxd_trans_add_inst(&trans, 0, NULL);
    amxd_trans_set_value(cstring_t, &trans, "CommandKey", commandKey);
    amxd_trans_set_value(cstring_t, &trans, "Initiator", initiator);
    amxd_trans_set_value(cstring_t, &trans, "Status", "");
    amxd_trans_select_pathf(&trans, ".Operations.");
    amxd_trans_add_inst(&trans, 0, NULL);
    operation_type = GET_CHAR(operation, "Type");
    if(strcmp(operation_type, "Install") == 0) {
        amxd_trans_set_value(cstring_t, &trans, "URL", GET_CHAR(operation, "URL"));
        amxd_trans_set_value(cstring_t, &trans, "UUID", GET_CHAR(operation, "UUID"));
        amxd_trans_set_value(cstring_t, &trans, "Username", GET_CHAR(operation, "Username"));
        amxd_trans_set_value(cstring_t, &trans, "Password", GET_CHAR(operation, "Password"));
        amxd_trans_set_value(cstring_t, &trans, "ExecutionEnvRef", GET_CHAR(operation, "ExecutionEnvRef"));
    } else if(strcmp(operation_type, "Update") == 0) {
        amxd_trans_set_value(cstring_t, &trans, "UUID", GET_CHAR(operation, "UUID"));
        amxd_trans_set_value(cstring_t, &trans, "Version", GET_CHAR(operation, "Version"));
        amxd_trans_set_value(cstring_t, &trans, "URL", GET_CHAR(operation, "URL"));
        amxd_trans_set_value(cstring_t, &trans, "Username", GET_CHAR(operation, "Username"));
        amxd_trans_set_value(cstring_t, &trans, "Password", GET_CHAR(operation, "Password"));
    } else if(strcmp(operation_type, "Uninstall") == 0) {
        amxd_trans_set_value(cstring_t, &trans, "UUID", GET_CHAR(operation, "UUID"));
        amxd_trans_set_value(cstring_t, &trans, "Version", GET_CHAR(operation, "Version"));
        amxd_trans_set_value(cstring_t, &trans, "ExecutionEnvRef", GET_CHAR(operation, "ExecutionEnvRef"));
    } else {
        SAH_TRACEZ_ERROR(ME, "Invalid operation type [%s]", operation_type);
        goto stop;
    }
    amxd_trans_set_value(cstring_t, &trans, "Status", "In Progress");
    amxd_trans_set_value(cstring_t, &trans, "Type", GET_CHAR(operation, "Type"));
    amxd_trans_set_value(uint32_t, &trans, "ID", operation_id);
    when_str_empty_trace(operation_type, stop, ERROR, "Invalid operation type");
    amxd_trans_select_pathf(&trans, ".^");
    status = amxd_trans_apply(&trans, cwmp_plugin_get_dm());
    when_true_trace((status != amxd_status_ok), stop, ERROR, "Failed to create new instance [%d]", status);
    status = amxd_status_ok;
    amxc_var_set(uint32_t, ret, 0);

stop:
    amxd_trans_clean(&trans);
    return status;
}

static int monitor_smm(void) {
    int retval = -1;
    amxb_bus_ctx_t* bus_ctx = amxb_be_who_has(SOFTWAREMODULES);
    when_null_trace(bus_ctx, stop, ERROR, "Unable to get SMM bus ctx");
    retval = amxb_subscribe(bus_ctx, SOFTWAREMODULES, "notification == 'OpResultDU!'",
                            op_handle_results, NULL);
    when_failed_trace(retval, stop, ERROR, "Failed to create subscription [%d]", retval);
    retval = 0;
stop:
    return retval;
}

static void smm_available_cb(UNUSED const char* signame,
                             UNUSED const amxc_var_t* const data,
                             UNUSED void* const priv) {
    SAH_TRACEZ_WARNING(ME, "SMM available");
    monitor_smm();
}

int smm_init(void) {
    int retval = -1;
    SAH_TRACEZ_IN(ME);

    if(amxb_be_who_has(SOFTWAREMODULES) == NULL) {
        amxb_wait_for_object(SOFTWAREMODULES);
        when_failed_trace(amxp_slot_connect_filtered(NULL, SOFTWAREMODULES_SIGNAL_REGEX, NULL, smm_available_cb, NULL),
                          stop, ERROR, "Unable to wait for SMM to become available.");
        SAH_TRACEZ_WARNING(ME, "Waiting for SMM");
    } else {
        when_failed(monitor_smm(), stop);
    }
    retval = 0;
stop:
    SAH_TRACEZ_OUT(ME);
    return retval;
}
