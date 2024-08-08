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
#ifdef CONFIG_SAH_AMX_TR069_MANAGER_USE_GSDM

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
#include <string.h>

#include "DM_AmxCommon.h"
#include "DM_DeviceAdapter.h"

//---------------------------------------------------------------------------------------------
/**
 * @addtogroup sah_cwmp_amxdeviceadapter
 * @{
 */

//---------------------------------------------------------------------------------------------

#define DM_ENG_Device_SetParameterValuesFault(fl, p, f, nb) { \
        DM_ENG_SetParameterValuesFault* pf = DM_ENG_newSetParameterValuesFault(p, f); \
        DM_ENG_addSetParameterValuesFault(fl, pf); \
        if(nb) { \
            *nb = *nb + 1; \
        } \
}

/**
   @brief
   Simple Helper to read and parse the result variant

   @details
   Helper : read and parse result var from amxb_get then fill
   the resulting parameter value struct list

   @param object The resulting variant from amxb_get call for the
    object for which we want to get the parameter values
   @param data Pointer to the resulting parameter value struct list

   @return
   - false if an error occurred
   - true if succesfull
 */
static int DM_ENG_Device_GetParameterValues_ParseValues(dm_amx_env_t* amx, const char* acspath, amxc_var_t* object, void* data) {
    SAH_TRACEZ_IN("DM_DA");
    int error = 0;
    DM_ENG_ParameterValueStruct** pvsList = (DM_ENG_ParameterValueStruct**) data;
    DM_ENG_ParameterValueStruct* dmvs = NULL;
    const amxc_htable_t* htable = NULL;
    amxc_string_t param_name;
    amxc_var_t desc;

    htable = amxc_var_constcast(amxc_htable_t, GETI_ARG(object, 0));

    amxc_htable_iterate(hit, htable) {
        amxc_var_init(&desc);
        const char* key = amxc_htable_it_get_key(hit);
        amxc_var_t* hit_val = amxc_var_from_htable_it(hit);
        const amxc_htable_t* param = amxc_var_constcast(amxc_htable_t, hit_val);
        amxc_var_t* pm_var = NULL;
        char* supported_path = NULL;
        amxc_string_t supported_path_inst;
        amxd_path_t obj_path;

        if(amxb_get_supported(amx->bus_ctx, key, AMXB_FLAG_PARAMETERS, &desc, 1)) {
            SAH_TRACEZ_WARNING("DM_DA", "amxb_get_supported failed for [%s], parameter types maybe reported wrong in the GPV response", key);
        }

        amxd_path_init(&obj_path, NULL);
        amxd_path_setf(&obj_path, false, "%s", key);
        supported_path = amxd_path_build_supported_path(&obj_path);

        amxc_string_init(&supported_path_inst, 0);
        amxc_string_setf(&supported_path_inst, "%s{i}.", supported_path);

        pm_var = amxc_var_get_key(GETI_ARG(&desc, 0), amxc_string_get(&supported_path_inst, 0), AMXC_VAR_FLAG_DEFAULT);
        if(pm_var == NULL) {
            pm_var = amxc_var_get_key(GETI_ARG(&desc, 0), supported_path, AMXC_VAR_FLAG_DEFAULT);
        }

        amxc_htable_iterate(hit_param, param) {
            int32_t type = -1;
            char* param_val = NULL;
            char* alias_path = NULL;
            const char* paramkey = amxc_htable_it_get_key(hit_param);
            amxc_var_t* param_var = amxc_var_from_htable_it(hit);
            amxc_var_t* value = GETP_ARG(param_var, paramkey);

            const amxc_llist_t* llist = amxc_var_constcast(amxc_llist_t, GET_ARG(pm_var, "supported_params"));
            amxc_llist_iterate(lit, llist) {
                amxc_var_t* parameter = amxc_var_from_llist_it(lit);
                if(strcmp(GETP_CHAR(parameter, "param_name"), paramkey) == 0) {
                    type = GET_INT32(parameter, "type");
                    break;
                }
            }

            if(type == -1) {
                type = amxc_var_type_of(value);
            }

            param_val = amxc_var_dyncast(cstring_t, value);
            amxc_string_init(&param_name, 0);
            amxc_string_setf(&param_name, "%s%s", key, paramkey);

            if(amx->instanceAlias) {
                DM_ENG_Device_Common_IndexToAlias(amx, acspath, amxc_string_get(&param_name, 0), &alias_path);
            }

            dmvs = DM_ENG_newParameterValueStruct(alias_path != NULL ? alias_path : amxc_string_get(&param_name, 0),
                                                  DM_ENG_Device_Common_ConvertParameterType(type), param_val);

            DM_ENG_addParameterValueStruct(pvsList, dmvs);
            error = 0;
            amxc_string_clean(&param_name);
            free(param_val);
            free(alias_path);
        }
        amxd_path_clean(&obj_path);
        free(supported_path);
        amxc_string_clean(&supported_path_inst);
        amxc_var_clean(&desc);
    }

    SAH_TRACEZ_OUT("DM_DA");
    return error;
}

