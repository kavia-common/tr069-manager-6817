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

/**
 * @file DM_DeviceAdapter.c
 *
 * This file implements routine declared into DM_ENG_Device.h header.
 *
 * @brief
 *
 **/
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <dmengine/DM_ENG_Common.h>
#include <dmengine/DM_ENG_TransferRequest.h>
#include <dmengine/DM_ENG_Device.h>
#include <dmengine/DM_ENG_RPCInterface.h>
#include <dmengine/DM_ENG_ParameterAttributesCache.h>
#include <dmengine/DM_ENG_InformMessageScheduler.h>
#include <dmengine/DM_ENG_Type.h>
#include <dmengine/DM_ENG_Fault.h>
#include <dmcommon/DM_GlobalDefs.h>

#include "DM_AmxCommon.h"
#include "DM_AmxSystemConnection.h"
#include "DM_AmxACSConnection.h"
#include "DM_DeviceAdapter.h"
#include "DM_AmxParameterNames.h"
#include "DM_AmxParameterValues.h"
#include "DM_AmxAddDelete.h"
#include "DM_AmxReset.h"
#include "DM_AmxUpDownload.h"
#include "DM_AmxScheduleDownload.h"
#include "DM_AmxChangeDUState.h"
#include "DM_AmxRPCInterface.h"
#include <debug/sahtrace.h>
#include <string.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <amxc/amxc_llist.h>
#include <amxc/amxc_string.h>
#include <amxc/amxc_variant.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

const char* DM_ENG_EVENT_VALUE_CHANGE = "4 VALUE CHANGE";

static dm_deviceadapter_t da;
static char* externalIPAddress = NULL;
static char* persistentRPCPath = NULL;

//----------------------------------------------------------------------------------
/**
   @defgroup sah_cwmp_amxdeviceadapter SoftAtHome cwmp ambiorix device adapter
   @{
   This on-line documentation contains a description of the cwmp ambiorix device adapter functionality
   and a description of the implementation.

   @section sah_cwmp_sah_cwmp_amxdeviceadapter_purpose Purpose
   The device adapter layer is an abstraction layer between the tr069 engine, which contains the tr069 logic
   and the datamodel. In this case this is a backend written to interface with a ambiorix datamodel.

   @section sah_cwmp_amxdeviceadapter_architecture AMX device adapter architecture
   When using ambiorix as a backend along with a supported system bus (ubus in this case),
   CWMP will be integrated on an embedded system as illustrated below.

   @image html cwmp-pcbinteraction.png "CWMP amxinteraction"

   In the above drawing:
   - The system bus contains the complete datamodel and functionality
   - The mapper is a 'view' on the system bus, it only contains mapping logic described in the odl format

   The CWMP AMX backend has two interfaces to AMX:
   - The system connection: This will be used for accessing the system bus directly for internal use e.g. to get "ManagementServer" parameters, trigger a reset,...
   - The ACS connection: This will be used for all datamodel data that will be transferred to the ACS

   The main goal of the mapper is to have an abstraction interface for each project/customer.

   The source code is structured in the following way:
   - DM_DeviceAdapter.c contains all the public functions that are exposed to dm_engine
   - DM_AMXCommon.c contains some generic functions
   - DM_AMXACSConnection.c and DM_AMXSystemConnection.c setup/handle/close the 2 interfaces to ambiorix
   - all other .c files contain specific TR069 RPC code on ambiorix

   @}
   @ingroup sah_cwmp
 */

/**
 * @addtogroup sah_cwmp_amxdeviceadapter
 * @{
 */

/**
 * Performs the necessary initializations of the device adapter if any, when starting the DM Agent.
 */
bool DM_ENG_Device_Init(void** systemCtx, void** acsCtx, const char* rpcPath, const char* aclfile, const char* vendorPrefix, const char* backend, const char* uri) {
    bool result;
    char* tmp = NULL;
    char* instance_alias = NULL;

    da.data.vendor_prefix = vendorPrefix;
    result = DM_ENG_Device_SystemConnectionInitialize(&da.system, backend, uri);
    if(result == false) {
        SAH_TRACEZ_ERROR("DM_DA", "Error initializing System-bus ");
        return false;
    }

    /* depend on TR version : IGD or Device.*/

    da.acs.prefix = DM_ENG_getDatamodelPrefix();

    result = DM_ENG_Device_ACSConnectionInitialize(&da.acs, backend, uri);
    if(result == false) {
        SAH_TRACEZ_ERROR("DM_DA", "Error initializing ACS system");
        return false;
    }
    *systemCtx = (void*) (da.system.bus_ctx);
    *acsCtx = (void*) (da.acs.bus_ctx);

    if(!rpcPath) {
        SAH_TRACEZ_WARNING("DM_DA", "rpcPath is not set , not blocking !!!");
    } else {
        persistentRPCPath = strdup(rpcPath);
    }

    if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_INSTANCEMODE, &instance_alias) != 0) {
        SAH_TRACEZ_ERROR("DM_DA", "Cannot fetch the DM_ENG_INSTANCEMODE param");
    }

    if(instance_alias && (strcmp(instance_alias, "InstanceAlias") == 0)) {
        da.acs.instanceAlias = true;
    } else {
        da.acs.instanceAlias = false;
    }

    if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_AUTOCREATEINSTANCES, &tmp) != 0) {
        SAH_TRACEZ_ERROR("DM_DA", "Cannot fetch the DM_ENG_AUTOCREATEINSTANCES param");
    }
    if(tmp) {
        da.acs.autoCreateInstances = atoi(tmp) ? true : false;
        free(tmp);
    } else {
        da.acs.autoCreateInstances = true;
    }

    da.acs.acl = aclfile;
    da.acs.acl_rules = amxa_parse_files(aclfile);

    free(instance_alias);
    return result;
}

/**
 * Performs the necessary operations when stopping the DM Agent.
 */
bool DM_ENG_Device_Release() {
    bool result = true;
    SAH_TRACEZ_IN("DM_DA");

    if(DM_AmxScheduleDownload_clean() != 0) {
        SAH_TRACEZ_ERROR("DM_DA", "Failed to clean AMX ScheduleDownload");
    }

    if(DM_AmxChangeDUState_clean() != 0) {
        SAH_TRACEZ_ERROR("DM_DA", "Failed to clean AMX ChangeDUState");
    }

    if(DM_AmxRPCInterface_clean() != 0) {
        SAH_TRACEZ_ERROR("DM_DA", "Failed to clean AMX RPCInterface");
    }

    DM_ENG_Device_ACSConnectionCleanup(&da.acs);
    DM_ENG_Device_SystemConnectionCleanup(&da.system);
    free(persistentRPCPath);
    persistentRPCPath = NULL;
    SAH_TRACEZ_OUT("DM_DA");
    return result;
}


/**
 * Notify the Device Adapter of the beginning of a session with the ACS. It allows to perform the necessary initializations.
 */
