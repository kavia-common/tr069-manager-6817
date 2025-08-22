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
#include <amxc/amxc_macros.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>


#include <dmengine/DM_ENG_Common.h>
#include <dmengine/DM_ENG_ParameterValueStruct.h>
#include <dmengine/DM_ENG_Device.h>
#include <dmengine/DM_ENG_ParameterAttributesCache.h>
#include <dmengine/DM_ENG_Error.h>
#include <debug/sahtrace.h>
#include <debug/sahtrace_macros.h>
#include <string.h>

#include "DM_AmxCommon.h"
#include "DM_AmxParameter.h"
#include "DM_DeviceAdapter.h"
//---------------------------------------------------------------------------------------------
/**
 * @addtogroup sah_cwmp_amxdeviceadapter
 * @{
 */

//---------------------------------------------------------------------------------------------

static amxc_var_t* DM_AmxParameter_GetGsdmInfo(const char* parameter_name, amxc_var_t* data) {
    amxc_var_t* info = NULL;
    amxc_var_t* response = NULL;
    char* supported_path = NULL;
    amxc_string_t new_supported_path_str;
    amxd_path_t path;

    amxd_path_init(&path, parameter_name);
    amxc_string_init(&new_supported_path_str, 0);
    when_str_empty(parameter_name, stop);
    when_true(amxc_var_is_null(data), stop);

    response = GET_ARG(data, "Response");
    when_true(amxc_var_is_null(response), stop);
    supported_path = amxd_path_build_supported_path(&path);
    when_str_empty_trace(supported_path, stop, ERROR, "Failed to build supported path");

    info = amxc_var_get_key(response, supported_path, AMXC_VAR_FLAG_DEFAULT);
    if(!info && DM_ENG_Device_Common_EndsWithDot(supported_path)) {
        amxc_string_setf(&new_supported_path_str, "%s{i}.", supported_path);
        info = amxc_var_get_key(response, amxc_string_get(&new_supported_path_str, 0), AMXC_VAR_FLAG_DEFAULT);
    }
stop:
    amxc_string_clean(&new_supported_path_str);
    amxd_path_clean(&path);
    free(supported_path);
    return info;
}

static void DM_AmxParameter_ParseForEachParameter(dm_amx_env_t* amx, const char* acs_path, const char* object_path, const amxc_htable_it_t* param_it, amxc_htable_t* plist, add_parameter_to_list_t add_param_to_list, alias_list_t aliases, amxc_var_t* info) {
    const char* param_key = amxc_htable_it_get_key(param_it);
    amxc_var_t* param_var = amxc_var_from_htable_it(param_it);
    char* alias_path = NULL;
    amxc_string_t parampath_str;
    const char* param_path = NULL;

    amxc_string_init(&parampath_str, 0);
    amxc_string_setf(&parampath_str, "%s%s", object_path, param_key);
    param_path = amxc_string_get(&parampath_str, 0);

    if(amx->instanceAlias) {
        DM_ENG_Device_Common_IndexToAlias(amx, acs_path, param_path, &alias_path);
    }
    if(!alias_path && aliases.count) {
        alias_path = DM_ENG_Device_Common_Modify_Path_With_Aliases(param_path, aliases);
    }

    if(add_param_to_list != NULL) {
        add_param_to_list(plist, alias_path != NULL ? alias_path : param_path, param_var, param_key, info);
    }

    free(alias_path);
    amxc_string_clean(&parampath_str);
}

static void DM_AmxParameter_ParseForEachObject(dm_amx_env_t* amx, const char* acs_path, const amxc_htable_it_t* object_it, amxc_htable_t* plist, amxc_var_t* gsdm_data, add_object_to_list_t add_object_to_list, add_parameter_to_list_t add_param_to_list, alias_list_t aliases) {
    const char* object_path = amxc_htable_it_get_key(object_it);
    amxc_var_t* info = NULL;
    char* alias_path = NULL;
    const amxc_htable_t* parameters = NULL;

    info = DM_AmxParameter_GetGsdmInfo(object_path, gsdm_data);
    when_true_trace(amxc_var_is_null(info), stop, INFO, "No GSDM info found for [%s]", object_path);

    if(amx->instanceAlias) {
        DM_ENG_Device_Common_IndexToAlias(amx, acs_path, object_path, &alias_path);
    }
    if(!alias_path && aliases.count) {
        alias_path = DM_ENG_Device_Common_Modify_Path_With_Aliases(object_path, aliases);
    }

    if(add_object_to_list != NULL) {
        add_object_to_list(plist, alias_path != NULL ? alias_path : object_path, info, gsdm_data);
    }

    parameters = amxc_var_constcast(amxc_htable_t, amxc_var_from_htable_it(object_it));
    amxc_htable_iterate(param_it, parameters) {
        DM_AmxParameter_ParseForEachParameter(amx, acs_path, object_path, param_it, plist, add_param_to_list, aliases, info);
    }

stop:
    free(alias_path);
}

