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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <debug/sahtrace.h>
#include <debug/sahtrace_macros.h>

#include <amxc/amxc.h>
#include <amxc/amxc_macros.h>
#include <amxp/amxp.h>
#include <amxm/amxm.h>
#include "cwmp_plugin.h"

#include <amxa/amxa_merger.h>

#define DEFAULT_CRH "0.0.0.0"
#define FIREWALL_CONN_REQUEST_ID "cwmpd_conn_request"

static amxp_proc_ctrl_t* cwmpd_proc = NULL;
static amxp_timer_t* restart_timer = NULL;



// GCOVR_EXCL_START

static void cwmp_timer_cb(UNUSED amxp_timer_t* timer, UNUSED void* priv) {
    SAH_TRACEZ_INFO(ME, "wait-timer-expired start cwmpd");
    start_cwmpd();
}

static void cwmpd_proc_stopped(UNUSED void* priv) {
    SAH_TRACEZ_NOTICE(ME, "cwmpd stopped !!!");
    amxp_proc_ctrl_delete(&cwmpd_proc);
    cwmpd_proc = NULL;
    close_cwmpd_listening_port();
    amxp_timer_new(&restart_timer, cwmp_timer_cb, NULL);
    amxp_timer_start(restart_timer, 10000);
}

static int build_cwmpd_proc_args(amxc_array_t* cmd, UNUSED amxc_var_t* settings) {
    amxc_array_init(cmd, 2);
    amxc_string_t odl_config_opt;
    amxc_string_init(&odl_config_opt, 0);

    const char* amxrt_prefix = getenv("AMXRT_PREFIX_PATH");
    SAH_TRACEZ_ERROR("CWMPD", "AMXRT_PREFIX_PATH:%s", amxrt_prefix);
    if(amxrt_prefix && *amxrt_prefix) {
        amxc_string_t cwmpd_path;
        amxc_string_init(&cwmpd_path, 0);
        amxc_string_appendf(&cwmpd_path, "%s/usr/bin/cwmpd", amxrt_prefix);
        amxc_array_append_data(cmd, amxc_string_take_buffer(&cwmpd_path));
        amxc_string_setf(&odl_config_opt, "-c%s%s", amxrt_prefix, GETP_CHAR(cwmp_plugin_get_config(), "odl_config"));
        amxc_array_append_data(cmd, amxc_string_take_buffer(&odl_config_opt));
        amxc_string_clean(&cwmpd_path);
        amxc_array_append_data(cmd, strdup("-f"));
    } else {
        amxc_array_append_data(cmd, strdup("cwmpd"));
        amxc_string_setf(&odl_config_opt, "-c%s", GETP_CHAR(cwmp_plugin_get_config(), "odl_config"));
        amxc_array_append_data(cmd, amxc_string_take_buffer(&odl_config_opt));
        amxc_array_append_data(cmd, strdup("-D"));
    }
    //clean up
    amxc_string_clean(&odl_config_opt);
    return 0;
}

