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
#include <string.h>
#include <uriparser/Uri.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <fcntl.h>
#include <debug/sahtrace.h>
#include <debug/sahtrace_macros.h>

#include <amxc/amxc.h>
#include <amxc/amxc_macros.h>
#include <amxp/amxp.h>
#include <amxm/amxm.h>
#include "cwmp_plugin.h"

#include <amxa/amxa_merger.h>

// NI_MAXHOST normally defined in netdb.h but if it's not defined we redefine here
#ifndef NI_MAXHOST
    #define NI_MAXHOST 1025
#endif

#define DEFAULT_CRH "0.0.0.0"
#define FIREWALL_CONN_REQUEST_ID "cwmpd_conn_request"

typedef struct uri_s {
    const char* uri;
    char* scheme;
    char* host;
    char* port;
    char* path;
} uri_t;

typedef enum {
    IPv4ONLY,
    IPv6PREFERRED,
    IPANY
} wan_ip_mode_t;

// Declaration of DM functions
static void updateLocalIP(void);

// Declaration of util functions
uri_t* uri_parse(const char* uri);
static bool isAddressIpV6(const char* address);
static bool assembleConnectionRequestURL(amxd_object_t* object, amxc_string_t* url, const char* host, uint16_t port);
static void ipv4address_changed_cb(const char* sig_name, const amxc_var_t* data, void* priv);

static void ipv6address_changed_cb(const char* sig_name, const amxc_var_t* data, void* priv);
static void cwmp_plugin_netmodel_open_queries(const char* intf_path);

static void open_cwmpd_listening_port(void);
static void close_cwmpd_listening_port(void);

// Static variables
static amxc_string_t ipv4address; // CPE WAN IPv4
static amxc_string_t ipv6address; // CPE WAN IPv6
static wan_ip_mode_t wanipmode = IPANY;
static amxp_proc_ctrl_t* cwmpd_proc = NULL;
static amxp_timer_t* restart_timer = NULL;

static netmodel_query_t* query_ipv4 = NULL;
static netmodel_query_t* query_ipv6 = NULL;
static amxp_timer_t* ipv6_timer = NULL;


int cwmp_proc_ctx_new(cwmp_proc_ctx_t** ctx,
                      amxp_proc_ctrl_t* proc,
                      proc_ctrl_cb_t cb,
                      proc_ctrl_clean_cb_t clean_cb,
                      void* priv) {
    int ret = -1;
    when_null(ctx, stop);
    when_null(cb, stop);

    *ctx = (cwmp_proc_ctx_t*) calloc(1, sizeof(cwmp_proc_ctx_t));
    when_null(*ctx, stop);

    (*ctx)->cb = cb;
    (*ctx)->clean_cb = clean_cb;
    (*ctx)->proc = proc;
    (*ctx)->priv = priv;
    ret = 0;
stop:
    if((ret != 0) && (*ctx != NULL)) {
        free(*ctx);
        *ctx = NULL;
    }
    return ret;
}

static const char* getLocalIP(void) {
    const char* localIP = DEFAULT_CRH;
    switch(wanipmode) {
    case IPv4ONLY:
        if(amxc_string_text_length(&ipv4address)) {
            localIP = amxc_string_get(&ipv4address, 0);
        }
        break;
    case IPv6PREFERRED:
        //Prefer IPv6
        if(amxc_string_text_length(&ipv6address)) {
            localIP = amxc_string_get(&ipv6address, 0);
        } else if(amxc_string_text_length(&ipv4address)) {
            localIP = amxc_string_get(&ipv4address, 0);
        }
        break;
    case IPANY:
        if(amxc_string_text_length(&ipv4address)) {
            localIP = amxc_string_get(&ipv4address, 0);
        } else if(amxc_string_text_length(&ipv6address)) {
            localIP = amxc_string_get(&ipv6address, 0);
        }
        break;
    default:
        break;
    }
    return localIP;
}

