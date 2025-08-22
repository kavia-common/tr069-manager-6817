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
#include <amxc/amxc_string.h>
#include <stdio.h>
#include <stdlib.h>

#include <dmengine/DM_ENG_ParameterType.h>
#include <dmengine/DM_ENG_Device.h>
#include <dmengine/DM_ENG_Error.h>
#include <debug/sahtrace.h>
#include <debug/sahtrace_macros.h>
#include <string.h>
#include <ctype.h>

#include "DM_AmxCommon.h"

#define ME "DM_DA"


//---------------------------------------------------------------------------------------------
/**
 * @addtogroup sah_cwmp_amxdeviceadapter
 * @{
 */

//---------------------------------------------------------------------------------------------
/**
   @brief
   Connect to Ambiorix backend.

   @details
   Connect to Ambiorix backend.

   @param amx A pointer to the amx env variable that we want to initialize.
   @param backend backend to load.
   @param uri uri to connect to.

   @return
   - false if an error occurred
   - true if succesfull
 */
bool DM_ENG_Device_Common_AmxConnect(dm_amx_env_t* amx, const char* backend, const char* uri) {
    bool retval = false;
    const char* backend_ptr = "/usr/bin/mods/amxb/mod-amxb-ubus.so";
    const char* uri_ptr = "ubus:/var/run/ubus/ubus.sock";

    if(backend && *backend) {
        SAH_TRACEZ_INFO("DM_DA", "Using [%s] as backend", backend);
        backend_ptr = backend;
    } else {
        SAH_TRACEZ_INFO("DM_DA", "backend not available, default backend [%s] will be used", backend_ptr);
    }

    if(uri && *uri) {
        SAH_TRACEZ_INFO("DM_DA", "Using [%s] as uri", uri);
        uri_ptr = uri;
    } else {
        SAH_TRACEZ_INFO("DM_DA", "uri not available, default uri [%s] will be used", uri_ptr);
    }

    if(amxb_be_load(backend_ptr) != 0) {
        SAH_TRACEZ_ERROR("DM_DA", "Failed To load AMX backend [%s]", backend_ptr);
        goto stop;
    }

    if(amxb_connect(&amx->bus_ctx, uri_ptr) != 0) {
        SAH_TRACEZ_ERROR("DM_DA", "Failed to connect to bus [%s]", uri_ptr);
        goto stop;
    }

    retval = true;
stop:
    return retval;
}

//---------------------------------------------------------------------------------------------
/**
   @brief
   Convert the amx parameter type to the tr69 parameter type

   @details
   Convert the amx parameter type to the tr69 parameter type

   @param type the amx parameter type

   @return
   -  the tr69 parameter type
 */
DM_ENG_ParameterType DM_ENG_Device_Common_ConvertParameterType(u_int32_t type) {
    switch(type) {
    case AMXC_VAR_ID_INT8:
    case AMXC_VAR_ID_INT16:
    case AMXC_VAR_ID_INT32:
    case AMXC_VAR_ID_INT64:
        return DM_ENG_ParameterType_INT;
    case AMXC_VAR_ID_UINT8:
    case AMXC_VAR_ID_UINT16:
    case AMXC_VAR_ID_UINT32:
    case AMXC_VAR_ID_UINT64:
        return DM_ENG_ParameterType_UINT;
    case AMXC_VAR_ID_CSTRING:
    case AMXC_VAR_ID_CSV_STRING:
    case AMXC_VAR_ID_SSV_STRING:
        return DM_ENG_ParameterType_STRING;
    case AMXC_VAR_ID_BOOL:
        return DM_ENG_ParameterType_BOOLEAN;
    case AMXC_VAR_ID_TIMESTAMP:
        return DM_ENG_ParameterType_DATE;
    case AMXC_VAR_ID_ANY:
        return DM_ENG_ParameterType_ANY;
    default:
        //parameter_type_unknown,   /**< unknown (only for internal use)*/
        return DM_ENG_ParameterType_UNDEFINED;
    }
}


//---------------------------------------------------------------------------------------------
/**
   @brief
   Check if the amx environment variable is valid.

   @details
   Check if the amx environment variable is valid.

   @param amx A pointer to the amx system bus environment variable

   @return
   - false in case it's invalid
   - true in case it's valid
 */
