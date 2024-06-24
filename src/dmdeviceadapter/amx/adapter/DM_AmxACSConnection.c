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
#include <amxc/amxc_variant.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <arpa/inet.h>

#include <debug/sahtrace.h>
#include <debug/sahtrace_macros.h>

#include <amxc/amxc.h>
#include <amxc/amxc_macros.h>
#include <amxp/amxp.h>
#include <amxd/amxd_dm.h>
#include <amxb/amxb.h>

#include <dmengine/DM_ENG_NotificationInterface.h>
#include <dmengine/DM_ENG_RPCInterface.h>
#include <dmengine/DM_ENG_ParameterAttributesCache.h>
#include <dmengine/DM_ENG_InformMessageScheduler.h>
#include <dmengine/DM_ENG_Error.h>
#include <dmengine/DM_ENG_SubscriptionStruct.h>
#include "DM_AmxParameterValues.h"
#include "DM_DeviceAdapter.h"
#include "DM_AmxCommon.h"
#include <dmengine/DM_ENG_Device.h>
#include <debug/sahtrace_macros.h>

#define ME "DM_DA"

#define AMX_WAIT_FOR_REPLY_TIMEOUT 3
/* Acs subscription list */
static amxc_llist_t acsSubsList;
//---------------------------------------------------------------------------------------------
/**
 * @addtogroup sah_cwmp_amxdeviceadapter
 * @{
 */
//---------------------------------------------------------------------------------------------

static void set_subscription_new_value(const char* path, const char* value) {
    dm_amx_env_t* amx = DM_ENG_Device_GetACSInfo();
    amxc_string_t expr_path;
    amxc_var_t values, ret;
    int amx_ret;

    amxc_var_init(&values);
    amxc_var_init(&ret);
    amxc_string_init(&expr_path, 0);
    amxc_string_setf(&expr_path, "Device.ManagementServer.Subscription.[ Path == '%s']", path);
    amxc_var_set_type(&values, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &values, "Value", value);
    if((amx_ret = amxb_set(amx->bus_ctx, amxc_string_get(&expr_path, 0), &values, &ret, 0)) != AMXB_STATUS_OK) {
        SAH_TRACEZ_WARNING("DM_DA", "Couldn't set subscription new value (%d)", amx_ret);
    }
    amxc_var_clean(&values);
    amxc_var_clean(&ret);
    amxc_string_clean(&expr_path);
}