static void updateLocalIP(void) {
    SAH_TRACEZ_INFO(ME, "cwmp_plugin updateLocalIP");
    amxd_object_t* conn_request = amxd_dm_findf(cwmp_plugin_get_dm(), "ManagementServer.ConnRequest");
    const char* crh_value = DEFAULT_CRH;
    amxd_status_t ret;

    if(ipv6_timer) {
        amxp_timer_stop(ipv6_timer);
        amxp_timer_delete(&ipv6_timer);
    }

    crh_value = getLocalIP();
    SAH_TRACEZ_INFO(ME, "updateLocalIP (%s)", crh_value ? crh_value : "");

    amxd_object_set_cstring_t(conn_request, "LocalIPAddress", crh_value);
    bool updateCRH = amxd_object_get_bool(conn_request, "UpdateConnRequestURL", &ret);
    if((ret == amxd_status_ok) && updateCRH) {
        amxd_trans_t trans;
        amxd_trans_init(&trans);
        amxd_trans_select_object(&trans, conn_request);
        amxd_trans_set_cstring_t(&trans, "ConnRequestHost", crh_value);
        amxd_trans_apply(&trans, cwmp_plugin_get_dm());
        amxd_trans_clean(&trans);
    }
}


static void transac_new_connection_request_url(amxc_string_t* url) {
    amxd_object_t* management_server = amxd_dm_findf(cwmp_plugin_get_dm(), "ManagementServer");
    amxd_trans_t* trans = NULL;

    if(!management_server) {
        SAH_TRACEZ_ERROR(ME, "Couldn't access dm ManagementServer");
        return;
    }
    amxd_trans_new(&trans);
    amxd_trans_select_object(trans, management_server);
    amxd_trans_set_attr(trans, amxd_tattr_change_ro, true);
    amxd_trans_set_value(cstring_t, trans, "ConnectionRequestURL", amxc_string_get(url, 0));
    amxd_trans_apply(trans, cwmp_plugin_get_dm());
    amxd_trans_delete(&trans);
}

amxd_status_t _ManagementServer_updateConnectionRequestURL(amxd_object_t* object,
                                                           UNUSED amxd_function_t* func,
                                                           amxc_var_t* args,
                                                           UNUSED amxc_var_t* ret) {
    amxd_object_t* conn_request = amxd_dm_findf(cwmp_plugin_get_dm(), "ManagementServer.ConnRequest");
    bool update = GET_BOOL(args, "update");
    const char* host = NULL;
    uint16_t port = 0;
    amxc_string_t* url = NULL;
    amxd_status_t retval = amxd_status_ok;

    amxd_object_set_bool(conn_request, "UpdateConnRequestURL", !update);
    if(update) {
        host = GET_CHAR(args, "host");
        port = GET_UINT32(args, "port");
        amxc_string_new(&url, 0);

        if(!host || !*host || !port || !assembleConnectionRequestURL(object, url, host, port)) {
            retval = amxd_status_invalid_value;
            goto error;
        }
        transac_new_connection_request_url(url);
        if(cwmpd_proc) {
            close_cwmpd_listening_port();
            open_cwmpd_listening_port();
        }
    }
error:
    amxc_string_delete(&url);
    return retval;
}

void _updateConnectionRequestURL(UNUSED const char* const sig_name,
                                 UNUSED const amxc_var_t* const data,
                                 UNUSED void* const priv) {
    SAH_TRACEZ_IN(ME);
    amxd_object_t* management_server = amxd_dm_findf(cwmp_plugin_get_dm(), "ManagementServer");
    amxd_object_t* conn_request = amxd_dm_findf(cwmp_plugin_get_dm(), "ManagementServer.ConnRequest");
    amxc_string_t* url = NULL;
    char* host = NULL;
    uint16_t port = 0;
    bool update;
    amxd_status_t ret = amxd_status_ok;

    if(!management_server || !conn_request) {
        SAH_TRACEZ_ERROR(ME, "Couldn't access dm ManagementServer");
        goto clean;
    }
    update = amxd_object_get_bool(conn_request, "UpdateConnRequestURL", &ret);
    if(update) {
        host = amxd_object_get_cstring_t(conn_request, "ConnRequestHost", &ret);
        if(ret != amxd_status_ok) {
            SAH_TRACEZ_ERROR(ME, "Couldn't find ConnRequest Host");
            goto clean;
        }
        port = amxd_object_get_uint16_t(conn_request, "ConnRequestPort", &ret);
        if(ret != amxd_status_ok) {
            SAH_TRACEZ_ERROR(ME, "Couldn't find ConnRequest Port");
            goto clean;
        }
        amxc_string_new(&url, 0);
        if(!assembleConnectionRequestURL(conn_request, url, host, port)) {
            goto clean;
        }
        transac_new_connection_request_url(url);
        if(cwmpd_proc) {
            close_cwmpd_listening_port();
            open_cwmpd_listening_port();
        }
    }

    if((!STRING_EMPTY(host)) && (strcmp(DEFAULT_CRH, host) != 0)) {
        start_cwmpd();
    } else {
        stop_cwmpd();
    }

clean:
    amxc_string_delete(&url);
    free(host);
    SAH_TRACEZ_OUT(ME);
}