bool DM_ENG_Device_Common_CheckSystem(dm_amx_env_t* amx) {
    if(amx->bus_ctx == NULL) {
        SAH_TRACEZ_ERROR("DM_DA", "No bus Connection ?");
        return false;
    }
    return true;
}
//---------------------------------------------------------------------------------------------
/**
   @brief
   Check if a path is valid.

   @details
   Check if a path is valid.

   @param path The path to check

   @return
   - false in case it's not valid path
   - true if the path is valid
 */
bool DM_ENG_Device_Common_IsValidPath(const char* path) {
    return (path && ((strlen(path) == 0) || (*path && (strncmp(path, "Device.", 7) == 0) && !(path[0] == '.') && !(path[0] == '*'))));
}
//---------------------------------------------------------------------------------------------
/**
   @brief
   Check if a path has wildcard elements.

   @details
   Check if a path has wildcard elements.

   @param path The path to check

   @return
   - false in case it's not a wildcard path
   - true in case it's a wildcard path
 */
bool DM_ENG_Device_Common_IsWildcardPath(const char* path) {
    // No '*' is normally allowed, so by just having this
    // character present, it should be a wildcard path.
    return (path && *path && strchr(path, '*'));
}

//---------------------------------------------------------------------------------------------
/**
   @brief
   Check if a wildcard path is valid.

   @details
   Check if a wildcard path is valid.

   @param path The wildcard path to check

   @return
   - false in case it's not valid
   - true in case it's valid or not a wildcard path
 */
bool DM_ENG_Device_Common_IsWildcardPathValid(const char* path) {
    char* p = NULL, * iwcs = NULL;

    if(!DM_ENG_Device_Common_IsWildcardPath(path)) {
        return true;
    }

    // Check if support for wildcards is enabled
    if(DM_ENG_Device_GetConfigValue(DM_ENG_INSTANCEWILDCARDSSUPPORTED, &iwcs) || !iwcs) {
        SAH_TRACEZ_ERROR("DM_DA", "cant get DM_ENG_INSTANCEWILDCARDSSUPPORTED");
        return false;
    }
    if(!strcmp(iwcs, "0")) {
        free(iwcs);
        return false;
    }
    free(iwcs);
    iwcs = NULL;

    // Only instances, so the first element can't be '*'
    if(path[0] == '*') {
        return false;
    }

    // Each '*' must be a complete element
    for(p = strchr((char*) path, '*'); p; p = strchr(p + 1, '*')) {
        // Safe to access p-1 due to check above
        if((*(p - 1) != '.') || ((*(p + 1) != '.') && (*(p + 1) != 0))) {
            return false;
        }
    }

    return true;
}

//---------------------------------------------------------------------------------------------
/**
    @brief
    translate an index based TR069 path into an alias based one

    @details
    translate an indexed TR069 path into an alias path
    example "Deice.Object.Template.3." -> "Deice.Object.Template.[alias-3]."
    for more info : https://www.broadband-forum.org/technical/download/TR-069.pdf
    section 3.6.1

    @param amx A pointer to the amx system bus environment variable
    @param path path to resolve
    @param resolved list of the resolved path


    @return
    - false : error path couldn't be resolved
    - true : no error path resolved successfully
 */
