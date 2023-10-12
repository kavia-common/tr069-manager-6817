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
#include <amxc/amxc_llist.h>
#include <amxc/amxc_string.h>
#include <amxc/amxc_variant.h>
#include <stdio.h>
#include <stdlib.h>

#include <dmengine/DM_ENG_ParameterInfoStruct.h>
#include <dmengine/DM_ENG_Device.h>
#include <dmengine/DM_ENG_Error.h>
#include <debug/sahtrace.h>
#include <string.h>

#include "DM_AmxCommon.h"
#include "DM_DeviceAdapter.h"

// //---------------------------------------------------------------------------------------------
// /**
//  * @addtogroup sah_cwmp_amxdeviceadapter
//  * @{
//  */

//---------------------------------------------------------------------------------------------
/**
   @brief
   Get the parameter names of the specified object, put the result into the parameter info struct.

   @details
   Get the parameters for the specified object and fill in the pararmeternamelist.

   @param object_path The full tr69 path of the parent object
   @param parameter The parent object data for the parameter which we want to get
   @param pnsList Linked list of parameter info structs which will be returned to the calling routine

   @return
   - false if an error occurred
   - true if succesfull
 */
static bool DM_ENG_Device_GetParameterNames_GetParameter(const char* object_path, amxc_var_t* parameter, DM_ENG_ParameterInfoStruct** pnsList) {
    bool ret = false;
    DM_ENG_ParameterInfoStruct* dmis = NULL;
    amxc_string_t paramPath;
    amxc_string_init(&paramPath, 0);
    const char* param_name = NULL;
    amxc_var_t* attributes = NULL;
    int is_read_only = 0;
    dm_amx_env_t* acsInfo = DM_ENG_Device_GetACSInfo();
    amxc_llist_t filters;
    amxc_llist_init(&filters);

    param_name = GETP_CHAR(parameter, "name");

    if(param_name == NULL) {
        GotoStop("The parameterName is NULL?, parent Path [%s]", object_path);
    }

    amxc_string_setf(&paramPath, "%s%s", object_path, param_name);
    amxa_get_filters(acsInfo->acl_rules, AMXA_PERMIT_GET, &filters, amxc_string_get(&paramPath, 0));


    if(!amxa_is_get_allowed(&filters, amxc_string_get(&paramPath, 0))) {
        ret = true;
        GotoStop("Filtred Path [%s], no access rights", amxc_string_get(&paramPath, 0));
    }

    // Find parameter attributes
    attributes = GET_ARG(parameter, "attributes");
    is_read_only = GET_INT32(attributes, "read-only");

    dmis = DM_ENG_newParameterInfoStruct(amxc_string_get(&paramPath, 0), (is_read_only == 0));
    DM_ENG_addParameterInfoStruct(pnsList, dmis);

    ret = true;
stop:
    amxc_llist_clean(&filters, amxc_string_list_it_free);
    amxc_string_clean(&paramPath);
    return ret;
}

//---------------------------------------------------------------------------------------------
/**
   @brief
   Get the parameter names of the specified object, put the result into the parameter info struct.

   @details
   Get the parameters for the specified object and fill in the pararmeternamelist.

   @param path The full tr69 path of the parent object
   @param incObject determine if we should include parent object in result
   @param object The data of the object for which we want to get the parameter values from
   @param pnsList Linked list of parameter info structs which will be returned to the calling routine

   @return
   - false if an error occurred
   - true if succesfull
 */