void _writeInterface(UNUSED const char* const sig_name,
                     const amxc_var_t* const data,
                     UNUSED void* const priv) {
    SAH_TRACEZ_IN(ME);
    const cstring_t intf = GETP_CHAR(data, "parameters.Interface.to");
    if(STRING_EMPTY(intf)) {
        SAH_TRACEZ_ERROR(ME, "Interface parameter is empty");
        goto exit;
    }
    SAH_TRACEZ_INFO(ME, "Queries IP address From Interface %s", intf);
    cwmp_plugin_netmodel_clean_intf_info();
    cwmp_plugin_netmodel_open_queries(intf);
exit:
    SAH_TRACEZ_OUT(ME);
    return;
}

void _writePreferredIPVersion(UNUSED const char* const sig_name,
                              const amxc_var_t* const data,
                              UNUSED void* const priv) {
    const cstring_t mode = GETP_CHAR(data, "parameters.PreferredIPVersion.to");

    if(mode) {
        if(strcmp(mode, "IPV4ONLY") == 0) {
            wanipmode = IPv4ONLY;
        } else if(strcmp(mode, "IPV6PREFERRED") == 0) {
            wanipmode = IPv6PREFERRED;
        } else {
            wanipmode = IPANY;
        }
    }
}

void cwmp_plugin_netmodel_clean_intf_info(void) {
    if(query_ipv4) {
        netmodel_closeQuery(query_ipv4);
        query_ipv4 = NULL;
    }

    if(query_ipv6) {
        netmodel_closeQuery(query_ipv6);
        query_ipv6 = NULL;
    }
    if(ipv6_timer) {
        amxp_timer_stop(ipv6_timer);
        amxp_timer_delete(&ipv6_timer);
    }
    amxc_string_clean(&ipv4address);
    amxc_string_clean(&ipv6address);
}

void cwmp_plugin_netmodel_open_queries(const cstring_t intf) {
    query_ipv6 = netmodel_openQuery_luckyAddrAddress(intf,
                                                     ME,
                                                     "ipv6 global",
                                                     netmodel_traverse_down,
                                                     ipv6address_changed_cb,
                                                     NULL);

    query_ipv4 = netmodel_openQuery_luckyAddrAddress(intf,
                                                     ME,
                                                     "ipv4",
                                                     netmodel_traverse_down,
                                                     ipv4address_changed_cb,
                                                     NULL);
}

static void ipv6_timer_cb(UNUSED amxp_timer_t* timer, UNUSED void* priv) {
    updateLocalIP();
}

static void ipv4address_changed_cb(UNUSED const char* sig_name,
                                   const amxc_var_t* data,
                                   UNUSED void* priv) {
    SAH_TRACEZ_IN(ME);
    const cstring_t new_ip = amxc_var_constcast(cstring_t, data);
    SAH_TRACEZ_INFO(ME, "IPv4 Address changed to (%s)", new_ip ? new_ip : "");

    amxd_object_t* internal_settings = amxd_dm_findf(cwmp_plugin_get_dm(), "ManagementServer.InternalSettings");
    uint32_t timeout = amxc_var_constcast(uint32_t, amxd_object_get_param_value(internal_settings, "IPV4IPV6WANMaxWaitTime"));

    amxc_string_clean(&ipv4address);
    amxc_string_init(&ipv4address, 64);
    amxc_string_append(&ipv4address, new_ip ? new_ip : "", new_ip ? strlen(new_ip) : 0);

    switch(wanipmode) {
    case IPv4ONLY:
    case IPANY:
        updateLocalIP();
        break;
    case IPv6PREFERRED:
        if(!amxc_string_is_empty(&ipv6address)) {
            updateLocalIP();
        } else {
            amxp_timer_new(&ipv6_timer, ipv6_timer_cb, NULL);
            amxp_timer_start(ipv6_timer, timeout * 1000);
        }
        break;
    default:
        break;
    }
    SAH_TRACEZ_OUT(ME);
}