int DM_ENG_Device_Common_IndexToAlias(dm_amx_env_t* amx, const char* acspath, const char* path, char** resolved) {
    int ret = -1;
    amxc_string_t string;
    amxc_string_t resolved_path;
    amxc_llist_t string_list;
    amxc_llist_init(&string_list);
    amxc_llist_t path_list;
    amxc_llist_init(&path_list);
    amxc_string_init(&string, 0);
    amxc_string_t tmp_string;
    amxc_string_init(&tmp_string, 0);
    amxc_string_init(&resolved_path, 0);
    amxc_llist_it_t* it_path = NULL;

    amxd_path_t amxpath;
    amxd_path_init(&amxpath, acspath);
    char* fixed_part = amxd_path_get_fixed_part(&amxpath, false);

    when_null(path, stop);
    when_str_empty(path, stop);

    amxc_string_setf(&string, "%s", path);
    when_failed(amxc_string_split_to_llist(&string, &string_list, '.'), stop);

    if(fixed_part) {
        amxc_string_setf(&string, "%s", fixed_part);
        when_failed(amxc_string_split_to_llist(&string, &path_list, '.'), stop);
        it_path = amxc_llist_get_first(&path_list);
    }

    /* replace index with alias ==>  Device.IP.Interface.3. ->  Device.IP.Interface.[lan]. */
    amxc_llist_iterate(it, &string_list) {
        char* alias = NULL;
        amxc_string_t part;
        const char* val = amxc_string_get(amxc_string_from_llist_it(it), 0);
        const char* orig_part = (it_path) ? amxc_string_get(amxc_string_from_llist_it(it_path), 0) : NULL;

        if(it_path && orig_part && *orig_part) {
            amxc_string_append(&resolved_path, orig_part, strlen(orig_part));
            amxc_string_append(&resolved_path, ".", 1);
            it_path = amxc_llist_it_get_next(it_path);
            continue;
        }
        amxc_string_init(&part, 0);
        amxc_string_setf(&part, "%s", val);
        if(amxc_string_is_numeric(&part)) {
            amxc_var_t get;
            amxc_var_init(&get);
            amxc_string_setf(&tmp_string, "%s%s.Alias", amxc_string_get(&resolved_path, 0), amxc_string_get(&part, 0));

            if((amxb_get(amx->bus_ctx, amxc_string_get(&tmp_string, 0), 0, &get, 10) == 0) && !amxc_var_is_null(&get)) {
                const amxc_htable_t* htable = amxc_var_constcast(amxc_htable_t, GETI_ARG(&get, 0));
                amxc_htable_iterate(hit, htable) {
                    amxc_var_t* alias_var = amxc_var_from_htable_it(hit);
                    const char* val = amxc_var_constcast(cstring_t, GET_ARG(alias_var, "Alias"));
                    if(val) {
                        alias = strdup(val);
                        break;
                    }
                }
            }
            amxc_var_clean(&get);
            amxc_string_clean(&tmp_string);
        }

        if(alias && *alias) {
            amxc_string_setf(&tmp_string, "[%s].", alias);
            amxc_string_append(&resolved_path, amxc_string_get(&tmp_string, 0), amxc_string_text_length(&tmp_string));
        } else {
            amxc_string_append(&resolved_path, amxc_string_get(&part, 0), amxc_string_text_length(&part));
            amxc_string_append(&resolved_path, ".", 1);
        }
        free(alias);
        amxc_string_clean(&tmp_string);
        amxc_string_clean(&part);
    }
    *resolved = strndup(amxc_string_get(&resolved_path, 0), amxc_string_text_length(&resolved_path) - 1);
    ret = 0;
stop:
    amxc_string_clean(&string);
    amxc_string_clean(&resolved_path);
    amxd_path_clean(&amxpath);
    amxc_llist_clean(&string_list, amxc_string_list_it_free);
    amxc_llist_clean(&path_list, amxc_string_list_it_free);
    free(fixed_part);
    return ret;
}

//---------------------------------------------------------------------------------------------
/**
    @brief
    translate an obscured TR069 path into a list of amx paths

    @details
    translate an obscured TR069 path into a list of amx paths
    (like "Device." or "" -> list of root object paths)
    (wildcard path -> list of full path to all possible instances)
    ...

    @param amx A pointer to the amx system bus environment variable
    @param path path to resolve
    @param resolved list of the resolved path


    @return
    - false : error path couldn't be resolved
    - true : no error path resolved successfully
 */
bool DM_ENG_Device_Common_Resolve_Path(dm_amx_env_t* amx, const char* path, amxc_var_t* resolved) {
    bool ret = false;
    amxd_path_t amxd_path;
    amxc_var_init(resolved);
    // resolve the search path
    amxd_path_init(&amxd_path, path);
    amxc_var_set_type(resolved, AMXC_VAR_ID_LIST);

    if(DM_ENG_Device_Common_IsWildcardPath(path)) {
        if(!DM_ENG_Device_Common_IsWildcardPathValid(path)
           || !amxd_path_is_search_path(&amxd_path)) {
            goto stop;
        }

        //amxb_resolve ommit Parameter Path
        amxc_var_t result;
        bool is_parameter = (path[strlen(path) - 1] != '.');
        amxc_var_init(&result);
        if((amxb_resolve(amx->bus_ctx, &amxd_path, &result) != 0)
           || amxc_var_is_null(&result)
           || ( amxc_var_type_of(&result) != AMXC_VAR_ID_LIST)) {
            amxc_var_clean(&result);
            goto stop;
        }

        amxc_var_for_each(path, &result) {
            const char* resolved_path = amxc_var_constcast(cstring_t, path);
            amxc_string_t param_name;
            amxc_string_init(&param_name, 0);

            if(is_parameter) {
                amxc_string_setf(&param_name, "%s%s", resolved_path, amxd_path_get_param(&amxd_path));
            } else {
                amxc_string_setf(&param_name, "%s", resolved_path);
            }
            amxc_var_add(cstring_t, resolved, amxc_string_get(&param_name, 0));
            amxc_string_clean(&param_name);
        }
        amxc_var_clean(&result);
    } else if((strlen(path) == 0) || (strcmp(amx->prefix, path) == 0)) {
        amxc_var_add(cstring_t, resolved, "Device.");
    } else {
        amxc_var_add(cstring_t, resolved, path);
    }
    ret = true;
stop:
    amxd_path_clean(&amxd_path);
    return ret;
}

