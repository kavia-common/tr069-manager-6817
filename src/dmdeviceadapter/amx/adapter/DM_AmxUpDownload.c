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
#include <stdio.h>
#include <stdlib.h>


#include <debug/sahtrace.h>
#include <string.h>
#include <time.h>
#include <dmengine/DM_ENG_AllQueuedTransferStruct.h>
#include <dmengine/DM_ENG_InformMessageScheduler.h>
#include <dmengine/DM_ENG_Error.h>

#include "DM_AmxCommon.h"
#include "DM_AmxUpDownload.h"
#include "DM_AmxSystemConnection.h"
#include "DM_DeviceAdapter.h"

#define TRANSFER_ROOT_PATH "ManagementServer.ACSTransfers."
#define TRANSFER_ENTRY_PATH TRANSFER_ROOT_PATH "ACSTransfer."

// Subscription list
static amxc_llist_t transferSubsList;

//---------------------------------------------------------------------------------------------
/**
 * @addtogroup sah_cwmp_amxdeviceadapter
 * @{
 */

//---------------------------------------------------------------------------------------------

static int DM_ENG_Device_new_dm_entry(amxb_bus_ctx_t* bus_ctx, amxc_var_t* args, int* index) {
    int rv = -1;
    amxc_var_t ret;
    amxc_var_init(&ret);

    when_null(bus_ctx, stop);
    when_null(args, stop);
    when_null(index, stop);

    when_failed(amxb_add(bus_ctx, TRANSFER_ENTRY_PATH, 0, NULL, args, &ret, 2), stop);
    *index = GET_INT32(GETI_ARG(&ret, 0), "index");
    rv = 0;

stop:
    if(rv != 0) {
        SAH_TRACEZ_ERROR("DM_DA", "failed to add instance [%s]", TRANSFER_ENTRY_PATH);
    }
    amxc_var_clean(&ret);
    return rv;
}

static void DM_ENG_Device_UpdateTransferState(amxb_bus_ctx_t* bus_ctx, const char* path, uint32_t faultCode, const char* faultString, const char* completeTime) {
    amxc_var_t set;
    amxc_var_t ret;
    amxc_var_init(&set);
    amxc_var_init(&ret);

    when_str_empty(path, stop);
    when_null(bus_ctx, stop);

    amxc_var_set_type(&set, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &set, "FaultString", faultString);
    amxc_var_add_key(uint32_t, &set, "FaultCode", faultCode);
    amxc_var_add_key(cstring_t, &set, "Status", "Finished");
    if((completeTime != NULL) && (strcmp(completeTime, "") != 0)) {
        amxc_var_add_key(cstring_t, &set, "CompleteTime", completeTime);
    }

    if(amxb_set(bus_ctx, path, &set, &ret, 1) != 0) {
        SAH_TRACEZ_ERROR("DM_DA", "Failed to update file transfer state");
    }
stop:
    amxc_var_clean(&set);
    amxc_var_clean(&ret);
}