void DM_ENG_Device_ACSConnectionHandleNotification(const char* path, const amxc_var_t* const data) {
    DM_ENG_NotificationMode mode = DM_ENG_NotificationMode_OFF;
    int id = -1;
    const amxc_htable_t* htable = NULL;
    const amxc_var_t* parameters = NULL;
    amxc_var_t filtred;
    dm_amx_env_t* acs = DM_ENG_Device_GetACSInfo();
    char** acclist;
    char* acs_path = NULL;
    char* value = NULL;
    DM_ENG_ParameterValueStruct* pvsList = NULL;
    DM_Subscription_t* sub = DM_ENG_Device_Common_FindSubscription(&acsSubsList, path);
    amxc_var_init(&filtred);
    amxc_var_copy(&filtred, data);
    amxa_filter_notif(&filtred, acs->acl_rules);
    when_null(sub, stop);


    parameters = GETP_ARG(&filtred, "parameters");

    htable = amxc_var_constcast(amxc_htable_t, parameters);
    amxc_htable_iterate(hit, htable) {
        amxc_var_t* parameter = NULL;
        const char* key = amxc_htable_it_get_key(hit);
        when_null(key, stop);

        //Do we have a subscription for this parameter
        amxc_llist_for_each(infoit, &sub->subscription_info_list) {
            DM_Subscription_info_t* subscription_info = amxc_container_of(infoit, DM_Subscription_info_t, infoit);
            if(strcmp(subscription_info->parameter, key) == 0) {
                id = subscription_info->uniqueID;
                break;
            }
        }
        when_true(id == -1, stop);

        acs_path = DM_ENG_GetParameterAttributesCacheEllementPath(id);
        when_null(acs_path, stop);

        parameter = amxc_var_from_htable_it(hit);
        value = amxc_var_dyncast(cstring_t, GETP_ARG(parameter, "to"));
        when_true((strncmp(acs_path, "Device.IP.Interface.", 20) == 0) && (!value || !*value ), stop);

        set_subscription_new_value(acs_path, value);

        if(DM_ENG_ValueWasCachedInParameterAttributesCache(acs_path, value) == 0) {
            DM_ENG_ParameterValueStruct* nextParam = NULL;
            DM_ENG_GetParameterAttributesCacheEllement(acs_path, &mode, &acclist);
            SAH_TRACEZ_INFO("DM_DA", "notificationmode for element %s = %d", acs_path, mode);
            if(DM_ENG_Device_GetParameterValues_GetValues(acs, acs_path, &pvsList) != 0) {
                SAH_TRACEZ_ERROR("DM_DA", "Could not get ParameterValueStruct for param %s", acs_path);
                goto stop;
            }
            SAH_TRACEZ_INFO("DM_DA", "send notification %s", acs_path);
            /* Update the inform message scheduler */
            while(pvsList != NULL) {
                nextParam = pvsList->next;
                pvsList->next = NULL;
                SAH_TRACEZ_INFO("DM_ENGINE", "Adding [%s] with mode [%d] to inform message", pvsList->parameterName, mode);
                DM_ENG_InformMessageScheduler_parameterValueChanged(pvsList, mode);
                pvsList = nextParam;
            }
            pvsList = NULL;
        }
    }

stop:
    if(pvsList != NULL) {
        DM_ENG_deleteAllParameterValueStruct(&pvsList);
    }
    amxc_var_clean(&filtred);
    free(value);
    free(acs_path);
    return;
}
/**
   @brief
   Get the valid IGD WAN object path (e.g. InternetGatewayDevice.Wandevice. ... .ExternalIPAddress).

   @details
   Get the valid IGD WAN object path (e.g. InternetGatewayDevice.Wandevice. ... .ExternalIPAddress).

   This routine initiates a Ambiorix request that will return all possible WAN devices.
   In the reply handler we will look for the specified WAN IP address

   @param amx The ambiorix context
   @param addressFound The wan object path
   @param ipaddress The ip address we are looking for

   @return
   - NULL in case of an error
   - the wandevice IGD path
 */
char* DM_ENG_Device_ACSConnectionHandleGetwandevice(dm_amx_env_t* amx, const char* ipaddress, char** addressFound) {
    (void) amx;
    (void) ipaddress;
    (void) addressFound;
    return NULL;
}

bool DM_ENG_Device_ACSConnectionHandleGetwaninterface(UNUSED dm_amx_env_t* amx, const char* ipaddress, char** ppWanInterface) {
    bool ret = false;
    amxc_string_t wanInterface;
    amxc_string_init(&wanInterface, 0);
    amxc_var_t result;
    amxc_var_init(&result);
    amxc_string_t path;
    amxc_string_init(&path, 0);

    if(!ipaddress || !ipaddress[0] || !strcmp(ipaddress, "0.0.0.0")) {
        SAH_TRACE_ERROR("Wrong IP address");
        goto exit;
    }

    amxb_bus_ctx_t* ctx = amxb_be_who_has("IP.");
    when_null(ctx, exit);

    // Check if IPv6 Address
    struct in6_addr res;
    bool isIPv6 = false;
    if(inet_pton(AF_INET6, ipaddress, &res) == 1) {
        isIPv6 = true;
    }

    amxc_string_setf(&path, "IP.Interface.*.%s.[IPAddress=='%s']",
                     isIPv6 ? "IPv6Address" : "IPv4Address", ipaddress);

    int rv = amxb_get(ctx, amxc_string_get(&path, 0), 0, &result, 0);
    if((rv != AMXB_STATUS_OK) || amxc_var_is_null(&result)) {
        SAH_TRACE_ERROR("failed to get WAN interface object");
        goto exit;
    }

    //WAN IP Interface found
    const char* key = amxc_var_key(amxc_var_get_first(GET_ARG(&result, "0")));
    when_null_trace(key, exit, ERROR, "WAN Interface path should not be NULL");
    when_str_empty_trace(key, exit, ERROR, "WAN Interface path should not be empty");

    const char* prefix = DM_ENG_getDatamodelPrefix();
    amxc_string_append(&wanInterface, prefix, strlen(prefix));
    amxc_string_append(&wanInterface, key, strlen(key));
    amxc_string_append(&wanInterface, "IPAddress", 10);
    SAH_TRACE_INFO("[%s] WAN Interface: %s", ipaddress, amxc_string_get(&wanInterface, 0));

    if(!amxc_string_is_empty(&wanInterface)) {
        *ppWanInterface = strdup(amxc_string_get(&wanInterface, 0));
        ret = true;
    }

exit:
    amxc_string_clean(&wanInterface);
    amxc_string_clean(&path);
    amxc_var_clean(&result);
    return ret;
}

