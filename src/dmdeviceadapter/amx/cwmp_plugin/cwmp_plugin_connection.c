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
#include <uriparser/Uri.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <fcntl.h>
#include <debug/sahtrace.h>
#include <amxc/amxc.h>
#include <amxp/amxp.h>
#include "cwmp_plugin.h"

// NI_MAXHOST normally defined in netdb.h but if it's not defined we redefine here
#ifndef NI_MAXHOST
    #define NI_MAXHOST 1025
#endif

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

// Declaration of DM functions
static void updateACSIP(bool update_only);
static void updateLocalIP(void);

// Declaration of util functions
uri_t* uri_parse(const char* uri);
static bool isIPAddress(const char* ip);
static bool isAddressIpV6(const char* address);
static void acsDNScb(const amxb_bus_ctx_t* bus_ctx, amxb_request_t* req, int status, void* priv);
static bool assembleConnectionRequestURL(amxd_object_t* object, amxc_string_t* url, const char* host, uint16_t port);
static void findAndUpdateLocalIP(const char* interface);
static void updateACSIPAddressParameter(void);

// Static variables
static amxc_string_t ipv4address; // CPE WAN IPv4
static amxc_string_t ipv6address; // CPE WAN IPv6
static amxc_string_t ipv4AcsAddressList;
static amxc_string_t ipv6AcsAddressList;
static wan_ip_mode_t wanipmode = IPV4ONLY;
static amxb_request_t* dns_req_ctx = NULL;
static unsigned int dns_ttl_timer_value = 0;
static amxp_proc_ctrl_t* cwmpd_proc;
static FILE* log_file = NULL;

void _writeURL(UNUSED const char* const sig_name,
               UNUSED const amxc_var_t* const data,
               UNUSED void* const priv) {
    updateACSIP(false);
}

static void updateLocalIP(void) {
    amxd_object_t* conn_request = amxd_dm_findf(cwmp_plugin_get_dm(), "ManagementServer.ConnRequest");
    amxc_string_t* crh_value = NULL;
    bool delete_crh = false;
    amxd_status_t ret;

    switch(wanipmode) {
    case IPV4ONLY:
        if(amxc_string_text_length(&ipv4address)) {
            crh_value = &ipv4address;
        }
        break;
    case IPV4ANDIPV6:
        if(amxc_string_text_length(&ipv6address) && amxc_string_text_length(&ipv6AcsAddressList)) {
            crh_value = &ipv6address;
        } else if(amxc_string_text_length(&ipv4address) && amxc_string_text_length(&ipv4AcsAddressList)) {
            crh_value = &ipv4address;
        } else if(amxc_string_text_length(&ipv6address)) {
            crh_value = &ipv6address;
            SAH_TRACE_WARNING("updateLocalIP setting ipv6 conreqhost, but no proper DNS record found");
        } else if(amxc_string_text_length(&ipv4address)) {
            crh_value = &ipv4address;
            SAH_TRACE_WARNING("updateLocalIP setting ipv4 conreqhost, but no proper DNS record found");
        }
        break;
    default:
        amxc_string_new(&crh_value, 8);
        amxc_string_append(crh_value, "0.0.0.0", 7);
        delete_crh = true;
        break;
    }
    updateACSIPAddressParameter();
    amxd_object_set_cstring_t(conn_request, "LocalIPAddress", amxc_string_get(crh_value, 0));
    bool updateCRH = amxd_object_get_bool(conn_request, "UpdateConnRequestURL", &ret);
    if((ret == amxd_status_ok) && updateCRH) {
        amxd_trans_t trans;
        amxd_trans_init(&trans);
        amxd_trans_select_object(&trans, conn_request);
        amxd_trans_set_cstring_t(&trans, "ConnRequestHost", amxc_string_get(crh_value, 0));
        amxd_trans_apply(&trans, cwmp_plugin_get_dm());
    }
    if(delete_crh) {
        amxc_string_delete(&crh_value);
    }
    _updateConnectionRequestURL(NULL, NULL, NULL);
}

