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
const char* ROOT_DM_ACS_PARAMETER_PATH[3] = {
    "Device.RootDataModelVersion",
    "Device.InterfaceStackNumberOfEntries",
    NULL
};

// Root data model parameters internal path
const char* ROOT_DM_INTERNAL_PARAMETER_PATH[3] = {
    "ManagementServer.InternalSettings.RootDataModelVersion",
    "ManagementServer.InternalSettings.InterfaceStackNumberOfEntries",
    NULL
};
char* DM_ENG_Device_Common_GetRootParameterAcsPath(char* path) {
    int index = 0;
    while(ROOT_DM_INTERNAL_PARAMETER_PATH[index]) {
        if(strcmp(ROOT_DM_INTERNAL_PARAMETER_PATH[index], path) == 0) {
            return (char*) ROOT_DM_ACS_PARAMETER_PATH[index];
        }
        index++;
    }
    return NULL;
}

char* DM_ENG_Device_Common_GetRootParameterInternalPath(char* path) {
    int index = 0;
    while(ROOT_DM_ACS_PARAMETER_PATH[index]) {
        if(strcmp(ROOT_DM_ACS_PARAMETER_PATH[index], path) == 0) {
            return (char*) ROOT_DM_INTERNAL_PARAMETER_PATH[index];
        }
        index++;
    }
    return NULL;
}

bool DM_ENG_Device_Common_IsRootParameter(char* path) {
    int index = 0;
    while(ROOT_DM_ACS_PARAMETER_PATH[index]) {
        if(strcmp(ROOT_DM_ACS_PARAMETER_PATH[index], path) == 0) {
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

    char* uri = getenv(envURI);
    char* path = getenv(envVariable);

    if(!path) {
        SAH_TRACEZ_INFO("DM_DA", "env var [%s] not found, using default path [%s]", envVariable, defaultLocation);
        path = (char*) defaultLocation;
    }

    if(!uri) {
        SAH_TRACEZ_INFO("DM_DA", "env [%s] not found, using default uri [%s]", envURI, defaultURI);
        uri = (char*) defaultURI;
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
char* DM_ENG_Device_Common_ACSToAMXPath_noalloc(const char* acsPath) {
    char* amxPath = NULL;
    int prefixlen = 0;
    const char* prefixName = "";
    prefixName = DM_ENG_getDatamodelPrefix();

    if((acsPath == NULL) || (prefixName == NULL)) {
        return NULL;
    }

    if(strlen(acsPath) == 0) { //if path is an empty string, return the top of the name hierarchy
        return (char*) acsPath;
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

    return (char*) amxPath;
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
        if(i == 1) {
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
bool DM_ENG_Device_Common_Resolve_Path(dm_amx_env_t* amx, char* path, amxc_var_t* resolved) {
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
    if(subscription->path) {
        free(subscription->path);
    }

    free(subscription);
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
            (*sub->cb)(sub->path, data);
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
int DM_ENG_Device_Common_AddSubscription(dm_amx_env_t* amx,
                                         const char* path,
                                         const char* filter,
                                         notification_cb_t cb,
                                         int* subscriptionID) {
    SAH_TRACE_INFO("DM_DA Create Subscription for [%s]", path);
    int error = 0;
    int rv = 0;
    amxc_string_t uidstr;
    amxc_string_init(&uidstr, 0);

    DM_Subscription_t* sub = (DM_Subscription_t*) calloc(1, sizeof(DM_Subscription_t));

    if(sub == NULL) {
        SetErrorGotoStop(9000, "calloc failed");
    }
    sub->cb = cb;
    sub->path = strdup(path);

    // generate a UID for this subscription
    amxc_string_setf(&uidstr, "%s%s", path, filter);
    sub->uniqueID = amxc_AP_hash(amxc_string_get(&uidstr, 0), amxc_string_text_length(&uidstr));
    (*subscriptionID) = sub->uniqueID;

    rv = amxb_subscribe(amx->bus_ctx,
                        path,
                        filter,
                        DM_ENG_Device_Common_NotificationHandler,
                        (void*) sub);
    if(rv != 0) {
        SetErrorGotoStop(9000, "Failed to Create a new subscription");
    }
    // add to list
    amxc_llist_append((amxc_llist_t* const) &subscription_list, &sub->it);
    error = 0;
stop:
    if((error != 0) && sub) {
        free(sub);
    }
    amxc_string_clean(&uidstr);
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
int DM_ENG_Device_Common_DeleteSubscription(dm_amx_env_t* amx, int id) {
    int error = 0;
    int rv = 0;
    DM_Subscription_t* sub = NULL;
    int index = 0;
    // search for notification
    amxc_llist_for_each(it, ((amxc_llist_t* const) &subscription_list)) {
        sub = amxc_container_of(it, DM_Subscription_t, it);
        if(sub->uniqueID == id) {
            break;
        }
        index++;
    }

    if(!sub) {
        //ERROR ?
        SetErrorGotoStop(9000, "Subscription already removed ?");
    }
    SAH_TRACE_INFO("DM_DA Delete Subscription for [%s]", sub->path);
    rv = amxb_unsubscribe(amx->bus_ctx,
                          sub->path,
                          DM_ENG_Device_Common_NotificationHandler,
                          (void*) sub);


    if(rv != 0) {
        SetErrorGotoStop(9000, "Failed to Create a new subscription");
    }
    amxc_llist_it_t* todel = amxc_llist_take_at((amxc_llist_t* const) &subscription_list, index);
    DM_list_removeSub(todel);// free memory
stop:
    return error;
}

void DM_ENG_Device_Common_Cleanup() {
    DM_Subscription_t* sub = NULL;
    dm_amx_env_t* amx = NULL;
    int rv = 0;
    amx = DM_ENG_Device_GetSystemInfo();

    if(amx == NULL) {
        goto stop;
    }
    // Delete All subscription when exit
    amxc_llist_for_each(it, ((amxc_llist_t* const) &subscription_list)) {
        sub = amxc_container_of(it, DM_Subscription_t, it);
        // calling DM_ENG_Device_Common_DeleteSubscription while accessing
        // the list may cause a crash ? not clear from documentation ?
        rv = amxb_unsubscribe(amx->bus_ctx,
                              sub->path,
                              DM_ENG_Device_Common_NotificationHandler,
                              (void*) sub);
        if(rv != 0) {
            SAH_TRACE_ERROR("ERROR while removing subscription %s", sub->path);
            // proceed any way, we are going to exit?
        }
    }
stop:
    // Cleanup the list
    amxc_llist_clean((amxc_llist_t* const) &subscription_list, DM_list_removeSub);
}
/** @} */