void cwmp_plugin_netmodel_find_ip(void) {
    SAH_TRACEZ_IN(ME);
    cstring_t interface = NULL;
    amxd_object_t* managementServer = amxd_dm_findf(cwmp_plugin_get_dm(), "ManagementServer");
    when_null(managementServer, exit);
    interface = amxd_object_get_cstring_t(managementServer, "Interface", NULL);

    if(STRING_EMPTY(interface)) {
        SAH_TRACEZ_ERROR(ME, "Interface parameter is empty");
        goto exit;
    }

    SAH_TRACEZ_NOTICE(ME, "Opening queries to get wan interface info");
    cwmp_plugin_netmodel_open_queries(interface);
exit:
    SAH_TRACEZ_OUT(ME);
    if(interface) {
        free(interface);
    }
    return;
}

static void ipv6address_changed_cb(UNUSED const char* sig_name,
                                   const amxc_var_t* data,
                                   UNUSED void* priv) {
    SAH_TRACEZ_IN(ME);
    const cstring_t new_ip = amxc_var_constcast(cstring_t, data);
    SAH_TRACEZ_INFO(ME, "IPv6 Address changed to (%s)", new_ip ? new_ip : "");

    amxc_string_clean(&ipv6address);
    amxc_string_init(&ipv6address, 64);
    amxc_string_append(&ipv6address, new_ip ? new_ip : "", new_ip ? strlen(new_ip) : 0);

    switch(wanipmode) {
    case IPv4ONLY:
        break;
    case IPv6PREFERRED:
    case IPANY:
        updateLocalIP();
        break;
    default:
        break;
    }
    SAH_TRACEZ_OUT(ME);
}


// GCOVR_EXCL_START
static int amxb_uri_part_to_string(amxc_string_t* buffer, UriTextRangeA* tr) {
    return amxc_string_append(buffer, tr->first, tr->afterLast - tr->first);
}

static int amxb_uri_path_to_string(amxc_string_t* buffer, UriPathSegmentA* ps) {
    const char* sep = "/";
    while(ps) {
        amxc_string_append(buffer, sep, strlen(sep));
        amxb_uri_part_to_string(buffer, &ps->text);
        sep = "/";
        ps = ps->next;
    }
    return 0;
}

static int amxb_uri_parse(const char* uri,
                          char** scheme,
                          char** host,
                          char** port,
                          char** path) {
    int retval = -1;
    UriUriA parsed_uri;
    amxc_string_t uri_part;
    amxc_string_init(&uri_part, 0);

    when_failed(uriParseSingleUriA(&parsed_uri, uri, NULL), exit);
    amxb_uri_part_to_string(&uri_part, &parsed_uri.scheme);
    *scheme = amxc_string_take_buffer(&uri_part);
    when_null(*scheme, exit_clean);
    amxb_uri_part_to_string(&uri_part, &parsed_uri.hostText);
    *host = amxc_string_take_buffer(&uri_part);
    amxb_uri_part_to_string(&uri_part, &parsed_uri.portText);
    *port = amxc_string_take_buffer(&uri_part);
    amxb_uri_path_to_string(&uri_part, parsed_uri.pathHead);
    *path = amxc_string_take_buffer(&uri_part);

    retval = 0;

exit_clean:
    uriFreeUriMembersA(&parsed_uri);

exit:
    amxc_string_clean(&uri_part);
    return retval;
}

uri_t* uri_parse(const char* uri) {
    uri_t* uri_struct = (uri_t*) calloc(sizeof(uri_t), 1);

    if(uri_struct && amxb_uri_parse(uri, &uri_struct->scheme, &uri_struct->host, &uri_struct->port, &uri_struct->path)) {
        free(uri_struct);
        return NULL;
    }
    return uri_struct;
}