static void updateACSIPAddressParameter(void) {
    amxd_object_t* managementServer = amxd_dm_findf(cwmp_plugin_get_dm(), "ManagementServer");
    amxc_string_t* result = NULL;

    amxc_string_new(&result, 0);
    if(ipv6address.length) {
        amxc_string_append(result, amxc_string_get(&ipv6AcsAddressList, 0), amxc_string_text_length(&ipv6AcsAddressList));
        if(amxc_string_text_length(&ipv4AcsAddressList)) {
            amxc_string_append(result, ",", 1);
        }
    }
    amxc_string_append(result, amxc_string_get(&ipv4AcsAddressList, 0), amxc_string_text_length(&ipv4AcsAddressList));
    amxd_object_set_cstring_t(managementServer, "ACSIP", amxc_string_get(result, 0));
    amxc_string_delete(&result);
}


static void updateACSIP(bool update_only) {
    amxd_object_t* managementServer = amxd_dm_findf(cwmp_plugin_get_dm(), "ManagementServer");
    amxd_object_t* internal_settings = amxd_dm_findf(cwmp_plugin_get_dm(), "ManagementServer.InternalSettings");
    amxd_param_t* urlParam = amxd_object_get_param_def(managementServer, "URL");
    const cstring_t uri = amxc_var_get_const_cstring_t(&urlParam->value);
    if(uri) {
        uri_t* acs_uri = uri_parse(uri);
        if(acs_uri && (acs_uri->host != NULL)) {
            if(isIPAddress(acs_uri->host)) {
                amxc_string_clean(&ipv4AcsAddressList);
                amxc_string_clean(&ipv6AcsAddressList);
                amxc_string_init(&ipv4AcsAddressList, 64);
                amxc_string_init(&ipv6AcsAddressList, 64);
                amxc_string_append((isAddressIpV6(acs_uri->host)) ? &ipv6AcsAddressList : &ipv4AcsAddressList, acs_uri->host, strlen(acs_uri->host));
                updateLocalIP(); //Change local URL depending on Wan interface IP
            } else {
                //Find ip address of ACS server by resolving DN
                amxd_param_t* acsipaffParam = amxd_object_get_param_def(internal_settings, "ACSIPAffinity");
                bool ACSIP_Affinity = amxc_var_get_bool(&acsipaffParam->value);
                if(!update_only || (!ACSIP_Affinity && (dns_ttl_timer_value > 0))) { // If dns is already being resolved
                    // stop current resolving request
                    amxp_timer_stop(NULL);                                           // STUB
                }
                // Launch amx call to resolv host that will call our callback when resolved
                amxb_async_invoke(cwmp_plugin_get_dns_resolv_invoke(), NULL, NULL, acsDNScb, NULL, &dns_req_ctx); // STUB
            }
        }
        free(acs_uri);
    }
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
        amxd_object_set_cstring_t(object, "ConnectionRequestURL", url->buffer);
    }
error:
    amxc_string_delete(&url);
    return retval;
}

void _updateConnectionRequestURL(UNUSED const char* const sig_name,
                                 UNUSED const amxc_var_t* const data,
                                 UNUSED void* const priv) {
    SAH_TRACE_IN();
    amxd_object_t* management_server = amxd_dm_findf(cwmp_plugin_get_dm(), "ManagementServer");
    amxd_object_t* conn_request = amxd_dm_findf(cwmp_plugin_get_dm(), "ManagementServer.ConnRequest");
    amxc_string_t* url = NULL;
    const char* host = NULL;
    uint16_t port = 0;
    bool update;
    amxd_status_t ret = amxd_status_ok;

    if(!management_server || !conn_request) {
        SAH_TRACE_ERROR("Couldn't access dm ManagementServer\n");
        goto clean;
    }
    update = amxd_object_get_bool(conn_request, "UpdateConnRequestURL", &ret);
    if(update) {
        host = amxd_object_get_cstring_t(conn_request, "ConnRequestHost", &ret);
        if(ret != amxd_status_ok) {
            SAH_TRACE_ERROR("Couldn't find ConnRequest Host");
            goto clean;
        }
        port = amxd_object_get_uint16_t(conn_request, "ConnRequestPort", &ret);
        if(ret != amxd_status_ok) {
            SAH_TRACE_ERROR("Couldn't find ConnRequest Port");
            goto clean;
        }
        amxc_string_new(&url, 0);
        if(!assembleConnectionRequestURL(conn_request, url, host, port)) {
            goto clean;
        }
        amxd_object_set_cstring_t(management_server, "ConnectionRequestURL", url->buffer);
    }
clean:
    amxc_string_delete(&url);
}

void _writeInterface(UNUSED const char* const sig_name,
                     UNUSED const amxc_var_t* const data,
                     UNUSED void* const priv) {
    findAndUpdateLocalIP(GETP_CHAR(data, "parameters.Interface.to"));
}

