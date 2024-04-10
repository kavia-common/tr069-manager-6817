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


typedef struct _timewindow_info_t {
    amxd_object_t* timewindow_obj;
    amxp_timer_t* start;
    amxp_timer_t* end;
} timewindow_info_t;


static void timewindow_set_status(amxd_object_t* obj, timewindow_status_t status) {
    const char* value = NULL;
    amxd_trans_t trans;
    amxd_trans_init(&trans);

    when_null_trace(obj, stop, ERROR, "Invalid arg(s)");
    amxd_trans_select_object(&trans, obj);

    switch(status) {
    case timewindow_status_idle:
        value = "Idle";
        break;

    case timewindow_status_started:
        value = "Started";
        break;

    case timewindow_status_ended:
        value = "Ended";
        break;
    default:
        SAH_TRACEZ_ERROR(ME, "Unkown Status %d", status);
        break;
    }

    when_null_trace(value, stop, ERROR, "Invalid Status's value");

    amxd_trans_set_value(cstring_t, &trans, "Status", value);

    when_failed_trace(amxd_trans_apply(&trans, cwmp_plugin_get_dm()),
                      stop, ERROR, "Failed to set TimeWindow's Status");

stop:
    amxd_trans_clean(&trans);
    return;
}

void transfers_timewindow_set_maxretries(amxd_object_t* obj, int32_t value) {
    amxd_trans_t trans;
    amxd_trans_init(&trans);

    when_null_trace(obj, stop, ERROR, "Invalid arg(s)");

    amxd_trans_select_object(&trans, obj);

    amxd_trans_set_value(int32_t, &trans, "MaxRetries", value);

    when_failed_trace(amxd_trans_apply(&trans, cwmp_plugin_get_dm()),
                      stop, ERROR, "Failed to set TimeWindow's MaxRetries");
stop:
    amxd_trans_clean(&trans);
    return;
}

int32_t transfers_timewindow_get_maxretries(amxd_object_t* obj) {
    int32_t value = -1;
    when_null_trace(obj, stop, ERROR, "Invalid arg(s)");
    value = amxd_object_get_value(int32_t, obj, "MaxRetries", NULL);
stop:
    return value;
}

timewindow_mode_t transfers_timewindow_get_mode(amxd_object_t* obj) {
    timewindow_mode_t timewindow_mode = timewindow_mode_undefined;
    char* value = NULL;

    when_null_trace(obj, stop, ERROR, "Invalid arg(s)");
    value = amxd_object_get_value(cstring_t, obj, "WindowMode", NULL);

    when_str_empty_trace(value, stop, ERROR, "Failed to get TimeWindow's WindowMode");

    if(strcmp(value, "1 At Any Time") == 0) {
        timewindow_mode = timewindow_mode_at_any_time;
    } else if(strcmp(value, "2 Immediately") == 0) {
        timewindow_mode = timewindow_mode_immediately;
    } else if(strcmp(value, "3 When Idle") == 0) {
        timewindow_mode = timewindow_mode_when_idle;
    } else if(strcmp(value, "4 Confirmation Needed") == 0) {
        timewindow_mode = timewindow_mode_confirmation_needed;
    } else {
        SAH_TRACEZ_ERROR(ME, "Unkown mode %s", value);
    }

stop:
    free(value);
    return timewindow_mode;
}

timewindow_status_t transfers_timewindow_get_status(amxd_object_t* obj) {
    timewindow_status_t timewindow_status = timewindow_status_undefined;
    char* value = NULL;

    when_null_trace(obj, stop, ERROR, "Invalid arg(s)");
    value = amxd_object_get_value(cstring_t, obj, "Status", NULL);

    when_str_empty_trace(value, stop, ERROR, "Failed to get TimeWindow's Status");

    if(strcmp(value, "Idle") == 0) {
        timewindow_status = timewindow_status_idle;
    } else if(strcmp(value, "Started") == 0) {
        timewindow_status = timewindow_status_started;
    } else if(strcmp(value, "Ended") == 0) {
        timewindow_status = timewindow_status_ended;
    } else {
        SAH_TRACEZ_ERROR(ME, "Unkown Status %s", value);
    }

stop:
    free(value);
    return timewindow_status;
}

static void timewindow_start_timer_cb(amxp_timer_t* timer, void* priv) {
    (void) timer;
    char* obj_path = NULL;

    timewindow_info_t* info = (timewindow_info_t*) priv;
    when_null_trace(info, stop, ERROR, "Invalid arg(s)");

    obj_path = amxd_object_get_path(info->timewindow_obj, AMXD_OBJECT_INDEXED);

    SAH_TRACEZ_INFO(ME, "[%s] started", obj_path);

    timewindow_set_status(info->timewindow_obj, timewindow_status_started);

stop:
    free(obj_path);
    return;
}