static bool isAddressIpV6(const char* address) {
    struct in6_addr result;
    if(!address) {
        return false;
    }

    if(inet_pton(AF_INET6, address, &result) == 1) {
        return true;
    }
    return false;
}

static bool assembleConnectionRequestURL(amxd_object_t* object, amxc_string_t* url, const char* host, uint16_t port) {
    amxd_param_t* connrequestpath = amxd_object_get_param_def(object, "ConnRequestPath");
    char* path = amxc_var_get_cstring_t(&connrequestpath->value);
    bool isIPV6 = false;

    if(!path) {
        SAH_TRACEZ_ERROR(ME, "Couldn't find ConnRequestPath");
        return false;
    }

    isIPV6 = isAddressIpV6(host);
    amxc_string_setf(url, "http://%s%s%s:%hu/%s", isIPV6 ? "[" : "", host, isIPV6 ? "]" : "", port, path);
    free(path);
    return true;
}

static void cwmp_timer_cb(UNUSED amxp_timer_t* timer, UNUSED void* priv) {
    SAH_TRACEZ_INFO(ME, "wait-timer-expired start cwmpd");
    start_cwmpd();
}

static void cwmpd_proc_stopped(UNUSED void* priv) {
    stop_cwmpd();
    SAH_TRACEZ_NOTICE(ME, "cwmpd stopped !!!");
    amxp_timer_new(&restart_timer, cwmp_timer_cb, NULL);
    amxp_timer_start(restart_timer, 10000);
}

static int build_cwmpd_proc_args(amxc_array_t* cmd, UNUSED amxc_var_t* settings) {
    amxc_array_init(cmd, 2);
    amxc_string_t odl_config_opt;
    amxc_string_init(&odl_config_opt, 0);
    amxc_array_append_data(cmd, strdup("cwmpd"));
    amxc_string_setf(&odl_config_opt, "-c%s", GETP_CHAR(cwmp_plugin_get_config(), "odl_config"));
    amxc_array_append_data(cmd, strdup(amxc_string_get(&odl_config_opt, 0)));
    amxc_array_append_data(cmd, strdup("-D"));
    //clean up
    amxc_string_clean(&odl_config_opt);
    return 0;
}

static void open_cwmpd_listening_port(void) {
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
    SAH_TRACEZ_INFO("FIREWALL", "Opening cwmpd listening port %d for %s", conn_req_port, ((allowed_address[0]) ? allowed_address : "all"));
    if(amxm_execute_function("fw", "fw", "set_service", &args, &ret)) {
        SAH_TRACEZ_ERROR("FIREWALL", "Couldn't execute set_service from firewall controller");
    }
    amxc_var_clean(&ret);
    amxc_var_clean(&args);
}

static void close_cwmpd_listening_port(void) {
    amxc_var_t args, ret;

    amxc_var_init(&args);
    amxc_var_init(&ret);
    amxc_var_set_type(&args, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &args, "id", FIREWALL_CONN_REQUEST_ID);
    amxm_execute_function("fw", "fw", "delete_service", &args, &ret);
    amxc_var_clean(&ret);
    amxc_var_clean(&args);
}

void start_cwmpd(void) {
    cwmp_proc_ctx_t* ctx = NULL;
    if(cwmpd_proc) {
        SAH_TRACEZ_INFO(ME, "cwmpd already started");
        return;
    }
    open_cwmpd_listening_port();
    SAH_TRACEZ_INFO(ME, "Starting cwmpd");
    amxp_proc_ctrl_new(&cwmpd_proc, build_cwmpd_proc_args);
    cwmp_proc_ctx_new(&ctx, NULL, cwmpd_proc_stopped, NULL, NULL);
    amxp_slot_connect(cwmpd_proc->proc->sigmngr, "stop", NULL, proc_finished_cb, (void*) ctx);
    amxp_proc_ctrl_start(cwmpd_proc, 0, NULL);
}

void stop_cwmpd(void) {
    if(cwmpd_proc) {
        amxp_proc_ctrl_stop(cwmpd_proc);
        amxp_proc_ctrl_delete(&cwmpd_proc);
        cwmpd_proc = NULL;
    }
    close_cwmpd_listening_port();
}


// GCOVR_EXCL_STOP