//---------------------------------------------------------------------------------------------
/**
   @brief
   Get the parameter values for a given path and return the valuelist.

   @details
   Get the parameter values for a given path and return the valuelist

   @param amx A pointer to the ambiorix system bus environment variable
   @param path The partial tr69 path (e.g. InternetGatewayDevice.DeviceInfo.)
   @param pvsList Pointer to the resulting parameter value struct list

   @return
   - Tr69 error code (90xx) in case of an error
   - 0 if succesfull
 */
int DM_ENG_Device_GetParameterValues_GetValues(dm_amx_env_t* amx, const char* path, DM_ENG_ParameterValueStruct** pvsList) {
    SAH_TRACEZ_IN("DM_DA");
    int error = 0;
    amxc_var_t get;
    amxc_var_init(&get);
    int ret = 0;
    amxc_llist_t filters;
    amxc_llist_init(&filters);

    if(strlen(path) == 0) {
        path = "Device.";
    }

    amxa_resolve_search_paths(amx->bus_ctx, amx->acl_rules, path);
    amxa_get_filters(amx->acl_rules, AMXA_PERMIT_GET, &filters, path);

    if(!amxa_is_get_allowed(&filters, path)) {
        SetErrorGotoStop(DM_ENG_INVALID_PARAMETER_NAME, "cwmp has no access rights to [%s] ", path);
    }

    ret = amxb_get(amx->bus_ctx, path, (path[strlen(path) - 1] == '.') ? 20 : 0, &get, 10);

    if((ret != AMXB_STATUS_OK) || amxc_var_is_null(&get)) {
        if((ret == AMXB_ERROR_NOT_SUPPORTED_SCHEME)) {
            SetErrorGotoStop(0, "Skip non tr181-component, path [%s] %d", path, ret);
        } else {
            SetErrorGotoStop(DM_ENG_INVALID_PARAMETER_NAME, "Failed to get object path [%s] error [%d] ", path, ret);
        }
    }

    if(path[strlen(path) - 1] == '.') {
        amxa_filter_get_resp(&get, &filters);
    }
    error = DM_ENG_Device_GetParameterValues_ParseValues(amx, path, &get, pvsList);

stop:
    SAH_TRACEZ_OUT("DM_DA");
    amxc_llist_clean(&filters, amxc_string_list_it_free);
    amxc_var_clean(&get);
    return error;
}

//---------------------------------------------------------------------------------------------
/**
   @brief
   Check the parameterlist to see if there was no previous set of the same parameter.
   Tr69 does not allow setting the same parameter more than once in one RPC

   @details
   Check the parameterlist to see if there was no previous set of the same parameter.
   Tr69 does not allow setting the same parameter more than once in one RPC.

   @param parameterList The complete ParameterList
   @param currentItem The index of the current item in the list
   @param faultsList Linked list of faults untill now
   @param nbFaults Total number of faults untill now

   @return
   - false if the parameter is not a duplicate
   - true if the parameter was a duplicate
 */