// Clean up subscription
static void DM_list_removeSub(amxc_llist_it_t* it) {

    DM_Subscription_t* subscription = amxc_container_of(it, DM_Subscription_t, it);
    if(subscription->objectpath) {
        free(subscription->objectpath);
    }

    free(subscription);
}

// Clean up subscription Info
static void DM_list_removeSubInfo(amxc_llist_it_t* infoit) {

    DM_Subscription_info_t* subscription_info = amxc_container_of(infoit, DM_Subscription_info_t, infoit);
    if(subscription_info->parameter) {
        free(subscription_info->parameter);
    }
    free(subscription_info);
}

DM_Subscription_t* DM_ENG_Device_Common_FindSubscriptionByID(amxc_llist_t* list, int id) {
    amxc_llist_for_each(it, list) {
        DM_Subscription_t* sub = amxc_container_of(it, DM_Subscription_t, it);
        amxc_llist_for_each(infoit, &sub->subscription_info_list) {
            DM_Subscription_info_t* info = amxc_container_of(infoit, DM_Subscription_info_t, infoit);
            if(info && (info->uniqueID == id)) {
                return sub;
            }
        }
    }
    return NULL;
}

DM_Subscription_t* DM_ENG_Device_Common_FindSubscription(amxc_llist_t* list, const char* path) {
    amxc_llist_for_each(it, list) {
        DM_Subscription_t* sub = amxc_container_of(it, DM_Subscription_t, it);
        if(strcmp(sub->objectpath, path) == 0) {
            return sub;
        }
    }
    return NULL;
}

static void DM_ENG_Device_Common_DeleteSubscriptionInfo(DM_Subscription_t* sub, int id) {
    int index = 0;
    int todel = -1;
    amxc_llist_it_t* item = NULL;
    if(sub) {
        amxc_llist_for_each(infoit, &sub->subscription_info_list) {
            DM_Subscription_info_t* info = amxc_container_of(infoit, DM_Subscription_info_t, infoit);
            if(info && (info->uniqueID == id)) {
                todel = index;
                break;
            }
            index++;
        }
        if(todel >= 0) {
            item = amxc_llist_take_at(&sub->subscription_info_list, index);
            DM_list_removeSubInfo(item);// free memory
        }
    }
}

static void DM_ENG_Device_Common_DeleteSubscriptionFromList(amxc_llist_t* list, DM_Subscription_t* sub) {
    amxc_llist_it_t* item = NULL;
    int index = 0;
    int todel = -1;
    amxc_llist_for_each(it, list) {
        DM_Subscription_t* itsub = amxc_container_of(it, DM_Subscription_t, it);
        if(sub == itsub) {
            todel = index;
            break;
        }
        index++;
    }
    if(todel >= 0) {
        item = amxc_llist_take_at(list, index);
        DM_list_removeSub(item);    // free memory
    }
}

// subscription callback, dispatch events to the right handler
static void DM_ENG_Device_Common_NotificationHandler(UNUSED const char* const sig_name,
                                                     const amxc_var_t* const data,
                                                     void* const priv) {
    if(priv) {
        DM_Subscription_t* sub = (DM_Subscription_t*) priv;
        if(sub && sub->cb) {
            // call the the event handler
            (*sub->cb)(sub->objectpath, data);
        }
    }
}

//---------------------------------------------------------------------------------------------
/**
    @brief
    Create and Subscribe for notification on the system bus.

    @details
    Create and Subscribe for notification on the system bus.

    @param amx A pointer to the ambiorix system bus environment variable
    @param path Path on which to set the notification
    @param filter notification filter (filter on object/parameter ...)
    @param cb Callback called when notification is received
    @param subscriptionID A unique subscription ID for this notification that is returned to the calling routine

    @return
    - 0 if OK , error code otherwise ...
 */
