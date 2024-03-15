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
#include <stddef.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <debug/sahtrace.h>

#include <amxc/amxc.h>
#include <amxp/amxp.h>
#include <amxd/amxd_dm.h>
#include <amxb/amxb.h>

#include <dmengine/DM_ENG_ParameterValueStruct.h>
#include <dmengine/DM_ENG_NotificationInterface.h>
#include <dmengine/DM_ENG_RPCInterface.h>
#include <dmengine/DM_ENG_InformMessageScheduler.h>
#include <dmengine/DM_ENG_DownloadRequest.h>
#include <dmengine/DM_ENG_Error.h>
#include "DM_DeviceAdapter.h"
#include "DM_AmxCommon.h"

//---------------------------------------------------------------------------------------------
/**
 * @addtogroup sah_cwmp_amxdeviceadapter
 * @{
 */

// Subscription list
static amxc_llist_t systemSubsList;
//---------------------------------------------------------------------------------------------
/**
   @brief
   Handle a parameter change event.

   @details
   This function handles a download request event:
   - If the event is coming from the ManagementServer:
   - if ConnRequestHost or ConnRequestPort changes, the http server is restarted, the inform message scheduler is updated and an inform message scheduled
   - if PeriodicInformInterval or PeriodicInformTime or PeriodicInformEnable changes, re-initialize periodic inform
   - if URL changes, trigger a bootstrap inform
   - if EnableCWMP changes, start/stop the http server, trigger an inform message
   - If the event is coming from the ManagementServer.QueuedTransfers.* objects.
   - if a "status" parameter changes to "Finished", trigger a transfer complete message

   @param path parameter path
   @param data notification data
 */