//---------------------------------------------------------------------------------------------
/**
   @brief
   Initialize the system connection.

   @details
   Initialize the system connection:
   - create the amx context,
   - connect to the CPE bus (default system bus path can be overwritten by using the environment variable)

   @param amx A pointer to the pcb system bus environment variable

   @return
    - false in case of error
    - true in case of success
 */
bool DM_ENG_Device_ACSConnectionInitialize(dm_amx_env_t* amx) {
    SAH_TRACEZ_IN("DM_DA");

    if(!DM_ENG_Device_Common_AmxConnect(amx, AMXB_BACKEND, AMXB_BACKEND_DEFAULT,
                                        AMXB_URI, AMXB_URI_DEFAULT)) {
        return false; // Connection failed

    }
    amxc_llist_init(&acsSubsList);
    amxb_set_access(amx->bus_ctx, AMXB_PUBLIC);
    SAH_TRACEZ_OUT("DM_DA");
    return true;
}

//---------------------------------------------------------------------------------------------
/**
   @brief
   Cleanup the ACS connection.

   @details
   Cleanup the ACS connection.

   This function currently only closes the ambiorix ACS context, this will also automatically clean all subscriptions,...

   @param amx A pointer to the amx ACS bus environment variable
 */
void DM_ENG_Device_ACSConnectionCleanup(dm_amx_env_t* amx) {
    SAH_TRACEZ_IN("DM_DA");
    DM_ENG_Device_Common_Cleanup_Subscription(&acsSubsList, amx);
    amxc_var_delete(&amx->acl_rules);
    amx->acl_rules = NULL;
    // amx bus connection cleanup
    amxb_free(&amx->bus_ctx);
    amx->bus_ctx = NULL;

    SAH_TRACEZ_OUT("DM_DA");
}

static amxc_var_t* find_subscription_by_path(const char* path) {
    amxc_var_t* var = NULL;
    dm_amx_env_t* amx = DM_ENG_Device_GetSystemInfo();
    amxc_string_t path_expr;

    amxc_var_new(&var);
    amxc_string_init(&path_expr, 0);
    amxc_string_setf(&path_expr, "ManagementServer.Subscription.[ Path == '%s' ]", path);
    if(amxb_get(amx->bus_ctx, amxc_string_get(&path_expr, 0), 0, var, 0) != AMXB_STATUS_OK) {
        amxc_var_delete(&var);
        goto exit;
    }
    if(amxc_htable_is_empty(amxc_var_constcast(amxc_htable_t, GETP_ARG(var, "0.")))) {
        amxc_var_delete(&var);
    }
exit:
    amxc_string_clean(&path_expr);
    return (var);
}

static amxc_var_t* get_subscription_value(const char* path) {
    dm_amx_env_t* amx = DM_ENG_Device_GetACSInfo();
    amxc_var_t* ret = NULL, * tmp;
    amxc_var_t* value = NULL;
    int amx_ret;

    amxc_var_new(&ret);
    if((amx_ret = amxb_get(amx->bus_ctx, path, 0, ret, 0)) != AMXB_STATUS_OK) {
        SAH_TRACEZ_WARNING("DM_DA", "Couldn't get subscription value (%d)", amx_ret);
        goto exit;
    }
    if((tmp = GETI_ARG(ret, 0)) && (tmp = GETI_ARG(tmp, 0)) && (tmp = GETI_ARG(tmp, 0))) {
        amxc_var_new(&value);
        amxc_var_convert(value, tmp, AMXC_VAR_ID_CSTRING);
    }
exit:
    amxc_var_delete(&ret);
    return (value);
}