static bool DM_ENG_Device_GetParameterNames_GetParameters(const char* acspath, const char* path, bool incObject, amxc_var_t* object, DM_ENG_ParameterInfoStruct** pnsList) {
    bool ret = false;
    DM_ENG_ParameterInfoStruct* dmis = NULL;
    const amxc_htable_t* htable = NULL;
    amxc_var_t* parameters = NULL;
    dm_amx_env_t* acsInfo = DM_ENG_Device_GetACSInfo();
    char* object_path = NULL;
    amxc_llist_t filters;
    amxc_llist_init(&filters);

    int type_id = GET_INT32(object, "type_id");

    amxa_resolve_search_paths(acsInfo->bus_ctx, acsInfo->acl_rules, path);
    amxa_get_filters(acsInfo->acl_rules, AMXA_PERMIT_GET, &filters, path);

    if(!amxa_is_get_allowed(&filters, path)) {
        ret = true;
        GotoStop("Object Filtred out [%s], user cwmp has no access rights", path);
    }

    if(acsInfo->instanceAlias) {
        DM_ENG_Device_Common_IndexToAlias(acsInfo, acspath, path, &object_path);
    }

    if(incObject) {
        dmis = DM_ENG_newParameterInfoStruct((object_path != NULL) ? object_path : path, (type_id == 2) || (type_id == 3));
        DM_ENG_addParameterInfoStruct(pnsList, dmis);
    }

    /*
     * Response exemple : only template object Name is included not its parameters
     * parameters are included only in case of a instance
     * Device.LANDevice.1.Hosts.
     * Device.LANDevice.1.Hosts.HostNumberOfEntries
     * Device.LANDevice.1.Hosts.Host.
     * Device.LANDevice.1.Hosts.Host.1.
     * Device.LANDevice.1.Hosts.Host.1.IPAddress
     * Device.LANDevice.1.Hosts.Host.1.AddressSource
     */
    if(type_id == 2) {//template
        ret = true;
        goto stop;
    }

    parameters = GETP_ARG(object, "parameters");
    if(parameters) {
        htable = amxc_var_constcast(amxc_htable_t, parameters);
        amxc_htable_iterate(hit, htable) {
            amxc_var_t* parameter = amxc_var_from_htable_it(hit);
            if(!DM_ENG_Device_GetParameterNames_GetParameter((object_path != NULL) ? object_path : path, parameter, pnsList)) {
                ret = false;
                GotoStop("Adding parameter failed [%s]", path);
            }
        }
    }

    ret = true;
stop:
    if(object_path) {
        free(object_path);
        object_path = NULL;
    }
    amxc_llist_clean(&filters, amxc_string_list_it_free);
    return ret;
}
//---------------------------------------------------------------------------------------------
/**
    @brief
    Get the object whose name exactly matches the path argument, plus all
    Parameters and objects that are descendents.

    @details
    Get the object whose name exactly matches the path argument,then recursively Get all
    Parameters and objects that are descendents of the object given by the path argument,
    (all levels below the specified object in the object hierarchy if nextlevel = false)
    (stop at this object level if nexlevel = true)

    @param amx A pointer to the amx system bus environment variable
    @param path The object path in TR069 format
    @param pnsList The resulting parameter info struct list

   @return
   Returns 0 (zero) if OK or a fault code (9002, ...) according to the TR-069.
 */

