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
extern char* ROOT_DM_PARAMETERS[];
extern char* ROOT_DM_INTERNAL_PARAMETER_PATH[];
//---------------------------------------------------------------------------------------------
/**
 * @addtogroup sah_cwmp_amxdeviceadapter
 * @{
 */

//---------------------------------------------------------------------------------------------


/**
   @brief
   Simple Helper to search the right parameter type

   @details
   Helper :search the right parameter type
   used to work arround ubus problem of the types supported.

   @param bus_ctx bus env variable
   @param object object Path
   @param parameter parameter name without the object prefix

   @return
   type (integer id of the type)
 */
static int DM_ENG_Device_GetParameterValues_FindType(amxb_bus_ctx_t* bus_ctx, const char* object, const char* parameter) {
    int type = -1;
    int rv = 0;
    amxc_var_t desc;
    amxc_var_t* param_var = NULL;
    amxc_string_t paramPath;
    amxc_string_init(&paramPath, 0);
    amxc_var_init(&desc);

    rv = amxb_describe(bus_ctx, object, AMXB_FLAG_PARAMETERS, &desc, 1);

    if(((rv != 0) || amxc_var_is_null(&desc))) {
        type = -1;
        goto stop;
    }
    amxc_string_setf(&paramPath, "0.parameters.%s", parameter);

    param_var = GETP_ARG(&desc, amxc_string_get(&paramPath, 0));
    if(!param_var) {
        type = -1;
        goto stop;
    }
    type = GET_INT32(param_var, "type_id");

stop:
    amxc_string_clean(&paramPath);
    amxc_var_clean(&desc);
    return type;
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
static int DM_ENG_Device_GetParameterValues_ParseValues(amxb_bus_ctx_t* bus_ctx, amxc_var_t* object, void* data) {
    /*
     * result may be something like this
     * htable of a htable ?
       {
        Hosts.Host.1. = {
            IPAddress = "192.168.0.220",
            MACAddress = "DD:DD:DD:FF:EE:DD"
        },
        Hosts.Host.2. = {
            IPAddress = "192.168.0.221",
            MACAddress = "DD:DD:DD:FF:FF:FF"
        }
       }
     */
    int error = 0;
    DM_ENG_ParameterValueStruct** pvsList = (DM_ENG_ParameterValueStruct**) data;
    DM_ENG_ParameterValueStruct* dmvs = NULL;
    const char* dmprefix = NULL;
    const amxc_htable_t* htable = NULL;
    amxc_string_t param_name;

    dmprefix = DM_ENG_getDatamodelPrefix();
    htable = amxc_var_constcast(amxc_htable_t, GETI_ARG(object, 0));

    // iterate througth objects
    amxc_htable_iterate(hit, htable) {
        const char* key = amxc_htable_it_get_key(hit);
        amxc_var_t* hit_val = amxc_var_from_htable_it(hit);

        const amxc_htable_t* param = amxc_var_constcast(amxc_htable_t, hit_val);

        // each object may contains multiple parameters
        amxc_htable_iterate(hit_param, param) {
            const char* paramkey = amxc_htable_it_get_key(hit_param);
            amxc_var_t* param_var = amxc_var_from_htable_it(hit);
            amxc_var_t* value = GETP_ARG(param_var, paramkey);
            //amxc_var_dump(value , 0);
            u_int32_t type = amxc_var_type_of(value);
            char* param_val = amxc_var_dyncast(cstring_t, value);
            amxc_string_init(&param_name, 0);
            amxc_string_setf(&param_name, "%s%s%s", dmprefix, key, paramkey);

            //(check via describe API), ubus report the wrong type
            //bool is reported as int8 , all uintX are reported as intX
            if((type == AMXC_VAR_ID_INT8) || (type == AMXC_VAR_ID_INT16) || (type == AMXC_VAR_ID_INT32) || (type == AMXC_VAR_ID_INT64)) {
                int new_type = DM_ENG_Device_GetParameterValues_FindType(bus_ctx, key, paramkey);
                if(new_type != -1) {
                    type = new_type;
                }
            }

            dmvs = DM_ENG_newParameterValueStruct(amxc_string_get(&param_name, 0),
                                                  DM_ENG_Device_Common_ConvertParameterType(type), param_val);

            DM_ENG_addParameterValueStruct(pvsList, dmvs);
            error = 0;
            amxc_string_clean(&param_name);
            free(param_val);
        }
    }

    SAH_TRACEZ_OUT("DM_DA");
    return error;
}

//---------------------------------------------------------------------------------------------
/**
   @brief
   perform a get on the system bus using ambiorix API then parse result

   @details
   perform a get on the system bus using ambiorix API then parse result
   this function can deal with any kind of path (object,parameter,wilcard or not)

   @param bus_ctx bus env variable
   @param path the full path of the requested object/parameter
   @param data Pointer to the resulting parameter value struct list

   @return
   - false if an error occurred
   - true if succesfull
 */
static int DM_ENG_Device_GetParameterValues_GetData(amxb_bus_ctx_t* bus_ctx, const char* path, void* data) {

    int error = 0;
    DM_ENG_ParameterValueStruct** pvsList = (DM_ENG_ParameterValueStruct**) data;
    int32_t depth = 0;
    int timeout = 0;
    int ret = 0;
    amxc_var_t object;
    amxc_var_init(&object);

    if(path[strlen(path) - 1] == '.') {
        // 20 is just a guess,there is no way to get the real depth
        depth = 20;
        timeout = 10;// 10s timeout
    } else {
        depth = 0;
        timeout = 1;
    }

    ret = amxb_get(bus_ctx, path, depth, &object, timeout);

    if((ret != AMXB_STATUS_OK) || amxc_var_is_null(&object)) {
        if((ret == AMXB_ERROR_NOT_SUPPORTED_SCHEME) || (ret == AMXB_ERROR_NOT_SUPPORTED_OP)) {
            //not a tr181 component skip it
            SetErrorGotoStop(0, "non tr181 component, skipping it code [%d] path [%s]", ret, path);
        } else {
            SetErrorGotoStop(DM_ENG_INVALID_PARAMETER_NAME, "failed to get object code [%d] path [%s]", ret, path);
        }
    }
    // Parse result
    error = DM_ENG_Device_GetParameterValues_ParseValues(bus_ctx, &object, pvsList);
stop:
    amxc_var_clean(&object);
    return error;
}
//---------------------------------------------------------------------------------------------
/**
   @brief
   perform a get on the system bus using ambiorix API then parse result

   @details
   perform a get on the system bus using ambiorix API then parse result
   this function can deal only with Root Datamodel parameters ( parameters
   that are directly under Device. object)

   @param bus_ctx bus env variable
   @param path the full path of the requested object/parameter
   @param data Pointer to the resulting parameter value struct list

   @return
   - false if an error occurred
   - true if succesfull
 */
static int DM_ENG_Device_GetParameterValues_GetRootParameter(dm_amx_env_t* amx, const char* path, DM_ENG_ParameterValueStruct** pvsList) {
    int error = 0;
    int ret = 0;
    amxc_var_t* var = NULL;
    char* val = NULL;
    u_int32_t type = 0;
    amxc_var_t object;
    amxd_path_t parameterPath;
    amxc_string_t parameterAmxPath;
    amxd_path_init(&parameterPath, 0);
    amxc_string_init(&parameterAmxPath, 0);
    amxc_var_init(&object);
    const char* internalPath = DM_ENG_Device_Common_GetRootParameterInternalPath(path);
    if(internalPath == NULL) {
        SetErrorGotoStop(DM_ENG_INVALID_PARAMETER_NAME, "Failed to get parameter [%s]", path);
    }
    amxd_path_clean(&parameterPath);
    amxd_path_init(&parameterPath, internalPath);

    ret = amxb_get(amx->bus_ctx, internalPath, 0, &object, 1);
    if((ret != 0) || amxc_var_is_null(&object)) {
        SetErrorGotoStop(DM_ENG_INVALID_PARAMETER_NAME, "failed to get object [%s]", internalPath);
    }
    amxc_string_clean(&parameterAmxPath);
    amxc_string_setf(&parameterAmxPath, "0.'%s'.%s",
                     amxd_path_get(&parameterPath, AMXD_OBJECT_TERMINATE),
                     amxd_path_get_param(&parameterPath));

    var = GETP_ARG(&object, amxc_string_get(&parameterAmxPath, 0));
    type = amxc_var_type_of(var);
    val = amxc_var_dyncast(cstring_t, var);
    DM_ENG_addParameterValueStruct(pvsList,
                                   DM_ENG_newParameterValueStruct(path,
                                                                  DM_ENG_Device_Common_ConvertParameterType(type),
                                                                  val)
                                   );
stop:
    if(val) {
        free(val);
    }
    amxd_path_clean(&parameterPath);
    amxc_string_clean(&parameterAmxPath);
    amxc_var_clean(&object);
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
    int error = 0;
    const char* internalPath = NULL;
    amxc_var_t objects;
    amxc_var_init(&objects);

    internalPath = DM_ENG_Device_Common_ACSToAMXPath_noalloc(path);
    if(internalPath == NULL) {
        SetErrorGotoStop(DM_ENG_INVALID_PARAMETER_NAME, "Not a valid object path");
    }

    // Case of Device. or IGD.
    if((strlen(path) == 0) || (strcmp(amx->prefix, path) == 0)) {
        int i = 0;
        while(ROOT_DM_INTERNAL_PARAMETER_PATH[i]) {
            error = DM_ENG_Device_GetParameterValues_GetRootParameter(amx, ROOT_DM_INTERNAL_PARAMETER_PATH[i], pvsList);
            if(error) {
                SetErrorGotoStop(DM_ENG_INVALID_PARAMETER_NAME, "Couldn't get Root datamodel paramete [%s]", ROOT_DM_INTERNAL_PARAMETER_PATH[i]);
            }
            i++;
        }
        // Get the rest of the data model
        if(DM_ENG_Device_Common_Resolve_Path(amx, internalPath, &objects)) {
            const amxc_llist_t* path_list = amxc_var_constcast(amxc_llist_t, &objects);
            amxc_llist_iterate(it, path_list) {
                const char* objpath = amxc_var_constcast(cstring_t, amxc_var_from_llist_it(it));
                error = DM_ENG_Device_GetParameterValues_GetData(amx->bus_ctx, objpath, pvsList);
                if(error != 0) {
                    goto stop;
                }
            }
        } else {
            SetErrorGotoStop(DM_ENG_INVALID_PARAMETER_NAME, "couldn't resolve object Path [%s]", internalPath);
        }
    } else {
        if(DM_ENG_Device_Common_IsWildcardPath(path) &&
           !DM_ENG_Device_Common_IsWildcardPathValid(path)) {
            SetErrorGotoStop(DM_ENG_INVALID_PARAMETER_NAME, "path format is not supported [%s]", path);
        }

        if(DM_ENG_Device_Common_IsRootParameter(path)) {
            error = DM_ENG_Device_GetParameterValues_GetRootParameter(amx, path, pvsList);
        } else {
            error = DM_ENG_Device_GetParameterValues_GetData(amx->bus_ctx, internalPath, pvsList);
        }
    }
stop:
    SAH_TRACEZ_OUT("DM_DA");
    amxc_var_clean(&objects);
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
    SAH_TRACEZ_IN("DM_DA");

    // check for duplicates
    for(j = 0; j < currentItem; j++) {
        if(( parameterList[currentItem]->parameterName != NULL) &&
           ( parameterList[j]->parameterName != NULL) &&
           ( strcmp(parameterList[currentItem]->parameterName, parameterList[j]->parameterName) == 0)) {
            SAH_TRACEZ_ERROR("DM_DA", "Duplicate parameter found");
            DM_ENG_addSetParameterValuesFault(faultsList,
                                              DM_ENG_newSetParameterValuesFault(parameterList[currentItem]->parameterName,
                                                                                DM_ENG_INVALID_ARGUMENTS)
                                              );
            (*nbFaults)++;
            ret = true;
            break;
        }
    }

    SAH_TRACEZ_OUT("DM_DA");
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
    const char* internalPath = NULL;
    amxc_var_t* parameters = NULL;
    amxd_path_t obj_path;
    amxc_var_t obj_desc;
    char* object_path = NULL;
    amxd_path_init(&obj_path, "");
    amxc_var_init(&obj_desc);

    SAH_TRACEZ_IN("DM_DA");

    internalPath = DM_ENG_Device_Common_ACSToAMXPath_noalloc(parameterList[*i]->parameterName);
    if(internalPath == NULL) {
        DM_ENG_addSetParameterValuesFault(faultsList, DM_ENG_newSetParameterValuesFault(parameterList[*i]->parameterName, DM_ENG_INVALID_PARAMETER_NAME));
        if(nbFaults) {
            (*nbFaults)++;
        }
        SetErrorGotoStop(DM_ENG_INVALID_ARGUMENTS, "Not a valid parameter Name");
    }

    amxd_path_setf(&obj_path, false, "%s", internalPath);
    object_path = strdup(amxd_path_get(&obj_path, AMXD_OBJECT_TERMINATE));

    if(amxd_path_is_search_path(&obj_path)) {
        // Not a supported path
        DM_ENG_addSetParameterValuesFault(faultsList, DM_ENG_newSetParameterValuesFault(parameterList[*i]->parameterName, DM_ENG_INVALID_PARAMETER_NAME));
        if(nbFaults) {
            (*nbFaults)++;
        }
        SetErrorGotoStop(DM_ENG_INVALID_ARGUMENTS, "Not a complete object path ");
    }

    // perform a describe on the parent object
    flags = AMXB_FLAG_PARAMETERS;
    amxb_ret = amxb_describe(amx->bus_ctx, (char*) amxd_path_get(&obj_path, AMXD_OBJECT_TERMINATE), flags, &obj_desc, 2);

    if((amxb_ret != 0) || amxc_var_is_null(&obj_desc)) {
        // Not a supported path
        DM_ENG_addSetParameterValuesFault(faultsList, DM_ENG_newSetParameterValuesFault(parameterList[*i]->parameterName, DM_ENG_INVALID_PARAMETER_NAME));
        if(nbFaults) {
            (*nbFaults)++;
        }
        SetErrorGotoStop(DM_ENG_INVALID_ARGUMENTS, "Object dosent exist ?");
    }
    // Here we have the object validate its parameters

    if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_VERIFYPARAMETERTYPE, &tempString) == 0) {
        if(tempString) {
            checkType = atoi(tempString) ? true : false;
            free(tempString);
        }
    }

    parameters = GETP_ARG(&obj_desc, "0.parameters");

    // check all parameters that belong to the current object
    while(strcmp(amxd_path_get(&obj_path, AMXD_OBJECT_TERMINATE), object_path) == 0) {

        amxc_var_t* parameter = GETP_ARG(parameters, amxd_path_get_param(&obj_path));

        // check param validity
        if(amxc_var_is_null(parameter)) {
            DM_ENG_addSetParameterValuesFault(faultsList, DM_ENG_newSetParameterValuesFault(parameterList[*i]->parameterName, DM_ENG_INVALID_PARAMETER_NAME));
            if(nbFaults) {
                (*nbFaults)++;
            }
            SetErrorGotoStop(DM_ENG_INVALID_PARAMETER_NAME, "Parameter Not found");
        }
        // check parameter Type
        if(checkType) {
            u_int32_t type_id = GET_INT32(parameter, "type_id");
            if(parameterList[*i]->type != DM_ENG_Device_Common_ConvertParameterType(type_id)) {
                DM_ENG_addSetParameterValuesFault(faultsList, DM_ENG_newSetParameterValuesFault(parameterList[*i]->parameterName, DM_ENG_INVALID_PARAMETER_TYPE));
                if(nbFaults) {
                    (*nbFaults)++;
                }
                SetErrorGotoStop(DM_ENG_INVALID_ARGUMENTS, "Wrong type");
            }
        }
        // check for duplicated entries duplicates
        if(DM_ENG_Device_SetParameterValues_CheckForDuplicates(parameterList, *i, faultsList, nbFaults)) {
            SetErrorGotoStop(DM_ENG_INVALID_ARGUMENTS, "Duplicated entry");
        }
        // check parameter attributes
        amxc_var_t* attributes = GETP_ARG(parameter, "attributes");

        if(amxc_var_is_null(attributes)) {
            DM_ENG_addSetParameterValuesFault(faultsList, DM_ENG_newSetParameterValuesFault(parameterList[*i]->parameterName, DM_ENG_INVALID_PARAMETER_TYPE));
            if(nbFaults) {
                (*nbFaults)++;
            }
            SetErrorGotoStop(DM_ENG_INVALID_PARAMETER_NAME, "Parameter Not found");
        }

        int is_read_only = GET_INT32(attributes, "read-only");

        if(is_read_only) {
            DM_ENG_addSetParameterValuesFault(faultsList, DM_ENG_newSetParameterValuesFault(parameterList[*i]->parameterName, DM_ENG_READ_ONLY_PARAMETER));
            if(nbFaults) {
                (*nbFaults)++;
            }
            SetErrorGotoStop(DM_ENG_INVALID_ARGUMENTS, "parameter is read-only");
        }

        /*
         * Few cases are not yet verified :
         *
         * •A parameter value or combination of parameter values that are explicitly prohibited
         * in the definition of the data model(s) supported by the CPE.
         * •A parameter value or combination of parameter values that are not supported by the
         * CPE and are not required by the data model(s) or profiles (as defined in [13]) supported by the CPE.
         * In both of the above cases, the response from the CPE MUST include a SetParameterValuesFault
         * element for each such parameter, indicating the Invalid Parameter Value fault code (9007).
         */

        // pass to next parameter
        (*i)++;
        if(parameterList[*i] == NULL) {
            break;// we reached the end

        }
        internalPath = DM_ENG_Device_Common_ACSToAMXPath_noalloc(parameterList[*i]->parameterName);
        if(internalPath == NULL) {
            DM_ENG_addSetParameterValuesFault(faultsList, DM_ENG_newSetParameterValuesFault(
                                                  parameterList[*i]->parameterName, DM_ENG_INVALID_PARAMETER_NAME));
            if(nbFaults) {
                (*nbFaults)++;
            }
            SetErrorGotoStop(DM_ENG_INVALID_ARGUMENTS, "Not a valid parameter Name");
        }
        amxd_path_clean(&obj_path);
        amxd_path_init(&obj_path, internalPath);
    }
    (*i)--; // restore i