static bool DM_ENG_Device_SetParameterValues_CheckForDuplicates(DM_ENG_ParameterValueStruct* parameterList[], int currentItem, DM_ENG_SetParameterValuesFault** faultsList, int* nbFaults) {
    int j;
    bool ret = false;

    for(j = 0; j < currentItem; j++) {
        if(( parameterList[currentItem]->parameterName != NULL) &&
           ( parameterList[j]->parameterName != NULL) &&
           ( strcmp(parameterList[currentItem]->parameterName, parameterList[j]->parameterName) == 0)) {
            SAH_TRACEZ_ERROR("DM_DA", "SPV Error, duplicated parameter [%s]", parameterList[currentItem]->parameterName);
            DM_ENG_Device_SetParameterValuesFault(faultsList, parameterList[currentItem]->parameterName, DM_ENG_INVALID_ARGUMENTS, nbFaults);
            ret = true;
            break;
        }
    }

    return ret;
}

//---------------------------------------------------------------------------------------------
/**
   @brief
   Validate the parameter values, in case of an error, add the fault to the faultslist.

   @details
   Validate the parameter values, in case of an error, add the fault to the faultslist.
   All parameters in the parameter list from the same object will be validated, including the object itself.
   A check for duplicate parameter,parameters types and names done.

   @param amx A pointer to the amx system bus environment variable
   @param parameterList The list of parameters containing the complete tr69 parameter path (e.g. InternetGatewayDevice.DeviceInfo.ProvisioningCode) and the new value
   @param i The index we need to start from, the index is increased.
   @param faultsList Linked list of faults untill now
   @param nbFaults Total number of faults untill now

   @return
   - TR069 error code or 0 when success
 */