int DM_ENG_Device_Common_AddSubscription(amxc_llist_t* slist,
                                         dm_amx_env_t* amx,
                                         const char* path,
                                         const char* filter,
                                         notification_cb_t cb,
                                         int* subscriptionID) {
    SAH_TRACEZ_INFO("DM_DA", "Create New Subscription for [%s]", path);
    int error = 0;
    int rv = 0;
    amxc_string_t uidstr;
    amxc_string_init(&uidstr, 0);
    amxd_path_t amxpath;
    amxd_path_init(&amxpath, path);
    DM_Subscription_info_t* subInfo = NULL;
    int pathlen = strlen(path);

    DM_Subscription_t* sub = DM_ENG_Device_Common_FindSubscription(slist, amxd_path_get(&amxpath, AMXD_OBJECT_TERMINATE));

    if(strcmp(amxd_path_get(&amxpath, AMXD_OBJECT_TERMINATE), ".") == 0) {
        //TODO! override path for root parameters
        SetErrorGotoStop(9000, "Root Parameter");
    }
    subInfo = (DM_Subscription_info_t*) calloc(1, sizeof(DM_Subscription_info_t));

    if(subInfo == NULL) {
        SetErrorGotoStop(9000, "Failed to allocate more memory");
    }
    if(path[pathlen - 1] == '.') {
        //its an object
        subInfo->parameter = strdup(path);
    } else {
        subInfo->parameter = strdup(amxd_path_get_param(&amxpath));
    }

    // generate a UID for this subscription
    amxc_string_setf(&uidstr, "%s%s", path, filter);
    subInfo->uniqueID = amxc_AP_hash_string(amxc_string_get(&uidstr, 0));
    (*subscriptionID) = subInfo->uniqueID;

    if(sub == NULL) { //NO previous subscription for this object
        //CREATE A NEW ONE
        sub = (DM_Subscription_t*) calloc(1, sizeof(DM_Subscription_t));
        if(sub == NULL) {
            SetErrorGotoStop(9000, "Failed to allocate more memory");
        }
        //Init the subscription Info list
        amxc_llist_init(&sub->subscription_info_list);
        sub->cb = cb;
        sub->objectpath = strdup(amxd_path_get(&amxpath, AMXD_OBJECT_TERMINATE));
        rv = amxb_subscribe(amx->bus_ctx,
                            amxd_path_get(&amxpath, AMXD_OBJECT_TERMINATE),
                            filter,
                            DM_ENG_Device_Common_NotificationHandler,
                            (void*) sub);
        if(rv != 0) {
            SetErrorGotoStop(9000, "Failed to Create a new subscription");
        }
        // add to list
        amxc_llist_append(slist, &sub->it);
    }
    //Do not create a new subscription
    amxc_llist_append(&sub->subscription_info_list, &subInfo->infoit);
    error = 0;
stop:
    if((error != 0) && sub) {
        free(sub);
    }
    if((error != 0) && subInfo) {
        free(subInfo);
    }
    amxc_string_clean(&uidstr);
    amxd_path_clean(&amxpath);
    return error;
}

//---------------------------------------------------------------------------------------------
/**
    @brief
    Delete a Subscribe for notification from the system bus.

    @details
    Delete a Subscribe for notification from the system bus.

    @param amx A pointer to the ambiorix system bus environment variable
    @param id subscription ID for this notification that we wont to delete

    @return
    - 0 if OK , error code otherwise ...
 */
int DM_ENG_Device_Common_DeleteSubscription(amxc_llist_t* slist,
                                            dm_amx_env_t* amx,
                                            int id) {
    int error = 0;
    int rv = 0;
    DM_Subscription_t* sub = NULL;

    sub = DM_ENG_Device_Common_FindSubscriptionByID(slist, id);
    if(!sub) {
        SetErrorGotoStop(9000, "Subscription already removed");
    }

    // Remove parameter from subscription
    DM_ENG_Device_Common_DeleteSubscriptionInfo(sub, id);

    // Do we need a unsubscribe ?
    if(amxc_llist_is_empty(&sub->subscription_info_list)) {
        // subscription for this object is no longer needed
        SAH_TRACEZ_INFO("DM_DA", "Delete Subscription for [%s]", sub->objectpath);
        rv = amxb_unsubscribe(amx->bus_ctx,
                              sub->objectpath,
                              DM_ENG_Device_Common_NotificationHandler,
                              (void*) sub);
        if(rv != 0) {
            SetErrorGotoStop(9000, "Failed to Create a new subscription");
        }
        // Delete the subscription from subscription list
        DM_ENG_Device_Common_DeleteSubscriptionFromList(slist, sub);
    }

stop:
    return error;
}