amxd_status_t _getACSIPTTL(UNUSED amxd_object_t* object,
                           UNUSED amxd_param_t* param,
                           UNUSED amxd_action_t reason,
                           UNUSED const amxc_var_t* const args,
                           amxc_var_t* const retval,
                           UNUSED void* priv) {
    amxc_var_set(int32_t, retval, 1);
    return amxd_status_ok;
}

static void findAndUpdateLocalIP(const char* interface) {
    SAH_TRACE_NOTICE("CWMPD listening interface is set to %s", interface);
    if(!interface || !*interface) {
        return;
    }
    struct ifaddrs* ifaddr, * ifa;
    int family, s;
    char host[NI_MAXHOST];
    amxd_status_t ret;
    amxd_object_t* conn_request = amxd_dm_findf(cwmp_plugin_get_dm(), "ManagementServer.ConnRequest");
    if(!conn_request) {
        SAH_TRACE_ERROR("Couldn't access dm ConnRequest");
        return;
    }
    if(getifaddrs(&ifaddr) == -1) {
        SAH_TRACE_ERROR("getifaddrs failed");
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
                SAH_TRACE_ERROR("getnameinfo() failed: %s", gai_strerror(s));
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

static bool isIPAddress(const char* ip) {
    struct in6_addr result;
    int res = inet_pton(AF_INET, ip, &result);
    if(res) {
        return true;
    }
    res = inet_pton(AF_INET6, ip, &result);
    if(res) {
        return true;
    }
    return false;
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

static void acsDNScb(UNUSED const amxb_bus_ctx_t* bus_ctx, UNUSED amxb_request_t* req, UNUSED int status, UNUSED void* priv) {
    // Check for error
    // Clean current IP List
    // For each interface add ip found to lists separate by a comma
}

static bool assembleConnectionRequestURL(amxd_object_t* object, amxc_string_t* url, const char* host, uint16_t port) {
    amxd_param_t* connrequestpath = amxd_object_get_param_def(object, "ConnRequestPath");
    const char* path = amxc_var_get_cstring_t(&connrequestpath->value);
    bool isIPV6 = false;

    if(!path || !*path) {
        SAH_TRACE_ERROR("Couldn't find ConnRequestPath");
        return false;
    }

    isIPV6 = isAddressIpV6(host);
    amxc_string_setf(url, "http://%s%s%s:%hu/%s", isIPV6 ? "[" : "", host, isIPV6 ? "]" : "", port, path);
    return true;
}

static int build_cwmpd_proc_args(amxc_array_t* cmd, UNUSED amxc_var_t* settings) {
    char log_level[16] = {0};

    amxc_array_init(cmd, 4);
    amxc_array_append_data(cmd, strdup("cwmpd"));
    //daemonize by default
    amxc_array_append_data(cmd, strdup("-f"));
    //TODO! manage app settings
    amxc_array_append_data(cmd, strdup("-d/usr/lib/libdmda_amx.so"));
    amxc_var_t* trace = amxc_var_get_key(cwmp_plugin_get_config(), "sahtrace", AMXC_VAR_FLAG_DEFAULT);
    sprintf(log_level, "-s%d", GET_UINT32(trace, "level"));
    amxc_array_append_data(cmd, strdup(log_level));
    return 0;
}

void start_cwmpd(void) {
    amxd_object_t* mgmt_server = amxd_dm_findf(cwmp_plugin_get_dm(), "ManagementServer");
    amxd_object_t* conn_request = amxd_dm_findf(cwmp_plugin_get_dm(), "ManagementServer.ConnRequest");
    amxd_status_t status;
    char* localip = amxd_object_get_cstring_t(conn_request, "LocalIPAddress", &status);
    if(amxd_object_get_bool(mgmt_server, "EnableCWMP", &status) && (strcmp(localip, "0.0.0.0") != 0)) {
        amxp_proc_ctrl_new(&cwmpd_proc, build_cwmpd_proc_args);
        amxp_proc_ctrl_start(cwmpd_proc, 0, NULL);
    }
}

void stop_cwmpd(void) {
    if(cwmpd_proc) {
        amxp_proc_ctrl_stop(cwmpd_proc);
        amxp_proc_ctrl_delete(&cwmpd_proc);
    }
    if(log_file) {
        fclose(log_file);
    }
}

