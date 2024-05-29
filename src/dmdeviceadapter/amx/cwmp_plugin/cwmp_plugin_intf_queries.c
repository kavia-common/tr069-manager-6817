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

#include <amxc/amxc.h>
#include <amxc/amxc_macros.h>
#include <amxp/amxp.h>
#include <debug/sahtrace.h>
#include <debug/sahtrace_macros.h>

#include "cwmp_plugin.h"

#include <amxa/amxa_merger.h>
#include <stdlib.h>
#include <string.h>

// NI_MAXHOST normally defined in netdb.h but if it's not defined we redefine here
#ifndef NI_MAXHOST
    #define NI_MAXHOST 1025
#endif


typedef enum {
    IPv4ONLY,
    IPv6PREFERRED,
    IPANY
} wan_ip_mode_t;


// Static variables
static amxc_string_t ipv4address; // CPE WAN IPv4
static amxc_string_t ipv6address; // CPE WAN IPv6
static wan_ip_mode_t wanipmode = IPANY;

static netmodel_query_t* query_ipv4 = NULL;
static netmodel_query_t* query_ipv6 = NULL;
static amxp_timer_t* ipv6_timer = NULL;


static const char* getLocalIP(void) {
    const char* localIP = "0.0.0.0";
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
    const char* crh_value = "0.0.0.0";
    if(ipv6_timer) {
        amxp_timer_stop(ipv6_timer);
        amxp_timer_delete(&ipv6_timer);
    }

    crh_value = getLocalIP();
    cwmp_plugin_update_conreq_host(crh_value);
}

static void ipv6_timer_cb(UNUSED amxp_timer_t* timer, UNUSED void* priv) {
    updateLocalIP();
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


void _writeInterface(UNUSED const char* const sig_name,
                     const amxc_var_t* const data,
                     UNUSED void* const priv) {
    SAH_TRACEZ_IN(ME);
    const cstring_t intf = GETP_CHAR(data, "parameters.Interface.to");
    when_null_trace(intf, exit, ERROR, "Interface is NULL");
    when_str_empty_trace(intf, exit, ERROR, "interface cannot be empty");

    SAH_TRACEZ_INFO(ME, "Queries IPs From Interface %s", intf);
    cwmp_plugin_netmodel_close_queries();
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

void cwmp_plugin_netmodel_close_queries(void) {
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


void cwmp_plugin_netmodel_find_ip(void) {
    SAH_TRACEZ_IN(ME);
    cstring_t interface = NULL;
    amxd_object_t* managementServer = amxd_dm_findf(cwmp_plugin_get_dm(), "ManagementServer");
    when_null(managementServer, exit);
    interface = amxd_object_get_cstring_t(managementServer, "Interface", NULL);

    when_null_trace(interface, exit, ERROR, "Interface parameter is empty");
    when_true_trace(STRING_EMPTY(interface), exit, ERROR, "Interface parameter is empty");

    SAH_TRACEZ_NOTICE(ME, "Opening queries to get wan interface info [%s]", interface);
    cwmp_plugin_netmodel_open_queries(interface);
exit:
    SAH_TRACEZ_OUT(ME);
    if(interface) {
        free(interface);
    }
    return;
}