void DM_ENG_Device_OpenSession() {
    char* paramName = NULL;
    char* ipAddress = NULL;

    SAH_TRACEZ_INFO("DM_DA", "Call to DM_ENG_Device_openSession");

    //TODO! Find external ip and wan device

    if(( da.acs.prefix != NULL) && ( strcmp(da.acs.prefix, "InternetGatewayDevice.") == 0)) {
        char* addressFound = NULL;
        SAH_TRACEZ_INFO("DM_DA", "IGD model, updating the wandevice path for forced parameters");
        //construct the new path - may be NULL
        if(DM_ENG_Device_GetConfigValue(DM_ENG_CONNECTIONREQUESTHOST, &ipAddress) == -1) {
            SAH_TRACEZ_ERROR("DM_DA", "Cannot get the connection request host");
            goto error;
        }
        //TODO! Handle Find wan device
        SAH_TRACEZ_INFO("DM_DA", "returned %s value %s",
                        (paramName != NULL) ? paramName : "nil",
                        (addressFound != NULL) ? addressFound : "nil");

        free(ipAddress);
        externalIPAddress = addressFound;
    } else if(( da.acs.prefix != NULL) && ( strcmp(da.acs.prefix, "Device.") == 0)) {

        SAH_TRACEZ_INFO("DM_DA", "Device model, updating the device.ip path for forced parameters");
        //construct the new path - may be NULL
        if(DM_ENG_Device_GetConfigValue(DM_ENG_CONNECTIONREQUESTHOST, &ipAddress) == -1) {
            SAH_TRACEZ_ERROR("DM_DA", "Cannot get the connection request host");
            goto error;
        }
        if(DM_ENG_Device_ACSConnectionHandleGetwaninterface(&da.system, ipAddress, &paramName)) {
            SAH_TRACEZ_INFO("DM_DA", "WAN Interface: path=[%s] and IP Address=[%s]", paramName, ipAddress);
            DM_ENG_TR181SetExternalIPAddressPath(paramName);

            // reset the cached value to make sure a value change event is send out if the external ip address would have changed
            DM_ENG_AddCachedValueToParameterAttributesCache(paramName, ipAddress);
        }
        free(externalIPAddress);
        externalIPAddress = ipAddress;
    }
error:
    return;
}

/**
 * Notify the Device Adapter of the end of the session with the ACS.
 */
void DM_ENG_Device_CloseSession() {
    //does nothing!!
}

static const char* amx_type_to_xml_type (uint32_t type) {
    switch(type) {
    case AMXC_VAR_ID_INT8:
    case AMXC_VAR_ID_INT16:
    case AMXC_VAR_ID_INT32:
    case AMXC_VAR_ID_INT64:
        return "xsd:int";
    case AMXC_VAR_ID_UINT8:
    case AMXC_VAR_ID_UINT16:
    case AMXC_VAR_ID_UINT32:
    case AMXC_VAR_ID_UINT64:
        return "xsd:unsignedInt";
    case AMXC_VAR_ID_CSTRING:
    case AMXC_VAR_ID_CSV_STRING:
    case AMXC_VAR_ID_SSV_STRING:
        return "xsd:string";
    case AMXC_VAR_ID_TIMESTAMP:
        return "xsd:dateTime";
    case AMXC_VAR_ID_BOOL:
        return "xsd:boolean";
    default:
        return "xsd:any";
    }
}

static xmlNodePtr xml_add_parameter_value_struct(xmlNodePtr node, const char* name, const char* value, const char* type) {
    xmlNodePtr pvs_node = NULL;
    xmlNodePtr value_node = NULL;
    pvs_node = xmlNewChild(node, NULL, BAD_CAST "ParameterValueStruct", NULL);
    xmlNewChild(pvs_node, NULL, BAD_CAST "Name", BAD_CAST name);
    value_node = xmlNewChild(pvs_node, NULL, BAD_CAST "Value", BAD_CAST(value ? value:""));
    xmlNewProp(value_node, BAD_CAST "xsi:type", BAD_CAST type);
    return pvs_node;
}

static int build_gpv_all_body(dm_amx_env_t* acs_info, UNUSED xmlNodePtr node_body, amxc_var_t* get_result) {
    SAH_TRACEZ_IN("DM_DA");
    int error = 0;
    const char* path = "Device.";
    const amxc_htable_t* root_htable = NULL;
    amxc_array_t* root_keys = NULL;
    const amxc_htable_t* param_htable = NULL;
    amxc_var_t* entry = NULL;
    const char* key = NULL;
    amxc_var_t desc_result;
    xmlNodePtr rsp_node = NULL;
    xmlNodePtr plist_node = NULL;
    amxc_string_t pvs_node_attr_value;
    uint32_t param_count = 0;
    amxc_var_t* pm_var = NULL;
    char* supported_path = NULL;
    amxc_string_t supported_path_inst;
    amxd_path_t obj_path;

    rsp_node = xmlNewChild(node_body, NULL, BAD_CAST "cwmp:GetParameterValuesResponse", NULL);
    plist_node = xmlNewChild(rsp_node, NULL, BAD_CAST "ParameterList", NULL);

    root_htable = amxc_var_constcast(amxc_htable_t, GETI_ARG(get_result, 0));
    root_keys = amxc_htable_get_sorted_keys(root_htable);

    for(uint32_t i = 0; i < amxc_array_capacity(root_keys); i++) {
        amxc_var_init(&desc_result);

        key = (const char*) amxc_array_it_get_data(amxc_array_get_at(root_keys, i));
        entry = amxc_var_from_htable_it(amxc_htable_get(root_htable, key));
        param_htable = amxc_var_constcast(amxc_htable_t, entry);
        if(amxc_htable_is_empty(param_htable)) {
            continue;
        }
        SAH_TRACEZ_INFO("DM_DA", "Key [%s]", key);
        if(amxb_get_supported(acs_info->bus_ctx, key, AMXB_FLAG_PARAMETERS, &desc_result, 1)) {
            SAH_TRACEZ_WARNING("DM_DA", "get_supported failed [%s]", key);
            continue;
        }

        amxd_path_init(&obj_path, NULL);
        amxd_path_setf(&obj_path, false, "%s", key);
        supported_path = amxd_path_build_supported_path(&obj_path);

        amxc_string_init(&supported_path_inst, 0);
        amxc_string_setf(&supported_path_inst, "%s{i}.", supported_path);

        pm_var = amxc_var_get_key(GETI_ARG(&desc_result, 0), amxc_string_get(&supported_path_inst, 0), AMXC_VAR_FLAG_DEFAULT);
        if(pm_var == NULL) {
            pm_var = amxc_var_get_key(GETI_ARG(&desc_result, 0), supported_path, AMXC_VAR_FLAG_DEFAULT);
        }

        amxc_array_t* param_keys = amxc_htable_get_sorted_keys(param_htable);
        for(uint32_t i = 0; i < amxc_array_capacity(param_keys); i++) {
            char* alias_path = NULL;
            char* param_value = NULL;
            uint32_t param_type = 0;
            const char* param_key = (const char*) amxc_array_it_get_data(amxc_array_get_at(param_keys, i));
            amxc_htable_it_t* hit = amxc_htable_get(param_htable, param_key);
            amxc_var_t* param_var = amxc_var_from_htable_it(hit);
            amxc_string_t param_name;

            const amxc_llist_t* llist = amxc_var_constcast(amxc_llist_t, GET_ARG(pm_var, "supported_params"));
            amxc_llist_iterate(lit, llist) {
                amxc_var_t* parameter = amxc_var_from_llist_it(lit);
                if(strcmp(GETP_CHAR(parameter, "param_name"), param_key) == 0) {
                    param_type = GET_INT32(parameter, "type");
                    break;
                }
            }

            param_value = amxc_var_dyncast(cstring_t, param_var);
            amxc_string_init(&param_name, 0);
            amxc_string_setf(&param_name, "%s%s", key, param_key);
            if(acs_info->instanceAlias) {
                DM_ENG_Device_Common_IndexToAlias(acs_info, path, amxc_string_get(&param_name, 0), &alias_path);
            }
            SAH_TRACEZ_INFO("DM_DA", "Name [%s] - Value [%s] - Type [%d]", alias_path != NULL ? alias_path : amxc_string_get(&param_name, 0), param_value, param_type);
            xml_add_parameter_value_struct(plist_node, alias_path != NULL ? alias_path : amxc_string_get(&param_name, 0), param_value, amx_type_to_xml_type(param_type));
            param_count++;

            error = 0;
            amxc_string_clean(&param_name);
            free(param_value);
            free(alias_path);
        }
        amxc_array_delete(&param_keys, NULL);
        param_keys = NULL;
        amxd_path_clean(&obj_path);
        free(supported_path);
        amxc_string_clean(&supported_path_inst);
        amxc_var_clean(&desc_result);
    }

    amxc_string_init(&pvs_node_attr_value, 0);
    amxc_string_appendf(&pvs_node_attr_value, "cwmp:ParameterValueStruct[%d]", param_count);
    xmlNewProp(plist_node, BAD_CAST "soap-enc:arrayType", BAD_CAST amxc_string_get(&pvs_node_attr_value, 0));
    amxc_string_clean(&pvs_node_attr_value);

    amxc_array_delete(&root_keys, NULL);
    SAH_TRACEZ_OUT("DM_DA");
    return error;
}