void DM_ENG_Device_SystemConnectionHandleParameterChanged(const char* path, const amxc_var_t* const data) {
    const amxc_htable_t* htable = NULL;

    if((path == NULL) || (data == NULL)) { //DIE HERE
        SAH_TRACEZ_ERROR("DM_DA", "Received notification with NULL path/data ?");
        return;
    }
    const char* objpath = GETP_CHAR(data, "path");
    const amxc_var_t* parameters = GETP_ARG(data, "parameters");

    //Handle ManagementServer Events
    if(strcmp(path, MANAGEMENTSERVER_PATH) == 0) {
        htable = amxc_var_constcast(amxc_htable_t, parameters);

        if((objpath != NULL) && (strcmp(objpath, "Device.ManagementServer.HeartbeatPolicy.") == 0)) {
            SAH_TRACEZ_INFO("DM_DA", "HeartbeatPolicy config has changed");
            DM_ENG_InformMessageScheduler_Heartbeat_configure();
        }

        amxc_htable_iterate(hit, htable) {
            const char* key = amxc_htable_it_get_key(hit);
            if(key == NULL) {
                break;
            }
            // re-initialize the periodic inform timer with the new values
            if((strcmp("PeriodicInformInterval", key) == 0) ||
               (strcmp("PeriodicInformTime", key) == 0) ||
               (strcmp("PeriodicInformEnable", key) == 0)) {
                DM_ENG_InformMessageScheduler_initializePeriodicInform();
            } else if(strcmp("URL", key) == 0) {
                /* 3.7.1.5 :
                   The specific conditions that MUST result in the BOOTSTRAP EventCode are:
                   ...
                   - First time connection of the CPE to the ACS after the ACS URL has been modified in any way. */
                /* DNS resolution is needed before sending the bootstrap */
                DM_ENG_NotificationInterface_engineEvent(EVENT_ENG_CLEAR_ACS_IP);
                DM_ENG_NotificationInterface_engineEvent(EVENT_ENG_SRV_RESTART);
                DM_ENG_NotificationInterface_engineEvent(EVENT_ENG_URL_CHANGED);
            } else if(strcmp("AllowConnectionRequestFromUnknownHost", key) == 0) {
                DM_ENG_NotificationInterface_engineEvent(EVENT_ENG_SRV_RESTART);
            } else if(strcmp("AllowConnectionRequestFromAddress", key) == 0) {
                DM_ENG_NotificationInterface_engineEvent(EVENT_ENG_SRV_RESTART);
            } else if(strcmp("EnableCWMP", key) == 0) {
                amxc_var_t* parameter = amxc_var_from_htable_it(hit);
                bool enable_cwmp = GETP_BOOL(parameter, "to");
                if(enable_cwmp) {
                    DM_ENG_NotificationInterface_engineEvent(EVENT_ENG_SRV_START);
                } else {
                    DM_ENG_NotificationInterface_engineEvent(EVENT_ENG_SRV_STOP);
                }
                DM_ENG_NotificationInterface_timerStart("send_inform_message", 0, 0, DM_ENG_InformMessageScheduler_sendMessage);
            } else if(strcmp("BlockedEvents", key) == 0) {
                DM_ENG_InformMessageScheduler_blockedEventsChanged();
            } else if(strcmp("InstanceMode", key) == 0) {
                amxc_var_t* parameter = amxc_var_from_htable_it(hit);
                char* value = amxc_var_dyncast(cstring_t, GETP_ARG(parameter, "to"));
                dm_amx_env_t* amx_acs_env = DM_ENG_Device_GetACSInfo();
                SAH_TRACEZ_WARNING("DM_DA", "Instance Mode changed to %s", value);
                if(strcmp(value, "InstanceAlias") == 0) {
                    amx_acs_env->instanceAlias = true;
                } else {
                    amx_acs_env->instanceAlias = false;
                }
                if(value) {
                    free(value);
                    value = NULL;
                }
            } else if((strcmp(key, "ConnRequestHost") == 0) ||
                      (strcmp(key, "ConnRequestPort") == 0)) {
                amxc_var_t* parameter = amxc_var_from_htable_it(hit);
                char* value = NULL;
                DM_ENG_ParameterValueStruct* param = NULL;
                amxc_string_t parameterName;
                amxc_string_init(&parameterName, 0);

                DM_ENG_NotificationInterface_engineEvent(EVENT_ENG_SRV_RESTART);
                // remove the forced parameters from the session queue
                DM_ENG_InformMessageScheduler_removeForcedParametersFromSession();
                value = amxc_var_dyncast(cstring_t, GETP_ARG(parameter, "to"));

                // send out an inform with parameter update
                DM_ENG_NotificationMode mode = DM_ENG_NotificationMode_FORCED;
                // create a fake param structure: forced parameters are added anyway when composing the inform
                amxc_string_setf(&parameterName, "%s%s", objpath, key);
                param = DM_ENG_newParameterValueStruct(amxc_string_get(&parameterName, 0),
                                                       DM_ENG_ParameterType_STRING,
                                                       value);
                if(param == NULL) {
                    SAH_TRACEZ_ERROR("DM_DA", "Could not create ParameterValueStruct");
                } else {
                    // Update the inform message scheduler
                    DM_ENG_InformMessageScheduler_parameterValueChanged(param, mode);
                }
                amxc_string_clean(&parameterName);
                if(value) {
                    free(value);
                }
            }
        }
    }
    //Handle Time plugin Events
    else if(strcmp(path, TIME_PATH) == 0) {
        // here we look only for the Status parameters , ignore others
        const char* syncronized = GETP_CHAR(parameters, "Status.to");
        if(syncronized && (strcmp("Synchronized", syncronized) == 0)) {
            char* periodicInformTime = NULL;
            DM_ENG_InformMessageScheduler_time_synchronized_notification();
            if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM,
                                               DM_ENG_PERIODICINFORMTIME,
                                               &periodicInformTime) == 0) {
                if(periodicInformTime != NULL) {
                    if(strcmp(periodicInformTime, "0001-01-01T00:00:00Z") != 0) {
                        DM_ENG_InformMessageScheduler_initializePeriodicInform();
                    }
                    free(periodicInformTime);
                }
            }
        }
    }
}

