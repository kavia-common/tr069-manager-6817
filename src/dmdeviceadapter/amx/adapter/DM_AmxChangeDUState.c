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
#include "DM_AmxChangeDUState.h"
#include "DM_AmxCommon.h"

#define ME "DM_DA"

#define MANAGEMENTSERVER_SMM "ManagementServer.SMM."
#define DUSTATECHANGECOMPLETE_SUB MANAGEMENTSERVER_SMM "DUStateChangeComplete.[Initiator == 'ACS']."
#define DUSTATECHANGECOMPLETE_DONE MANAGEMENTSERVER_SMM "DUStateChangeComplete.[Initiator == 'ACS' and Status == 'Done']."

static amxb_bus_ctx_t* bus_ctx = NULL;

int DM_ENG_Device_DoChangeDUState(OperationStruct* operations, const char* commandKey) {

    //TODO: support multiple operations instead of one

    int retval = DM_ENG_FAULTCODE_9002;
    amxc_var_t* operationsListPtr = NULL;
    amxc_var_t* operationPtr = NULL;
    amxc_var_t args;

    SAH_TRACEZ_IN(ME);

    amxc_var_init(&args);

    when_null_trace(commandKey, stop, ERROR, "Invalid CommandKey");
    when_null_trace(operations, stop, ERROR, "Invalid Operations");

    amxc_var_set_type(&args, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &args, "Initiator", "ACS");
    amxc_var_add_key(cstring_t, &args, "CommandKey", commandKey);

    operationsListPtr = amxc_var_add_new_key(&args, "Operations");
    amxc_var_set_type(operationsListPtr, AMXC_VAR_ID_LIST);
    operationPtr = amxc_var_add_new(operationsListPtr);
    amxc_var_set_type(operationPtr, AMXC_VAR_ID_HTABLE);
    if(operations->type == OPERATION_TYPE_INSTALL) {
        amxc_var_add_key(cstring_t, operationPtr, "Type", "Install");
        amxc_var_add_key(cstring_t, operationPtr, "URL", operations->data.install.url);
        amxc_var_add_key(cstring_t, operationPtr, "UUID", operations->data.install.uuid);
        amxc_var_add_key(cstring_t, operationPtr, "Username", operations->data.install.username);
        amxc_var_add_key(cstring_t, operationPtr, "Password", operations->data.install.password);
        amxc_var_add_key(cstring_t, operationPtr, "ExecutionEnvRef", operations->data.install.executionEnvRef);
    } else if(operations->type == OPERATION_TYPE_UPDATE) {
        amxc_var_add_key(cstring_t, operationPtr, "Type", "Update");
        amxc_var_add_key(cstring_t, operationPtr, "UUID", operations->data.update.uuid);
        amxc_var_add_key(cstring_t, operationPtr, "Version", operations->data.update.version);
        amxc_var_add_key(cstring_t, operationPtr, "URL", operations->data.update.url);
        amxc_var_add_key(cstring_t, operationPtr, "Username", operations->data.update.username);
        amxc_var_add_key(cstring_t, operationPtr, "Password", operations->data.update.password);
    } else if(operations->type == OPERATION_TYPE_UNINSTALL) {
        amxc_var_add_key(cstring_t, operationPtr, "Type", "Uninstall");
        amxc_var_add_key(cstring_t, operationPtr, "UUID", operations->data.uninstall.uuid);
        amxc_var_add_key(cstring_t, operationPtr, "Version", operations->data.uninstall.version);
        amxc_var_add_key(cstring_t, operationPtr, "ExecutionEnvRef", operations->data.uninstall.executionEnvRef);
    } else {
        SAH_TRACEZ_ERROR(ME, "Unsupported operation type");
        goto stop;
    }

    when_failed_trace(amxb_call(bus_ctx, MANAGEMENTSERVER_SMM, "changeDUState", &args, NULL, 5),
                      stop, ERROR, "Failed to call %schangeDUState()", MANAGEMENTSERVER_SMM);

    retval = 0;
stop:
    amxc_var_clean(&args);
    SAH_TRACEZ_OUT(ME);
    return retval;
}

