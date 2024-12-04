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
#if !defined(_CWMPD_H_)
#define _CWMPD_H_

#include <libwebsockets.h>
#include <event2/event.h>
#include <debug/sahtrace.h>

#include <amxc/amxc.h>
#include <amxp/amxp.h>
#include <amxd/amxd_dm.h>
#include <amxd/amxd_object.h>
#include <amxb/amxb.h>
#include <amxo/amxo.h>
#include <amxa/amxa_merger.h>

#include <dmengine/DM_ENG_NotificationInterface.h>
#include <ares.h>

#define ME "CWMPD"

#define CWMPD_FREE(p) { free(p); p = NULL; }

#define COPY_BUFFER_SIZE 4 * 1024

#define EVENT_ENG_SRV_RESTART      "HTTP_SERVER_RESTART"
#define EVENT_ENG_SRV_STOP         "HTTP_SERVER_STOP"
#define EVENT_ENG_SRV_START        "HTTP_SERVER_START"
#define EVENT_ENG_CLEAR_ACS_IP     "CLIENT_CLEAR_ACS_IP"
#define EVENT_ENG_URL_CHANGED      "ACS_URL_CHANGED"

typedef enum server_state {INIT = 0, RUN, EXIT, ERROR } server_state_t;
typedef enum cwmp_status {cwmp_status_ok=0, cwmp_status_ko} cwmp_status_t;

struct application {
    const char* name;
    const char* odl_config;
    int daemonize;
    int traceLevel;
    sah_trace_type traceType;
    server_state_t state;
    const char* prefix;
    const char* trustedCA;
    const char* ssl_client_priv_key;
    const char* ssl_client_cert;
    const char* pidFile;
    const char* persistent_rpc_path;
    char* da_path;
    const char* aclfile;
};

typedef struct application application_t;

application_t cwmp_app_getconf(void);

//Server
cwmp_status_t cwmp_server_start(void);

cwmp_status_t cwmp_server_stop(void);

//Client
cwmp_status_t cwmp_client_init(void);

cwmp_status_t cwmp_client_stop(void);

void cwmp_client_clear_ACSIP();

// This routine is used to check the connection acceptance policy
void cwmp_server_initConnectionTimestampList(void);

void cwmp_server_maxConnectionsCleanup(void);

void cwmp_server_maxConnectionsAdd(void);

bool cwmp_server_maxConnectionsReached(void);

//Eventloop
cwmp_status_t cwmp_evlp_create(amxb_bus_ctx_t* acs_bus_ctx, amxb_bus_ctx_t* sys_bus_ctx);

cwmp_status_t cwmp_evlp_start(void);

cwmp_status_t cwmp_evlp_stop(void);

cwmp_status_t cwmp_evlp_clean(void);

struct event_base* cwmp_evlp_get(void);

//Timer Interface
int cwmp_timer_stop(const char* name);

int cwmp_timer_start(const char* name, int waitTime, int intervalTime, timerHandler handler);

void cwmp_timer_remove_all(void);

unsigned int cwmp_timer_remainingTime(const char* name);

//DNS Resolver
cwmp_status_t cwmp_dns_init();

cwmp_status_t cwmp_dns_resolve(bool send_boot_strap);

cwmp_status_t cwmp_dns_stop();

void cwmp_dns_get_random_ip(char** ip);

bool is_ipaddr(const char* ip);
int ip_type(const char* ip);
bool is_valid_ipv4(const char* ip);
bool is_valid_ipv6(const char* ip);

#endif // !_CWMPD_H_
