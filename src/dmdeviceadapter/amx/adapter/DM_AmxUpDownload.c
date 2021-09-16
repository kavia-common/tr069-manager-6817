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


#include <debug/sahtrace.h>
#include <string.h>
#include <time.h>
#include <dmengine/DM_ENG_AllQueuedTransferStruct.h>
#include <dmengine/DM_ENG_Error.h>

#include "DM_AmxCommon.h"
#include "DM_AmxUpDownload.h"
#include "DM_AmxSystemConnection.h"

//---------------------------------------------------------------------------------------------
/**
 * @addtogroup sah_cwmp_pcbdeviceadapter
 * @{
 */

//---------------------------------------------------------------------------------------------
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
void DM_ENG_Device_TransferTimedOut(char* commandkey) {
    (void) commandkey;
    fprintf(stderr, "DM_ENG_Device_TransferTimedOut Not yet implemented \n");
    return;
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
int DM_ENG_Device_DoDownload(dm_amx_env_t* amx, char* commandkey, char* fileType, char* url, char* username, char* password, unsigned int fileSize, char* targetFileName,
                             unsigned int delayseconds, char* successURL, char* failureURL) {
    (void) amx;
    (void) commandkey;
    (void) fileType;
    (void) url;
    (void) username;
    (void) password;
    (void) fileSize;
    (void) targetFileName;
    (void) delayseconds;
    (void) successURL;
    (void) failureURL;
    int error = DM_ENG_METHOD_NOT_SUPPORTED;
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
int DM_ENG_Device_DoUpload(dm_amx_env_t* amx, char* commandkey, char* fileType, char* url, char* username, char* password, unsigned int delayseconds) {
    (void) amx;
    (void) commandkey;
    (void) fileType;
    (void) url;
    (void) username;
    (void) password;
    (void) delayseconds;
    int error = DM_ENG_METHOD_NOT_SUPPORTED;
    return error;
}


//---------------------------------------------------------------------------------------------
/**
   @brief
   This function fetches a list of ManagementServer.QueuedTranfers.Entry.x upload instances and returns the
   result to the calling function.

   @details
   If the GetQueuedTransfers RPC was valid, this function is called to create a DM_ENG_AllQueuedTransferStruct list of
   items found in ManagementServer.QueuedTranfers.Entry.x

   The calling function is responsible for cleaning up the resulting pResult.

   @param amx A pointer to the amx system bus environment variable
   @param pResult Array containing a list and descripption of the current transfers that is returned to the calling function

   @return
    - TR69 error in case of error
    - 0 in case of success
 */
int DM_ENG_Device_GetQueuedTransfers(dm_amx_env_t* amx, DM_ENG_AllQueuedTransferStruct** pResult[]) {
    (void) amx;
    (void) pResult;
    int error = DM_ENG_METHOD_NOT_SUPPORTED;
    return error;
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
    (void) amx;
    return false;
}

/** @} */