static DM_ENG_TransferCompleteStruct* DM_ENG_Device_BuildTransferCompleteResponse(amxc_var_t* transfer, const char* path) {
    DM_ENG_TransferCompleteStruct* tcs = NULL;
    time_t startTimeUF = 0;
    time_t completeTimeUF = 0;
    bool autonomousTransfer = false;
    amxc_string_t pname;
    const char* commandKey = NULL;
    const char* transferURL = NULL;
    const char* status = NULL;
    const char* fileType = NULL;
    const char* targetFileName = NULL;
    const char* startTime = NULL;
    const char* completeTime = NULL;
    const char* faultString = NULL;
    const char* initiator = NULL;
    uint32_t fileSize = 0;
    int32_t faultCode = -1;
    bool isDownload = false;
    amxc_var_t* transfer_data = NULL;

    amxc_string_init(&pname, 0);
    amxc_string_setf(&pname, "0.'%s'", path);
    transfer_data = GETP_ARG(transfer, amxc_string_get(&pname, 0));
    initiator = GETP_CHAR(transfer_data, "Initiator");

    if(initiator && (strncmp(initiator, "Autonomous", strlen("Autonomous")) == 0)) {
        autonomousTransfer = true;
    }

    commandKey = GETP_CHAR(transfer_data, "CommandKey");
    transferURL = GETP_CHAR(transfer_data, "Url");
    status = GETP_CHAR(transfer_data, "Status");
    isDownload = GETP_BOOL(transfer_data, "IsDownload");
    fileType = GETP_CHAR(transfer_data, "FileType");
    fileSize = GETP_INT32(transfer_data, "FileSize");
    targetFileName = GETP_CHAR(transfer_data, "TargetFileName");
    startTime = GETP_CHAR(transfer_data, "StartTime");
    completeTime = GETP_CHAR(transfer_data, "CompleteTime");
    faultCode = GETP_INT32(transfer_data, "FaultCode");
    faultString = GETP_CHAR(transfer_data, "FaultString");

    SAH_TRACEZ_INFO("DM_DA", "TransferComplete: key:%s status:%s url:%s faultcode:%d",
                    commandKey, status, transferURL, faultCode);
    SAH_TRACEZ_INFO("DM_DA", "----------------: type:%s size:%d targetfileName:%s isDownload:%s",
                    fileType, fileSize, targetFileName, isDownload ? "DOWNLOAD" : "UPLOAD");
    SAH_TRACEZ_INFO("DM_DA", "----------------: startTime:%s | endTime:%s faultString:%s",
                    startTime, completeTime, faultString);

    DM_ENG_dateStringToTime((char*) startTime, &startTimeUF);
    DM_ENG_dateStringToTime((char*) completeTime, &completeTimeUF);

    tcs = DM_ENG_newTransferCompleteStruct(autonomousTransfer, "announceURL", transferURL, fileType,
                                           fileSize, targetFileName, isDownload, false, commandKey,
                                           faultCode, faultString, startTimeUF, completeTimeUF, path);

    amxc_string_clean(&pname);
    return tcs;
}

static void DM_ENG_Device_TransferCompleteEvent(char* path) {
    int32_t uid = -1;
    DM_ENG_TransferCompleteStruct* tcs = NULL;
    amxc_var_t transfer_object;
    amxc_string_t pname;
    amxc_string_init(&pname, 0);
    amxc_var_init(&transfer_object);
    dm_amx_env_t* amx = DM_ENG_Device_GetSystemInfo();
    when_null(path, stop);

    if(amxb_get(amx->bus_ctx, path, 0, &transfer_object, 1) != 0) {
        GotoStop("object already deleted [%s]", path);
    }

    amxc_string_setf(&pname, "0.'%s'.%s", path, "CommandKey");
    const char* commandKey = GETP_CHAR(&transfer_object, amxc_string_get(&pname, 0));
    DM_ENG_NotificationInterface_timerStop(commandKey);
    tcs = DM_ENG_Device_BuildTransferCompleteResponse(&transfer_object, path);
    when_null(tcs, stop);
    SAH_TRACEZ_INFO("DM_DA", "Sending a transfer complete event, commandkey=%s, path=%s, faultstring=%s, faultcode=%d", commandKey, path, tcs->faultString, tcs->faultCode);
    DM_ENG_InformMessageScheduler_transferComplete(tcs);

    amxc_string_setf(&pname, "0.'%s'.%s", path, "SubscriptionId");
    uid = GETP_INT32(&transfer_object, amxc_string_get(&pname, 0));

    if(DM_ENG_Device_Common_DeleteSubscription(&transferSubsList, amx, uid) != 0) {
        SAH_TRACEZ_ERROR("DM_DA", "Failed to remove subscription for [%s], uid %d", path, uid);
    }

stop:
    amxc_var_clean(&transfer_object);
    amxc_string_clean(&pname);
}

/**
   @brief
   Called when a transfer initiated by the ACS has timed out

   @details
   This function is called when a transfer initiated by the ACS has timed out.

   In case the transfer is not locked and in still in the initial state, it retrieves the information
   of the transfer that timed out and triggers the engine to send out a transfercomplete event.

   @param name The object path of the download that timed out

   @return
    - -1 in case of error
    - 0 in case of success
 */
