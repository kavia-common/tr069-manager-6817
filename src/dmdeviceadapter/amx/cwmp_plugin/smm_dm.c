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

amxd_status_t _changeDUState(amxd_object_t* obj,
                             UNUSED amxd_function_t* func,
                             amxc_var_t* args,
                             amxc_var_t* ret) {

    amxd_status_t status = amxd_status_invalid_arg;
    const char* initiator = NULL;
    const char* commandKey = NULL;
    amxc_var_t* operations = NULL;
    const char* operation_type = NULL;
    when_null_trace(args, stop, ERROR, "Invalid arg(s)");

    initiator = GET_CHAR(args, "Initiator");
    commandKey = GET_CHAR(args, "CommandKey");
    operations = GET_ARG(args, "Operations");

    amxd_object_t* dustatechangecomplete_obj = amxd_object_get(obj, "DUStateChangeComplete.");

    amxd_trans_t trans;
    amxd_trans_init(&trans);
    amxd_trans_select_object(&trans, dustatechangecomplete_obj);
    amxd_trans_add_inst(&trans, 0, NULL);

    amxd_trans_set_value(cstring_t, &trans, "CommandKey", commandKey);
    amxd_trans_set_value(cstring_t, &trans, "Initiator", initiator);
    amxd_trans_set_value(cstring_t, &trans, "Status", "");

    amxd_trans_select_pathf(&trans, ".Operations.");

    amxc_var_for_each(operation, operations) {
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
        amxd_trans_set_value(cstring_t, &trans, "Status", "");
        amxd_trans_set_value(cstring_t, &trans, "Type", GET_CHAR(operation, "Type"));
        when_str_empty_trace(operation_type, stop, ERROR, "Invalid operation type");
        amxd_trans_select_pathf(&trans, ".^");
    }

    status = amxd_trans_apply(&trans, cwmp_plugin_get_dm());

    when_true_trace((status != amxd_status_ok), stop, ERROR, "Failed to create new instance [%d]", status);

    status = amxd_status_ok;
    amxc_var_set(uint32_t, ret, 0);

stop:
    amxd_trans_clean(&trans);
    return status;
}
