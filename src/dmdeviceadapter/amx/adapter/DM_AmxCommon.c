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

#include <dmengine/DM_ENG_ParameterType.h>
#include <dmengine/DM_ENG_Device.h>
#include <dmengine/DM_ENG_Error.h>
#include <debug/sahtrace.h>
#include <string.h>

#include "DM_AmxCommon.h"


// Root data model parameters ACS path
const char* ROOT_DM_PARAMETERS[3] = {
    "RootDataModelVersion",
    "InterfaceStackNumberOfEntries",
    NULL
};

// Root data model parameters ACS path
const char* ROOT_DM_INTERNAL_PARAMETER_PATH[3] = {
    "Device.RootDataModelVersion",
    "Device.InterfaceStackNumberOfEntries",
    NULL
};

const char* DM_ENG_Device_Common_GetRootParameterInternalPath(const char* path) {

    const char* parameterName = DM_ENG_Device_Common_ACSToAMXPath_noalloc(path);

    if(parameterName == NULL) {
        return NULL;
    }

    int index = 0;
    while(ROOT_DM_PARAMETERS[index]) {
        if(strcmp(ROOT_DM_PARAMETERS[index], parameterName) == 0) {
            return ROOT_DM_INTERNAL_PARAMETER_PATH[index];
        }
        index++;
    }
    return NULL;
}

bool DM_ENG_Device_Common_IsRootParameter(const char* path) {

    const char* parameterName = DM_ENG_Device_Common_ACSToAMXPath_noalloc(path);

    if(parameterName == NULL) {
        return false;
    }

    int index = 0;
    while(ROOT_DM_PARAMETERS[index]) {
        if(strcmp(ROOT_DM_PARAMETERS[index], parameterName) == 0) {
            return true;
        }
        index++;
    }
    return false;
}
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
   @param envVariable The name of the environment variable that can be used to override the defaultLocation
   @param defaultLocation The default location of the bus you want to connect to (e.g. /var/run/pcb_sys)

   @return
   - false if an error occurred
   - true if succesfull
 */
bool DM_ENG_Device_Common_AmxConnect(dm_amx_env_t* amx, const char* envVariable, const char* defaultLocation,
                                     const char* envURI, const char* defaultURI) {

    const char* uri = getenv(envURI);
    const char* path = getenv(envVariable);

    if(!path) {
        SAH_TRACEZ_INFO("DM_DA", "env var [%s] not found, using default path [%s]", envVariable, defaultLocation);
        path = defaultLocation;
    }

    if(!uri) {
        SAH_TRACEZ_INFO("DM_DA", "env [%s] not found, using default uri [%s]", envURI, defaultURI);
        uri = defaultURI;
    }

    SAH_TRACEZ_INFO("DM_DA", "Loading AMX backend [%s] ...", path);

    int rv = amxb_be_load(path);

    if(rv != 0) {
        SAH_TRACEZ_ERROR("DM_DA", "Failed To load AMX backend [%s]", path);
        return false;
    }

    SAH_TRACEZ_INFO("DM_DA", "Connecting To [%s] ...", uri);

    rv = amxb_connect(&amx->bus_ctx, uri);

    if(rv != 0) {
        SAH_TRACEZ_ERROR("DM_DA", "Failed to connect to bus [%s]\n", uri);
        return false;
    }
    return true;
}

//---------------------------------------------------------------------------------------------
/**
   @brief
   Function to convert a TR69 OBJECT path "Device.object.param" into a Amx path "object.param"

   @details
   Function to convert a TR69 OBJECT path "Device.object.param"  into a Amx path "object.param"
   No dynamic alloc here no free is needed

   @param acsPath The Tr69 OBJECT path "Device.object." or "Device.object.param"

   @return
   - NULL if an error occurred
   - a pointer tot the translated Ambiorix path
 */