void DM_ENG_Device_TransferTimedOut(char* cmdkey) {
    amxc_var_t transfers;
    const amxc_htable_t* htable = NULL;
    dm_amx_env_t* dm_system = DM_ENG_Device_GetSystemInfo();
    amxc_var_init(&transfers);

    when_null(cmdkey, stop);
    SAH_TRACEZ_INFO("DM_DA", "Transfer [%s] has timedout", cmdkey);

    if(amxb_get(dm_system->bus_ctx, TRANSFER_ENTRY_PATH "*.", 1, &transfers, 5) != 0) {
        GotoStop("Failed to read transfers from dm");
    }
    htable = amxc_var_constcast(amxc_htable_t, GETI_ARG(&transfers, 0));

    amxc_htable_iterate(hit, htable) {
        const char* key = amxc_htable_it_get_key(hit);
        amxc_var_t* transfer = amxc_var_from_htable_it(hit);
        const char* commandKey = GETP_CHAR(transfer, "CommandKey");
        const char* status = GETP_CHAR(transfer, "Status");
        int32_t uid = GETP_INT32(transfer, "SubscriptionId");

        if(commandKey && (strcmp(commandKey, cmdkey) == 0)) {
            //Remove the subscription
            if(DM_ENG_Device_Common_DeleteSubscription(&transferSubsList, dm_system, uid) != 0) {
                SAH_TRACEZ_ERROR("DM_DA", "Failed to remove Subscription for [%s]", key);
            }

            //Notify transfer Complete
            if(status && strcmp(status, "Initial")) {
                DM_ENG_Device_UpdateTransferState(dm_system->bus_ctx, key,
                                                  GETP_BOOL(transfer, "IsDownload") ? DM_ENG_DOWNLOAD_FAILURE : DM_ENG_UPLOAD_FAILURE,
                                                  "Transfer timed out", NULL);
            }

            DM_ENG_Device_TransferCompleteEvent((char*) key);
            break;
        }
    }
stop:
    amxc_var_clean(&transfers);
}



void DM_ENG_Device_TransferNotification(UNUSED const char* path, const amxc_var_t* const data) {
    const amxc_htable_t* htable = NULL;
    const char* objpath = NULL;
    const amxc_var_t* parameters = NULL;

    when_null(data, stop);

    objpath = GETP_CHAR(data, "path");
    parameters = GETP_ARG(data, "parameters");
    htable = amxc_var_constcast(amxc_htable_t, parameters);

    amxc_htable_iterate(hit, htable) {
        const char* key = amxc_htable_it_get_key(hit);
        if(key && (strcmp("Status", key) == 0)) {
            amxc_var_t* parameter = amxc_var_from_htable_it(hit);
            const char* transfer_state = GETP_CHAR(parameter, "to");

            if(transfer_state && (strcmp("Finished", transfer_state) == 0)) {
                SAH_TRACEZ_INFO("DM_DA", "Transfer Completed Event -> [%s]", objpath);
                DM_ENG_Device_TransferCompleteEvent((char*) objpath);
            }
        }
    }
stop:
    return;
}

static int DM_ENG_Device_AddTransferSubscription(dm_amx_env_t* amx, int instance) {
    int error = 0;
    int uid = -1;
    amxc_string_t instancePath;
    amxc_var_t set;
    amxc_var_t ret;

    amxc_var_init(&ret);
    amxc_var_init(&set);
    amxc_var_set_type(&set, AMXC_VAR_ID_HTABLE);
    amxc_var_set_type(&ret, AMXC_VAR_ID_HTABLE);
    amxc_string_init(&instancePath, 0);
    amxc_string_setf(&instancePath, TRANSFER_ENTRY_PATH "%d.", instance);
    when_null(amx, stop);

    if(DM_ENG_Device_Common_AddSubscription(&transferSubsList, amx, amxc_string_get(&instancePath, 0),
                                            EVENT_DM_FILTER_OBJECT_CHANGED,
                                            &DM_ENG_Device_TransferNotification,
                                            &uid) != 0) {
        SetErrorGotoStop(-1, "Could not create notification for %s", amxc_string_get(&instancePath, 0));
    }

    SAH_TRACEZ_INFO("DM_DA", "Subscription OK [%s], id=%d, uid %d", amxc_string_get(&instancePath, 0), instance, uid);
    amxc_var_add_key(int32_t, &set, "SubscriptionId", uid);

    if(amxb_set(amx->bus_ctx, amxc_string_get(&instancePath, 0), &set, &ret, 1) != 0) {
        SAH_TRACEZ_INFO("DM_DA", "Failed to set the SubscriptionId for %s", amxc_string_get(&instancePath, 0));
    }
stop:
    amxc_var_clean(&ret);
    amxc_var_clean(&set);
    amxc_string_clean(&instancePath);
    return error;
}