//---------------------------------------------------------------------------------------------
/**
   @brief
   Translate the parameter ID into an object name.

   @details
   Translate the parameter ID into an object name.

   @param parameter The parameter ID we are interested in

   @return
    - NULL if an error occurred
    - a pointer to the object name
 */
static const char* DM_ENG_Device_ConvertToObjectName(DM_ENG_SystemParameter_t parameter) {
    switch(parameter) {
    case DM_ENG_USERNAME:
    case DM_ENG_PASSWORD:
    case DM_ENG_URL:
    case DM_ENG_PERIODICINFORMINTERVAL:
    case DM_ENG_PERIODICINFORMTIME:
    case DM_ENG_PERIODICINFORMENABLE:
    case DM_ENG_PERIODICINFORMCOUNTER:
    case DM_ENG_PARAMETERKEY:
    case DM_ENG_ENABLECWMP:
    case DM_ENG_DEFAULTACTIVENOTIFICATIONTHROTTLE:
    case DM_ENG_MANAGEABLEDEVICENOTIFICATIONLIMIT:
    case DM_ENG_INSTANCEWILDCARDSSUPPORTED:
    case DM_ENG_CONNECTIONREQUESTUSERNAME:
    case DM_ENG_CONNECTIONREQUESTPASSWORD:
    case DM_ENG_CONNECTIONREQUESTURL:
    case DM_ENG_INTERFACE:
    case DM_ENG_MAXDOWNLOADDELAY:
    case DM_ENG_MAXUPLOADDELAY:
    case DM_ENG_BOOTPERSISTENTSCHEDULEINFORM:
    case DM_ENG_MAXDOWNLOADS:
    case DM_ENG_MAXDOWNLOADSERRORCODE:
    case DM_ENG_INSTANCEMODE:
    case DM_ENG_AUTOCREATEINSTANCES:
    case DM_ENG_VERIFYPARAMETERTYPE:
        return "Device.ManagementServer.";
    case DM_ENG_MANUFACTURER:
    case DM_ENG_MANUFACTUREROUI:
    case DM_ENG_SERIALNUMBER:
    case DM_ENG_PRODUCTCLASS:
        return "Device.DeviceInfo.";
    case DM_ENG_NTPSTATUS:
        return TIME_PATH;
    case DM_ENG_ACSIPAFFINITY:
    case DM_ENG_ACSADDRFAMILY:
    case DM_ENG_ALLOWCONNECTIONREQUESTFROMADDRESS:
    case DM_ENG_DATAMODEL:
    case DM_ENG_DATAMODELURL:
    case DM_ENG_EMPTYFULLPARAMETERLIST:
    case DM_ENG_MAXSTARTUPDELAY:
    case DM_ENG_RETURNEMPTYLISTONPARIALPATH:
    case DM_ENG_SSLACCEPTSELFSIGNED:
    case DM_ENG_SSLVERIFYHOSTNAME:
    case DM_ENG_SSLACCEPTEXPIRED:
    case DM_ENG_SSLVERIFYPARTIALCHAIN:
    case DM_ENG_PERSISTENTRPCPATH:
    case DM_ENG_UPGRADEBOOTDELAY:
    case DM_ENG_SESSIONTIMEOUT:
    case DM_ENG_VERIFYSUPPORTEDACSMETHODS:
    case DM_ENG_INHIBIT_VALUE_CHANGE_UPON_BOOT:
    case DM_ENG_ACSEVENTS:
    case DM_ENG_DELIVEREDEVENTS:
    case DM_ENG_BLOCKEDEVENTS:
    case DM_ENG_ALLOWMULTIPLESCHEDULEINFORM:
        return "Device.ManagementServer.InternalSettings.";
    case DM_ENG_HEARTBEATENABLE:
    case DM_ENG_HEARTBEATINITIATIONTIME:
    case DM_ENG_HEARTBEATREPORTINGINTERVAL:
        return "Device.ManagementServer.HeartbeatPolicy.";
    case DM_ENG_CONNECTIONREQUESTHOST:
    case DM_ENG_CONNECTIONREQUESTPORT:
    case DM_ENG_CONNECTIONREQUESTPATH:
    case DM_ENG_CONNECTIONREQUESTPATHTYPE:
    case DM_ENG_FREQCONNECTIONREQUEST:
    case DM_ENG_LOCALIPADDRESS:
    case DM_ENG_MAXCONNECTIONREQUEST:
        return "Device.ManagementServer.ConnRequest.";
    case DM_ENG_REBOOTBYACS:
    case DM_ENG_LASTSESSION:
    case DM_ENG_BOOTSTRAPSENT:
    case DM_ENG_FACTORYRESETOCCURRED:
    case DM_ENG_SESSIONSTATUS:
    case DM_ENG_REBOOTCOMMANDKEY:
    case DM_ENG_ACSIPTTL:
    case DM_ENG_ACSIP:
    case DM_ENG_ACSIPLIST:
        return "Device.ManagementServer.State.";
    case DM_ENG_GETPARAMETERVALUEREQUESTS:
    case DM_ENG_SESSIONSSINCEBOOT:
        return "Device.ManagementServer.Stats.";
    default: break;
    }
    return NULL;
}

