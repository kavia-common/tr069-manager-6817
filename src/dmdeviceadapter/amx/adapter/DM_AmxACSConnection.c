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
#include <amxp/amxp.h>
#include <amxd/amxd_dm.h>
#include <amxb/amxb.h>

#include <dmengine/DM_ENG_NotificationInterface.h>
#include <dmengine/DM_ENG_RPCInterface.h>
#include <dmengine/DM_ENG_ParameterAttributesCache.h>
#include <dmengine/DM_ENG_InformMessageScheduler.h>
#include <dmengine/DM_ENG_Error.h>
#include "DM_AmxParameterValues.h"
#include "DM_DeviceAdapter.h"
#include "DM_AmxCommon.h"
#include <dmengine/DM_ENG_Device.h>

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

void DM_ENG_Device_ACSConnectionHandleNotification(const char* path, const amxc_var_t* const data) {
    SAH_TRACEZ_INFO("DM_DA", "notification event path is [%s]", path);
    DM_ENG_NotificationMode mode = DM_ENG_NotificationMode_OFF;
    int id = -1;
    const amxc_htable_t* htable = NULL;
    const amxc_var_t* parameters = GETP_ARG(data, "parameters");
    char** acclist;
    DM_ENG_ParameterValueStruct* pvsList = NULL;
    DM_Subscription_t* sub = DM_ENG_Device_Common_FindSubscription(&acsSubsList, path);

    if(sub == NULL) {
        return;
    }

    htable = amxc_var_constcast(amxc_htable_t, parameters);

    amxc_htable_iterate(hit, htable) {
        const char* acs_path = NULL;
        char* value = NULL;
        amxc_var_t* parameter = NULL;
        const char* key = amxc_htable_it_get_key(hit);
        if(key == NULL) {
            break;
        }

        //Do we have a subscription for this parameter
        amxc_llist_for_each(infoit, &sub->subscription_info_list) {
            DM_Subscription_info_t* subscription_info = amxc_container_of(infoit, DM_Subscription_info_t, infoit);
            if(strcmp(subscription_info->parameter, key) == 0) {
                id = subscription_info->uniqueID;
                break;
            }
        }

        if(id == -1) {
            return;
        }

        acs_path = DM_ENG_GetParameterAttributesCacheEllementPath(id);
        if(acs_path == NULL) {
            // return there is no subscription for this parameter
            return;
        }

        parameter = amxc_var_from_htable_it(hit);
        value = amxc_var_dyncast(cstring_t, GETP_ARG(parameter, "to"));

        if(DM_ENG_ValueWasCachedInParameterAttributesCache((char*) acs_path, value) == 0) {
            DM_ENG_GetParameterAttributesCacheEllement((char*) acs_path, &mode, &acclist);
            SAH_TRACEZ_INFO("DM_DA", "notificationmode for element %s = %d", acs_path, mode);
            dm_amx_env_t* acs = DM_ENG_Device_GetACSInfo();
            if(DM_ENG_Device_GetParameterValues_GetValues(acs, (char*) acs_path, &pvsList) != 0) {
                SAH_TRACEZ_ERROR("DM_DA", "Could not get ParameterValueStruct for param %s", acs_path);
                free(value);
                return;
            }
            SAH_TRACEZ_INFO("DM_DA", "send notification %s", acs_path);
            /* Update the inform message scheduler */
            DM_ENG_InformMessageScheduler_parameterValueChanged(pvsList, mode);
        }
        free(value);
    }
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

    // Check if IPv6 Address
    struct in6_addr res;
    bool isIPv6 = false;
    if(inet_pton(AF_INET6, ipaddress, &res) == 1) {
        isIPv6 = true;
    }

    amxc_string_setf(&path, "IP.Interface.[Alias=='wan'].%s.[IPAddress=='%s']",
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
    // amx bus connection cleanup
    amxb_free(&amx->bus_ctx);
    amx->bus_ctx = NULL;

    SAH_TRACEZ_OUT("DM_DA");
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
bool DM_ENG_Device_ACSConnectionAddSubscription(dm_amx_env_t* amx, const char* subscriptionPath, int* subscriptionID) {
    SAH_TRACEZ_INFO("DM_DA", "Event subscription [%s]", subscriptionPath);

    const char* internalPath = DM_ENG_Device_Common_ACSToAMXPath_noalloc(subscriptionPath);

    if(DM_ENG_Device_Common_AddSubscription(&acsSubsList, amx, internalPath,
                                            EVENT_DM_FILTER_OBJECT_CHANGED,
                                            &DM_ENG_Device_ACSConnectionHandleNotification,
                                            subscriptionID) != 0) {
        SAH_TRACEZ_ERROR("DM_DA", "Could not create notification for %s", internalPath);
        return false;
    }
    return true;
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
    return (DM_ENG_Device_Common_DeleteSubscription(&acsSubsList, amx, subscriptionID) == 0);
}

// /** @} */
