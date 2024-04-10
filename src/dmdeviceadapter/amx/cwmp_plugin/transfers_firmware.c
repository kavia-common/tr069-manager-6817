/****************************************************************************
**
** SPDX-License-Identifier: BSD-2-Clause-Patent
**
** SPDX-FileCopyrightText: Copyright (c) 2024 SoftAtHome
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

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <amxc/amxc.h>
#include <amxp/amxp.h>
#include <amxd/amxd_object.h>
#include <amxd/amxd_dm.h>
#include <amxd/amxd_transaction.h>

#include <debug/sahtrace.h>
#include <debug/sahtrace_macros.h>

#include "cwmp_plugin.h"
#include "transfers.h"

#define DEVICEINFO_FIRMWAREIMAGE "DeviceInfo.FirmwareImage.active."

static amxd_object_t* transfer_in_progress = NULL;

static void firmwareimage_notification(UNUSED const char* const sig_name,
                                       const amxc_var_t* const data,
                                       UNUSED void* const priv) {


    const char* status = NULL;
    const char* bootFailureLog = NULL;
    uint32_t fault_code = 0;

    when_null(transfer_in_progress, stop);

    status = GETP_CHAR(data, "parameters.Status.to");
    bootFailureLog = GETP_CHAR(data, "parameters.BootFailureLog.to");

    when_null_trace(status, stop, ERROR, "Failed to get the firmware upgrade status");

    if(bootFailureLog == NULL) {
        bootFailureLog = "";
    }

    SAH_TRACEZ_INFO(ME, "Firmware upgrade status: [%s], bootFailureLog [%s]", status, bootFailureLog);

    if((strcmp(status, "DownloadFailed") == 0) ||
       (strcmp(status, "ValidationFailed") == 0) ||
       (strcmp(status, "InstallationFailed") == 0) ||
       (strcmp(status, "ActivationFailed") == 0)) {
        fault_code = FAULT_CODE_FILE_TRANSFER_FAILURE;
        if(strcmp(bootFailureLog, "Protocol not supported") == 0) {
            fault_code = FAULT_CODE_UNSUPPORTED_PROTOCOL_FOR_FILE_TRANSFER;
        }
        transfers_firmwareupgrade_done(transfer_in_progress, fault_code, bootFailureLog);
        transfer_in_progress = NULL;
    } else if(strcmp(status, "Validating") == 0) {
        transfers_save();
        /* expecting a reboot now or an error */
    }

stop:
    return;
}

static int firmwareimage_add_sub(void) {
    int retval = -1;

    retval = amxb_subscribe(amxb_be_who_has("DeviceInfo."), DEVICEINFO_FIRMWAREIMAGE, "(notification == 'dm:object-changed') && \
                                (contains('parameters.Status'))", firmwareimage_notification, NULL);

    if(retval != 0) {
        SAH_TRACEZ_ERROR(ME, "Failed to add subscription to [%s]", DEVICEINFO_FIRMWAREIMAGE);
    }
    return retval;
}

static int firmwareimage_del_sub(void) {
    int retval = -1;

    retval = amxb_unsubscribe(amxb_be_who_has("DeviceInfo."), DEVICEINFO_FIRMWAREIMAGE, firmwareimage_notification, NULL);

    if(retval != 0) {
        SAH_TRACEZ_ERROR(ME, "Failed to delete subscription from [%s]", DEVICEINFO_FIRMWAREIMAGE);
    }

    return retval;
}

static int upgrade_firmware(const char* url, const char* username, const char* password) {
    int retval = -1;
    amxc_var_t args;
    amxc_var_init(&args);
    amxc_var_set_type(&args, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &args, "URL", url);
    amxc_var_add_key(bool, &args, "AutoActivate", true);
    amxc_var_add_key(cstring_t, &args, "Username", username);
    amxc_var_add_key(cstring_t, &args, "Password", password);
    retval = amxb_call(amxb_be_who_has("DeviceInfo."), DEVICEINFO_FIRMWAREIMAGE, "Download", &args, NULL, 5);
    when_failed_trace(retval, stop, ERROR, "Failed to Call %sDownload()", DEVICEINFO_FIRMWAREIMAGE);
stop:
    amxc_var_clean(&args);
    return retval;
}

int transfers_firmware_do_firmwareupgrade(amxd_object_t* obj) {
    int retval = -1;
    bool new_transfer = false;
    char* url = NULL;
    char* username = NULL;
    char* password = NULL;

    when_null_trace(obj, stop, ERROR, "Invalid arg(s)");
    when_not_null_trace(transfer_in_progress, stop, ERROR, "Previous firmware upgrade is progress");

    new_transfer = true;
    transfer_in_progress = obj;

    url = amxd_object_get_cstring_t(obj, "URL", NULL);
    when_null_trace(url, stop, ERROR, "Invalid URL");

    username = amxd_object_get_cstring_t(obj, "Username", NULL);
    when_null_trace(username, stop, ERROR, "Invalid Username");

    password = amxd_object_get_cstring_t(obj, "Password", NULL);
    when_null_trace(password, stop, ERROR, "Invalid Password");

    SAH_TRACEZ_INFO(ME, "[%s] [%s] [%s]", url, username, password);

    when_failed(upgrade_firmware(url, username, password), stop);

    retval = 0;
stop:
    if(retval != 0) {
        if(new_transfer == true) {
            transfer_in_progress = NULL;
        }
        SAH_TRACEZ_ERROR(ME, "Failed to perform firmware upgrade");
    }

    free(url);
    free(username);
    free(password);

    return retval;
}

void transfers_firmware_init(void) {
    transfer_in_progress = NULL;
    firmwareimage_add_sub();
}

void transfers_firmware_clean(void) {
    transfer_in_progress = NULL;
    firmwareimage_del_sub();
}

void transfers_firmware_transfer_done(amxd_object_t* obj) {
    when_null(obj, stop);
    when_null(transfer_in_progress, stop);
    when_true((transfer_in_progress != obj), stop);
    transfer_in_progress = NULL;
stop:
    return;
}