static int DM_ENG_Device_AddTransfer(dm_amx_env_t* amx,
                                     const char* commandkey,
                                     const char* fileType,
                                     const char* url,
                                     const char* username,
                                     const char* password,
                                     unsigned int fileSize,
                                     const char* targetFileName,
                                     unsigned int delayseconds,
                                     const char* successURL,
                                     const char* failureURL,
                                     bool isDownload,
                                     int instanceID) {
    amxc_var_t ret;
    amxc_var_t args;
    amxc_var_t object;
    amxc_ts_t ts_start_time;
    amxc_ts_t ts_complete_time;
    int instance_id = instanceID;
    int error = DM_ENG_REQUEST_DENIED;

    amxc_var_init(&ret);
    amxc_var_init(&args);
    amxc_var_init(&object);

    SAH_TRACEZ_INFO("DM_DA", "ADD New transfer %s", commandkey);

    amxc_ts_now(&ts_start_time);
    ts_start_time.sec += delayseconds;
    amxc_ts_now(&ts_complete_time);
    ts_complete_time.sec += 3600;

    amxc_var_set_type(&args, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &args, "Initiator", "ACS");
    amxc_var_add_key(cstring_t, &args, "CommandKey", commandkey);
    amxc_var_add_key(bool, &args, "IsDownload", isDownload);
    amxc_var_add_key(cstring_t, &args, "Url", url);
    amxc_var_add_key(cstring_t, &args, "FileType", fileType);
    amxc_var_add_key(uint32_t, &args, "FileSize", fileSize);
    amxc_var_add_key(cstring_t, &args, "TargetFileName", targetFileName);
    amxc_var_add_key(cstring_t, &args, "SaveFileName", targetFileName);
    amxc_var_add_key(cstring_t, &args, "Username", username);
    amxc_var_add_key(cstring_t, &args, "Password", password);
    amxc_var_add_key(cstring_t, &args, "SuccessURL", successURL);
    amxc_var_add_key(cstring_t, &args, "FailureURL", failureURL);
    amxc_var_add_key(uint32_t, &args, "FaultCode", 0);
    amxc_var_add_key(uint32_t, &args, "DelaySeconds", delayseconds);
    amxc_var_add_key(amxc_ts_t, &args, "StartTime", &ts_start_time);
    amxc_var_add_key(amxc_ts_t, &args, "CompleteTime", &ts_complete_time);

    if(instance_id <= 0) {
        amxc_var_copy(&object, &args);
        when_failed(DM_ENG_Device_new_dm_entry(amx->bus_ctx, &object, &instance_id), stop);
    }

    when_true(instance_id <= 0, stop);
    amxc_var_add_key(int32_t, &args, "index", instance_id);

    if(amxb_call(amx->bus_ctx, TRANSFER_ROOT_PATH, "AddTransfer", &args, &ret, 2) != 0) {
        SetErrorGotoStop(DM_ENG_REQUEST_DENIED, "Invoke to AddTransfer instance %d", instance_id);
    }

    error = DM_ENG_Device_AddTransferSubscription(amx, instance_id);

stop:
    amxc_var_clean(&ret);
    amxc_var_clean(&object);
    amxc_var_clean(&args);
    return error;
}