int DM_ENG_Device_Set(const char* object, amxc_var_t* values, amxc_var_t* ret) {
    int retval = -1;
    dm_amx_env_t* system_info = NULL;
    if((values == NULL) || (ret == NULL)) {
        SAH_TRACEZ_ERROR("DM_DA", "Invalid arg(s)");
        goto stop;
    }
    system_info = DM_ENG_Device_GetSystemInfo();
    retval = amxb_set(system_info->bus_ctx, object, values, ret, 5);
stop:
    return retval;
}

int DM_ENG_Device_GetMapping(amxc_var_t* data) {
    int retval = -1;
    dm_amx_env_t* system_info = NULL;
    amxc_var_t get_result;
    amxc_var_init(&get_result);

    SAH_TRACEZ_IN("DM_DA");
    if(data == NULL) {
        SAH_TRACEZ_ERROR("DM_DA", "Invalid arg(s)");
        goto stop;
    }

    system_info = DM_ENG_Device_GetSystemInfo();
    SAH_TRACEZ_INFO("DM_DA", "Getting All Mappings");
    if(amxb_get(system_info->bus_ctx, "ManagementServer.Mapping.", 0, &get_result, 10) != AMXB_STATUS_OK) {
        SAH_TRACEZ_ERROR("DM_DA", "Could not get Mapping Object");
        goto stop;
    }
    amxc_var_copy(data, &get_result);
    retval = 0;
stop:
    amxc_var_clean(&get_result);
    SAH_TRACEZ_OUT("DM_DA");
    return retval;
}

int DM_ENG_Device_GetParameterValuesAll(xmlNodePtr node_body) {
    SAH_TRACEZ_IN("DM_DA");
    int error = 0;
    int ret = 0;
    amxc_llist_t filters;
    amxc_llist_init(&filters);
    const char* path = "Device.";
    amxc_var_t get_result;
    amxc_var_init(&get_result);
    dm_amx_env_t* acs_info = DM_ENG_Device_GetACSInfo();

    amxa_resolve_search_paths(acs_info->bus_ctx, acs_info->acl_rules, path);
    amxa_get_filters(acs_info->acl_rules, AMXA_PERMIT_GET, &filters, path);
    if(!amxa_is_get_allowed(&filters, path)) {
        SetErrorGotoStop(DM_ENG_INVALID_PARAMETER_NAME, "cwmp has no access rights to [%s] ", path);
    }
    ret = amxb_get(acs_info->bus_ctx, path, -1, &get_result, 10);
    if((ret != AMXB_STATUS_OK) || amxc_var_is_null(&get_result)) {
        if((ret == AMXB_ERROR_NOT_SUPPORTED_SCHEME)) {
            SetErrorGotoStop(0, "Skip non tr181-component, path [%s] %d", path, ret);
        } else {
            SetErrorGotoStop(DM_ENG_INVALID_PARAMETER_NAME, "Failed to get object path [%s] error [%d] ", path, ret);
        }
    }
    amxa_filter_get_resp(&get_result, &filters);
    build_gpv_all_body(acs_info, node_body, &get_result);
stop:
    SAH_TRACEZ_OUT("DM_DA");
    amxc_llist_clean(&filters, amxc_string_list_it_free);
    amxc_var_clean(&get_result);
    return error;
}

static xmlNodePtr xml_add_parameter_attribute_struct(xmlNodePtr node, const char* name, DM_ENG_NotificationMode nm, char** al) {
    char notification[2];
    xmlNodePtr pas_node = NULL;
    xmlNodePtr al_node = NULL;
    uint32_t al_count = 0;
    amxc_string_t al_node_attr_value;
    amxc_string_init(&al_node_attr_value, 0);
    pas_node = xmlNewChild(node, NULL, BAD_CAST "ParameterAttributeStruct", NULL);
    xmlNewChild(pas_node, NULL, BAD_CAST "Name", BAD_CAST name);
    sprintf(notification, "%u", nm);
    xmlNewChild(pas_node, NULL, BAD_CAST "Notification", BAD_CAST notification);
    al_node = xmlNewChild(pas_node, NULL, BAD_CAST "AccessList", BAD_CAST "");
    if(al) {
        while(al[al_count] != NULL) {
            xmlNewChild(al_node, NULL, BAD_CAST "string", BAD_CAST al[al_count]);
            al_count++;
        }
    }
    amxc_string_appendf(&al_node_attr_value, "string[%d]", al_count);
    xmlNewProp(al_node, BAD_CAST "soap-enc:arrayType", BAD_CAST amxc_string_get(&al_node_attr_value, 0));
    amxc_string_clean(&al_node_attr_value);
    return pas_node;
}