//---------------------------------------------------------------------------------------------
/**
   @brief
   Translate the parameter ID into a parameter name.

   @details
   Translate the parameter ID into a parameter name.

   @param parameter The parameter ID we are interested in

   @return
    - NULL if an error occurred
    - a pointer to the parameter name
 */
static const char* DM_ENG_Device_ConvertToParameterName(DM_ENG_SystemParameter_t parameter) {
    switch(parameter) {
    case DM_ENG_USERNAME:                          return "Username";
    case DM_ENG_PASSWORD:                          return "Password";
    case DM_ENG_URL:                               return "URL";
    case DM_ENG_ACSIPAFFINITY:                     return "ACSIPAffinity";
    case DM_ENG_ACSIPTTL:                          return "ACSIPTTL";
    case DM_ENG_ACSADDRFAMILY:                     return "ACSAddrFamily";
    case DM_ENG_PERIODICINFORMINTERVAL:            return "PeriodicInformInterval";
    case DM_ENG_PERIODICINFORMTIME:                return "PeriodicInformTime";
    case DM_ENG_PERIODICINFORMENABLE:              return "PeriodicInformEnable";
    case DM_ENG_PERIODICINFORMCOUNTER:             return "PeriodicInformCounter";
    case DM_ENG_PARAMETERKEY:                      return "ParameterKey";
    case DM_ENG_ENABLECWMP:                        return "EnableCWMP";
    case DM_ENG_DEFAULTACTIVENOTIFICATIONTHROTTLE: return "DefaultActiveNotificationThrottle";
    case DM_ENG_INSTANCEWILDCARDSSUPPORTED:        return "InstanceWildcardsSupported";
    case DM_ENG_CONNECTIONREQUESTUSERNAME:         return "ConnectionRequestUsername";
    case DM_ENG_CONNECTIONREQUESTURL:              return "ConnectionRequestURL";
    case DM_ENG_CONNECTIONREQUESTPASSWORD:         return "ConnectionRequestPassword";
    case DM_ENG_HEARTBEATENABLE:                   return "Enable";
    case DM_ENG_HEARTBEATINITIATIONTIME:           return "InitiationTime";
    case DM_ENG_HEARTBEATREPORTINGINTERVAL:        return "ReportingInterval";
    case DM_ENG_CONNECTIONREQUESTHOST:             return "ConnRequestHost";
    case DM_ENG_CONNECTIONREQUESTPORT:             return "ConnRequestPort";
    case DM_ENG_CONNECTIONREQUESTPATH:             return "ConnRequestPath";
    case DM_ENG_CONNECTIONREQUESTPATHTYPE:         return "ConnRequestPathType";
    case DM_ENG_MAXCONNECTIONREQUEST:              return "MaxConnectionRequest";
    case DM_ENG_FREQCONNECTIONREQUEST:             return "FreqConnectionRequest";
    case DM_ENG_SESSIONTIMEOUT:                    return "SessionTimeout";
    case DM_ENG_MANUFACTURER:                      return "Manufacturer";
    case DM_ENG_MANUFACTUREROUI:                   return "ManufacturerOUI";
    case DM_ENG_SERIALNUMBER:                      return "SerialNumber";
    case DM_ENG_PRODUCTCLASS:                      return "ProductClass";
    case DM_ENG_MAXSTARTUPDELAY:                   return "MaxStartupDelay";
    case DM_ENG_REBOOTCOMMANDKEY:                  return "RebootCommandKey";
    case DM_ENG_BOOTSTRAPSENT:
    case DM_ENG_FACTORYRESETOCCURRED:              return "BootstrapSent";
    case DM_ENG_REBOOTBYACS:                       return "RebootByACS";
    case DM_ENG_SESSIONSTATUS:                     return "SessionStatus";
    case DM_ENG_SESSIONSSINCEBOOT:                 return "SessionsSinceReboot";
    case DM_ENG_LASTSESSION:                       return "LastSession";
    case DM_ENG_SSLACCEPTSELFSIGNED:               return "SSLAcceptSelfSigned";
    case DM_ENG_SSLVERIFYHOSTNAME:                 return "SSLVerifyHostname";
    case DM_ENG_SSLACCEPTEXPIRED:                  return "SSLAcceptExpired";
    case DM_ENG_SSLVERIFYPARTIALCHAIN:             return "SSLVerifyPartialChain";
    case DM_ENG_INTERFACE:                         return "Interface";
    case DM_ENG_VERIFYSUPPORTEDACSMETHODS:         return "VerifySupportedACSMethods";
    case DM_ENG_MAXDOWNLOADDELAY:                  return "MaxDownloadDelay";
    case DM_ENG_MAXUPLOADDELAY:                    return "MaxUploadDelay";
    case DM_ENG_BOOTPERSISTENTSCHEDULEINFORM:      return "BootPersistentScheduleInform";
    case DM_ENG_ACSIP:                             return "ACSIP";
    case DM_ENG_ACSIPLIST:                         return "ACSIPList";
    case DM_ENG_ALLOWCONNECTIONREQUESTFROMADDRESS: return "AllowConnectionRequestFromAddress";
    case DM_ENG_GETPARAMETERVALUEREQUESTS:         return "GetParameterValuesRequests";
    case DM_ENG_MAXDOWNLOADS:                      return "MaxDownloads";
    case DM_ENG_UPGRADEBOOTDELAY:                  return "UpgradeBootDelay";
    case DM_ENG_ACSEVENTS:                         return "ACSEvents";
    case DM_ENG_DELIVEREDEVENTS:                   return "DeliveredEvents";
    case DM_ENG_BLOCKEDEVENTS:                     return "BlockedEvents";
    case DM_ENG_MAXDOWNLOADSERRORCODE:             return "MaxDownloadsErrorCode";
    case DM_ENG_NTPSTATUS:                         return "Status";
    case DM_ENG_DATAMODEL:                         return "Datamodel";
    case DM_ENG_DATAMODELURL:                      return "DatamodelURL";
    case DM_ENG_PERSISTENTRPCPATH:                 return "PersistentRPCPath";
    case DM_ENG_ALIASBASEDADDRESSING:              return "AliasBasedAddressing";
    case DM_ENG_INSTANCEMODE:                      return "InstanceMode";
    case DM_ENG_AUTOCREATEINSTANCES:               return "AutoCreateInstances";
    case DM_ENG_LOCALIPADDRESS:                    return "LocalIPAddress";
    case DM_ENG_INHIBIT_VALUE_CHANGE_UPON_BOOT:    return "InhibitValueChangeUponBoot";
    case DM_ENG_RETURNEMPTYLISTONPARIALPATH:       return "ReturnEmptyListOnPartialPath";
    case DM_ENG_EMPTYFULLPARAMETERLIST:            return "EmptyFullParameterList";
    case DM_ENG_ALLOWMULTIPLESCHEDULEINFORM:       return "AllowMultipleScheduleInform";
    case DM_ENG_VERIFYPARAMETERTYPE:               return "VerifyParameterType";
    default: break;
    }
    return NULL;
}

