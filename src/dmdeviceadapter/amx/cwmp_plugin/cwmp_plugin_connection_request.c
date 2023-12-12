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

#include <amxc/amxc_string.h>
#include <amxc/amxc_variant.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <debug/sahtrace.h>
#include <debug/sahtrace_macros.h>

#include <amxc/amxc.h>
#include <amxc/amxc_macros.h>
#include <amxp/amxp.h>
#include "cwmp_plugin.h"

#include <amxa/amxa_merger.h>

// NI_MAXHOST normally defined in netdb.h but if it's not defined we redefine here
#ifndef NI_MAXHOST
    #define NI_MAXHOST 1025
#endif


#define DEFAULT_CRH "0.0.0.0"
#define FIREWALL_CONN_REQUEST_ID "cwmpd_conn_request"
#define CPE_URL_SIZE (16)

static bool conn_req_path_created = false;

// GCOVR_EXCL_START

bool isAddressIpV6(const char* address) {
    struct in6_addr result;
    if(!address) {
        return false;
    }

    if(inet_pton(AF_INET6, address, &result) == 1) {
        return true;
    }
    return false;
}

//it will be better to use the MAC from data model ???
static int get_hw_addr_from_ip(const char* ip, char* macbuf, size_t buflen) {
    char ifbuf[1024];
    int total;
    struct ifconf ifc;
    struct ifreq* ifr;
    int sock;
    int i;
    char ipbuf[INET6_ADDRSTRLEN];

    ipbuf[0] = '\0';
    macbuf[0] = 0;
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock < 0) {
        SAH_TRACEZ_ERROR(ME, "cannot create socket");
        return 1;
    }

    ifc.ifc_len = sizeof(ifbuf);
    ifc.ifc_buf = ifbuf;
    if(ioctl(sock, SIOCGIFCONF, &ifc) < 0) {
        SAH_TRACEZ_ERROR(ME, "ioctl failed");
        close(sock);
        return 1;
    }

    ifr = ifc.ifc_req;
    total = ifc.ifc_len / sizeof(struct ifreq);
    for(i = 0; i < total; i++) {
        struct ifreq* item = &ifr[i];
        struct sockaddr_in* sa = (struct sockaddr_in*) &item->ifr_addr;
        switch(sa->sin_family) {
        case AF_INET:
        case AF_INET6:
            if(inet_ntop(sa->sin_family, &sa->sin_addr, ipbuf, sizeof(ipbuf)) == NULL) {
                close(sock);
                return -1;
            }
            break;
        default:
            break;
        }
        if(strcmp(ipbuf, ip) == 0) {
            if(ioctl(sock, SIOCGIFHWADDR, item) < 0) {
                SAH_TRACEZ_ERROR(ME, "ioctl(SIOCGIFHWADDR) failed");
                close(sock);
                return 1;
            }
            snprintf(macbuf, buflen, "%02hhx:%02hhx:%02hhx:%02hhx:%02x:%02hhx\n",
                     item->ifr_hwaddr.sa_data[0], item->ifr_hwaddr.sa_data[1],
                     item->ifr_hwaddr.sa_data[2], item->ifr_hwaddr.sa_data[3],
                     item->ifr_hwaddr.sa_data[4], item->ifr_hwaddr.sa_data[5]);
        }
    }
    close(sock);
    return 0;
}