//---------------------------------------------------------------------------------------------
/**
    @brief
    Cleanup Subscription list

    @details
    Delete a Subscribe for notification from the system bus and
    Cleanup the Subscription list.

    @param slist Subscription list
    @param id subscription ID for this notification that we wont to delete

    @return
    - 0 if OK , error code otherwise ...
 */
void DM_ENG_Device_Common_Cleanup_Subscription(amxc_llist_t* slist, dm_amx_env_t* amx) {
    DM_Subscription_t* sub = NULL;
    int rv = 0;

    if(amx == NULL) {
        goto stop;
    }
    // Delete All subscription when exit
    amxc_llist_for_each(it, slist) {
        sub = amxc_container_of(it, DM_Subscription_t, it);
        // calling DM_ENG_Device_Common_DeleteSubscription while accessing
        // the list may cause a crash ? not clear from documentation ?
        rv = amxb_unsubscribe(amx->bus_ctx,
                              sub->objectpath,
                              DM_ENG_Device_Common_NotificationHandler,
                              (void*) sub);
        if(rv != 0) {
            SAH_TRACEZ_ERROR("DM_DA", "ERROR while removing subscription [%s]", sub->objectpath);
            // proceed any way, we are going to exit?
        }
        // Clean Subscription Info list
        amxc_llist_clean(&sub->subscription_info_list, DM_list_removeSubInfo);
    }
stop:
    // Cleanup the list
    amxc_llist_clean(slist, DM_list_removeSub);
}

//---------------------------------------------------------------------------------------------
/**
    @brief
    String compare function for llist

    @param it1 the first llist iterator
    @param it2 the second llist iterator

    @return
    The callback function returns
    - 0 when the string values are equal
    - < 0 when the first string is less then the second
    - > 0 when the first string is greater then then second
 */
int DM_ENG_Device_Common_String_Compare(amxc_llist_it_t* it1, amxc_llist_it_t* it2) {
    const char* key1 = amxc_var_constcast(cstring_t, amxc_var_from_llist_it(it1));
    const char* key2 = amxc_var_constcast(cstring_t, amxc_var_from_llist_it(it2));
    return strcmp(key1 == NULL ? "" : key1, key2 == NULL ? "" : key2);
}

static bool is_valid_alias(const char* alias) {
    bool retval = false;
    if((alias == NULL) || (*alias == 0)) {
        goto stop;
    }
    if(!isalpha(alias[0]) && (alias[0] != '_')) {
        goto stop;
    }
    for(size_t i = 1; alias[i] != '\0'; i++) {
        if(!isalnum(alias[i]) && (alias[i] != '_') && (alias[i] != '-')) {
            goto stop;
        }
    }
    retval = true;
stop:
    return retval;
}

bool DM_ENG_Device_Common_Is_Valid_Alias_Path(const char* path) {
    bool retval = false;
    char* path_ptr = NULL;
    char* start = NULL;
    char* end = NULL;

    if((path == NULL) || (*path == 0)) {
        SAH_TRACEZ_ERROR("DM_DA", "Invalid arg(s)");
        goto stop;
    }

    if(strstr(path, "*")) {
        SAH_TRACEZ_ERROR("DM_DA", "Invalid path \'%s\': contains wildcard(s)", path);
        goto stop;
    }

    path_ptr = (char*) path;
    while((start = strstr(path_ptr, ".[")) && (end = strstr(start, "]"))) {
        char substring[256];
        start += 2;
        size_t len = end - start;
        if(len < 256) {
            strncpy(substring, start, len);
            substring[len] = '\0';
            if(is_valid_alias(substring) == false) {
                SAH_TRACEZ_ERROR("DM_DA", "Invalid path \'%s\': contains invalid alias \'%s\'", path, substring);
                goto stop;
            }
        }
        path_ptr = end + 1;
    }
    retval = true;
stop:
    return retval;
}

bool DM_ENG_Device_Common_Is_Alias_Based(const char* path) {
    const char* p = path;
    while(*p) {
        if(*p == '[') {
            const char* end = strchr(p, ']');
            if(end && (end > p + 1)) {
                return true;
            }
        }
        p++;
    }
    return false;
}