static int DM_ENG_Device_GetParameterNames_GetNames(dm_amx_env_t* amx, bool nextlevel, const char* acspath, const char* path, DM_ENG_ParameterInfoStruct** pnsList) {
    int error = 0;
    int rv = 0;
    int type_id = 0;
    amxc_string_t childPath;
    amxc_var_t result;
    amxc_var_t* childobjects = NULL;
    amxc_var_t* object = NULL;
    char* object_path = NULL;
    amxc_var_init(&result);
    amxc_string_init(&childPath, 0);

    u_int32_t flags = AMXB_FLAG_PARAMETERS | AMXB_FLAG_OBJECTS | AMXB_FLAG_INSTANCES;
    rv = amxb_describe(amx->bus_ctx, path, flags, &result, 1);

    if((rv != 0) || amxc_var_is_null(&result)) {
        if((rv == AMXB_ERROR_NOT_SUPPORTED_SCHEME) || (rv == AMXB_ERROR_NOT_SUPPORTED_OP)) {
            SetErrorGotoStop(0, "Skip Object [%s]", path);
        } else {
            SetErrorGotoStop(DM_ENG_INVALID_PARAMETER_NAME, "amxb_describe failed for object [%s]", path);
        }
    }

    object = GETI_ARG(&result, 0);

    if(!DM_ENG_Device_GetParameterNames_GetParameters(acspath, path, !nextlevel, object, pnsList)) {
        SetErrorGotoStop(DM_ENG_INVALID_PARAMETER_NAME, "GetParameterNames(nextLevel=TRUE) failed, object [%s]", path);
    }

    type_id = GET_INT32(object, "type_id");

    if(type_id == 2) {
        childobjects = GETP_ARG(object, "instances");
    } else {
        childobjects = GETP_ARG(object, "objects");
    }

    if(childobjects) {
        amxc_var_for_each(child, childobjects) {
            if(nextlevel) {
                amxc_string_t childPath;
                amxc_string_init(&childPath, 0);
                amxc_var_t childobject;
                amxc_var_init(&childobject);
                amxc_llist_t filters;
                amxc_llist_init(&filters);
                DM_ENG_ParameterInfoStruct* dmis = NULL;
                int type_id = 0;
                const char* childName = amxc_var_constcast(cstring_t, child);
                amxc_string_setf(&childPath, "%s%s.", path, childName);
                flags = AMXB_FLAG_OBJECTS | AMXB_FLAG_INSTANCES;
                amxa_resolve_search_paths(amx->bus_ctx, amx->acl_rules, amxc_string_get(&childPath, 0));
                amxa_get_filters(amx->acl_rules, AMXA_PERMIT_GET, &filters, path);
                rv = amxb_describe(amx->bus_ctx, amxc_string_get(&childPath, 0), flags, &childobject, 1);

                if((rv == 0) && (amxa_is_get_allowed(&filters, amxc_string_get(&childPath, 0)))) {
                    type_id = GET_INT32(GETI_ARG(&childobject, 0), "type_id");

                    if(amx->instanceAlias) {
                        DM_ENG_Device_Common_IndexToAlias(amx, acspath, amxc_string_get(&childPath, 0), &object_path);
                    }

                    dmis = DM_ENG_newParameterInfoStruct((object_path != NULL) ? object_path : amxc_string_get(&childPath, 0), (type_id == 2) || (type_id == 3));
                    DM_ENG_addParameterInfoStruct(pnsList, dmis);

                    if(object_path) {
                        free(object_path);
                        object_path = NULL;
                    }
                } else {
                    SAH_TRACEZ_WARNING("DM_DA", "Object [%s] doesn't exist on target", amxc_string_get(&childPath, 0));
                }
                amxc_llist_clean(&filters, amxc_string_list_it_free);
                amxc_var_clean(&childobject);
                amxc_string_clean(&childPath);
            } else {
                const char* childName = amxc_var_constcast(cstring_t, child);
                amxc_string_clean(&childPath);
                amxc_string_setf(&childPath, "%s%s.", path, childName);
                error = DM_ENG_Device_GetParameterNames_GetNames(amx, false, acspath, amxc_string_get(&childPath, 0), pnsList);
                amxc_string_clean(&childPath);
            }
        }
    }
stop:
    if(object_path) {
        free(object_path);
        object_path = NULL;
    }
    amxc_string_clean(&childPath);
    amxc_var_clean(&result);
    return error;
}

//---------------------------------------------------------------------------------------------
/**
   @brief
   Get the parameter names of the specified partial path and return the infolist.

   @details
   Get the parameter names of the specified partial path and return the infolist.

   @param amx_env A pointer to the amx system bus environment variable
   @param path The partial tr69 path (e.g. InternetGatewayDevice.DeviceInfo.)
   @param nextLevel To indicate if the nexlevel flag is true or not in the getparameternames RPC
   @param infoList The resulting parameter info struct list

   @return
   - The tr69 error code (90xx) in case of an error
   - 0 if succesfull
 */
int DM_ENG_Device_GetParameterNames_PartialPath(dm_amx_env_t* amx_env, char* path, bool nextLevel, DM_ENG_ParameterInfoStruct** infoList) {
    int error = 0;
    amxc_var_t objects;
    amxc_var_init(&objects);
    SAH_TRACEZ_IN("DM_DA");

    if(!DM_ENG_Device_Common_IsValidPath(path)) {
        SetErrorGotoStop(DM_ENG_INVALID_PARAMETER_NAME, "Not a valid tr181 path [%s]", path);
    }

    if(DM_ENG_Device_Common_Resolve_Path(amx_env, path, &objects)) {
        const amxc_llist_t* path_list = amxc_var_constcast(amxc_llist_t, &objects);
        amxc_llist_iterate(it, path_list) {
            const char* objpath = amxc_var_constcast(cstring_t, amxc_var_from_llist_it(it));
            error = DM_ENG_Device_GetParameterNames_GetNames(amx_env, nextLevel, path, objpath, infoList);
            if(error != 0) {
                GotoStop("GPN Failed on object [%s]", objpath);
            }
        }
    } else {
        SetErrorGotoStop(DM_ENG_INVALID_PARAMETER_NAME, "Failed to resolve path [%s]", path);
    }

stop:
    amxc_var_clean(&objects);
    SAH_TRACEZ_OUT("DM_DA");
    return error;
}