static void DM_ENG_Device_RestartTransfer(char* path) {
    amxc_var_t transfer;
    amxc_string_t pname;
    amxd_path_t amxd_path;
    int instanceID = -1;
    const char* commandKey = NULL;
    const char* transferURL = NULL;
    const char* successURL = NULL;
    const char* failureURL = NULL;
    const char* fileType = NULL;
    const char* targetFileName = NULL;
    uint32_t fileSize = 0;
    const char* username = NULL;
    const char* password = NULL;
    char* id_str = NULL;
    bool isDownload = false;
    amxc_var_t* transfer_data = NULL;
    amxd_path_init(&amxd_path, path);
    amxc_string_init(&pname, 0);
    amxc_var_init(&transfer);
    dm_amx_env_t* amx = DM_ENG_Device_GetSystemInfo();
    when_str_empty(path, stop);

    if(amxb_get(amx->bus_ctx, path, 0, &transfer, 1) != 0) {
        GotoStop("Failed to read Transfer node from the data-model %s", path);
    }

    amxc_string_setf(&pname, "0.'%s'", path);
    transfer_data = GETP_ARG(&transfer, amxc_string_get(&pname, 0));
    commandKey = GETP_CHAR(transfer_data, "CommandKey");
    transferURL = GETP_CHAR(transfer_data, "Url");
    successURL = GETP_CHAR(transfer_data, "SuccessURL");
    failureURL = GETP_CHAR(transfer_data, "FailureURL");
    isDownload = GETP_BOOL(transfer_data, "IsDownload");
    fileType = GETP_CHAR(transfer_data, "FileType");
    fileSize = GETP_INT32(transfer_data, "FileSize");
    targetFileName = GETP_CHAR(transfer_data, "TargetFileName");
    username = GETP_CHAR(transfer_data, "Username");
    password = GETP_CHAR(transfer_data, "Password");

    id_str = amxd_path_get_last(&amxd_path, false);
    when_null(id_str, stop);
    instanceID = atoi(id_str);

    if(DM_ENG_Device_AddTransfer(amx, commandKey, fileType, transferURL,
                                 username, password, fileSize, targetFileName,
                                 5, successURL, failureURL, isDownload, instanceID) == 0) {
        DM_ENG_NotificationInterface_timerStart(commandKey, 3600, 0, DM_ENG_Device_TransferTimedOut);
    }
stop:
    amxc_var_clean(&transfer);
    amxc_string_clean(&pname);
    amxd_path_clean(&amxd_path);
    free(id_str);
}

//---------------------------------------------------------------------------------------------
/**
   @brief
   This function adds a ManagementServer.QueuedTranfers.Entry.x download instance, and fills in it's
   parameters.

   @details
   If the download RPC was valid, this function is called to add a ManagementServer.QueuedTranfers.Entry.x
   download instance. It fills in:
   - BeginTime = now + delay
   - EndTime = now + delay + 3600
   - CommandKey
   - IsDownload = true
   - FileSize
   - FileType
   - URL
   - Username
   - Password
   - TargetFileName
   - SuccesURL
   - FailureURL
   - Initiator = ACS

   A timeout timer is started to report transfer timeouts.


   @param amx A pointer to the amx system bus environment variable
   @param commandkey The commandkey of the download
   @param fileType The fileType for the download
   @param url The url for the download
   @param username The username for the download
   @param password The password for the download
   @param fileSize The fileSize for the download
   @param targetFileName The targetFileName for the download
   @param delayseconds The delayseconds for the download
   @param successURL The successURL for the download
   @param failureURL The failureURL for the download

   @return
    - TR69 error in case of error
    - 0 in case of success
 */
int DM_ENG_Device_DoDownload(dm_amx_env_t* amx,
                             char* commandkey,
                             char* fileType,
                             char* url,
                             char* username,
                             char* password,
                             unsigned int fileSize,
                             char* targetFileName,
                             unsigned int delayseconds,
                             char* successURL,
                             char* failureURL) {

    int error = DM_ENG_Device_AddTransfer(amx, commandkey, fileType, url,
                                          username, password, fileSize,
                                          targetFileName, delayseconds,
                                          successURL, failureURL, true, -1);
    if(error == 0) {
        DM_ENG_NotificationInterface_timerStart(commandkey, delayseconds + 3600, 0, DM_ENG_Device_TransferTimedOut);
    }
    return error;
}

//---------------------------------------------------------------------------------------------
/**
   @brief
   This function adds a ManagementServer.QueuedTranfers.Entry.x upload instance, and fills in it's
   parameters.

   @details
   If the upload RPC was valid, this function is called to add a ManagementServer.QueuedTranfers.Entry.x
   upload instance. It fills in:
   - BeginTime = now + delay
   - EndTime = now + delay + 3600
   - CommandKey
   - IsDownload = false
   - FileType
   - URL
   - Username
   - Password
   - Initiator = ACS

   A timeout timer is started to report transfer timeouts.

   @param amx A pointer to the amx system bus environment variable
   @param commandkey The commandkey of the download
   @param fileType The fileType for the download
   @param url The url for the download
   @param username The username for the download
   @param password The password for the download
   @param delayseconds The delayseconds for the download

   @return
    - TR69 error in case of error
    - 0 in case of success
 */