alias_list_t DM_ENG_Device_Common_Extract_Aliases(const char* path) {
    alias_list_t result = {0};
    result.entries = NULL;
    result.count = 0;
    result.total_alias_length = 0;

    char* path_copy = strdup(path);
    char* token;
    int token_index = 0;

    token = strtok(path_copy, ".");
    while(token != NULL) {
        size_t len = strlen(token);
        if((len >= 3) && (token[0] == '[') && (token[len - 1] == ']')) {
            result.entries = realloc(result.entries, (result.count + 1) * sizeof(alias_entry_t));
            result.entries[result.count].index = token_index;
            result.entries[result.count].alias = strdup(token);
            result.total_alias_length += len;
            result.count++;
        }
        token = strtok(NULL, ".");
        token_index++;
    }

    free(path_copy);
    return result;
}

void DM_ENG_Device_Common_Clean_Aliases(alias_list_t* aliases) {
    if(aliases) {
        for(size_t i = 0; i < aliases->count; ++i) {
            free(aliases->entries[i].alias);
        }
        free(aliases->entries);
    }
}

char* DM_ENG_Device_Common_Modify_Path_With_Aliases(const char* path, alias_list_t aliases) {
    char* path_copy = strdup(path);
    char* token;

    bool is_partial_path = path[strlen(path) - 1] == '.';

    size_t buf_size = strlen(path) + aliases.total_alias_length + 1;
    char* new_path = malloc(buf_size);
    new_path[0] = '\0';

    int index = 0;
    token = strtok(path_copy, ".");

    while(token) {
        const char* replacement = token;
        for(size_t i = 0; i < aliases.count; ++i) {
            if(aliases.entries[i].index == index) {
                replacement = aliases.entries[i].alias;
                break;
            }
        }

        strcat(new_path, replacement);

        token = strtok(NULL, ".");
        if((token != NULL) || is_partial_path) {
            strcat(new_path, ".");
        }

        index++;
    }

    free(path_copy);
    return new_path;
}

bool DM_ENG_Device_Common_EndsWithDot(const char* s) {
    size_t len = strlen(s);
    return len > 0 && s[len - 1] == '.';
}