void open_cwmpd_listening_port(void) {
    amxd_object_t* mgmt_server = amxd_dm_findf(cwmp_plugin_get_dm(), "ManagementServer");
    amxd_object_t* conn_req = amxd_dm_findf(cwmp_plugin_get_dm(), "ManagementServer.ConnRequest");
    amxd_object_t* internal_settings = amxd_dm_findf(cwmp_plugin_get_dm(), "ManagementServer.InternalSettings");
    int conn_req_port, local_ip_version;
    const char* interface, * allowed_address, * local_ip;
    amxc_var_t args, ret;

    conn_req_port = amxc_var_constcast(uint32_t, amxd_object_get_param_value(conn_req, "ConnRequestPort"));
    allowed_address = amxc_var_constcast(cstring_t, amxd_object_get_param_value(internal_settings, "AllowConnectionRequestFromAddress"));
    interface = amxc_var_constcast(cstring_t, amxd_object_get_param_value(mgmt_server, "Interface"));
    local_ip = amxc_var_constcast(cstring_t, amxd_object_get_param_value(conn_req, "LocalIPAddress"));
    local_ip_version = (isAddressIpV6(local_ip) == true) ? 6 : 4;
    amxc_var_init(&args);
    amxc_var_init(&ret);
    amxc_var_set_type(&args, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &args, "id", FIREWALL_CONN_REQUEST_ID);
    amxc_var_add_key(cstring_t, &args, "protocol", "6");
    amxc_var_add_key(cstring_t, &args, "interface", interface);
    amxc_var_add_key(uint32_t, &args, "destination_port", conn_req_port);
    amxc_var_add_key(cstring_t, &args, "source_prefix", allowed_address);
    amxc_var_add_key(uint32_t, &args, "ipversion", local_ip_version);
    amxc_var_add_key(bool, &args, "enable", true);
    SAH_TRACEZ_INFO(ME, "Opening conn_req_port %d for [%s] on [%s]", conn_req_port, ((allowed_address[0]) ? allowed_address : "all"), interface);

    if(amxm_execute_function("fw", "fw", "set_service", &args, &ret)) {
        SAH_TRACEZ_ERROR(ME, "call to set_service, Opening conn_req_port %d for [%s] on [%s] failed", conn_req_port, ((allowed_address[0]) ? allowed_address : "all"), interface);
    }
    amxc_var_clean(&ret);
    amxc_var_clean(&args);
}

void close_cwmpd_listening_port(void) {
    amxc_var_t args, ret;
    amxc_var_init(&args);
    amxc_var_init(&ret);
    when_null_trace(cwmpd_proc, exit, INFO, "cwmpd listening port already closed, exit");

    SAH_TRACEZ_INFO(ME, "Closing cwmpd listening port");
    amxc_var_set_type(&args, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &args, "id", FIREWALL_CONN_REQUEST_ID);
    amxm_execute_function("fw", "fw", "delete_service", &args, &ret);
exit:
    amxc_var_clean(&ret);
    amxc_var_clean(&args);
}

void start_cwmpd(void) {
    const char* cwmpd_standalone = getenv("CWMPD_STANDALONE");
    if(cwmpd_standalone && *cwmpd_standalone) {
        SAH_TRACEZ_WARNING("CWMPD", "CWMPD_STANDALONE:%s", cwmpd_standalone);
        open_cwmpd_listening_port();
        return;
    }
    cwmp_proc_ctx_t* ctx = NULL;
    when_not_null_trace(cwmpd_proc, exit, NOTICE, "cwmpd already running, firewall service updated");
    amxp_proc_ctrl_new(&cwmpd_proc, build_cwmpd_proc_args);
    cwmp_proc_ctx_new(&ctx, NULL, cwmpd_proc_stopped, NULL, NULL);
    amxp_slot_connect(cwmpd_proc->proc->sigmngr, "stop", NULL, proc_finished_cb, (void*) ctx);
    amxp_proc_ctrl_start(cwmpd_proc, 0, NULL);
    open_cwmpd_listening_port();
exit:
    SAH_TRACEZ_INFO(ME, "cwmpd started");
}

void stop_cwmpd(void) {
    const char* cwmpd_standalone = getenv("CWMPD_STANDALONE");
    if(cwmpd_standalone && *cwmpd_standalone) {
        SAH_TRACEZ_WARNING("CWMPD", "CWMPD_STANDALONE:%s", cwmpd_standalone);
        close_cwmpd_listening_port();
        return;
    }
    if(cwmpd_proc) {
        SAH_TRACEZ_INFO(ME, "stopping cwmpd");
        amxp_proc_ctrl_stop(cwmpd_proc);
        amxp_proc_ctrl_delete(&cwmpd_proc);
        cwmpd_proc = NULL;
    }
}


// GCOVR_EXCL_STOP
