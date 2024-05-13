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

static amxp_timer_t* deferred_init_timer = NULL;

static bool is_current_cnx(const char* cnx) {
    bool retval = false;
    char* current_cnx = NULL;
    when_str_empty(cnx, stop);
    current_cnx = amxd_object_get_value(cstring_t, amxd_dm_findf(cwmp_plugin_get_dm(), "ManagementServer."), "ConnReqXMPPConnection", NULL);
    when_str_empty(cnx, stop);
    if(strcmp(cnx, current_cnx) == 0) {
        retval = true;
    }
stop:
    free(current_cnx);
    return retval;
}

static void set_connreq_jabber_id(const char* jid) {
    amxd_trans_t trans;
    amxd_trans_init(&trans);
    when_null_trace(jid, stop, ERROR, "Invalid arg(s)");

    amxd_trans_select_object(&trans, amxd_dm_findf(cwmp_plugin_get_dm(), "ManagementServer."));
    amxd_trans_set_attr(&trans, amxd_tattr_change_ro, true);
    amxd_trans_set_value(cstring_t, &trans, "ConnReqJabberID", jid);
    when_failed_trace(amxd_trans_apply(&trans, cwmp_plugin_get_dm()),
                      stop, ERROR, "Failed to set ConnReqJabberID");
    SAH_TRACEZ_INFO(ME, "Set JabberID to [%s]", jid);
stop:
    amxd_trans_clean(&trans);
    return;
}

static void update_connreq_jabber_id(const char* cnx) {
    amxc_var_t ret;
    amxc_var_init(&ret);
    when_null_trace(cnx, stop, ERROR, "Invalid arg(s)");
    SAH_TRACEZ_INFO(ME, "XMPP Connection [%s]", cnx);
    if(*cnx == 0) {
        set_connreq_jabber_id("");
    } else {
        bool enable = false;
        const char* jid = NULL;
        when_failed_trace(amxb_get(amxb_be_who_has("Device."), cnx, 0, &ret, 5), stop, ERROR, "Failed to get %s", cnx);
        enable = GETP_BOOL(&ret, "0.0.Enable");
        jid = GETP_CHAR(&ret, "0.0.JabberID");
        SAH_TRACEZ_INFO(ME, "%s: Enable [%s], JabberID [%s]", cnx, enable ? "yes":"no", jid);
        if(enable) {
            set_connreq_jabber_id(jid);
        } else {
            set_connreq_jabber_id("");
        }
    }
stop:
    amxc_var_clean(&ret);
    return;
}

static void xmpp_connection_deleted_cb(UNUSED const char* const sig_name,
                                       const amxc_var_t* const data,
                                       UNUSED void* const priv) {

    uint32_t index = 0;
    const char* path = NULL;
    amxd_trans_t trans;
    amxc_string_t cnx_path;
    amxd_trans_init(&trans);
    amxc_string_init(&cnx_path, 0);
    when_true(amxc_var_is_null(data), stop);
    index = GET_UINT32(data, "index");
    path = GET_CHAR(data, "path");
    when_true(((index == 0) || (path == NULL || *path == 0)), stop);
    when_failed(amxc_string_appendf(&cnx_path, "%s%d.", path, index), stop);
    if(is_current_cnx(amxc_string_get(&cnx_path, 0))) {
        SAH_TRACEZ_INFO(ME, "%s has been deleted", amxc_string_get(&cnx_path, 0));
        amxd_trans_select_object(&trans, amxd_dm_findf(cwmp_plugin_get_dm(), "ManagementServer."));
        amxd_trans_set_value(cstring_t, &trans, "ConnReqXMPPConnection", "");
        when_failed_trace(amxd_trans_apply(&trans, cwmp_plugin_get_dm()),
                          stop, ERROR, "Failed to set ConnReqXMPPConnection");
    }
stop:
    amxd_trans_clean(&trans);
    amxc_string_clean(&cnx_path);
    return;
}

static void xmpp_connection_updated_cb(UNUSED const char* const sig_name,
                                       const amxc_var_t* const data,
                                       UNUSED void* const priv) {

    const char* cnx = NULL;
    when_true(amxc_var_is_null(data), stop);
    cnx = GETP_CHAR(data, "path");
    SAH_TRACEZ_INFO(ME, "xmpp connection updated [%s]", cnx);
    update_connreq_jabber_id(cnx);
stop:
    return;
}

static void xmpp_connection_add_sub(const char* cnx) {
    const char* expr = "(notification == 'dm:object-changed') && \
                                (contains('parameters.Enable') || contains('parameters.JabberID'))";
    when_str_empty(cnx, stop);
    when_failed_trace(amxb_subscribe(amxb_be_who_has("Device."), cnx, expr, xmpp_connection_updated_cb, NULL),
                      stop, ERROR, "Failed to subscribe to [%s]", cnx);
    SAH_TRACEZ_INFO(ME, "Subscribed to %s", cnx);
stop:
    return;
}

static void xmpp_connection_del_sub(const char* cnx) {
    when_str_empty(cnx, stop);
    amxb_unsubscribe(amxb_be_who_has("Device."), cnx, xmpp_connection_updated_cb, NULL);
    SAH_TRACEZ_INFO(ME, "Unsubscribed from %s", cnx);
stop:
    return;
}

void _connreq_xmpp_connection_changed(UNUSED const char* const sig_name,
                                      const amxc_var_t* const data,
                                      UNUSED void* const priv) {

    const char* old_cnx = NULL;
    const char* new_cnx = NULL;
    when_true(amxc_var_is_null(data), stop);
    old_cnx = GETP_CHAR(data, "parameters.ConnReqXMPPConnection.from");
    new_cnx = GETP_CHAR(data, "parameters.ConnReqXMPPConnection.to");
    xmpp_connection_del_sub(old_cnx);
    xmpp_connection_add_sub(new_cnx);
    update_connreq_jabber_id(new_cnx);
stop:
    return;
}

static void deferred_init_timer_cb(UNUSED amxp_timer_t* timer, UNUSED void* priv) {
    char* cnx = NULL;
    SAH_TRACEZ_IN(ME);
    when_failed_trace(amxb_subscribe(amxb_be_who_has("Device."), "Device.XMPP.Connection.", "notification == 'dm:instance-removed'", xmpp_connection_deleted_cb, NULL),
                      stop, ERROR, "Failed to create subscription");
    cnx = amxd_object_get_value(cstring_t, amxd_dm_findf(cwmp_plugin_get_dm(), "ManagementServer."), "ConnReqXMPPConnection", NULL);
    when_null_trace(cnx, stop, ERROR, "Failed to get ConnReqXMPPConnection");
    xmpp_connection_add_sub(cnx);
    update_connreq_jabber_id(cnx);
stop:
    free(cnx);
    amxp_timer_delete(&deferred_init_timer);
    SAH_TRACEZ_OUT(ME);
}

int cwmp_plugin_xmpp_init(void) {
    int retval = -1;
    SAH_TRACEZ_IN(ME);
    when_failed_trace(amxp_timer_new(&deferred_init_timer, deferred_init_timer_cb, NULL), stop, ERROR, "Failed to create timer");
    when_failed_trace(amxp_timer_start(deferred_init_timer, 0), stop, ERROR, "Failed to start timer");
    retval = 0;
stop:
    SAH_TRACEZ_OUT(ME);
    return retval;
}