static void timewindow_end_timer_cb(amxp_timer_t* timer, void* priv) {
    (void) timer;
    char* obj_path = NULL;

    timewindow_info_t* info = (timewindow_info_t*) priv;
    when_null_trace(info, stop, ERROR, "Invalid arg(s)");

    obj_path = amxd_object_get_path(info->timewindow_obj, AMXD_OBJECT_INDEXED);

    SAH_TRACEZ_INFO(ME, "[%s] ended", obj_path);

    timewindow_set_status(info->timewindow_obj, timewindow_status_ended);

stop:
    free(obj_path);
    return;
}

static void timewindow_internal_del_info(amxd_object_t* obj) {
    timewindow_info_t* info = NULL;
    when_null_trace(obj, stop, ERROR, "Invalid arg(s)");
    when_null_trace((obj->priv), stop, ERROR, "Invalid object's priv");

    info = (timewindow_info_t*) obj->priv;
    info->timewindow_obj = NULL;
    amxp_timer_delete(&(info->start));
    amxp_timer_delete(&(info->end));
    free(info);
    obj->priv = NULL;
stop:
    return;
}

static void timewindow_internal_add_info(amxd_object_t* obj) {
    timewindow_info_t* info = NULL;
    timewindow_status_t status = timewindow_status_undefined;

    when_null_trace(obj, stop, ERROR, "Invalid arg(s)");
    when_not_null_trace((obj->priv), stop, ERROR, "Invalid object's priv");

    info = (timewindow_info_t*) calloc(1, sizeof(timewindow_info_t));
    obj->priv = info;
    info->timewindow_obj = obj;
    info->start = NULL;
    info->end = NULL;

    status = transfers_timewindow_get_status(obj);

    switch(status) {
    case timewindow_status_idle:
        amxp_timer_new(&(info->start), timewindow_start_timer_cb, info);
        amxp_timer_new(&(info->end), timewindow_end_timer_cb, info);
        break;

    case timewindow_status_started:
        info->start = NULL;
        amxp_timer_new(&(info->end), timewindow_end_timer_cb, info);
        break;

    case timewindow_status_ended:
        info->start = NULL;
        info->end = NULL;
        break;

    default:
        SAH_TRACEZ_ERROR(ME, "Unkown timewindow status [%d]", status);
        break;
    }

stop:
    return;
}

void transfers_timewindow_add_info(amxd_object_t* obj) {
    amxd_object_t* inst1 = NULL;
    amxd_object_t* inst2 = NULL;
    when_null_trace(obj, stop, ERROR, "Invalid arg(s)");

    inst1 = amxd_object_findf(obj, ".TimeWindowList.1.");
    when_null_trace(inst1, stop, ERROR, "Failed to get TimeWindow1");
    timewindow_internal_add_info(inst1);
    inst2 = amxd_object_findf(obj, ".TimeWindowList.2.");
    if(inst2) {
        timewindow_internal_add_info(inst2);
    }

stop:
    return;
}

void transfers_timewindow_del_info(amxd_object_t* obj) {
    amxd_object_t* inst1 = NULL;
    amxd_object_t* inst2 = NULL;
    when_null_trace(obj, stop, ERROR, "Invalid arg(s)");
    inst1 = amxd_object_findf(obj, ".TimeWindowList.1.");
    when_null_trace(inst1, stop, ERROR, "Failed to get TimeWindow1");
    timewindow_internal_del_info(inst1);
    inst2 = amxd_object_findf(obj, ".TimeWindowList.2.");
    if(inst2) {
        timewindow_internal_del_info(inst2);
    }

stop:
    return;
}

static void timewindow_set_time_offset(amxd_object_t* obj, const char* time_offset_name, uint32_t value) {
    SAH_TRACEZ_IN(ME);
    char* inst_path = NULL;
    amxd_trans_t trans;
    amxd_trans_init(&trans);
    when_null_trace(obj, stop, ERROR, "Invalid arg(s)");
    when_str_empty_trace(time_offset_name, stop, ERROR, "Invalid timer name");

    inst_path = amxd_object_get_path(obj, AMXD_OBJECT_INDEXED);

    amxd_trans_select_object(&trans, obj);

    SAH_TRACEZ_INFO(ME, "Saving [%s.%s] offset: [%d sec]", inst_path, time_offset_name, value);

    amxd_trans_set_value(uint32_t, &trans, time_offset_name, value);

    when_failed_trace(amxd_trans_apply(&trans, cwmp_plugin_get_dm()),
                      stop, ERROR, "Failed to set TimeWindow's %s", time_offset_name);
stop:
    amxd_trans_clean(&trans);
    free(inst_path);
    SAH_TRACEZ_OUT(ME);
    return;
}