static int build_gpa_all_body(dm_amx_env_t* acs_info, xmlNodePtr node_body, amxc_var_t* get_result) {
    SAH_TRACEZ_IN("DM_DA");
    int error = 0;
    const char* path = "Device.";
    const amxc_htable_t* htable = NULL;
    const char* key = NULL;
    xmlNodePtr rsp_node = NULL;
    xmlNodePtr plist_node = NULL;
    amxc_string_t pvs_node_attr_value;
    uint32_t param_count = 0;
    amxc_string_init(&pvs_node_attr_value, 0);
    amxc_htable_t cache_dict;
    DM_ENG_CacheDict_Build(&cache_dict);
    rsp_node = xmlNewChild(node_body, NULL, BAD_CAST "cwmp:GetParameterAttributesResponse", NULL);
    plist_node = xmlNewChild(rsp_node, NULL, BAD_CAST "ParameterList", NULL);
    amxc_var_for_each(entry, GETI_ARG(get_result, 0)) {
        htable = amxc_var_constcast(amxc_htable_t, entry);
        if(amxc_htable_is_empty(htable)) {
            continue;
        }
        key = amxc_var_key(entry);
        SAH_TRACEZ_INFO("DM_DA", "Key [%s]", key);
        amxc_htable_iterate(hit, htable) {
            DM_ENG_NotificationMode nm;
            char** al;
            char* alias_path = NULL;
            const char* param_key = amxc_htable_it_get_key(hit);
            amxc_string_t param_name;
            amxc_string_init(&param_name, 0);
            amxc_string_setf(&param_name, "0.parameters.%s", param_key);
            amxc_string_clean(&param_name);
            amxc_string_init(&param_name, 0);
            amxc_string_setf(&param_name, "%s%s", key, param_key);
            if(acs_info->instanceAlias) {
                DM_ENG_Device_Common_IndexToAlias(acs_info, path, amxc_string_get(&param_name, 0), &alias_path);
            }
            SAH_TRACEZ_INFO("DM_DA", "Name [%s]", alias_path != NULL ? alias_path : amxc_string_get(&param_name, 0));
            DM_ENG_CacheDict_GetInfo(&cache_dict, amxc_string_get(&param_name, 0), &nm, &al);
            if(nm == DM_ENG_NotificationMode_FORCED) {
                nm = DM_ENG_NotificationMode_ACTIVE;
            }
            xml_add_parameter_attribute_struct(plist_node, alias_path != NULL ? alias_path : amxc_string_get(&param_name, 0), nm, al);
            param_count++;

            error = 0;
            amxc_string_clean(&param_name);
            free(alias_path);
        }
    }
    amxc_string_appendf(&pvs_node_attr_value, "cwmp:ParameterAttributeStruct[%d]", param_count);
    xmlNewProp(plist_node, BAD_CAST "soap-enc:arrayType", BAD_CAST amxc_string_get(&pvs_node_attr_value, 0));
    amxc_string_clean(&pvs_node_attr_value);
    DM_ENG_CacheDict_Destroy(&cache_dict);
    SAH_TRACEZ_OUT("DM_DA");
    return error;
}

int DM_ENG_Device_GetParameterAttributesAll(xmlNodePtr node_body) {
    SAH_TRACEZ_IN("DM_DA");
    int error = 0;
    int ret = 0;
    amxc_llist_t filters;
    amxc_llist_init(&filters);
    const char* path = "Device.";
    amxc_var_t get_result;
    amxc_var_init(&get_result);
    dm_amx_env_t* acs_info = DM_ENG_Device_GetACSInfo();
    amxa_resolve_search_paths(acs_info->bus_ctx, acs_info->acl_rules, path);
    amxa_get_filters(acs_info->acl_rules, AMXA_PERMIT_GET, &filters, path);
    if(!amxa_is_get_allowed(&filters, path)) {
        SetErrorGotoStop(DM_ENG_INVALID_PARAMETER_NAME, "cwmp has no access rights to [%s] ", path);
    }
    ret = amxb_get(acs_info->bus_ctx, path, -1, &get_result, 10);
    if((ret != AMXB_STATUS_OK) || amxc_var_is_null(&get_result)) {
        if((ret == AMXB_ERROR_NOT_SUPPORTED_SCHEME)) {
            SetErrorGotoStop(0, "Skip non tr181-component, path [%s] %d", path, ret);
        } else {
            SetErrorGotoStop(DM_ENG_INVALID_PARAMETER_NAME, "Failed to get object path [%s] error [%d] ", path, ret);
        }
    }
    amxa_filter_get_resp(&get_result, &filters);
    build_gpa_all_body(acs_info, node_body, &get_result);
stop:
    SAH_TRACEZ_OUT("DM_DA");
    amxc_llist_clean(&filters, amxc_string_list_it_free);
    amxc_var_clean(&get_result);
    return error;
}

/*********************************** getparametvalues RPC *****************************************/

/**
 * @brief Gets the parameter value from system level.
 *
 * @param parameterNames Array of the parameter names
 * @param pvsList The resulting parameter value struct list
 *
 * @return Returns 0 (zero) if OK or a fault code (9002, ...) according to the TR-069.
 */
int DM_ENG_Device_GetParameterValues(char* parameterNames[], DM_ENG_ParameterValueStruct** pvsList) {
    unsigned int error = 0;

    if(DM_ENG_Device_Common_CheckSystem(&da.acs) == false) {
        SetErrorGotoStop(DM_ENG_INTERNAL_ERROR, "Internal System error");
    }

    for(unsigned int i = 0; parameterNames[i] != NULL; i++) {
        SAH_TRACEZ_INFO("DM_DA", "Getting values for parameter %s", parameterNames[i]);
        error = DM_ENG_Device_GetParameterValues_GetValues(&da.acs, parameterNames[i], pvsList);
        if(error) {
            GotoStop("Error detected, stopping");
        }
    }

stop:
    if(error != 0) {
        DM_ENG_deleteAllParameterValueStruct(pvsList);
    }
    return error;
}

/*********************************** getinformparametervalues RPC *****************************************/

static bool DM_ENG_Device_MatchingEvent(const csv_string_t dmEvents, DM_ENG_EventStruct* eventList) {
    bool ret = false;
    amxc_string_t dmEventsStr;
    amxc_var_t dmEventsList;

    /* In case no events are seen in the dm */
    if(!dmEvents || !*dmEvents) {
        /* If the current event is a single “4 VALUE CHANGE” - we have no match */
        if((0 == strcmp(eventList->eventCode, DM_ENG_EVENT_VALUE_CHANGE)) && !eventList->next) {
            goto exit;
        }
        /* For all other cases this is a match */
        ret = true;
        goto exit;
    }

    /* convert to amxc_string_t */
    amxc_string_init(&dmEventsStr, 0);
    amxc_string_setf(&dmEventsStr, "%s", dmEvents);

    /* split into a list of strings */
    amxc_var_init(&dmEventsList);
    amxc_string_csv_to_var(&dmEventsStr, &dmEventsList, NULL);

    /* check or there is a datamodel event that is part of the current eventList */
    amxc_var_for_each(dmEvent, &dmEventsList) {
        const char* e = amxc_var_constcast(cstring_t, dmEvent);
        if(DM_ENG_InformMessageScheduler_containsEvent(eventList, e, NULL)) {
            ret = true;
            goto exit;
        }
    }
exit:
    amxc_string_clean(&dmEventsStr);
    amxc_var_clean(&dmEventsList);
    return ret;
}