//---------------------------------------------------------------------------------------------
/**
   @brief
   Execute a function in the system datamodel.

   @details
   This function can be used to execute a function in the datamodel:
   - It decodes the specified function ID (enum) into a function name
   - It fetches the object from the datamodel and executes the function


   @param amx A pointer to the ambiorix system bus environment variab
   @param function The function ID we are interested in

   @return
    - false if an error occurred
    - true if the execution was OK
 */
bool DM_ENG_Device_SystemConnectionExecuteFunction(dm_amx_env_t* amx, DM_ENG_SystemFunction_t function) {
    (void) amx;
    (void) function;
    SAH_TRACEZ_ERROR("DM_DA", "DM_ENG_Device_SystemConnectionExecuteFunction");
    bool res = true;
    return res;
}

/**
   @brief
   Set a parameter in the system datamodel.

   @details
   Cwmpd stores a lot of it's configuration parameters in the datamodel.
   This function can be used to set the parameter value in the datamodel:
   - It decodes the specified parameter ID (enum) into an object pah and parameter name
   - It fetches the object from the datamodel and sets the parameter value

   The calling function is responsible for freeing the string that is returned.

   @param amx A pointer to the ambiorix system bus environment variab
   @param parameter The parameter ID we are interested in
   @param pValue The new parameter value

   @return
    - false if an error occurred
    - true if the set was OK
 */