int DM_ENG_Device_SetParameterValues_Validate(dm_amx_env_t* amx, DM_ENG_ParameterValueStruct* parameterList[], int* i, DM_ENG_SetParameterValuesFault** faultsList, int* nbFaults) {
    int error = 0;
    u_int32_t flags = 0;
    int amxb_ret = 0;
    bool checkType = false;
    char* tempString = NULL;
    amxd_path_t obj_path;
    amxc_var_t obj_desc;
    char* object_path = NULL;
    amxd_path_init(&obj_path, "");
    amxc_var_init(&obj_desc);
    amxc_var_t* pm_var = NULL;
    amxc_string_t supported_path_inst;
    char* supported_path = NULL;

    SAH_TRACEZ_IN("DM_DA");

    if(!DM_ENG_Device_Common_IsValidPath(parameterList[*i]->parameterName)) {
        DM_ENG_Device_SetParameterValuesFault(faultsList, parameterList[*i]->parameterName, DM_ENG_INVALID_PARAMETER_NAME, nbFaults);
        SetErrorGotoStop(DM_ENG_INVALID_ARGUMENTS, "Not a valid parameter Name");
    }

    amxd_path_setf(&obj_path, false, "%s", parameterList[*i]->parameterName);
    object_path = strdup(amxd_path_get(&obj_path, AMXD_OBJECT_TERMINATE));

    if(amxd_path_is_search_path(&obj_path)) {
        DM_ENG_Device_SetParameterValuesFault(faultsList, parameterList[*i]->parameterName, DM_ENG_INVALID_PARAMETER_NAME, nbFaults);
        SetErrorGotoStop(DM_ENG_INVALID_ARGUMENTS, "Not a complete object path ");
    }

    flags = AMXB_FLAG_PARAMETERS;
    amxb_ret = amxb_get_supported(amx->bus_ctx, amxd_path_get(&obj_path, AMXD_OBJECT_TERMINATE), flags, &obj_desc, 2);

    if((amxb_ret != 0) || amxc_var_is_null(&obj_desc)) {
        DM_ENG_Device_SetParameterValuesFault(faultsList, parameterList[*i]->parameterName, DM_ENG_INVALID_PARAMETER_NAME, nbFaults);
        SetErrorGotoStop(DM_ENG_INVALID_ARGUMENTS, "Object dosent exist ?");
    }

    supported_path = amxd_path_build_supported_path(&obj_path);
    amxc_string_init(&supported_path_inst, 0);
    amxc_string_setf(&supported_path_inst, "%s{i}.", supported_path);
    pm_var = amxc_var_get_key(GETI_ARG(&obj_desc, 0), amxc_string_get(&supported_path_inst, 0), AMXC_VAR_FLAG_DEFAULT);
    if(pm_var == NULL) {
        pm_var = amxc_var_get_key(GETI_ARG(&obj_desc, 0), supported_path, AMXC_VAR_FLAG_DEFAULT);
    }

    if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_VERIFYPARAMETERTYPE, &tempString) == 0) {
        if(tempString) {
            checkType = atoi(tempString) ? true : false;
            free(tempString);
        }
    }
    amxa_resolve_search_paths(amx->bus_ctx, amx->acl_rules, amxd_path_get(&obj_path, AMXD_OBJECT_TERMINATE));

    // check all parameters that belong to the current object
    while(strcmp(amxd_path_get(&obj_path, AMXD_OBJECT_TERMINATE), object_path) == 0) {
        amxc_var_t* parameter = NULL;
        const amxc_llist_t* llist = amxc_var_constcast(amxc_llist_t, GET_ARG(pm_var, "supported_params"));

        amxc_llist_iterate(lit, llist) {
            amxc_var_t* param = amxc_var_from_llist_it(lit);
            if(strcmp(GETP_CHAR(param, "param_name"), amxd_path_get_param(&obj_path)) == 0) {
                parameter = param;
                break;
            }
        }

        if(amxc_var_is_null(parameter)) {
            DM_ENG_Device_SetParameterValuesFault(faultsList, parameterList[*i]->parameterName, DM_ENG_INVALID_PARAMETER_NAME, nbFaults);
            SetErrorGotoStop(DM_ENG_INVALID_ARGUMENTS, "Parameter Not found");
        }

        if(checkType &&
           (parameterList[*i]->type != DM_ENG_Device_Common_ConvertParameterType(GET_INT32(parameter, "type")))) {
            DM_ENG_Device_SetParameterValuesFault(faultsList, parameterList[*i]->parameterName, DM_ENG_INVALID_PARAMETER_TYPE, nbFaults);
            SetErrorGotoStop(DM_ENG_INVALID_ARGUMENTS, "Wrong type");
        }

        if(DM_ENG_Device_SetParameterValues_CheckForDuplicates(parameterList, *i, faultsList, nbFaults)) {
            SetErrorGotoStop(DM_ENG_INVALID_ARGUMENTS, "Duplicated entry");
        }


        if(!GET_INT32(parameter, "access")) {
            DM_ENG_Device_SetParameterValuesFault(faultsList, parameterList[*i]->parameterName, DM_ENG_READ_ONLY_PARAMETER, nbFaults);
            SetErrorGotoStop(DM_ENG_INVALID_ARGUMENTS, "parameter is read-only");
        }

        if(!amxa_is_set_allowed(amx->bus_ctx, amx->acl_rules, amxd_path_get(&obj_path, AMXD_OBJECT_TERMINATE), amxd_path_get_param(&obj_path))) {
            DM_ENG_Device_SetParameterValuesFault(faultsList, parameterList[*i]->parameterName, DM_ENG_READ_ONLY_PARAMETER, nbFaults);
            SetErrorGotoStop(DM_ENG_INVALID_ARGUMENTS, "cwmp have no write access to [%s%s]", amxd_path_get(&obj_path, AMXD_OBJECT_TERMINATE), amxd_path_get_param(&obj_path));
        }

        (*i)++;
        if(parameterList[*i] == NULL) {
            break;
        }

        if(!DM_ENG_Device_Common_IsValidPath(parameterList[*i]->parameterName)) {
            DM_ENG_Device_SetParameterValuesFault(faultsList, parameterList[*i]->parameterName, DM_ENG_INVALID_PARAMETER_NAME, nbFaults);
            SetErrorGotoStop(DM_ENG_INVALID_ARGUMENTS, "Not a valid parameter Name");
        }
        amxd_path_clean(&obj_path);
        amxd_path_init(&obj_path, parameterList[*i]->parameterName);
    }
    (*i)--;

stop:
    free(supported_path);
    amxc_string_clean(&supported_path_inst);
    amxd_path_clean(&obj_path);
    amxc_var_clean(&obj_desc);
    free(object_path);
    SAH_TRACEZ_OUT("DM_DA");
    return error;
}

static int tr069_spv_amxd_status_to_spv_fault_code(amxd_status_t status) {
    int retval = -1;
    switch(status) {
    case amxd_status_invalid_value:
        retval = DM_ENG_INVALID_PARAMETER_VALUE;
        break;
    case amxd_status_invalid_type:
        retval = DM_ENG_INVALID_PARAMETER_TYPE;
        break;
    default:
        SAH_TRACEZ_ERROR("DM_DA", "Undefined status: %d", status);
        retval = DM_ENG_INTERNAL_ERROR;
        break;
    }
    return retval;
}