static bool DM_ENG_Device_MatchingInterval(uint32_t interval) {
    bool ret = false;

    /* when interval=0 this restriction will be ignored -> always matched */
    if(interval == 0) {
        return true;
    }
    char* periodicInformInterval = NULL;
    if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_PERIODICINFORMINTERVAL, &periodicInformInterval) != 0) {
        GotoStop("Cannot fetch the periodic inform interval");
    }
    when_str_empty(periodicInformInterval, stop);

    uint32_t pi = (uint32_t) atoi(periodicInformInterval);
    ret = (pi == interval);
stop:
    free(periodicInformInterval);
    return ret;
}

static bool DM_ENG_Device_MatchingCount(uint32_t count) {
    bool ret = false;

    /* when count=0 this restriction will be ignored -> always matched */
    if(count == 0) {
        return true;
    }
    char* periodicInformCounter = NULL;
    if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_PERIODICINFORMCOUNTER, &periodicInformCounter) != 0) {
        GotoStop("Cannot fetch the periodic inform counter");
    }
    when_str_empty(periodicInformCounter, stop);

    uint32_t counter = (uint32_t) atoi(periodicInformCounter);
    counter++; // we need to increase by 1 here, as we need to find a match for the current inform session
    ret = (counter % count == 0);
stop:
    return ret;
}

/**
 * @brief Gets the inform parameter value list from system level.
 *
 * @param eventsList Array of the events that are available, needed to clear the condition.
 * @param pvsList The resulting parameter value struct list
 *
 * @return Returns 0 (zero) if OK or a fault code (9002, ...) according to the TR-069.
 */
int DM_ENG_Device_GetInformParameterValues(DM_ENG_EventStruct* eventList, DM_ENG_ParameterValueStruct** pvsList) {
    unsigned int error = 0;
    dm_amx_env_t* amx = &da.system;
    DM_ENG_ParameterValueStruct** pResult = NULL;
    char* paramArray[2];

    amxd_path_t searchPath;
    amxd_path_init(&searchPath, NULL);
    amxc_var_t values;
    amxc_var_init(&values);
    amxc_string_t path;
    amxc_string_init(&path, 0);
    amxc_var_t search;
    amxc_var_init(&search);

    int retcode = amxb_get(amx->bus_ctx, INFORMPARAMETER_PATH, 0, &values, 5);
    if((retcode != 0) || amxc_var_is_null(&values)) {
        SetErrorGotoStop(DM_ENG_INTERNAL_ERROR, "Failed to get '%s'", INFORMPARAMETER_PATH);
    }

    amxc_var_t* rvalues = GETI_ARG(&values, 0);
    amxc_var_for_each(value, rvalues) {
        /* match eventList, Interval and Count */
        const csv_string_t el = GET_CHAR(value, "EventList");
        if(!DM_ENG_Device_MatchingEvent(el, eventList)) {
            continue;
        }
        uint32_t interval = GET_UINT32(value, "Interval");
        if(!DM_ENG_Device_MatchingInterval(interval)) {
            continue;
        }
        uint32_t count = GET_UINT32(value, "Count");
        if(!DM_ENG_Device_MatchingCount(count)) {
            continue;
        }

        const char* parameterName = GET_CHAR(value, "ParameterName");
        if(parameterName == NULL) {
            continue;
        }
        amxd_path_clean(&searchPath);
        amxd_path_init(&searchPath, parameterName);
        if(amxd_path_is_search_path(&searchPath)) {
            amxc_var_clean(&search);
            retcode = amxb_get(amx->bus_ctx, parameterName, 0, &search, 5);
            if((retcode != 0) || amxc_var_is_null(&search)) {
                SAH_TRACEZ_WARNING("DM_DA", "Failed to get '%s'", parameterName);
                continue;
            }
            amxc_var_t* rsearch = GETI_ARG(&search, 0);
            amxc_var_for_each(s, rsearch) {
                amxc_var_for_each(kv, s) {
                    amxc_string_clean(&path);
                    amxc_string_setf(&path, "%s%s", amxc_var_key(s), amxc_var_key(kv));
                    paramArray[0] = (char*) amxc_string_get(&path, 0);
                    paramArray[1] = NULL;
                    DM_ENG_GetParameterValues(DM_ENG_EntityType_SYSTEM,
                                              paramArray,
                                              &pResult);
                    if(pResult) {
                        DM_ENG_addParameterValueStruct(pvsList, pResult[0]);
                        free(pResult);
                        pResult = NULL;
                    }
                }
            }
        } else {
            if(amxd_path_get_param(&searchPath) == NULL) {
                SAH_TRACEZ_ERROR("DM_DA", "Failed to get '%s' - invalid path", parameterName);
                continue;
            }
            paramArray[0] = (char*) parameterName;
            paramArray[1] = NULL;
            DM_ENG_GetParameterValues(DM_ENG_EntityType_SYSTEM,
                                      paramArray,
                                      &pResult);
            if(pResult) {
                DM_ENG_addParameterValueStruct(pvsList, pResult[0]);
                free(pResult);
                pResult = NULL;
            }
        }
    }

stop:
    amxc_string_clean(&path);
    amxd_path_clean(&searchPath);
    amxc_var_clean(&search);
    amxc_var_clean(&values);
    if(error != 0) {
        DM_ENG_deleteAllParameterValueStruct(pvsList);
    }
    return error;
}

/*********************************** getparameternames RPC *****************************************/

/**
 * @brief Get the parameter names
 *
 * @param path Parameter path
 * @param nextLevel If true, the response must only contains the parameters in the next level. If false, the response must also
 * contains the parameter below.
 * @param infoList Pointer to the array of the ParameterInfoStruct.
 *
 * @return Returns 0 (zero) if OK or a fault code (9002, ...) according to the TR-069.
 */