bool DM_ENG_Device_SystemConnectionSetParameter(dm_amx_env_t* amx, DM_ENG_SystemParameter_t parameter, char* pValue) {
    const char* object_name = NULL;
    const char* param_name = NULL;
    int retcode = 0;
    // char * param_path = NULL;
    bool rv = false;
    amxc_var_t ret;
    amxc_var_t set;

    amxc_var_init(&set);
    amxc_var_set_type(&set, AMXC_VAR_ID_HTABLE);
    amxc_var_init(&ret);
    amxc_var_set_type(&ret, AMXC_VAR_ID_HTABLE);

    object_name = DM_ENG_Device_ConvertToObjectName(parameter);
    param_name = DM_ENG_Device_ConvertToParameterName(parameter);

    if(( object_name == NULL) || ( param_name == NULL)) {
        SAH_TRACEZ_ERROR("DM_DA", "Invalid parameter name");
        rv = false;
        goto error;
    }

    SAH_TRACEZ_INFO("DM_DA", "Setting param %s%s to %s", object_name, param_name, pValue);

    amxc_var_add_key(cstring_t, &set, param_name, pValue);
    //amxc_var_dump(&set,STDOUT_FILENO);
    retcode = amxb_set(amx->bus_ctx, object_name, &set, &ret, 1);

    if(retcode != 0) {
        SAH_TRACEZ_ERROR("DM_DA", "object set failed");
        rv = false;
        goto error;
    }
    rv = true;

error:
    amxc_var_clean(&ret);
    amxc_var_clean(&set);
    return rv;
}

//---------------------------------------------------------------------------------------------
/**
   @brief
   Get a parameter needed by the cwmpd engine from the system datamodel.

   @details
   Cwmpd stores a lot of it's configuration parameters in the datamodel.
   This function can be used to fetch the parameter value from the datamodel:
   - It decodes the specified parameter ID (enum) into an object path and parameter name
   - It fetches the object from the datamodel and extracts the parameter value

   The caller of this function is responsible for freeing the string that is returned.

   @param amx A pointer to the ambiorix system bus environment variable
   @param parameter The parameter ID we are interested in

   @return
    - NULL in case of error
    - pointer to a string containing the parameter value
 */