static int generate_random_url_path(char** str, int size) {

    static int counter = 0;
    int ret = -1;
    int r, x;
    char* tmp_str;
    char token[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";
    uint32_t nb_token = strlen(token);

    *str = (char*) calloc(1, sizeof(char) * (size + 1));
    when_null(str, exit);
    tmp_str = *str;

    srand(time(NULL) + counter++);

    for(x = 0; x < size; x++) {
        r = (rand() % nb_token);
        sprintf(tmp_str, "%c", token[r]);
        tmp_str++;
    }

    memcpy((void*) tmp_str, "\0", 1);

    SAH_TRACEZ_INFO("DM_COM", "Random generated str [%s]", str);
    ret = 0;
exit:
    return ret;
}

static void generate_mac_based_url_path(char** str, const char* ip) {
    char token[] = "ABCDE67FGHqrtuvwSTU48VWabcdefIJKL39MNPQRghijkmnpxyz2";
    unsigned char nb_tokens = strlen(token);
    unsigned short rnd_idx = 59;
    char macAddress[40] = {0};
    char* seed = macAddress;
    char tmp[40] = {0};
    *str = (char*) calloc(1, sizeof(char) * (CPE_URL_SIZE + 1));

    // Use MACAddress as Connection Request Path
    memset(macAddress, 0, sizeof(macAddress));
    get_hw_addr_from_ip(ip, macAddress, sizeof(macAddress));
    sprintf(tmp, "%.2X:%.2X:%.2X:%.2X:%.2X:%.2X", macAddress[0], macAddress[1], macAddress[2], macAddress[3], macAddress[4], macAddress[5]);
    SAH_TRACEZ_INFO(ME, "WAN MACAddress: %s", tmp);

    for(uint i = 0; i < CPE_URL_SIZE; i++) {
        if(!*seed) { // end of seed string reached, recommence from start
            seed = macAddress;
        }
        rnd_idx = (rnd_idx << 1) ^ *seed++;
        *str[i] = token[rnd_idx % nb_tokens];
    }
}

static void cwmp_plugin_create_conreq_path(const char* ip) {
    char* random_cpe_url = NULL;
    char* pathType = NULL;
    const char* url_env = getenv("TR069_URL_PATH");
    amxd_object_t* conn_request = amxd_dm_findf(cwmp_plugin_get_dm(), "ManagementServer.ConnRequest");

    if(url_env) {
        random_cpe_url = strdup(url_env);
        goto exit;
    }

    pathType = amxd_object_get_cstring_t(conn_request, "ConnRequestPathType", NULL);

    if(strcmp(pathType, "Fixed-Default") == 0) {
        SAH_TRACEZ_INFO(ME, "Fixed cpe url [%s]", amxd_object_get_cstring_t(conn_request, "ConnRequestPath", NULL));
    } else if(strncmp(pathType, "Random", strlen("Random")) == 0) {
        generate_random_url_path(&random_cpe_url, CPE_URL_SIZE);
    } else if(strcmp(pathType, "Fixed-MacBased") == 0) {
        generate_mac_based_url_path(&random_cpe_url, ip);
    } else {
        generate_random_url_path(&random_cpe_url, CPE_URL_SIZE);
    }

exit:
    if(random_cpe_url) {
        amxd_trans_t trans;
        amxd_trans_init(&trans);
        amxd_trans_select_object(&trans, conn_request);
        amxd_trans_set_cstring_t(&trans, "ConnRequestPath", random_cpe_url);
        amxd_trans_apply(&trans, cwmp_plugin_get_dm());
        amxd_trans_clean(&trans);
    }
    SAH_TRACEZ_INFO(ME, "CPE URL PATH [%s]", (random_cpe_url) ? random_cpe_url : "");
    free(pathType);
    free(random_cpe_url);
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

void cwmp_plugin_update_conreq_host(const char* ip) {
    SAH_TRACEZ_INFO(ME, "cwmp_plugin update conn req host %s", ip);
    amxd_object_t* conn_request = amxd_dm_findf(cwmp_plugin_get_dm(), "ManagementServer.ConnRequest");
    amxd_status_t ret;

    SAH_TRACEZ_INFO(ME, "updateLocalIP (%s)", ip ? ip : "");

    amxd_object_set_cstring_t(conn_request, "LocalIPAddress", ip);
    bool updateCRH = amxd_object_get_bool(conn_request, "UpdateConnRequestURL", &ret);

    if((ret == amxd_status_ok) && updateCRH) {
        amxd_trans_t trans;
        amxd_trans_init(&trans);
        amxd_trans_select_object(&trans, conn_request);
        amxd_trans_set_cstring_t(&trans, "ConnRequestHost", ip);
        amxd_trans_apply(&trans, cwmp_plugin_get_dm());
        amxd_trans_clean(&trans);
    }
}

void cwmp_plugin_update_conreq_port(uint16_t port) {
    SAH_TRACEZ_INFO(ME, "cwmp_plugin update conn req port");
    amxd_object_t* conn_request = amxd_dm_findf(cwmp_plugin_get_dm(), "ManagementServer.ConnRequest");
    amxd_status_t ret;

    bool updateCRH = amxd_object_get_bool(conn_request, "UpdateConnRequestURL", &ret);
    if((ret == amxd_status_ok) && updateCRH) {
        amxd_trans_t trans;
        amxd_trans_init(&trans);
        amxd_trans_select_object(&trans, conn_request);
        amxd_trans_set_uint32_t(&trans, "ConnRequestPort", port);
        amxd_trans_apply(&trans, cwmp_plugin_get_dm());
        amxd_trans_clean(&trans);
    }
}


static void transac_new_connection_request_url(amxc_string_t* url) {
    amxd_object_t* management_server = amxd_dm_findf(cwmp_plugin_get_dm(), "ManagementServer");
    amxd_trans_t* trans = NULL;
    amxd_trans_new(&trans);
    when_null_trace(management_server, exit, ERROR, "couldn't access ManagementServer dm");

    amxd_trans_select_object(trans, management_server);
    amxd_trans_set_attr(trans, amxd_tattr_change_ro, true);
    amxd_trans_set_value(cstring_t, trans, "ConnectionRequestURL", amxc_string_get(url, 0));
    amxd_trans_apply(trans, cwmp_plugin_get_dm());
exit:
    amxd_trans_delete(&trans);
}

static void update_conn_req_url(void) {
    amxd_object_t* conn_request = amxd_dm_findf(cwmp_plugin_get_dm(), "ManagementServer.ConnRequest");
    amxc_string_t* url = NULL;
    char* host = NULL;
    uint16_t port = 0;
    bool update = false;
    amxd_status_t ret = amxd_status_ok;
    amxc_string_new(&url, 0);

    when_null(conn_request, clean);

    update = amxd_object_get_bool(conn_request, "UpdateConnRequestURL", &ret);
    when_false(update, clean);

    host = amxd_object_get_cstring_t(conn_request, "ConnRequestHost", &ret);
    when_false_trace(ret == amxd_status_ok, clean, ERROR, "Couldn't find ConnRequestHost");

    port = amxd_object_get_uint16_t(conn_request, "ConnRequestPort", &ret);
    when_false_trace(ret == amxd_status_ok, clean, ERROR, "Couldn't find ConnRequestPort");

    when_false(assembleConnectionRequestURL(conn_request, url, host, port), clean);
    transac_new_connection_request_url(url);
clean:
    free(host);
    amxc_string_delete(&url);
}

// GCOVR_EXCL_STOP



amxd_status_t _ManagementServer_updateConnectionRequestURL(UNUSED amxd_object_t* object,
                                                           UNUSED amxd_function_t* func,
                                                           UNUSED amxc_var_t* args,
                                                           UNUSED amxc_var_t* ret) {
    amxd_object_t* conn_request = amxd_dm_findf(cwmp_plugin_get_dm(), "ManagementServer.ConnRequest");
    bool update = GET_BOOL(args, "update");
    const char* host = NULL;
    uint16_t port = 0;
    amxc_string_t* url = NULL;
    amxd_status_t retval = amxd_status_ok;
    amxc_string_new(&url, 0);

    amxd_object_set_bool(conn_request, "UpdateConnRequestURL", !update);
    when_false(update, exit);

    host = GET_CHAR(args, "host");
    port = GET_UINT32(args, "port");

    if(!host || !*host || !port || !assembleConnectionRequestURL(object, url, host, port)) {
        retval = amxd_status_invalid_value;
        goto exit;
    }
    transac_new_connection_request_url(url);

exit:
    amxc_string_delete(&url);
    return retval;
}

void _updateConnectionRequestPath(UNUSED const char* const sig_name,
                                  UNUSED const amxc_var_t* const data,
                                  UNUSED void* const priv) {
    SAH_TRACEZ_IN(ME);
    update_conn_req_url();
    SAH_TRACEZ_OUT(ME);
}

void _updateConnectionRequestPort(UNUSED const char* const sig_name,
                                  UNUSED const amxc_var_t* const data,
                                  UNUSED void* const priv) {
    SAH_TRACEZ_IN(ME);
    update_conn_req_url();
    open_cwmpd_listening_port();
    SAH_TRACEZ_OUT(ME);
}

void _updateConnectionRequestURL(UNUSED const char* const sig_name,
                                 UNUSED const amxc_var_t* const data,
                                 UNUSED void* const priv) {
    SAH_TRACEZ_IN(ME);
    amxd_object_t* conn_request = amxd_dm_findf(cwmp_plugin_get_dm(), "ManagementServer.ConnRequest");
    amxc_string_t* url = NULL;
    char* host = NULL;
    uint16_t port = 0;
    bool update = false;
    amxd_status_t ret = amxd_status_ok;
    amxc_string_new(&url, 0);

    when_null(conn_request, clean);

    update = amxd_object_get_bool(conn_request, "UpdateConnRequestURL", &ret);
    when_false(update, clean);

    host = amxd_object_get_cstring_t(conn_request, "ConnRequestHost", &ret);
    when_false_trace(ret == amxd_status_ok, clean, ERROR, "Couldn't find ConnRequestHost");

    port = amxd_object_get_uint16_t(conn_request, "ConnRequestPort", &ret);
    when_false_trace(ret == amxd_status_ok, clean, ERROR, "Couldn't find ConnRequestPort");

    when_false(assembleConnectionRequestURL(conn_request, url, host, port), clean);
    transac_new_connection_request_url(url);

    if((!STRING_EMPTY(host)) && (strcmp(DEFAULT_CRH, host) != 0)) {
        if(!conn_req_path_created) {
            cwmp_plugin_create_conreq_path(host);
            conn_req_path_created = true;
        }
        start_cwmpd();
    } else {
        stop_cwmpd();
    }

clean:
    free(host);
    amxc_string_delete(&url);
    SAH_TRACEZ_OUT(ME);
}