static void add_subscription_to_dm(const char* path, DM_ENG_NotificationMode mode) {
    amxc_var_t sub, ret, * value;
    dm_amx_env_t* amx = DM_ENG_Device_GetACSInfo();
    int amx_ret;

    amxc_var_init(&sub);
    amxc_var_init(&ret);
    amxc_var_set_type(&sub, AMXC_VAR_ID_HTABLE);
    amxc_var_add_new_key_cstring_t(&sub, "Path", path);
    amxc_var_add_new_key_cstring_t(&sub, "Type", DM_ENG_NotificationModeString(mode));
    if((value = get_subscription_value(path))) {
        amxc_var_add_new_key_cstring_t(&sub, "Value", amxc_var_constcast(cstring_t, value));
        amxc_var_delete(&value);
    }
    if((amx_ret = amxb_add(amx->bus_ctx, "ManagementServer.Subscription.", 0, NULL, &sub, &ret, 1)) != AMXB_STATUS_OK) {
        SAH_TRACEZ_ERROR("DM_DA", "Couldn't add a new subscription (%d)", amx_ret);
    }
    amxc_var_clean(&sub);
    amxc_var_clean(&ret);
}

static void remove_subscription_from_dm(const char* subscriptionPath) {
    dm_amx_env_t* amx = DM_ENG_Device_GetACSInfo();
    amxc_string_t expr_path;
    amxc_var_t ret;
    int amx_ret;

    amxc_var_init(&ret);
    amxc_string_init(&expr_path, 0);
    amxc_string_setf(&expr_path, "Device.ManagementServer.Subscription.[ Path == '%s']", subscriptionPath);
    if((amx_ret = amxb_del(amx->bus_ctx, amxc_string_get(&expr_path, 0), 0, NULL, &ret, 0)) != AMXB_STATUS_OK) {
        SAH_TRACEZ_WARNING("DM_DA", "Couldn't delete subscription %s (%d)", subscriptionPath, amx_ret);
    }
    amxc_string_clean(&expr_path);
    amxc_var_clean(&ret);
}

static bool update_subscription_in_dm(const char* subscriptionPath, const char* notification_mode, DM_ENG_NotificationMode mode) {
    int amx_ret = false;
    if(strcmp(notification_mode, DM_ENG_NotificationModeString(mode)) != 0) {
        SAH_TRACEZ_INFO("DM_DA", "update type for %s", subscriptionPath);
        amxc_string_t expr_path;
        amxc_var_t values, ret;


        amxc_var_init(&values);
        amxc_var_init(&ret);
        amxc_string_init(&expr_path, 0);
        amxc_string_setf(&expr_path, "Device.ManagementServer.Subscription.[ Path == '%s']", subscriptionPath);
        amxc_var_set_type(&values, AMXC_VAR_ID_HTABLE);
        amxc_var_add_key(cstring_t, &values, "Type", DM_ENG_NotificationModeString(mode));
        if((amx_ret = amxb_set(DM_ENG_Device_GetSystemInfo()->bus_ctx, amxc_string_get(&expr_path, 0), &values, &ret, 0)) != AMXB_STATUS_OK) {
            SAH_TRACEZ_WARNING("DM_DA", "Couldn't set subscription new Type (%d)", amx_ret);
        }
        amxc_var_clean(&values);
        amxc_var_clean(&ret);
        amxc_string_clean(&expr_path);
    }
    return amx_ret;
}

//---------------------------------------------------------------------------------------------
/**
   @brief
   Add a parameter subscription.

   @details
   Add a parameter subscription.

   @param amx A pointer to the ambiorix system bus environment variable we want to initialize.
   @param subscriptionPath The TR69 path for which we want to subscribe
   @param subscriptionID The unique subscription ID that identifies the newly created subscription

   @return
   - false in case of an error
   - true if succesfull
 */