char* DM_ENG_Device_SystemConnectionGetParameter(dm_amx_env_t* amx, DM_ENG_SystemParameter_t parameter) {
    const char* object_name = NULL;
    const char* param_name = NULL;
    char* ret = NULL;
    int retcode = 0;
    amxc_var_t* var = NULL;
    amxc_var_t value;
    amxc_string_t path;
    amxc_var_init(&value);
    amxc_string_init(&path, 0);

    object_name = DM_ENG_Device_ConvertToObjectName(parameter);
    param_name = DM_ENG_Device_ConvertToParameterName(parameter);

    if(( object_name == NULL) || ( param_name == NULL)) {
        SAH_TRACEZ_ERROR("DM_DA", "Invalid parameter name");
        goto error;
    }

    amxc_string_setf(&path, "%s%s", object_name, param_name);
    retcode = amxb_get(amx->bus_ctx, amxc_string_get(&path, 0), 0, &value, 1);

    if((retcode != 0) || amxc_var_is_null(&value)) {
        SAH_TRACEZ_ERROR("DM_DA", "Failed to get parameter value (%s%s)", object_name, param_name);
        goto error;
    }

    amxc_string_clean(&path);
    amxc_string_setf(&path, "0.'%s'.%s", object_name, param_name);
    var = GETP_ARG(&value, amxc_string_get(&path, 0));
    ret = amxc_var_dyncast(cstring_t, var);

error:
    amxc_var_clean(&value);
    amxc_string_clean(&path);
    return ret;
}

//---------------------------------------------------------------------------------------------
/**
   @brief
   Sleep a random(ManagementServer.MAXStartupDelay) seconds before proceeding.

   @details
   Fetch the DM_ENG_MAXSTARTUPDELAY from the ManagementServer plugin.
   Use this number to create a random value between 0 - DM_ENG_MAXSTARTUPDELAY.
   Sleep for the calculated number of seconds.

 */
static void DM_ENG_Device_SystemConnectionSleepBeforeStarting() {
    char* tmp = NULL;
    int max = 0;
    int upgradesAvailable = 0;
    int upgradeBootDelay = 0;
    int initialDelay = 0;
    unsigned int r = 0;
    unsigned int randomValue = 0;

    if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_MAXSTARTUPDELAY, &tmp) == 0) {
        if(tmp) {
            max = atoi(tmp);
        }
        free(tmp);
    }

    // TODO : get upgradesAvailable from Device.UserInterface.UpgradeAvailable

    if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_UPGRADEBOOTDELAY, &tmp) == 0) {
        if(tmp) {
            upgradeBootDelay = atoi(tmp);
        }
        free(tmp);
    }

    // In case there are upgrades, add the initial boot delay to avoid sending out 2 informs
    if(upgradesAvailable) {
        initialDelay = upgradeBootDelay;
    }

    if(initialDelay > 60 * 60) {
        initialDelay = 60 * 60;
    }

    int randomData = open("/dev/urandom", O_RDONLY);
    if(randomData < 0) {
        SAH_TRACEZ_INFO("DM_DA", "Failed to read /dev/urandom");
    } else {

        int bytes = read(randomData, &r, sizeof r);
        if(bytes <= 0) {
            SAH_TRACEZ_ERROR("DM_DA", "failed to read /dev/urandom - %s", strerror(errno));
        }
        close(randomData);
    }
    if(max == 0) {
        //avoid divistion by 0
        max = 1;
    }
    randomValue = r % max;
    if(randomValue > 60 * 60) {
        randomValue = 60 * 60;
    }

    unsigned int sleepTime = initialDelay + randomValue;
    sleep(sleepTime);
}

//---------------------------------------------------------------------------------------------
/**
   @brief
   Initialize the system connection.

   @details
   Initialize the system connection:
   - create the amx context,
   - connect to the system bus (default system bus path can be overwritten by using the PCB_SYS_BUS environment variable)
   - wait for the ManagementServer object
   - wait for the DeviceInfo object
   - fetch the ManagementServer object into the cache
   - fetch the DeviceInfo object into the cache
   - sleep for random(MAXSTARTUPDELAY) number of seconds
   - create subscription on MANAGEMENTSERVER_PATH
   - create subscription on DEVICEINFO_PATH
   - create subscription on MANAGEMENTSERVER_TRANSFERS_NODE
   - create subscription on TIME_PATH

   @param amx A pointer to the amx system bus environment variable

   @return
    - false in case of error
    - true in case of success
 */