static void tr069_spv_build_spv_faults(DM_ENG_SetParameterValuesFault** faultsList, int* nbFaults, const amxc_var_t* data) {
    SAH_TRACEZ_IN("DM_DA");
    amxc_var_t* failure = GETI_ARG(data, 0);
    amxc_var_t* obj = amxc_var_get_first(failure);
    const char* obj_key = amxc_var_key(obj);
    SAH_TRACEZ_INFO("DM_DA", "Object key: %s", obj_key);
    amxc_var_for_each(param, obj) {
        const char* param_key = amxc_var_key(param);
        if((param_key != NULL) && (*param_key != 0)) {
            amxd_status_t status = (amxd_status_t) GET_UINT32(param, "error_code");
            SAH_TRACEZ_INFO("DM_DA", "Parameter key: %s, status %d", param_key, status);
            amxc_string_t param_full_path;
            amxc_string_init(&param_full_path, 0);
            amxc_string_setf(&param_full_path, "%s%s", obj_key, param_key);
            DM_ENG_Device_SetParameterValuesFault(faultsList, amxc_string_get(&param_full_path, 0), tr069_spv_amxd_status_to_spv_fault_code(status), nbFaults);
            amxc_string_clean(&param_full_path);
        }
    }
    SAH_TRACEZ_OUT("DM_DA");
}

//---------------------------------------------------------------------------------------------
/**
   @brief
   Set the parameter values, in case of an error, add the fault to the faultslist.

   @details
   Set the parameter values, in case of an error, add the fault to the faultslist.
   All parameters in the parameter list from the same object will be set at once.

   @param amx A pointer to the ambiorix system bus environment variable
   @param parameterList The list of parameters containing the complete tr69 parameter path (e.g. InternetGatewayDevice.DeviceInfo.ProvisioningCode) and the new value
   @param i The index we need to start from, the index is increased.
   @param faultsList Linked list of faults untill now
   @param nbFaults Total number of faults untill now

   @return
   - TR069 error code or 0 when success
 */
int DM_ENG_Device_SetParameterValues_SetValues(dm_amx_env_t* amx_env, DM_ENG_ParameterValueStruct* parameterList[], int* i, DM_ENG_SetParameterValuesFault** faultsList, int* nbFaults) {
    int error = 0;
    int rv = 0;
    amxd_path_t path;
    amxc_var_t set;
    amxc_var_t ret;

    char* object_path = NULL;
    amxc_var_init(&set);
    amxc_var_init(&ret);
    amxd_path_init(&path, "");
    amxc_var_set_type(&set, AMXC_VAR_ID_HTABLE);
    SAH_TRACEZ_IN("DM_DA");

    amxd_path_clean(&path);
    amxd_path_init(&path, parameterList[*i]->parameterName);

    object_path = strdup(amxd_path_get(&path, AMXD_OBJECT_TERMINATE));
    when_null(object_path, stop);

    // loop throught parameters that belong to the same object
    while(strcmp(amxd_path_get(&path, AMXD_OBJECT_TERMINATE), object_path) == 0) {

        DM_ENG_AddCachedValueToParameterAttributesCache(parameterList[*i]->parameterName, parameterList[*i]->value);
        amxc_var_add_key(cstring_t, &set, amxd_path_get_param(&path), parameterList[*i]->value);
        (*i)++;
        if(parameterList[*i] == NULL) {
            break;
        }
        amxd_path_clean(&path);
        amxd_path_init(&path, parameterList[*i]->parameterName);
    }
    (*i)--;

    rv = amxb_set(amx_env->bus_ctx, object_path, &set, &ret, 5);

    if(rv != AMXB_STATUS_OK) {
        tr069_spv_build_spv_faults(faultsList, nbFaults, &ret);
        SetErrorGotoStop(DM_ENG_INVALID_ARGUMENTS, "Invalid arguments");
    }
stop:
    amxc_var_clean(&set);
    amxc_var_clean(&ret);
    amxd_path_clean(&path);
    if(object_path) {
        free(object_path);
    }
    SAH_TRACEZ_OUT("DM_DA");
    return error;
}
#endif
// /** @} */