static int DM_AmxParameter_Parse(dm_amx_env_t* amx, const char* acs_path, amxc_var_t* object, amxc_htable_t* plist, amxc_var_t* ext_gsdm_data, add_acs_path_to_list_t add_acs_path_to_list, add_object_to_list_t add_object_to_list, add_parameter_to_list_t add_param_to_list) {
    int error = 0;
    const amxc_htable_t* object_htable = NULL;
    alias_list_t aliases = {0};
    amxc_var_t* gsdm_in_use = NULL;
    amxc_var_t gsdm_data;
    amxc_var_t* info = NULL;

    amxc_var_init(&gsdm_data);
    amxc_var_set_type(&gsdm_data, AMXC_VAR_ID_HTABLE);
    if(ext_gsdm_data == NULL) {
        DM_ENG_Device_Common_Get_Gsdm_data(amx->bus_ctx, acs_path, &gsdm_data);
        gsdm_in_use = &gsdm_data;
    } else {
        gsdm_in_use = ext_gsdm_data;
    }

    if(DM_ENG_Device_Common_Is_Alias_Based(acs_path)) {
        aliases = DM_ENG_Device_Common_Extract_Aliases(acs_path);
    }

    if(add_acs_path_to_list) {
        info = DM_AmxParameter_GetGsdmInfo(acs_path, gsdm_in_use);
        add_acs_path_to_list(plist, acs_path, info, gsdm_in_use);
    }

    object_htable = amxc_var_constcast(amxc_htable_t, GETI_ARG(object, 0));
    amxc_htable_iterate(object_it, object_htable) {
        DM_AmxParameter_ParseForEachObject(amx, acs_path, object_it, plist, gsdm_in_use, add_object_to_list, add_param_to_list, aliases);
    }

    DM_ENG_Device_Common_Clean_Aliases(&aliases);
    amxc_var_clean(&gsdm_data);
    return error;
}

static void DM_AmxParameter_AddParameterToPVS(dm_eng_pvs_list_t* pvs_list, const char* param_path, amxc_var_t* param_var, const char* param_key, amxc_var_t* info) {
    int32_t type = -1;
    char* param_val = NULL;
    dm_eng_pvs_t* pvs = NULL;
    amxc_var_t* value = NULL;
    amxc_string_t parameters_str;
    amxc_string_init(&parameters_str, 0);

    when_null(param_key, exit);

    pvs = dm_eng_pvs_list_get(pvs_list, param_path);
    when_not_null(pvs, exit);

    amxc_string_setf(&parameters_str, "parameters.%s", param_key);
    type = GET_INT32(GETP_ARG(info, amxc_string_get(&parameters_str, 0)), "type");
    if(type == -1) {
        type = amxc_var_type_of(value);
    }

    param_val = amxc_var_dyncast(cstring_t, param_var);
    pvs = dm_eng_pvs_pvs_new(param_path, dm_eng_amx_type_to_data_type(type), param_val);
    free(param_val);
    param_val = NULL;

    dm_eng_pvs_list_add(pvs_list, pvs);
exit:
    amxc_string_clean(&parameters_str);
}

static void DM_AmxParameter_AddSingleObjectToPN(dm_eng_pn_list_t* pn_list, const char* object_path, amxc_var_t* info) {
    bool writable = false;
    amxc_var_t* writable_var = NULL;
    dm_eng_pn_t* pn = NULL;

    pn = dm_eng_pn_list_get(pn_list, object_path);
    when_not_null(pn, exit);

    writable_var = GET_ARG(info, "writable");
    if(writable_var) {
        writable = amxc_var_dyncast(bool, writable_var);
    }

    pn = dm_eng_pn_new(object_path, writable);
    dm_eng_pn_list_add(pn_list, pn);
exit:
    return;
}

static void DM_AmxParameter_AddTemplateObjectsToPN(dm_eng_pn_list_t* pn_list, const char* object_path, amxc_var_t* info, amxc_var_t* gsdm_data) {
    amxc_string_t template_pathstr;
    const char* template_path = NULL;
    amxc_var_t* templates = NULL;
    amxc_var_t* template_info = NULL;

    amxc_string_init(&template_pathstr, 0);

    templates = GET_ARG(info, "templates");
    when_null(templates, exit);

    amxc_htable_iterate(hit, amxc_var_constcast(amxc_htable_t, templates)) {
        const char* template = amxc_htable_it_get_key(hit);
        amxc_string_clean(&template_pathstr);
        amxc_string_setf(&template_pathstr, "%s%s", object_path, template);
        template_path = amxc_string_get(&template_pathstr, 0);

        template_info = DM_AmxParameter_GetGsdmInfo(template_path, gsdm_data);
        when_true_trace(amxc_var_is_null(template_info), exit, INFO, "No GSDM info found for [%s]", template_path);
        DM_AmxParameter_AddSingleObjectToPN(pn_list, template_path, template_info);
    }
exit:
    amxc_string_clean(&template_pathstr);
    return;
}

static void DM_AmxParameter_AddObjectToPN(dm_eng_pn_list_t* pn_list, const char* object_path, amxc_var_t* info, amxc_var_t* gsdm_data) {
    DM_AmxParameter_AddSingleObjectToPN(pn_list, object_path, info);
    DM_AmxParameter_AddTemplateObjectsToPN(pn_list, object_path, info, gsdm_data);
}