bool DM_ENG_Device_ACSConnectionAddSubscription(dm_amx_env_t* amx, const char* subscriptionPath, int* subscriptionID, DM_ENG_NotificationMode mode) {
    SAH_TRACEZ_INFO("DM_DA", "Event subscription [%s]", subscriptionPath);

    bool ret = false;
    amxc_var_t* sub = NULL;

    if(!DM_ENG_Device_Common_IsValidPath(subscriptionPath)) {
        return ret;
    }

    if(!amxa_is_subs_allowed(amx->acl_rules, subscriptionPath, AMXA_PERMIT_SUBS_VAL_CHANGE)) {
        SAH_TRACEZ_ERROR("DM_DA", "notification not allowed for %s", subscriptionPath);
        return ret;
    }

    if((mode == DM_ENG_NotificationMode_FORCED) || (mode == DM_ENG_NotificationMode_ACTIVE)) {
        if(DM_ENG_Device_Common_AddSubscription(&acsSubsList, amx, subscriptionPath,
                                                EVENT_DM_FILTER_OBJECT_CHANGED,
                                                &DM_ENG_Device_ACSConnectionHandleNotification,
                                                subscriptionID) != 0) {
            SAH_TRACEZ_ERROR("DM_DA", "Could not create notification for %s", subscriptionPath);
            return ret;
        }
    }

    if(mode != DM_ENG_NotificationMode_FORCED) {
        sub = find_subscription_by_path(subscriptionPath);
        if(sub == NULL) {
            add_subscription_to_dm(subscriptionPath, mode);
            ret = true;
        } else {
            const char* notification_mode = GETP_CHAR(sub, "0.0.Type");
            when_null(notification_mode, stop);
            ret = update_subscription_in_dm(subscriptionPath, notification_mode, mode);
        }

    }

stop:
    amxc_var_delete(&sub);
    return ret;
}

//---------------------------------------------------------------------------------------------
/**
   @brief
   Remove a parameter subscription.

   @details
   Remove a parameter subscription.

   @param amx A pointer to the ambiorix system bus environment variable we want to initialize.
   @param subscriptionPath The path for which we want to unsubscribe (only used for displaying a trace)
   @param subscriptionID The unique subscription ID that will be used to remove the subscription

   @return
   - false in case of an error
   - true if succesfull
 */
bool DM_ENG_Device_ACSConnectionRemoveSubscription(dm_amx_env_t* amx, const char* subscriptionPath, int subscriptionID) {
    SAH_TRACEZ_INFO("DM_DA", "Removing subscription path=%s id=%d", subscriptionPath, subscriptionID);
    remove_subscription_from_dm(subscriptionPath);
    return (DM_ENG_Device_Common_DeleteSubscription(&acsSubsList, amx, subscriptionID) == 0);
}

//---------------------------------------------------------------------------------------------
/**
   @brief
   This function fetches a list of ManagementServer.Subscription.x instances and returns the
   result to the calling function.

   @details
   this function is called to create a DM_ENG_SubscriptionStruct list of
   items found in ManagementServer.Subscription.x

   The calling function is responsible for cleaning up the resulting pResult.

   @param amx A pointer to the amx system bus environment variable
   @param pResult Array containing a list and descripption of the current subscriptions that is returned to the calling function

   @return
    - TR69 error in case of error
    - 0 in case of success
 */
int DM_ENG_Device_ACSConnectionGetSubscriptions(dm_amx_env_t* amx, DM_ENG_SubscriptionStruct** pResult[]) {

    amxc_var_t subscriptions;
    int error;
    DM_ENG_SubscriptionStruct* tempList = NULL;
    int ret;
    const amxc_htable_t* htable = NULL;
    amxc_var_init(&subscriptions);

    SAH_TRACEZ_INFO("DM_DA", "Getting All Subscriptions from data-model");

    if((ret = amxb_get(amx->bus_ctx, "ManagementServer.Subscription.*.", 1, &subscriptions, 30)) != 0) {
        SAH_TRACEZ_ERROR("DM_DA", "Could not get subscriptions %d", ret);
        error = -1;
        goto stop;
    }

    htable = amxc_var_constcast(amxc_htable_t, GETI_ARG(&subscriptions, 0));

    amxc_htable_iterate(hit, htable) {
        amxc_var_t* subs = amxc_var_from_htable_it(hit);
        const char* path = GET_CHAR(subs, "Path");
        const char* type = GET_CHAR(subs, "Type");

        SAH_TRACEZ_INFO("DM_DA", "Adding subscription: path[%s] type[%s]", path, type);

        DM_ENG_SubscriptionStruct* subscription = DM_ENG_newSubscriptionStruct(path, type);
        if(subscription) {
            DM_ENG_addSubscriptionStruct(&tempList, subscription);
        }
    }
    *pResult = DM_ENG_toSubscriptionStructArray(tempList);
    error = 0;

stop:
    amxc_var_clean(&subscriptions);
    return error;
}

// /** @} */
