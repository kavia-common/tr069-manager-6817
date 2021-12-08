
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
#ifndef __DM_ADAPTER_AMXCOMMON_H__
#define __DM_ADAPTER_AMXCOMMON_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <amxc/amxc.h>
#include <amxp/amxp.h>
#include <amxd/amxd_dm.h>
#include <amxd/amxd_path.h>
#include <amxd/amxd_object.h>
#include <amxb/amxb.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dmengine/DM_ENG_ParameterType.h>
#include "DM_DeviceAdapter.h"

#define EVENT_DM_OBJECT_CHANGED         "dm:object-changed"
#define EVENT_DM_INSTANCE_ADDED         "dm:instance-added"
#define EVENT_DM_INSTANCE_DELETED       "dm:instance-removed"
#define EVENT_DM_FILTER_OBJECT_CHANGED  "notification in ['dm:object-changed']"
#define EVENT_DM_FILTER_TEMPLATE        "notification in ['%s'] && parameters.%s == '%s'"

#define EVENT_ENG_SRV_RESTART      "HTTP_SERVER_RESTART"
#define EVENT_ENG_SRV_STOP         "HTTP_SERVER_STOP"
#define EVENT_ENG_SRV_START        "HTTP_SERVER_START"
#define EVENT_ENG_CLEAR_ACS_IP     "CLIENT_CLEAR_ACS_IP"

// tr181-device name on the bus
#define TR181_DEVICE_OBJNAME "Device."

#define SetErrorGotoStop(errorNumber, message) \
    { error = errorNumber; SAH_TRACEZ_ERROR("DM_DA", message); goto stop; }

#define GotoStop(message) \
    { SAH_TRACEZ_ERROR("DM_DA", message); goto stop; }

typedef void (* notification_cb_t)(const char* path, const amxc_var_t* const data);

typedef struct DM_Subscription_info {
    int uniqueID;         // unique id
    char* parameter;      // path to object
    amxc_llist_it_t infoit;
} DM_Subscription_info_t;

typedef struct DM_Subscription {
    char* objectpath;                    // path to object
    notification_cb_t cb;                // callback used when create subscription
    amxc_llist_t subscription_info_list; //list of pair id:parameter
    amxc_llist_it_t it;
} DM_Subscription_t;

const char* DM_ENG_Device_Common_ACSToAMXPath_noalloc(const char* acsPath);
char* DM_ENG_Device_Common_ACSToAMXPath(const char* acsPath);
bool DM_ENG_Device_Common_CheckSystem(dm_amx_env_t* amx);
bool DM_ENG_Device_Common_AmxConnect(dm_amx_env_t* amx, const char* envVariable, const char* defaultLocation, const char* envURI, const char* defaultURI);
bool DM_ENG_Device_Common_IsWildcardPath(const char* path);
bool DM_ENG_Device_Common_IsWildcardPathValid(const char* path);
bool DM_ENG_Device_Common_Resolve_Path(dm_amx_env_t* amx, const char* path, amxc_var_t* resolved);
DM_ENG_ParameterType DM_ENG_Device_Common_ConvertParameterType(u_int32_t type);
const char* DM_ENG_Device_Common_GetRootParameterInternalPath(const char* path);
bool DM_ENG_Device_Common_IsRootParameter(const char* path);
int DM_ENG_Device_Common_AddSubscription(amxc_llist_t* list, dm_amx_env_t* amx, const char* path, const char* filter, notification_cb_t cb, int* subscriptionID);
int DM_ENG_Device_Common_DeleteSubscription(amxc_llist_t* list, dm_amx_env_t* amx, int id);
DM_Subscription_t* DM_ENG_Device_Common_FindSubscriptionByID(amxc_llist_t* list, int id);
DM_Subscription_t* DM_ENG_Device_Common_FindSubscription(amxc_llist_t* list, const char* path);
void DM_ENG_Device_Common_Cleanup_Subscription(amxc_llist_t* slist, dm_amx_env_t* amx);

#ifdef __cplusplus
}
#endif

#endif // __DM_ADAPTER_AMXCOMMON_H__