static void DM_AmxParameter_AddParameterToPN(dm_eng_pn_list_t* pn_list, const char* param_path, UNUSED amxc_var_t* param_var, const char* param_key, amxc_var_t* info) {
    bool writable = false;
    amxc_var_t* writable_var = NULL;
    dm_eng_pn_t* pn = NULL;
    amxc_string_t parameters_str;
    amxc_string_init(&parameters_str, 0);

    pn = dm_eng_pn_list_get(pn_list, param_path);
    when_not_null(pn, exit);

    amxc_string_setf(&parameters_str, "parameters.%s", param_key);
    writable_var = GET_ARG(GETP_ARG(info, amxc_string_get(&parameters_str, 0)), "writable");
    if(writable_var) {
        writable = amxc_var_dyncast(bool, writable_var);
    }

    pn = dm_eng_pn_new(param_path, writable);
    dm_eng_pn_list_add(pn_list, pn);
exit:
    amxc_string_clean(&parameters_str);
}

static int DM_AmxParameter_ParseValues(dm_amx_env_t* amx, const char* acs_path, amxc_var_t* object, dm_eng_pvs_list_t* pvs_list, amxc_var_t* ext_gsdm_data) {
    return DM_AmxParameter_Parse(amx, acs_path, object, pvs_list, ext_gsdm_data, NULL, NULL, DM_AmxParameter_AddParameterToPVS);
}

static int DM_AmxParameter_ParseNames(dm_amx_env_t* amx, const char* acs_path, amxc_var_t* object, dm_eng_pn_list_t* pn_list, amxc_var_t* ext_gsdm_data) {
    return DM_AmxParameter_Parse(amx, acs_path, object, pn_list, ext_gsdm_data, DM_AmxParameter_AddObjectToPN, DM_AmxParameter_AddObjectToPN, DM_AmxParameter_AddParameterToPN);
}

static int DM_AmxParameter_Get(dm_amx_env_t* amx, const char* path, bool nextLevel, amxc_htable_t* plist, amxc_var_t* gsdm_data, parse_get_t parse_get) {
    int error = 0;
    int32_t depth = 0;
    bool is_partialpath = false;
    bool is_wildcardpath = false;
    amxc_var_t get;
    int ret = 0;
    amxc_llist_t filters;

    amxc_var_init(&get);
    amxc_llist_init(&filters);

    if(strlen(path) == 0) {
        path = "Device.";
    }

    amxa_resolve_search_paths(amx->bus_ctx, amx->acl_rules, path);
    amxa_get_filters(amx->acl_rules, AMXA_PERMIT_GET, &filters, path);

    if(!amxa_is_get_allowed(&filters, path)) {
        SetErrorGotoStop(DM_ENG_INVALID_PARAMETER_NAME, "cwmp has no access rights to [%s] ", path);
    }

    is_partialpath = (path[strlen(path) - 1] == '.');
    is_wildcardpath = (strstr(path, "*") != NULL);

    /* if nextlevel : restrict to only first level parameters */
    if(nextLevel) {
        depth = 0;
    } else {
        depth = is_partialpath ? -1 : 0;
    }

    ret = amxb_get(amx->bus_ctx, path, depth, &get, 10);

    if((is_partialpath || is_wildcardpath) && ((ret != AMXB_STATUS_OK) || amxc_var_is_null(&get))) {
        SAH_TRACEZ_INFO(ME, "Empty response for unresolved Partial Path [%s]", path);
        goto stop;
    }

    if((ret != AMXB_STATUS_OK)) {
        if((ret == AMXB_ERROR_NOT_SUPPORTED_SCHEME)) {
            SetErrorGotoStop(0, "Skip non tr181-component, path [%s] %d", path, ret);
        } else {
            SetErrorGotoStop(DM_ENG_INVALID_PARAMETER_NAME, "Failed to get object path [%s] error [%d] ", path, ret);
        }
    }

    if(is_partialpath) {
        amxa_filter_get_resp(&get, &filters);
    }

    if(parse_get != NULL) {
        error = parse_get(amx, path, &get, plist, gsdm_data);
    }

stop:
    amxc_llist_clean(&filters, amxc_string_list_it_free);
    amxc_var_clean(&get);
    return error;
}

int DM_AmxParameter_GetValues(dm_amx_env_t* amx, const char* path, dm_eng_pvs_list_t* pvs_list, amxc_var_t* gsdm_data) {
    return DM_AmxParameter_Get(amx, path, false, pvs_list, gsdm_data, DM_AmxParameter_ParseValues);
}

int DM_AmxParameter_GetNames(dm_amx_env_t* amx, const char* path, bool nextLevel, dm_eng_pn_list_t* pn_list, amxc_var_t* gsdm_data) {
    return DM_AmxParameter_Get(amx, path, nextLevel, pn_list, gsdm_data, DM_AmxParameter_ParseNames);
}

// /** @} */