bool DM_ENG_Device_SystemConnectionInitialize(dm_amx_env_t* amx) {
    int cnt = 0;
    int retcode = 0;
    int id = 0;
    bool ret = false;
    const char* devstatus = NULL;
    // check the device status
    amxc_var_t value;
    amxc_string_t filter;
    amxc_string_t path;
    amxc_string_t valpath;
    amxc_var_init(&value);
    amxc_string_init(&path, 0);
    amxc_string_init(&valpath, 0);
    amxc_string_init(&filter, 0);

    if(!DM_ENG_Device_Common_AmxConnect(amx, AMXB_BACKEND, AMXB_BACKEND_DEFAULT,
                                        AMXB_URI, AMXB_URI_DEFAULT)) {
        ret = false;
        GotoStop("Connection to bus failed");
    }
    amxb_set_access(amx->bus_ctx, AMXB_PROTECTED);

    DM_ENG_Device_SystemConnectionSleepBeforeStarting();

    amxc_string_setf(&path, "%s%s", DEVICEINFO_PATH, "DeviceStatus");
    amxc_string_setf(&valpath, "0.'%s'.%s", DEVICEINFO_PATH, "DeviceStatus");

    do {
        retcode = amxb_get(amx->bus_ctx, amxc_string_get(&path, 0), 0, &value, 1);
        if((retcode < 0) || amxc_var_is_null(&value)) {
            break;// giveUP
        }

        devstatus = GETP_CHAR(&value, amxc_string_get(&valpath, 0));
        cnt++;
        /* 5sec * 24 = 2min */
        if(cnt > 24) {
            // this takes too long, fall trough: sending an inform with a missing
            // subscription is less important then not sending out an inform at all.
            break;
        }
        // wait if device is still down
        if(devstatus && (strcmp(devstatus, "Up") != 0)) {
            sleep(5);
        }

    } while(devstatus && strcmp(devstatus, "Up") != 0);

    amxc_llist_init(&systemSubsList);
    /* Create the notifications */
    if(DM_ENG_Device_Common_AddSubscription(&systemSubsList, amx, MANAGEMENTSERVER_PATH,
                                            EVENT_DM_FILTER_OBJECT_CHANGED,
                                            &DM_ENG_Device_SystemConnectionHandleParameterChanged,
                                            &id) != 0) {
        SAH_TRACEZ_ERROR("DM_DA", "Could not create notification for %s", MANAGEMENTSERVER_PATH);
        goto stop;
    }

    /* Create the notifications */
    if(DM_ENG_Device_Common_AddSubscription(&systemSubsList, amx, TIME_PATH,
                                            EVENT_DM_FILTER_OBJECT_CHANGED,
                                            &DM_ENG_Device_SystemConnectionHandleParameterChanged,
                                            &id) != 0) {
        SAH_TRACEZ_ERROR("DM_DA", "Could not create notification for %s", TIME_PATH);
        goto stop;
    }
    if(DM_ENG_SetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_SESSIONSTATUS, "Idle") != 0) {
        SAH_TRACEZ_ERROR("DM_DA", "Could not set Session Status in the datamodel");
    }
    ret = true;

stop:

    amxc_var_clean(&value);
    amxc_string_clean(&path);
    amxc_string_clean(&valpath);
    amxc_string_clean(&filter);
    return ret;
}

//---------------------------------------------------------------------------------------------
/**
   @brief
   Cleanup the system connection.

   @details
   Cleanup the system connection.

   This function currently only clean the amx system context

   @param amx A pointer to the ambiorix system bus environment variable
 */
void DM_ENG_Device_SystemConnectionCleanup(dm_amx_env_t* amx) {
    DM_ENG_Device_Common_Cleanup_Subscription(&systemSubsList, amx);

    // amx bus connection cleanup
    amxb_free(&amx->bus_ctx);
    amx->bus_ctx = NULL;
}

/** @} */