int DM_ENG_Device_GetParameterNames(char* path, bool nextLevel, DM_ENG_ParameterInfoStruct** infoList) {
    unsigned int error = 0;
    int pathLength = 0;

    if(DM_ENG_Device_Common_CheckSystem(&da.acs) == false) {
        SetErrorGotoStop(DM_ENG_INTERNAL_ERROR, "System error");
    }

    SAH_TRACEZ_INFO("DM_DA", "Getting parameter names for [%s] (nextlevel=%d)", path, nextLevel);

    if(path != NULL) {
        pathLength = strlen(path);
    }

    if((pathLength == 0) && ( nextLevel == true)) {
        /* A.3.2.3: Or, if ParameterPath were empty, with NextLevel equal true, the response would list
                     only “InternetGatewayDevice.” (if the CPE is an Internet Gateway Device). */
        DM_ENG_ParameterInfoStruct* dmis = NULL;
        dmis = DM_ENG_newParameterInfoStruct(da.acs.prefix, false);
        DM_ENG_addParameterInfoStruct(infoList, dmis);
    } else if(( pathLength == 0) || ( path[pathLength - 1] == '.') || ( path[pathLength - 1] == '*')) {
        // Paths that ends with '*' are part of the standard but we support them any way :)
        error = DM_ENG_Device_GetParameterNames_PartialPath(&da.acs, path, nextLevel, infoList);
    } else {
        if(nextLevel == true) {
            SetErrorGotoStop(DM_ENG_INVALID_ARGUMENTS, "Return an error if we request the nextlevel names of a parameter");
        }
        error = DM_ENG_Device_GetParameterNames_Parameter(&da.acs, path, infoList);
    }

stop:
    if(error != 0) {
        DM_ENG_deleteAllParameterInfoStruct(infoList);
    }

    return error;
}

/*********************************** setparametervalues RPC *****************************************/

/**
 * @brief Sets the parameter values
 *
 * @param parameterList Array of name-value pairs as specified. For each name-value pair, the CPE is instructed
 * to set the Parameter specified by the name to the corresponding value.
 *
 * @param parameterKey The value to set the ParameterKey parameter. This MAY be used by the server to identify
 * Parameter updates, or left empty.
 *
 * @param pStatus Pointer to get the status defined as follows :
 * - DM_ENG_ParameterStatus_APPLIED (=0) : Parameter changes have been validated and applied.
 * - DM_ENG_ParameterStatus_READY_TO_APPLY (=1) : Parameter changes have been validated and committed, but not yet applied
 * (for example, if a reboot is required before the new values are applied).
 * - DM_ENG_ParameterStatus_UNDEFINED : Undefined status (for example, parameterList is empty).
 *
 *  Note : The result is DM_ENG_ParameterStatus_APPLIED if all the parameter changes are APPLIED.
 * The result is DM_ENG_ParameterStatus_READY_TO_APPLY if one or more parameter changes is READY_TO_APPLY.
 *
 * @param faultsList Pointer to get the faults struct if any, as specified in TR-069
 *
 * @return Returns 0 (zero) if OK or a fault code (9002, ...) according to the TR-069.
 */
int DM_ENG_Device_SetParameterValues(DM_ENG_ParameterValueStruct* parameterList[],
                                     UNUSED const char* parameterKey,
                                     DM_ENG_ParameterStatus* pStatus,
                                     DM_ENG_SetParameterValuesFault** faultsList) {
    int i = 0;
    int len = 0;
    int error = 0;
    int suberror = 0;
    int nbFaults = 0;

    if(DM_ENG_Device_Common_CheckSystem(&da.acs) == false) {
        SetErrorGotoStop(DM_ENG_INTERNAL_ERROR, "System error");
    }

    len = DM_ENG_tablen((void**) parameterList);

    // The parameter list is always grouped per object
    // Check for duplicates and validate values + objects
    for(i = 0; i < len; i++) {
        suberror = DM_ENG_Device_SetParameterValues_Validate(&da.acs, parameterList, &i, faultsList, &nbFaults);
        if(error == 0) {
            error = suberror;
        }
    }

    if(error) {
        SAH_TRACEZ_ERROR("DM_DA", "Validation failed, returning error %d", error);
        goto stop;
    }

    // Apply the values, validation was ok,
    // This part should not fail, in the case it fails, we just return an error
    // and undo the changes already applied.
    for(i = 0; i < len; i++) {
        error = DM_ENG_Device_SetParameterValues_SetValues(&da.acs, parameterList, &i, faultsList, &nbFaults);
    }

    /*
     * A successful response to this method returns an integer enumeration defined as follows:
     *
     * 0 =  All Parameter changes have been validated and applied.
     *
     * 1 =  All Parameter changes have been validated and committed,
     * but some or all are not yet applied (for example, if a reboot
     * is required before the new values are applied).
     */
    if(error == 0) {
        // TODO : set status according to tr69 standard
        // here we assume we dont need reboot to apply the new
        // parameter value
        *pStatus = DM_ENG_ParameterStatus_APPLIED;
    } else {
        *pStatus = DM_ENG_ParameterStatus_UNDEFINED;
    }

stop:
    return error;
}

/*********************************** addobject RPC **************************************************/


/**
 * @brief Adds an object as specified in the TR-069.
 * @param objectName Name of an object prototype
 * @param parameterKey Key parameter value
 * @param pInstanceNumber Instance number that was created
 * @param pStatus Status of the new parameter
 *
 * @return Returns 0 (zero) if OK or a fault code (9002, ...) according to the TR-069.
 */
int DM_ENG_Device_AddObject(const char* objectName,
                            UNUSED const char* parameterKey,
                            unsigned int* pInstanceNumber,
                            DM_ENG_ParameterStatus* pStatus) {
    int error = 0;

    if(DM_ENG_Device_Common_CheckSystem(&da.acs) == false) {
        SetErrorGotoStop(DM_ENG_INTERNAL_ERROR, "System error");
    }

    SAH_TRACEZ_INFO("DM_DA", "Adding object %s", objectName);

    error = DM_ENG_Device_AddDeleteObject_Add(&da.acs, objectName, pInstanceNumber, pStatus);

stop:
    return error;
}

/*********************************** deleteobject RPC ***********************************************/


/**
 * @brief Deletes an object as specified in the TR-069.
 *
 * @param objectName Name of an object prototype
 * @param parameterKey Parameter key of the object
 * @param pStatus The status of the resulting object
 *
 * @return Returns 0 (zero) if OK or a fault code (9002, ...) according to the TR-069.
 */
int DM_ENG_Device_DeleteObject(const char* objectName,
                               UNUSED const char* parameterKey,
                               DM_ENG_ParameterStatus* pStatus) {
    int error = 0;

    if(DM_ENG_Device_Common_CheckSystem(&da.acs) == false) {
        SetErrorGotoStop(DM_ENG_INTERNAL_ERROR, "System error");
    }

    SAH_TRACEZ_INFO("DM_DA", "Deleting object %s", objectName);

    error = DM_ENG_Device_AddDeleteObject_Delete(&da.acs, objectName, pStatus);

stop:
    return error;
}


/*********************************** reboot RPC *****************************************************/

/**
 * Commands the device to reboot.
 *
 */
void DM_ENG_Device_Reboot() {
    SAH_TRACEZ_IN("DM_DA");

    if(DM_ENG_Device_Common_CheckSystem(&da.acs) == false) {
        GotoStop("System error");
    }

    DM_ENG_Device_Reboot_DoReboot(&da.system);

stop:
    SAH_TRACEZ_OUT("DM_DA");
}