int DM_ENG_Device_DoUpload(dm_amx_env_t* amx,
                           char* commandkey,
                           char* fileType,
                           char* url,
                           char* username,
                           char* password,
                           unsigned int delayseconds) {

    int error = DM_ENG_Device_AddTransfer(amx, commandkey, fileType,
                                          url, username, password,
                                          0, "", delayseconds,
                                          "", "", false, -1);
    if(error == 0) {
        DM_ENG_NotificationInterface_timerStart(commandkey, delayseconds + 3600, 0, DM_ENG_Device_TransferTimedOut);
    }
    return error;
}


//---------------------------------------------------------------------------------------------
/**
   @brief
   This function fetches a list of ManagementServer.ACSTransfers.ACSTransfer.x upload instances and returns the
   result to the calling function.

   @details
   If the GetQueuedTransfers RPC was valid, this function is called to create a DM_ENG_AllQueuedTransferStruct list of
   items found in ManagementServer.ACSTransfers.ACSTransfer.x

   The calling function is responsible for cleaning up the resulting pResult.

   @param amx A pointer to the amx system bus environment variable
   @param pResult Array containing a list and descripption of the current transfers that is returned to the calling function

   @return
    - TR69 error in case of error
    - 0 in case of success
 */
int DM_ENG_Device_GetQueuedTransfers(dm_amx_env_t* amx, DM_ENG_AllQueuedTransferStruct** pResult[]) {
    int error = DM_ENG_METHOD_NOT_SUPPORTED;
    amxc_var_t transfers;
    DM_ENG_AllQueuedTransferStruct* tempList = NULL;
    const amxc_htable_t* htable = NULL;
    amxc_var_init(&transfers);

    SAH_TRACEZ_INFO("DM_DA", "Getting All Queued Transfers from data-model");

    if(amxb_get(amx->bus_ctx, "ManagementServer.ACSTransfers.ACSTransfer.*.", 1, &transfers, 5) != 0) {
        SetErrorGotoStop(DM_ENG_INVALID_PARAMETER_NAME, "failed to read Queued Transfers");
    }
    htable = amxc_var_constcast(amxc_htable_t, GETI_ARG(&transfers, 0));

    amxc_htable_iterate(hit, htable) {
        amxc_var_t* transfer = amxc_var_from_htable_it(hit);
        const char* commandKey = GETP_CHAR(transfer, "CommandKey");
        const char* status = GETP_CHAR(transfer, "Status");
        bool isDownload = GETP_BOOL(transfer, "IsDownload");
        const char* fileType = GETP_CHAR(transfer, "FileType");
        uint32_t fileSize = GETP_INT32(transfer, "FileSize");
        const char* targetFileName = GETP_CHAR(transfer, "TargetFileName");
        int state = 1;
        if(strcmp(status, "Initial") == 0) {
            state = 1;
        } else if(strcmp(status, "Transferring") == 0) {
            state = 2;
        } else {
            state = 3;
        }
        SAH_TRACEZ_INFO("DM_DA", "Adding transfer: key[%s] status[%s] filetype[%s] filesize[%d] targetfileName[%s]",
                        commandKey, status, fileType, fileSize, targetFileName);

        DM_ENG_AllQueuedTransferStruct* queuedTransfer = DM_ENG_newAllQueuedTransferStruct(isDownload, state, (char*) fileType, fileSize,
                                                                                           (char*) targetFileName, (char*) commandKey);
        if(queuedTransfer) {
            DM_ENG_addAllQueuedTransferStruct(&tempList, queuedTransfer);
        }
    }

    *pResult = DM_ENG_toAllQueuedTransferStructArray(tempList);
    error = 0;
stop:
    amxc_var_clean(&transfers);
    return error;
}