stop:
    amxd_path_clean(&obj_path);
    amxc_var_clean(&obj_desc);
    free(object_path);
    SAH_TRACEZ_OUT("DM_DA");
    return error;
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
    const char* internalPath = NULL;
    amxd_path_t path;
    amxc_var_t set;
    amxc_var_t ret;

    char* object_path = NULL;
    amxc_var_init(&set);
    amxc_var_init(&ret);
    amxd_path_init(&path, "");
    SAH_TRACEZ_IN("DM_DA");

    internalPath = DM_ENG_Device_Common_ACSToAMXPath_noalloc(parameterList[*i]->parameterName);
    if(internalPath == NULL) {
        DM_ENG_addSetParameterValuesFault(faultsList, DM_ENG_newSetParameterValuesFault(
                                              parameterList[*i]->parameterName, DM_ENG_INVALID_PARAMETER_NAME));
        if(nbFaults) {
            (*nbFaults)++;
        }
        SetErrorGotoStop(DM_ENG_INVALID_ARGUMENTS, "Not a valid parameter Name");
    }
    amxd_path_clean(&path);
    amxd_path_init(&path, internalPath);

    object_path = strdup(amxd_path_get(&path, AMXD_OBJECT_TERMINATE));

    amxc_var_set_type(&set, AMXC_VAR_ID_HTABLE);

    // loop throught parameters that belong to the same object
    while(strcmp(amxd_path_get(&path, AMXD_OBJECT_TERMINATE), object_path) == 0) {
        DM_ENG_AddCachedValueToParameterAttributesCache(parameterList[*i]->parameterName, parameterList[*i]->value);
        // never care about type , ambiorix will do the job
        amxc_var_add_key(cstring_t, &set, amxd_path_get_param(&path), parameterList[*i]->value);
        // pass to next parameter
        (*i)++;
        if(parameterList[*i] == NULL) {
            break;// we reached the end of the table
        }

        internalPath = DM_ENG_Device_Common_ACSToAMXPath_noalloc(parameterList[*i]->parameterName);
        if(internalPath == NULL) {
            DM_ENG_addSetParameterValuesFault(faultsList, DM_ENG_newSetParameterValuesFault(
                                                  parameterList[*i]->parameterName, DM_ENG_INVALID_PARAMETER_NAME));
            if(nbFaults) {
                (*nbFaults)++;
            }
            SetErrorGotoStop(DM_ENG_INVALID_ARGUMENTS, "Not a valid parameter Name");
        }

        amxd_path_clean(&path);
        amxd_path_init(&path, internalPath);
    }
    (*i)--; // restore i

    rv = amxb_set(amx_env->bus_ctx, object_path, &set, &ret, 5);
    //amxc_var_dump(&ret,0);
    if(rv != 0) {
        SetErrorGotoStop(DM_ENG_INTERNAL_ERROR, "Not a valid parameter Name");
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

// /** @} */