/*********************************** factoryreset RPC ***********************************************/


/**
 * Commands the device to do a factoryReset.
 *
 */
void DM_ENG_Device_FactoryReset() {
    SAH_TRACEZ_IN("DM_DA");

    if(DM_ENG_Device_Common_CheckSystem(&da.acs) == false) {
        GotoStop("System error");
    }

    DM_ENG_Device_FactoryReset_DoReset(&da.system);

stop:
    SAH_TRACEZ_OUT("DM_DA");
}

int DM_ENG_Device_ScheduleDownload(const char* CommandKey, const char* FileType,
                                   const char* URL, const char* Username, const char* Password,
                                   uint32_t FileSize, const char* TargetFileName,
                                   TimeWindowStruct* TimeWindowList[TIMEWINDOWLIST_MAX_LENGTH]) {
    int retval = 0;

    // dm_amx_env_t* amx = DM_ENG_Device_GetSystemInfo();

    // if ((amx == NULL) || (amx->bus_ctx == NULL))
    // {
    //     SAH_TRACEZ_ERROR("DM_DA", "Failed to get system info");
    //     retval = DM_ENG_FAULTCODE_9002;
    // } else {

    retval = DM_ENG_Device_DoScheduleDownload(CommandKey, FileType,
                                              URL, Username, Password,
                                              FileSize, TargetFileName,
                                              TimeWindowList);
    // }

    return retval;
}

int DM_ENG_Device_ChangeDUState(OperationStruct* operations, const char* commandKey) {
    int retval = 0;
    retval = DM_ENG_Device_DoChangeDUState(operations, commandKey);
    return retval;
}

/*********************************** download RPC *************************************/

/**
 * Commands the device to download a file.
 *
 * @param commandkey The command key
 * @param fileType A string as defined in TR-069
 * @param url URL specifying the source file location
 * @param username Username to be used by the CPE to authenticate with the file server
 * @param password Password to be used by the CPE to authenticate with the file server
 * @param fileSize Size of the file to be downloaded in bytes
 * @param targetFileName Name of the file to be used on the target file system
 * @param delayseconds Delay in seconds before starting the download
 * @param successURL URL the CPE may redirect the user's browser to if the download completes successfully
 * @param failureURL URL the CPE may redirect the user's browser to if the download does not complete successfully
 *
 * @return 0 if the download is carried out synchronously, 1 if asynchronously, else an error code (9001, ..) as specified in TR-069
 */
int DM_ENG_Device_Download(char* commandkey, char* fileType, char* url, char* username, char* password, unsigned int fileSize, char* targetFileName,
                           unsigned int delayseconds, char* successURL, char* failureURL) {
    int error = 0;

    if(DM_ENG_Device_Common_CheckSystem(&da.system) == false) {
        error = DM_ENG_INVALID_ARGUMENTS;
    } else {
        error = DM_ENG_Device_DoDownload(&da.system, commandkey, fileType, url, username, password, fileSize, targetFileName, delayseconds, successURL, failureURL);
    }
    return error;
}


/*********************************** upload RPC *************************************/

/**
 * Commands the device to upload a file.
 *
 * @param commandkey The command key
 * @param fileType A string as defined in TR-069
 * @param url URL specifying the destination file location
 * @param username Username to be used by the CPE to authenticate with the file server
 * @param password Password to be used by the CPE to authenticate with the file server
 * @param delayseconds Delay in seconds before starting the download
 *
 * @return 0 if the upload id carried out synchronously, 1 if asynchronously, else an error code (9001, ..) as specified in TR-069
 */
int DM_ENG_Device_Upload(char* commandkey, char* fileType, char* url, char* username, char* password, unsigned int delayseconds) {
    int error = 0;

    if(DM_ENG_Device_Common_CheckSystem(&da.system) == false) {
        error = DM_ENG_INVALID_ARGUMENTS;
    } else {
        error = DM_ENG_Device_DoUpload(&da.system, commandkey, fileType, url, username, password, delayseconds);
    }
    return error;
}

#define IGD_WANIP_SPECIAL_ACACHE_NAME "IGDWANIPADDRESS"

/**
 * Load the schedule inform entries attributes
 *
 * @param rpcpath The persistent file location
 * @param is The resulting array
 * @return -1 in case of error, 0 on success
 */
static int DM_ENG_Device_LoadScheduleInform(char* rpcpath, DM_ENG_ScheduleInformStruct** is) {
    FILE* pFile;
    SAH_TRACEZ_ERROR("DM_DA", "Opening for read: %s", rpcpath);
    pFile = DM_COMMON_rpc_open_read(rpcpath, "cwmp_scheduleinform");
    if(pFile == NULL) {
        SAH_TRACEZ_INFO("DM_DA", "Could not open scheduleinform file");
        return -1;
    } else {
        char* commandKey;
        time_t time;
        while(DM_COMMON_rpc_read_scheduleinform(pFile, &commandKey, &time) != -1) {
            SAH_TRACEZ_INFO("DM_DA", "Commandkey = %s, time =%ld", commandKey, time);
            DM_ENG_ScheduleInformStruct* newIs = DM_ENG_newScheduleInformStruct(time, commandKey, 0);
            if(!newIs) {
                SAH_TRACEZ_INFO("DM_DA", "Could not create new schedule inform struct");
            } else {
                DM_ENG_addScheduleInformStruct(is, newIs);
                DM_ENG_acsEventListAddEvent(DM_ENG_EVENT_M_SCHEDULE_INFORM, commandKey);
            }
            free(commandKey);
        }
        fclose(pFile);
    }
    return 0;
}

/**
 * Load the saved engine parameters: reboot command key,  attribute cache.
 *
 * @param is Linked list containing all scheduleinform entries that were undelivered
 *
 * @ return 0 if save was succesfull, -1 if an error occurred
 */
int DM_ENG_Device_LoadConfig(DM_ENG_ScheduleInformStruct** is) {
    char* path = NULL;
    if(persistentRPCPath) {
        path = strdup(persistentRPCPath);
    }
    /* initialize */
    *is = NULL;

    if(DM_ENG_Device_LoadScheduleInform(path, is) == -1) {
        SAH_TRACEZ_ERROR("DM_DA", "Could not load schedule inform file");
    }

    if(DM_AmxScheduleDownload_init((&da.system)->bus_ctx) != 0) {
        SAH_TRACEZ_ERROR("DM_DA", "Failed to intialise AMX ScheduleDownload");
    }

    if(DM_AmxChangeDUState_init((&da.system)->bus_ctx) != 0) {
        SAH_TRACEZ_ERROR("DM_DA", "Failed to intialise AMX AmxChangeDUState");
    }

    if(DM_AmxRPCInterface_init((&da.system)->bus_ctx) != 0) {
        SAH_TRACEZ_ERROR("DM_DA", "Failed to intialise AMX RPCInterface");
    }

    free(path);
    bool result = DM_ENG_Device_UpDownloadInitialize(&da.system);
    if(result == false) {
        SAH_TRACEZ_ERROR("DM_DA", "Error initializing the transfers");
        return -1;
    }
    return 0;
}