//---------------------------------------------------------------------------------------------
/**
   @brief
   Get the parameter names of the specified complete parameter path and return the infolist

   @details
   Get the parameter names of the specified complete parameter path and return the infolist

   @param amx_env  A pointer to the amx system bus environment variable
   @param path     The complete tr69 parameter path (e.g. InternetGatewayDevice.DeviceInfo.ProvisioningCode)
   @param infoList The resulting parameter info struct list

   @return
   - The tr69 error code (90xx) in case of an error
   - 0 if succesfull
 */
int DM_ENG_Device_GetParameterNames_Parameter(dm_amx_env_t* amx_env, char* path, DM_ENG_ParameterInfoStruct** infoList) {
    int error = 0;
    u_int32_t flags = AMXB_FLAG_PARAMETERS;
    amxc_var_t parameters;
    amxd_path_t paramPath;
    amxc_var_t object;
    amxc_string_t paramName;
    amxd_path_init(&paramPath, 0);
    amxc_string_init(&paramName, 0);
    amxc_var_init(&object);
    amxc_var_init(&parameters);

    if(!DM_ENG_Device_Common_IsValidPath(path)) {
        SetErrorGotoStop(DM_ENG_INVALID_PARAMETER_NAME, "Not a valid parameter Name [%s]", path);
    }

    if(DM_ENG_Device_Common_Resolve_Path(amx_env, path, &parameters)) {
        const amxc_llist_t* path_list = amxc_var_constcast(amxc_llist_t, &parameters);
        amxc_llist_iterate(it, path_list) {
            const char* ppath = amxc_var_constcast(cstring_t, amxc_var_from_llist_it(it));
            char* object_path = NULL;
            amxd_path_clean(&paramPath);
            amxd_path_init(&paramPath, ppath);

            int rv = amxb_describe(amx_env->bus_ctx, amxd_path_get(&paramPath, AMXD_OBJECT_TERMINATE), flags, &object, 1);
            if((rv != 0) || amxc_var_is_null(&object)) {
                SetErrorGotoStop(DM_ENG_INVALID_PARAMETER_NAME, "amxb_describe failed to path [%s]", amxd_path_get(&paramPath, AMXD_OBJECT_TERMINATE));
            }
            amxc_string_clean(&paramName);
            amxc_string_setf(&paramName, "0.parameters.%s", amxd_path_get_param(&paramPath));
            amxc_var_t* parameter = GETP_ARG(&object, amxc_string_get(&paramName, 0));

            if(amx_env->instanceAlias) {
                DM_ENG_Device_Common_IndexToAlias(amx_env, path, amxd_path_get(&paramPath, AMXD_OBJECT_TERMINATE), &object_path);
            }

            amxa_resolve_search_paths(amx_env->bus_ctx, amx_env->acl_rules, amxd_path_get(&paramPath, AMXD_OBJECT_TERMINATE));

            if(!DM_ENG_Device_GetParameterNames_GetParameter((object_path != NULL) ? object_path : amxd_path_get(&paramPath, AMXD_OBJECT_TERMINATE), parameter, infoList)) {
                free(object_path);
                SetErrorGotoStop(DM_ENG_INVALID_PARAMETER_NAME, "Could not get the object parameters [%s]", amxd_path_get(&paramPath, AMXD_OBJECT_TERMINATE));
            }
            if(object_path) {
                free(object_path);
                object_path = NULL;
            }
        }
    } else {
        SetErrorGotoStop(DM_ENG_INVALID_PARAMETER_NAME, "failed to resolve path [%s]", path);
    }
stop:
    amxd_path_clean(&paramPath);
    amxc_var_clean(&parameters);
    amxc_var_clean(&object);
    amxc_string_clean(&paramName);
    return error;
}


// /** @} */
