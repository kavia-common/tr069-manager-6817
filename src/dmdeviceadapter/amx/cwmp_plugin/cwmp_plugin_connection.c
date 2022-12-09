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
    IPV4ONLY,
    IPV4ANDIPV6
} wan_ip_mode_t;

netmodel_query_t* query_ipv4 = NULL;

// Declaration of DM functions
static void updateLocalIP(void);

// Declaration of util functions
uri_t* uri_parse(const char* uri);
static bool isAddressIpV6(const char* address);
static bool assembleConnectionRequestURL(amxd_object_t* object, amxc_string_t* url, const char* host, uint16_t port);
UNUSED static void findAndUpdateLocalIP(const char* interface);
static void ipv4address_changed_cb(const char* sig_name, const amxc_var_t* data, void* priv);
static void open_cwmpd_listening_port(void);
static void close_cwmpd_listening_port(void);

// Static variables
static amxc_string_t ipv4address; // CPE WAN IPv4
static amxc_string_t ipv6address; // CPE WAN IPv6
static wan_ip_mode_t wanipmode = IPV4ONLY;
static amxp_proc_ctrl_t* cwmpd_proc = NULL;
static amxp_timer_t* restart_timer = NULL;

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

static void updateLocalIP(void) {
    SAH_TRACEZ_INFO(ME, "cwmp_plugin updateLocalIP");
    amxd_object_t* conn_request = amxd_dm_findf(cwmp_plugin_get_dm(), "ManagementServer.ConnRequest");
    const char* crh_value = DEFAULT_CRH;
    amxd_status_t ret;

    switch(wanipmode) {
    case IPV4ONLY:
        if(amxc_string_text_length(&ipv4address)) {
            crh_value = amxc_string_get(&ipv4address, 0);
        }
        SAH_TRACEZ_INFO(ME, "updateLocalIP (%s)", crh_value ? crh_value : "");
        break;
    case IPV4ANDIPV6:
        if(amxc_string_text_length(&ipv6address)) {
            crh_value = amxc_string_get(&ipv6address, 0);
        } else if(amxc_string_text_length(&ipv4address)) {
            crh_value = amxc_string_get(&ipv4address, 0);
        }
        SAH_TRACEZ_INFO(ME, "updateLocalIP (%s)", crh_value ? crh_value : "");
        break;
    default:
        break;
    }

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
    _updateConnectionRequestURL(NULL, NULL, NULL);
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

        if(!host || !*host || !port || !assembleConnectionRequestURL(object, url, host, port)) { // We might need to split the expression for more precise logging
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
        // IPAddress changed so it should start cwmpd here when not already started
        start_cwmpd();
    } else {
        // IPAddress was cleared, it should stop cwmpd.
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

    // Close the existing query.
    if(NULL != query_ipv4) {
        netmodel_closeQuery(query_ipv4);
        query_ipv4 = NULL;
        SAH_TRACEZ_INFO(ME, "Close exiting query for ipv4");
    }

    query_ipv4 = netmodel_openQuery_luckyAddrAddress(intf,
                                                     ME,
                                                     "ipv4",
                                                     netmodel_traverse_down,
                                                     ipv4address_changed_cb,
                                                     NULL);

exit:
    SAH_TRACEZ_OUT(ME);
    return;
}

void cwmp_plugin_netmodel_clean_intf_info(void) {
    SAH_TRACEZ_IN(ME);
    if(NULL != query_ipv4) {
        netmodel_closeQuery(query_ipv4);
        query_ipv4 = NULL;
    }
    SAH_TRACEZ_OUT(ME);
}

static void ipv4address_changed_cb(UNUSED const char* sig_name,
                                   const amxc_var_t* data,
                                   UNUSED void* priv) {
    SAH_TRACEZ_IN(ME);
    const cstring_t new_ip = amxc_var_constcast(cstring_t, data);
    SAH_TRACEZ_INFO(ME, "WAN IP Address changed to %s", new_ip ? new_ip : "");

    amxc_string_clean(&ipv4address);
    amxc_string_init(&ipv4address, 64);
    // When new_ip is null or empty (if IPAddress was cleared ) should update crh.
    amxc_string_append(&ipv4address, new_ip ? new_ip : "", new_ip ? strlen(new_ip) : 0);
    updateLocalIP();
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

    // TODO : add a query for ipv6-up
    SAH_TRACEZ_NOTICE(ME, "Opening queries to get wan interface info");
    query_ipv4 = netmodel_openQuery_luckyAddrAddress(interface,
                                                     ME,
                                                     "ipv4",
                                                     netmodel_traverse_down,
                                                     ipv4address_changed_cb,
                                                     NULL);
exit:
    SAH_TRACEZ_OUT(ME);
    if(interface) {
        free(interface);
    }
    return;
}

static void findAndUpdateLocalIP(const char* interface) {
    SAH_TRACEZ_NOTICE(ME, "CWMPD listening interface is set to %s", interface);
    if(!interface || !*interface) {
        return;
    }
    struct ifaddrs* ifaddr, * ifa;
    int family, s;
    char host[NI_MAXHOST];
    amxd_status_t ret;
    amxd_object_t* conn_request = amxd_dm_findf(cwmp_plugin_get_dm(), "ManagementServer.ConnRequest");
    if(!conn_request) {
        SAH_TRACEZ_ERROR(ME, "Couldn't access dm ConnRequest");
        return;
    }
    if(getifaddrs(&ifaddr) == -1) {
        SAH_TRACEZ_ERROR(ME, "getifaddrs failed");
        return;
    }
    for(ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if(ifa->ifa_addr == NULL) {
            continue;
        }
        family = ifa->ifa_addr->sa_family;
        if(!((family == AF_INET) || (family == AF_INET6))) {
            continue;
        }
        if(ifa->ifa_name && interface && (strcmp(ifa->ifa_name, interface) == 0)) {
            s = getnameinfo(ifa->ifa_addr, (family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6),
                            host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            if(s != 0) {
                SAH_TRACEZ_ERROR(ME, "getnameinfo() failed: %s", gai_strerror(s));
                continue;
            }
            amxc_string_t* ip = (family == AF_INET) ? &ipv4address : &ipv6address;
            amxc_string_clean(ip);
            amxc_string_init(ip, NI_MAXHOST);
            amxc_string_append(ip, host, strlen(host));
            amxd_object_set_cstring_t(conn_request, "LocalIPAddress", host);
            bool updateCRH = amxd_object_get_bool(conn_request, "UpdateConnRequestURL", &ret);
            if((ret == amxd_status_ok) && updateCRH) {
                amxd_trans_t trans;
                amxd_trans_init(&trans);
                amxd_trans_select_object(&trans, conn_request);
                amxd_trans_set_cstring_t(&trans, "ConnRequestHost", host);
                amxd_trans_apply(&trans, cwmp_plugin_get_dm());
            }
            goto stop;
        }
    }
stop:
    freeifaddrs(ifaddr);
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
    SAH_TRACEZ_INFO(ME, "wait-timer-expired start cwmpd again");
    start_cwmpd();
}

static void cwmpd_proc_stopped(UNUSED void* priv) {
    stop_cwmpd();
    SAH_TRACEZ_NOTICE(ME, "cwmpd stopped signal stopped ");
    // cwmpd is dead, wait for x time then restart it
    amxp_timer_new(&restart_timer, cwmp_timer_cb, NULL);
    // restart in 10 seconds
    amxp_timer_start(restart_timer, 10000);
}

static int build_cwmpd_proc_args(amxc_array_t* cmd, UNUSED amxc_var_t* settings) {
    SAH_TRACEZ_NOTICE(ME, "preparing cwmpd");
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
    int conn_req_port;
    const char* interface, * allowed_address;
    amxc_var_t args, ret;

    conn_req_port = amxc_var_constcast(uint32_t, amxd_object_get_param_value(conn_req, "ConnRequestPort"));
    allowed_address = amxc_var_constcast(cstring_t, amxd_object_get_param_value(internal_settings, "AllowConnectionRequestFromAddress"));
    interface = amxc_var_constcast(cstring_t, amxd_object_get_param_value(mgmt_server, "Interface"));
    amxc_var_init(&args);
    amxc_var_init(&ret);
    amxc_var_set_type(&args, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &args, "id", FIREWALL_CONN_REQUEST_ID);
    amxc_var_add_key(cstring_t, &args, "protocol", "TCP");
    amxc_var_add_key(cstring_t, &args, "interface", interface);
    amxc_var_add_key(uint32_t, &args, "destination_port", conn_req_port);
    amxc_var_add_key(cstring_t, &args, "source_prefix", allowed_address);
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
