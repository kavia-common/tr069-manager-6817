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

#include <debug/sahtrace.h>

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

#define AMX_WAIT_FOR_REPLY_TIMEOUT 3

//---------------------------------------------------------------------------------------------
/**
 * @addtogroup sah_cwmp_amxdeviceadapter
 * @{
 */
//---------------------------------------------------------------------------------------------
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
        return false;  // Connection failed

    }
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
    (void) amx;
    (void) subscriptionPath;
    (void) subscriptionID;
    SAH_TRACEZ_INFO("DM_DA", "Event subscription not yet implemented");
    return false;
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
bool DM_ENG_Device_ACSConnectionRemoveSubscription(dm_amx_env_t* amx_env, const char* subscriptionPath, int subscriptionID) {
    (void) amx_env;

    SAH_TRACEZ_INFO("DM_DA", "Removing subscription path=%s id=%d", subscriptionPath, subscriptionID);
    return false;
}



// /** @} */
