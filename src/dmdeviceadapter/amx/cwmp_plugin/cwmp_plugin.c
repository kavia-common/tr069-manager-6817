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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <amxc/amxc.h>
#include <amxc/amxc_macros.h>
#include <amxm/amxm.h>
#include "cwmp_plugin.h"
#include "transfers.h"
#include <smm.h>

#include <debug/sahtrace.h>
#include <debug/sahtrace_macros.h>

#include <netmodel/client.h>

static cwmp_plugin_app_t app;
static amxm_shared_object_t* fw_module = NULL;
static amxp_timer_t* deferred_init_timer = NULL;

static void deferred_init_timer_cb(UNUSED amxp_timer_t* timer, UNUSED void* priv) {
    SAH_TRACEZ_IN(ME);
    cwmp_plugin_netmodel_find_ip();
    smm_differed_init();
    amxp_timer_delete(&deferred_init_timer);
    start_cwmpd();
    SAH_TRACEZ_OUT(ME);
}

static void cwmp_plugin_deferred_init(void) {
    when_failed_trace(amxp_timer_new(&deferred_init_timer, deferred_init_timer_cb, NULL), stop, ERROR, "Failed to create timer");
    when_failed_trace(amxp_timer_start(deferred_init_timer, 0), stop, ERROR, "Failed to start timer");
stop:
    return;
}

amxd_status_t _sendInformMessage(amxd_object_t* object,
                                 UNUSED amxd_function_t* func,
                                 amxc_var_t* args,
                                 UNUSED amxc_var_t* ret) {
    const char* events = NULL;
    const char* source = NULL;
    amxc_var_t data;
    amxc_var_init(&data);
    amxd_status_t status = amxd_status_invalid_arg;
    SAH_TRACEZ_IN(ME);
    when_true_trace(amxc_var_is_null(args), stop, ERROR, "Invalid arg(s)");
    events = GET_CHAR(args, "events");
    when_str_empty_trace(events, stop, ERROR, "Invalid events");
    source = GET_CHAR(args, "source");
    when_str_empty_trace(source, stop, ERROR, "Invalid source");
    amxc_var_set_type(&data, AMXC_VAR_ID_HTABLE);
    amxc_var_set_key(&data, "data", args, AMXC_VAR_FLAG_COPY);
    amxd_object_send_signal(object, "SendInformMessage!", &data, true);
    status = amxd_status_ok;
stop:
    amxc_var_clean(&data);
    SAH_TRACEZ_OUT(ME);
    return status;
}

static void load_fw_controller(void) {
    const char* controller = GETP_CHAR(cwmp_plugin_get_config(), "firewall.controller");

    when_null(controller, exit);
    if(amxm_so_open(&fw_module, "fw", controller)) {
        SAH_TRACEZ_ERROR(ME, "Couldn't open fw controller (%s)", controller);
        goto exit;
    }
exit:
    return;
}

static void cwmp_plugin_init(amxd_dm_t* dm, amxo_parser_t* parser) {
    SAH_TRACEZ_INFO(ME, "cwmp_plugin started");
    app.dm = dm;
    app.parser = parser;

    amxc_array_t* uris = amxb_list_uris();
    const char* uri = (const char*) amxc_array_get_data_at(uris, 0);
    const amxc_llist_t* backends = NULL;
    backends = amxc_var_constcast(amxc_llist_t, GET_ARG(cwmp_plugin_get_config(), "backends"));
    const char* backend = amxc_var_constcast(cstring_t, amxc_var_from_llist_it(amxc_llist_get_first(backends)));

    // setenv variables to be used by the Adapter to connect to bus
    if(!STRING_EMPTY(uri)) {
        setenv("AMXB_URI", uri, 1);
        app.amxb_bus_ctx = amxb_find_uri(uri);
    }
    if(!STRING_EMPTY(backend)) {
        setenv("AMXB_BACKEND", backend, 1);
    }
    amxc_array_delete(&uris, NULL);

    load_fw_controller();
    cwmp_plugin_netmodel_init();
    cwmp_plugin_transfer_init();
    cwmp_plugin_manageableDevice_init();
}

static void cwmp_plugin_exit(UNUSED amxd_dm_t* dm,
                             UNUSED amxo_parser_t* parser) {
    app.dm = NULL;
    app.parser = NULL;
    app.amxb_bus_ctx = NULL;
    stop_cwmpd();
    cwmp_plugin_netmodel_cleanup();
    cwmp_plugin_manageableDevice_clean();
    cwmp_plugin_transfer_clean();
    unsetenv("AMXB_URI");
    unsetenv("AMXB_BACKEND");
    SAH_TRACEZ_INFO(ME, "cwmp_plugin stopped");
    if(fw_module) {
        amxm_so_close(&fw_module);
    }
}

void cwmp_plugin_netmodel_init(void) {
    netmodel_initialize();
}

void cwmp_plugin_netmodel_cleanup(void) {
    cwmp_plugin_netmodel_close_queries();
    netmodel_cleanup();
}

amxd_dm_t* cwmp_plugin_get_dm(void) {
    return app.dm;
}

amxo_parser_t* cwmp_plugin_get_parser(void) {
    return app.parser;
}

amxc_var_t* cwmp_plugin_get_config(void) {
    return &(app.parser->config);
}

amxb_bus_ctx_t* cwmp_plugin_get_bus(void) {
    return app.amxb_bus_ctx;
}

int _cwmp_plugin_main(int reason, amxd_dm_t* dm, amxo_parser_t* parser) {

    int retval = 0;

    //SAH_TRACEZ_INFO(ME, "cwmp_plugin_main, reason: %i", reason);
    switch(reason) {
    case AMXO_START: // START
        transfers_init();
        smm_init();
        cwmp_plugin_init(dm, parser);
        cwmp_plugin_xmpp_init();
        cwmp_plugin_deferred_init();
        break;
    case AMXO_STOP: // STOP
        transfers_clean();
        smm_clean();
        cwmp_plugin_exit(dm, parser);
        break;
    default:
        retval = -1;
        break;
    }

    return retval;
}