static void handle_pending_upgrade(const char* key) {

    SAH_TRACEZ_WARNING("DM_DA", "Handling pending upgrade");
    const char* status = NULL;
    const char* bootFailureLog = NULL;
    uint32_t faultCode = 0;
    const char* faultString = "";
    const char* completeTime = NULL;
    amxc_var_t ret;
    dm_amx_env_t* dm_system = DM_ENG_Device_GetSystemInfo();

    amxc_var_init(&ret);
    amxb_get(dm_system->bus_ctx, "DeviceInfo.FirmwareImage.[active].", 0, &ret, 5);
    status = GETP_CHAR(&ret, "0.0.Status");
    bootFailureLog = GETP_CHAR(&ret, "0.0.BootFailureLog");
    if(status == NULL) {
        SAH_TRACEZ_ERROR("DM_DA", "Failed to get FirmwareImage Status");
        faultCode = DM_ENG_INTERNAL_ERROR;
        faultString = "Internal failure";
    } else if(strcmp(status, "Active") != 0) {
        SAH_TRACEZ_WARNING("DM_DA", "FirmwareImage Info: Status [%s] - BootFailureLog [%s]", status, bootFailureLog ? bootFailureLog : "Null");
        faultCode = DM_ENG_DOWNLOAD_FAILURE;
        faultString = "Failed to apply firmware";
    }
    amxc_var_clean(&ret);

    if(faultCode == 0) {
        completeTime = "0001-01-01T00:00:00Z";
    }

    DM_ENG_Device_UpdateTransferState(dm_system->bus_ctx, key, faultCode, faultString, completeTime);
}

//---------------------------------------------------------------------------------------------
/**
   @brief
   Initialization of the existing transfers, should be called at startup

   @details
   We need to keep track of the transfers accross reboots.
   This routine must be called at startup of cwmpd to get a list the exising transfers.

   for each existing transfer who's "Initiator" is the ACS:
   - If transfer is "finished", trigger the engine to send a Transfer Complete message
   - If the transfer is still in the "initial" state, but timed out, trigger a transfer time out
   - If the transfer is ongoing, start a timeout timer


   @param amx A pointer to the amx system bus environment variable

   @return
    - false in case of an error
    - true in case of success
 */
bool DM_ENG_Device_UpDownloadInitialize(dm_amx_env_t* amx) {
    amxc_var_t transfers;
    const amxc_htable_t* htable = NULL;
    bool ret = false;
    amxc_var_init(&transfers);
    amxc_llist_init(&transferSubsList);

    SAH_TRACEZ_INFO("DM_DA", "Initialize transfers");
    amxb_get(amx->bus_ctx, "ManagementServer.ACSTransfers.ACSTransfer.*.", 1, &transfers, 5);
    when_true(amxc_var_is_null(&transfers), stop);
    htable = amxc_var_constcast(amxc_htable_t, GETI_ARG(&transfers, 0));

    amxc_htable_iterate(hit, htable) {
        const char* key = amxc_htable_it_get_key(hit);
        amxc_var_t* transfer = amxc_var_from_htable_it(hit);
        const char* status = GETP_CHAR(transfer, "Status");

        if(!status) {
            GotoStop("Failed to read the transfer status [%s]", key);
        }

        SAH_TRACEZ_INFO("DM_DA", "Transfer [%s]: Status [%s]", key, status);

        if(strcmp(status, "Initial") == 0) {
            amxc_ts_t then_ts;
            amxc_ts_t now_ts;
            const char* commandKey = GETP_CHAR(transfer, "CommandKey");
            const char* then_str = GETP_CHAR(transfer, "CompleteTime");

            if(commandKey && then_str && *then_str) {
                amxc_ts_now(&now_ts);
                amxc_ts_parse(&then_ts, then_str, strlen(then_str));
                SAH_TRACEZ_WARNING("DM_DA", "Transfer [%s] didn't start", commandKey);

                if(amxc_ts_compare(&now_ts, &then_ts) == 1) {
                    DM_ENG_NotificationInterface_timerStart(commandKey, 0, 0, DM_ENG_Device_TransferTimedOut);
                } else {
                    DM_ENG_NotificationInterface_timerStart(key, 0, 0, DM_ENG_Device_RestartTransfer);
                }
            }
        } else if(strcmp(status, "Finished") == 0) {
            DM_ENG_NotificationInterface_timerStart(key, 0, 0, DM_ENG_Device_TransferCompleteEvent);
        } else if(strcmp(status, "Applying") == 0) {
            const char* fileType = GETP_CHAR(transfer, "FileType");
            if(strcmp(fileType, "1 Firmware Upgrade Image") == 0) {
                handle_pending_upgrade(key);
            }
            DM_ENG_NotificationInterface_timerStart(key, 0, 0, DM_ENG_Device_TransferCompleteEvent);
        }
    }
    ret = true;
stop:
    amxc_var_clean(&transfers);
    return ret;
}

/** @} */