static uint32_t timewindow_get_time_offset(amxd_object_t* obj, const char* time_offset_name) {
    uint32_t value = 0;

    when_null_trace(obj, stop, ERROR, "Invalid arg(s)");
    when_str_empty_trace(time_offset_name, stop, ERROR, "Invalid timer name");

    value = amxd_object_get_value(uint32_t, obj, time_offset_name, NULL);

    SAH_TRACEZ_INFO(ME, "[%s] offset is [%d sec]", time_offset_name, value);
stop:
    return (value * 1000);
}

static void timewindow_internal_start(amxd_object_t* obj) {
    /* TODO: add more check for this function and handle errors if any */
    timewindow_status_t timewindow_status = timewindow_status_undefined;
    when_null_trace(obj, stop, ERROR, "Invalid arg(s)");

    timewindow_status = transfers_timewindow_get_status(obj);

    timewindow_info_t* info = (timewindow_info_t*) obj->priv;

    when_null_trace(info, stop, ERROR, "Invalid TimeWindow info");

    switch(timewindow_status) {
    case timewindow_status_idle:
    {
        uint32_t windowStart = timewindow_get_time_offset(obj, "WindowStart");
        uint32_t windowEnd = timewindow_get_time_offset(obj, "WindowEnd");
        amxp_timer_start(info->start, windowStart);
        amxp_timer_start(info->end, windowEnd);
    }
    break;

    case timewindow_status_started:
    {
        uint32_t windowEnd = timewindow_get_time_offset(obj, "WindowEnd");
        amxp_timer_start(info->end, windowEnd);
    }
    break;
    default:
        break;
    }
stop:
    return;
}

void transfers_timewindow_start(amxd_object_t* obj) {
    amxd_object_t* inst1 = NULL;
    amxd_object_t* inst2 = NULL;
    when_null_trace(obj, stop, ERROR, "Invalid arg(s)");
    inst1 = amxd_object_findf(obj, ".TimeWindowList.1.");
    when_null_trace(inst1, stop, ERROR, "Failed to get TimeWindow1");
    timewindow_internal_start(inst1);
    inst2 = amxd_object_findf(obj, ".TimeWindowList.2.");
    if(inst2) {
        timewindow_internal_start(inst2);
    }

stop:
    return;
}

static void timewindow_internal_save(amxd_object_t* obj) {
    timewindow_status_t timewindow_status = timewindow_status_undefined;
    when_null_trace(obj, stop, ERROR, "Invalid arg(s)");

    timewindow_status = transfers_timewindow_get_status(obj);

    timewindow_info_t* info = (timewindow_info_t*) obj->priv;

    when_null_trace(info, stop, ERROR, "Invalid TimeWindow info");

    switch(timewindow_status) {
    case timewindow_status_idle:
    {
        uint32_t windowStart = amxp_timer_remaining_time(info->start) / 1000;
        uint32_t windowEnd = amxp_timer_remaining_time(info->end) / 1000;
        timewindow_set_time_offset(info->timewindow_obj, "WindowStart", windowStart);
        timewindow_set_time_offset(info->timewindow_obj, "WindowEnd", windowEnd);
    }
    break;

    case timewindow_status_started:
    {
        uint32_t windowEnd = amxp_timer_remaining_time(info->end) / 1000;
        timewindow_set_time_offset(info->timewindow_obj, "WindowStart", 0);
        timewindow_set_time_offset(info->timewindow_obj, "WindowEnd", windowEnd);
    }
    break;
    default:
        break;
    }
stop:
    return;
}

void transfers_timewindow_save(amxd_object_t* obj) {
    amxd_object_t* inst1 = NULL;
    amxd_object_t* inst2 = NULL;

    when_null_trace(obj, stop, ERROR, "Invalid arg(s)");
    inst1 = amxd_object_findf(obj, ".TimeWindowList.1.");
    when_null_trace(inst1, stop, ERROR, "Failed to get TimeWindow1");
    timewindow_internal_save(inst1);
    inst2 = amxd_object_findf(obj, ".TimeWindowList.2.");
    if(inst2) {
        timewindow_internal_save(inst2);
    }

stop:
    return;
}