static int handle_dustatechangecomplete(const char* path) {
    const char* commandKey = NULL;
    int retval = -1;
    amxc_var_t ret1;
    amxc_var_t ret2;
    amxc_string_t tmp_path;
    amxc_var_init(&ret1);
    amxc_var_init(&ret2);
    amxc_string_init(&tmp_path, 0);

    when_str_empty_trace(path, stop, ERROR, "Invalid arg(s)");
    SAH_TRACEZ_INFO(ME, "Handling %s", path);

    amxc_string_appendf(&tmp_path, "%sCommandKey", path);
    retval = amxb_get(bus_ctx, amxc_string_get(&tmp_path, 0), 0, &ret1, 5);
    amxc_var_dump(&ret1, STDOUT_FILENO);
    when_failed_trace(retval, stop, ERROR, "Failed to get object [%d]", retval);
    when_true_trace(amxc_var_is_null(&ret1), stop, ERROR, "Empty result");
    commandKey = GETP_CHAR(&ret1, "0.0.CommandKey");
    when_null_trace(commandKey, stop, ERROR, "Failed to get CommandKey");

    amxc_string_reset(&tmp_path);
    amxc_string_appendf(&tmp_path, "%sOperations.*.Results.*.", path);
    retval = amxb_get(bus_ctx, amxc_string_get(&tmp_path, 0), 0, &ret2, 5);
    when_failed_trace(retval, stop, ERROR, "Failed to get object [%d]", retval);
    when_true_trace(amxc_var_is_null(&ret2), stop, ERROR, "Empty result");
    retval = DM_ENG_InformMessageScheduler_SendDUStateChangeComplete(GETP_ARG(&ret2, "0"), commandKey, path);
    when_failed_trace(retval, stop, ERROR, "Failed to send DUStateChangeComplete for [%s]", path);

    retval = 0;
stop:
    amxc_var_clean(&ret1);
    amxc_var_clean(&ret2);
    amxc_string_clean(&tmp_path);
    return retval;
}

static void handle_dustatechangecomplete_cb(char* path) {
    handle_dustatechangecomplete((const char*) path);
}

void differ_dustatechangecomplete_handling(const char* path) {
    when_str_empty_trace(path, stop, ERROR, "Invalid dustatechangecomplete path");
    SAH_TRACEZ_INFO(ME, "DUStateChangeComplete %s is Done", path);
    DM_ENG_NotificationInterface_timerStart(path, 0, 0, handle_dustatechangecomplete_cb);
stop:
    return;
}

void dustatechangecomplete_notification(UNUSED const char* const sig_name,
                                        const amxc_var_t* const data,
                                        UNUSED void* const priv) {

    SAH_TRACEZ_IN(ME);
    const char* status = NULL;
    const char* obj_path = NULL;

    status = GETP_CHAR(data, "parameters.Status.to");
    obj_path = GETP_CHAR(data, "object");

    when_null_trace(status, stop, ERROR, "Failed to get the DUStateChangeComplete status");
    when_null_trace(obj_path, stop, ERROR, "Failed to get the DUStateChangeComplete path");

    if(strcmp(status, "Done") == 0) {
        handle_dustatechangecomplete(obj_path);
    }

stop:
    SAH_TRACEZ_OUT(ME);
    return;
}

int DM_AmxChangeDUState_init(amxb_bus_ctx_t* ctx) {
    int retval = -1;
    SAH_TRACEZ_IN(ME);

    amxc_var_t ret;
    amxc_var_init(&ret);

    when_null_trace(ctx, stop, ERROR, "Invalid bus ctx");

    bus_ctx = ctx;

    retval = amxb_get(bus_ctx, DUSTATECHANGECOMPLETE_DONE, 0, &ret, 5);

    if(retval != amxd_status_ok) {
        SAH_TRACEZ_INFO(ME, "Failed to get TransferComplete");
    } else if(amxc_var_is_null(&ret) != true) {
        amxc_var_t* tmp = GETI_ARG(&ret, 0);
        amxc_var_dump(tmp, STDOUT_FILENO);
        amxc_var_for_each(element, tmp) {
            differ_dustatechangecomplete_handling(amxc_var_key(element));
        }
    }

    retval = amxb_subscribe(bus_ctx, DUSTATECHANGECOMPLETE_SUB,
                            "(notification == 'dm:object-changed') && (contains('parameters.Status'))",
                            dustatechangecomplete_notification, NULL);

stop:
    amxc_var_clean(&ret);
    SAH_TRACEZ_OUT(ME);
    return retval;
}


int DM_AmxChangeDUState_clean(void) {
    int retval = -1;
    SAH_TRACEZ_IN(ME);

    retval = amxb_unsubscribe(bus_ctx, DUSTATECHANGECOMPLETE_SUB, dustatechangecomplete_notification, NULL);

    SAH_TRACEZ_OUT(ME);
    return retval;
}

