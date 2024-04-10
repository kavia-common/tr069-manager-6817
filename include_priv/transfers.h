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

#if !defined(__TRANSFERS_H__)
#define __TRANSFERS_H__

#include <amxc/amxc_macros.h>
#include <amxc/amxc.h>
#include <amxp/amxp.h>
#include <amxd/amxd_dm.h>
#include <amxd/amxd_object.h>
#include <amxd/amxd_object_event.h>
#include <amxd/amxd_transaction.h>
#include <amxd/amxd_action.h>

#include <amxo/amxo.h>
#include <amxo/amxo_save.h>

#define FAULT_CODE_INTERNAL_ERROR 9002
#define FAULT_CODE_FILE_TRANSFER_FAILURE 9010
#define FAULT_CODE_UNSUPPORTED_PROTOCOL_FOR_FILE_TRANSFER 9013

#define FILETYPE_1_FIRMWARE_UPGRADE_IMAGE "1 Firmware Upgrade Image"
#define WINDOWMODE_1_AT_ANY_TIME "1 At Any Time"
#define WINDOWMODE_2_IMMEDIATELY "2 Immediately"

typedef enum _transfer_status_t
{
    transfer_status_undefined,
    transfer_status_idle,
    transfer_status_in_progress,
    transfer_status_failed,
    transfer_status_done
} transfer_status_t;

typedef struct _transfer_info_t {
    amxd_object_t* transfer_obj;
    uint32_t numberOfTimeWindow;
    uint32_t lastTimeWindow;
    bool withinTimeWindow;
} transfer_info_t;

typedef enum _timewindow_status_t
{
    timewindow_status_undefined,
    timewindow_status_idle,
    timewindow_status_started,
    timewindow_status_ended
} timewindow_status_t;

typedef enum _timewindow_mode_t
{
    timewindow_mode_undefined,
    timewindow_mode_at_any_time,
    timewindow_mode_immediately,
    timewindow_mode_when_idle,
    timewindow_mode_confirmation_needed
} timewindow_mode_t;

void transfers_transfer_add_info(amxd_object_t* obj);
void transfers_transfer_del_info(amxd_object_t* obj);
void transfers_handle_transfer(amxd_object_t* obj);
void transfers_start_transfer(amxd_object_t* obj);
transfer_status_t transfers_get_status(amxd_object_t* obj);
void transfers_set_status(amxd_object_t* obj, transfer_status_t status);
void transfers_firmwareupgrade_done(amxd_object_t* obj, uint32_t fault_code, const char* fault_string);

void transfers_init(void);
void transfers_clean(void);
void transfers_handle_in_progress_transfer(amxd_object_t* obj);
void transfers_transfer_done(amxd_object_t* obj);
void transfers_save(void);

/* timewindow */
void transfers_timewindow_add_info(amxd_object_t* obj);
void transfers_timewindow_del_info(amxd_object_t* obj);
void transfers_timewindow_start(amxd_object_t* obj);
void transfers_timewindow_save(amxd_object_t* obj);
timewindow_status_t transfers_timewindow_get_status(amxd_object_t* obj);
timewindow_mode_t transfers_timewindow_get_mode(amxd_object_t* obj);
int32_t transfers_timewindow_get_maxretries(amxd_object_t* obj);
void transfers_timewindow_set_maxretries(amxd_object_t* obj, int32_t value);

/* firmware */
void transfers_firmware_init(void);
void transfers_firmware_clean(void);
int transfers_firmware_do_firmwareupgrade(amxd_object_t* obj);
void transfers_firmware_transfer_done(amxd_object_t* obj);

/* methods, events, actions */
amxd_status_t _addScheduleDownload(amxd_object_t* object,
                                   amxd_function_t* func,
                                   amxc_var_t* args,
                                   amxc_var_t* ret);

void _print_event(const char* const sig_name,
                  const amxc_var_t* const data,
                  void* const priv);

void _scheduleDownload_added(const char* const sig_name,
                             const amxc_var_t* const data,
                             void* const priv);

void _timewindow_started(const char* const sig_name,
                         const amxc_var_t* const data,
                         void* const priv);

void _timewindow_ended(const char* const sig_name,
                       const amxc_var_t* const data,
                       void* const priv);

void _schedule_download_transfer_failed(const char* const sig_name,
                                        const amxc_var_t* const data,
                                        void* const priv);

void _app_stopped(const char* const sig_name,
                  const amxc_var_t* const data,
                  void* const priv);

amxd_status_t _scheduleDownload_del_inst(amxd_object_t* object,
                                         amxd_param_t* param,
                                         amxd_action_t reason,
                                         const amxc_var_t* const args,
                                         amxc_var_t* const retval,
                                         void* priv);

#endif // __TRANSFERS_H__