/**
 * Subscribe for parameter updates.
 *
 * @param subscriptionPath The path of the parameter we want to subscribe to
 * @param subscriptionID The subscription ID
 *
 * @ return 0 if succesfull, -1 if an error occurred
 */
int DM_ENG_Device_AddNotification(const char* subscriptionPath, int* subscriptionID, DM_ENG_NotificationMode mode) {
    SAH_TRACEZ_INFO("DM_DA", "Add Notification path=%s", subscriptionPath);
    if(DM_ENG_Device_ACSConnectionAddSubscription(&da.acs, subscriptionPath, subscriptionID, mode) == false) {
        return -1;
    }
    return 0;
}

/**
 * Unsubscribe for parameter updates.
 *
 * @param subscriptionPath The path of the parameter we want to unsubscribe
 * @param subscriptionID The subscription ID
 *
 * @ return 0 if succesfull, -1 if an error occurred
 */
int DM_ENG_Device_RemoveNotification(const char* subscriptionPath, int subscriptionID) {
    SAH_TRACEZ_INFO("DM_DA", "DM_ENG_Device_RemoveNotification path=%s id=%d", subscriptionPath, subscriptionID);
    if(DM_ENG_Device_ACSConnectionRemoveSubscription(&da.acs, subscriptionPath, subscriptionID) == false) {
        return -1;
    }
    return 0;
}

/**
 * Subscribe for parameter updates.
 *
 * @param subscriptionPath The path of the parameter we want to subscribe to
 * @param subscriptionID The subscription ID
 *
 * @ return 0 if succesfull, -1 if an error occurred
 */
int DM_ENG_Device_GetSubscriptions(DM_ENG_SubscriptionStruct** pResult) {
    SAH_TRACEZ_INFO("DM_DA", "Get subscriptions");
    if(DM_ENG_Device_ACSConnectionGetSubscriptions(&da.system, pResult) == false) {
        return -1;
    }
    return 0;
}

/**
 * Set the accessrights for the specified path.
 *
 * @param accessrightsPath The path of the parameter we want to unsubscribe
 * @param al The accesslist
 *
 * @ return 0 if succesfull, -1 if an error occurred
 */
int DM_ENG_Device_SetAccessRights(UNUSED const char* accessrightsPath, UNUSED char* al[]) {
    return 0;
}

/**
 * Set a configuration value
 *
 * @param parameter The parameter of interest
 * @param pValue The new value of this parameter
 *
 * @ return 0 if succesfull, -1 if an error occurred
 */
int DM_ENG_Device_SetConfigValue(DM_ENG_SystemParameter_t parameter, char* pValue) {
    int error = 0;
    if(DM_ENG_Device_SystemConnectionSetParameter(&da.system, parameter, pValue) == false) {
        SetErrorGotoStop(DM_ENG_INVALID_PARAMETER_NAME, "Could not set the parameter value");
    }

stop:
    return error;

}

/**
 * Get a configuration value
 *
 * @param parameter The parameter of interest
 * @param pResult The value of this parameter
 *
 * @ return 0 if succesfull, -1 if an error occurred
 */
int DM_ENG_Device_GetConfigValue(DM_ENG_SystemParameter_t parameter, char** pResult) {
    int error = 0;
    *pResult = DM_ENG_Device_SystemConnectionGetParameter(&da.system, parameter);
    if(*pResult == NULL) {
        SetErrorGotoStop(DM_ENG_INVALID_PARAMETER_NAME, "Could not get the parameter value");
    }

stop:
    return error;
}

/**
 * Execute a configuration function
 *
 * @param function The function of interest
 *
 * @ return 0 if succesfull, -1 if an error occurred
 */
int DM_ENG_Device_ExecuteConfigFunction(DM_ENG_SystemFunction_t function) {
    int error = 0;
    SAH_TRACEZ_IN("DM_DA");

    if(DM_ENG_Device_SystemConnectionExecuteFunction(&da.system, function) == false) {
        SetErrorGotoStop(DM_ENG_INVALID_PARAMETER_NAME, "Could not execute this function");
    }

stop:
    SAH_TRACEZ_OUT("DM_DA");
    return error;

}

dm_amx_env_t* DM_ENG_Device_GetACSInfo() {
    return &da.acs;
}


dm_amx_env_t* DM_ENG_Device_GetSystemInfo() {
    return &da.system;
}

dm_app_data_t* DM_ENG_Device_GetDataInfo() {
    return &da.data;
}


/**
 * Get a list of queued transfers
 *
 * @param pResult The queued transfer result structure
 *
 * @return 0 if succesfull, else an error code (9001, ..) as specified in TR-069
 */
int DM_ENG_Device_GetAllQueuedTransfers(DM_ENG_AllQueuedTransferStruct** pResult[]) {
    int error = 0;
    if(DM_ENG_Device_Common_CheckSystem(&da.system) == false) {
        error = DM_ENG_INVALID_ARGUMENTS;
    } else {
        error = DM_ENG_Device_GetQueuedTransfers(&da.system, pResult);
    }
    return error;
}

/**
 * Called when a transfer complete message has been acknowledged by the acs
 *
 * @param uniqueID The unique id identifying this download, in this case the id is the object path
 *
 * @return 0 if succesfull, -1 if an error occurred
 */
int DM_ENG_Device_TransferDoneAcknowledged(char* uniqueID) {
    SAH_TRACEZ_INFO("DM_DA", "ACK DONE delete object : %s", uniqueID);
    int error = 0;
    int rv = 0;
    amxc_var_t ret;
    amxc_var_init(&ret);
    dm_amx_env_t* amx = &da.system;

    rv = amxb_del(amx->bus_ctx, uniqueID, 0, NULL, &ret, 2);
    if((rv != 0) || amxc_var_is_null(&ret)) {
        SetErrorGotoStop(-1, "Delete Instance failed [%s]", uniqueID);
    }
stop:
    amxc_var_clean(&ret);
    return error;
}

int DM_ENG_Device_DUStateChangeCompleteAcknowledged(const char* path) {
    SAH_TRACEZ_INFO("DM_DA", "ACK DONE delete object : %s", path);
    int error = 0;
    int rv = 0;
    amxc_var_t ret;
    amxc_var_init(&ret);
    dm_amx_env_t* amx = &da.system;

    rv = amxb_del(amx->bus_ctx, path, 0, NULL, &ret, 2);
    if((rv != 0) || amxc_var_is_null(&ret)) {
        SetErrorGotoStop(-1, "Delete Instance failed [%s]", path);
    }
stop:
    amxc_var_clean(&ret);
    return error;
}

void __attribute__ ((destructor)) fini(void) {
    free(externalIPAddress);
}
#endif

/** @} */