void DM_ENG_Device_Common_Get_Gsdm_data(amxb_bus_ctx_t* bus_ctx, const char* parameter_name, amxc_var_t* data) {
    uint32_t flags = AMXB_FLAG_PARAMETERS;
    char* supported_path = NULL;
    amxc_var_t gsdm;
    amxd_path_t parameter_name_path;
    amxc_var_t* request = NULL;
    amxc_var_t* response = NULL;
    amxd_status_t gsdm_status = amxd_status_ok;
    amxc_string_t buffer;
    const amxc_htable_t* gsdm_htable = NULL;
    amxc_array_t* gsdm_keys = NULL;

    amxc_var_init(&gsdm);
    amxd_path_init(&parameter_name_path, parameter_name);
    amxc_string_init(&buffer, 0);

    when_str_empty_trace(parameter_name, stop, ERROR, "Invalid parameter name provided");
    when_true_trace(amxc_var_is_null(data), stop, ERROR, "Invalid argument: data is nil");
    when_true_trace(amxc_var_type_of(data) != AMXC_VAR_ID_HTABLE, stop, ERROR, "Invalid argument: data has invalid type");

    supported_path = amxd_path_build_supported_path(&parameter_name_path);
    when_str_empty_trace(supported_path, stop, ERROR, "Failed to build supported path for [%s]", parameter_name);

    if(!DM_ENG_Device_Common_EndsWithDot(parameter_name)) {
        flags = flags | AMXB_FLAG_FIRST_LVL;
    }

    request = GET_ARG(data, "Request");
    if(amxc_var_is_null(request)) {
        request = amxc_var_add_key(amxc_htable_t, data, "Request", NULL);
    }

    response = GET_ARG(data, "Response");
    if(amxc_var_is_null(response)) {
        response = amxc_var_add_key(amxc_htable_t, data, "Response", NULL);
    }

    amxc_string_setf(&buffer, "%s_%04X", supported_path, flags);
    if(amxc_var_get_key(request, amxc_string_get(&buffer, 0), AMXC_VAR_FLAG_DEFAULT)) {
        SAH_TRACEZ_INFO(ME, "[%s_%04X] was already requested", supported_path, flags);
        goto stop;
    } else {
        amxc_var_add_key(amxc_htable_t, request, amxc_string_get(&buffer, 0), NULL);
    }

    gsdm_status = amxb_get_supported(bus_ctx, supported_path, flags, &gsdm, 1);
    when_failed_trace(gsdm_status, stop, WARNING, "amxb_get_supported(%s,%04X) failed with status [%d] - Parameter name [%s]",
                      supported_path, flags, gsdm_status, parameter_name);

    // gsdmdata must be sorted for setting the template objects to the parent
    gsdm_htable = amxc_var_constcast(amxc_htable_t, GET_ARG(&gsdm, "0"));
    gsdm_keys = amxc_htable_get_sorted_keys(gsdm_htable);
    for(uint32_t i = 0; i < amxc_array_capacity(gsdm_keys); i++) {
        const char* key = (const char*) amxc_array_it_get_data(amxc_array_get_at(gsdm_keys, i));
        amxc_var_t* v = amxc_var_from_htable_it(amxc_htable_get(gsdm_htable, key));
        amxc_var_t* key_var = amxc_var_get_key(response, key, AMXC_VAR_FLAG_DEFAULT);
        if(amxc_var_is_null(key_var)) {
            key_var = amxc_var_add_key(amxc_htable_t, response, key, NULL);
        }

        amxc_var_t* access_var = GET_ARG(v, "access");
        if(access_var) {
            amxc_var_add_key(bool, key_var, "writable", amxc_var_dyncast(bool, access_var));
            SAH_TRACEZ_INFO(ME, "GSDM Info: Adding to key [%s]: Writable [%d]", key, amxc_var_dyncast(bool, access_var));
        }

        bool is_multi_instance = GET_BOOL(v, "is_multi_instance");
        if(is_multi_instance) {
            amxd_path_t gsdm_pathstr;
            char* template = NULL;
            const char* parent_path = NULL;

            amxd_path_init(&gsdm_pathstr, key);
            // take out the second last element
            template = amxd_path_get_last(&gsdm_pathstr, true);
            free(template);
            template = amxd_path_get_last(&gsdm_pathstr, true);
            parent_path = amxd_path_get(&gsdm_pathstr, AMXD_OBJECT_TERMINATE);

            amxc_var_t* parent_var = amxc_var_get_key(response, parent_path, AMXC_VAR_FLAG_DEFAULT);
            // use this as a key to add the template object to the parent
            if(amxc_var_is_null(parent_var)) {
                SAH_TRACEZ_WARNING(ME, "No parent [%s] found for path:[%s]", parent_path, key);
            } else {
                amxc_var_t* parent_templates_var = amxc_var_get_key(parent_var, "templates", AMXC_VAR_FLAG_DEFAULT);
                if(!parent_templates_var) {
                    parent_templates_var = amxc_var_add_new_key(parent_var, "templates");
                    amxc_var_set_type(parent_templates_var, AMXC_VAR_ID_HTABLE);
                }
                if(amxc_htable_contains(amxc_var_constcast(amxc_htable_t, parent_templates_var), template) == false) {
                    amxc_var_add_new_key(parent_templates_var, template);
                    SAH_TRACEZ_INFO(ME, "GSDM Info: Adding to key [%s]: Template [%s]", parent_path, template);
                }
            }
            free(template);
            template = NULL;
            amxd_path_clean(&gsdm_pathstr);
        }

        amxc_var_t* supported_params = GET_ARG(v, "supported_params");
        if(amxc_var_is_null(supported_params) || amxc_var_is_null(GETI_ARG(supported_params, 0))) {
            SAH_TRACEZ_INFO(ME, "[%s] doesn't have supported params, or has empty supported params. Skip adding parameters", key);
            continue;
        }

        amxc_var_t* parameters = amxc_var_add_new_key(key_var, "parameters");
        amxc_var_set_type(parameters, AMXC_VAR_ID_HTABLE);
        amxc_var_for_each(p, GET_ARG(v, "supported_params")) {
            const char* parameter_key = GET_CHAR(p, "param_name");
            if(parameter_key && *parameter_key) {
                int32_t parameter_type = GET_INT32(p, "type");
                int32_t access = GET_INT32(p, "access");
                amxc_var_t* htable = amxc_var_add_key(amxc_htable_t, parameters, parameter_key, NULL);
                amxc_var_add_key(int32_t, htable, "type", parameter_type);
                amxc_var_add_key(bool, htable, "writable", access == 1 ? true : false);
                SAH_TRACEZ_INFO(ME, "GSDM Info: Adding to key [%s]: Parameter [%s], Type [%d], Writable [%d]", key, parameter_key, parameter_type, access);
            }
        }
    }

stop:
    amxc_array_delete(&gsdm_keys, NULL);
    amxc_string_clean(&buffer);
    amxc_var_clean(&gsdm);
    amxd_path_clean(&parameter_name_path);
    free(supported_path);
}

/** @} */