const char* DM_ENG_Device_Common_ACSToAMXPath_noalloc(const char* acsPath) {
    char* amxPath = NULL;
    int prefixlen = 0;
    const char* prefixName = "";
    prefixName = DM_ENG_getDatamodelPrefix();

    if((acsPath == NULL) || (prefixName == NULL)) {
        return NULL;
    }

    if(strlen(acsPath) == 0) { //if path is an empty string, return the top of the name hierarchy
        return acsPath;
    }

    if(prefixName != NULL) {
        prefixlen = strlen(prefixName);
    }

    if(strncmp(acsPath, prefixName, prefixlen)) {
        SAH_TRACEZ_ERROR("DM_DA", "Object %s not found, not a correct prefix", acsPath);
        return NULL;
    }

    // remove the InternetGatewayDevice. or Device. prefix
    amxPath = strchr((char*) acsPath, '.');
    if(amxPath == NULL) {
        SAH_TRACEZ_ERROR("DM_DA", "Object %s not found", acsPath);
        return NULL;
    }
    amxPath++;

    return amxPath;
}

//---------------------------------------------------------------------------------------------
/**
   @brief
   Function to convert a TR69 OBJECT path "Device.object.param" into a Amx path "object.param"

   @details
   Function to convert a TR69 OBJECT path "Device.object.param"  into a Amx path "object.param"
   the returned string is dynamicaly allocated and need to be freed

   @param acsPath The Tr69 OBJECT path "Device.object." or "Device.object.param"

   @return
   - NULL if an error occurred
   - a pointer tot the translated Ambiorix path
 */
char* DM_ENG_Device_Common_ACSToAMXPath(const char* acsPath) {
    return strdup(DM_ENG_Device_Common_ACSToAMXPath_noalloc(acsPath));
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
        return DM_ENG_ParameterType_STRING;
    case AMXC_VAR_ID_BOOL:
        return DM_ENG_ParameterType_BOOLEAN;
    case AMXC_VAR_ID_TIMESTAMP:
        return DM_ENG_ParameterType_DATE;
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

static void DM_ENG_Device_Common_Resolve_Path_cb(const amxb_bus_ctx_t* bus_ctx, const amxc_var_t* const data, void* priv) {
    (void) bus_ctx;
    amxc_var_t* resolved = (amxc_var_t*) priv;

    if(data) {
        amxc_var_t* object = GETI_ARG(data, 0);
        const char* path = GETP_CHAR(object, NULL);
        int i = 0;
        const char* s_char = path;
        // only root data object are to be added
        for(i = 0; s_char[i]; s_char[i] == '.' ? i++ : *s_char++) {
        }

        // Device. already added dont add it twice
        if((i == 1) && (strcmp(path, TR181_DEVICE_OBJNAME) != 0)) {
            // add path to list
            amxc_var_add(cstring_t, resolved, path);
        }
    }
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
    bool ret = true;
    int rv = 0;
    amxd_path_t amxd_path;
    amxc_var_init(resolved);
    // resolve the search path
    amxd_path_init(&amxd_path, path);

    if(DM_ENG_Device_Common_IsWildcardPath(path)) {
        if(!DM_ENG_Device_Common_IsWildcardPathValid(path)
           || !amxd_path_is_search_path(&amxd_path)) {
            ret = false;
            goto stop;
        }

        if((amxb_resolve(amx->bus_ctx, &amxd_path, resolved) != 0)
           || amxc_var_is_null(resolved)
           || ( amxc_var_type_of(resolved) != AMXC_VAR_ID_LIST)) {
            ret = false;
            goto stop;
        }
    } else if((strlen(path) == 0) || (strcmp(amx->prefix, path) == 0)) {
        // listing all of Device. or IGD.
        int flags = AMXB_FLAG_OBJECTS | AMXB_FLAG_INSTANCES;
        amxc_var_set_type(resolved, AMXC_VAR_ID_LIST);
        // find all possible object paths on root data model
        rv = amxb_list(amx->bus_ctx, "", flags,
                       DM_ENG_Device_Common_Resolve_Path_cb, (void*) resolved);

        if(rv != 0) {
            ret = false;
        }
    } else {
        // there is nothing to resolve here
        amxc_var_set_type(resolved, AMXC_VAR_ID_LIST);
        amxc_var_add(cstring_t, resolved, path);
        ret = true;
    }
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
static void DM_ENG_Device_Common_NotificationHandler(const char* const sig_name,
                                                     const amxc_var_t* const data,
                                                     void* const priv) {
    SAH_TRACEZ_INFO("DM_DA", "event [%s]", sig_name);
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
    subInfo->uniqueID = amxc_AP_hash(amxc_string_get(&uidstr, 0), amxc_string_text_length(&uidstr));
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

/** @} */
