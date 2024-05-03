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

#include <stdint.h>
#include <debug/sahtrace.h>
#include <debug/sahtrace_macros.h>
#include <dmengine/DM_ENG_InformMessageScheduler.h>
#include "DM_AmxRPCInterface.h"

#define ME "DM_DA"

#define RPC_REQ "notification in ['SendInformMessage!']"
#define MANAGEMENTSERVER_PATH "ManagementServer."


static amxb_bus_ctx_t* bus_ctx = NULL;

static void handle_sendInformMessage(const amxc_var_t* const data) {
    const char* events = GETP_CHAR(data, "data.events");
    bool immediately = GETP_BOOL(data, "data.immediately");
    const char* source = GETP_CHAR(data, "data.source");

    SAH_TRACEZ_INFO(ME, "events [%s]", events);
    SAH_TRACEZ_INFO(ME, "immediately [%s], source [%s]", immediately ? "true":"false", source);

    DM_ENG_InformMessageScheduler_SendEvents(events, immediately, source);
}

void rpc_req_cb(const char* const sig_name,
                const amxc_var_t* const data,
                UNUSED void* const priv) {
    SAH_TRACEZ_IN(ME);

    when_str_empty_trace(sig_name, stop, ERROR, "Invalid arg(s)");

    if(strcmp(sig_name, "SendInformMessage!")) {
        handle_sendInformMessage(data);
    } else {
        SAH_TRACEZ_INFO(ME, "Unkown RPC request [%s]", sig_name);
    }

stop:
    SAH_TRACEZ_OUT(ME);
    return;
}

int DM_AmxRPCInterface_init(amxb_bus_ctx_t* ctx) {
    int retval = -1;
    SAH_TRACEZ_IN(ME);

    amxc_var_t ret;
    amxc_var_init(&ret);

    when_null_trace(ctx, stop, ERROR, "Invalid bus ctx");

    bus_ctx = ctx;

    retval = amxb_subscribe(bus_ctx, MANAGEMENTSERVER_PATH, RPC_REQ,
                            rpc_req_cb, NULL);

stop:
    amxc_var_clean(&ret);
    SAH_TRACEZ_OUT(ME);
    return retval;
}

int DM_AmxRPCInterface_clean(void) {
    int retval = -1;
    SAH_TRACEZ_IN(ME);

    retval = amxb_unsubscribe(bus_ctx, MANAGEMENTSERVER_PATH, rpc_req_cb, NULL);

    SAH_TRACEZ_OUT(ME);
    return retval;
}