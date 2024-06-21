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
#include <netinet/in.h>
#include <stdio.h>
#include <fcntl.h>
#include <debug/sahtrace.h>
#include <dmcom/dm_com.h>
#include <ares.h>
#include <arpa/inet.h>
#include <event2/event.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <limits.h>
#include <dmengine/DM_ENG_InformMessageScheduler.h>
#include "dmmain/cwmpd.h"

typedef struct event_pair_s {
    struct event* fd_event;
    int fd;
    amxc_llist_it_t it;
} event_pair_t;

typedef struct resolver_context_s {
    ares_channel ares_chann;
    struct ares_options ares_opts;
    struct ares_addrinfo_hints hints;
    bool send_boot_strap;
    amxc_llist_t event_list;

} resolver_context_t;

typedef struct addr_info_s {
    char* ip;
    unsigned int ttl;
    amxc_llist_it_t it;
} addr_info_t;

//TTL timer
static amxp_timer_t* dns_ttl_timer = NULL;
static amxp_timer_t* dns_clean_timer = NULL;
static amxp_timer_t* dns_timeout_timer = NULL;
static int dns_ttl_val = 0;
// list of ACS addresses with ttl.
static amxc_llist_t ainfo_list;

static void ainfo_list_clean(amxc_llist_it_t* it) {
    addr_info_t* ainfo = amxc_container_of(it, addr_info_t, it);
    if(ainfo && ainfo->ip) {
        free(ainfo->ip);
    }
    free(ainfo);
}

static void event_list_clean(amxc_llist_it_t* it) {
    event_pair_t* ev_pair = amxc_container_of(it, event_pair_t, it);
    if(ev_pair && ev_pair->fd_event) {
        event_del(ev_pair->fd_event);
        event_free(ev_pair->fd_event);
        ev_pair->fd_event = NULL;
    }
    free(ev_pair);
}

static event_pair_t* event_list_findby_fd(amxc_llist_t* event_list, int fd) {
    amxc_llist_it_t* ev_pair_it = amxc_llist_get_first(event_list);

    while(ev_pair_it) {
        event_pair_t* ev_pair = amxc_llist_it_get_data(ev_pair_it, event_pair_t, it);
        if(ev_pair && (ev_pair->fd == fd)) {
            return ev_pair;
        }
        ev_pair_it = amxc_llist_it_get_next(ev_pair_it);
    }
    return NULL;
}

/* clean up DNS resolver */
static void cwmp_dns_resolver_destroy(resolver_context_t* resolver) {
    if(resolver) {
        amxc_llist_clean(&resolver->event_list, event_list_clean);
        ares_destroy(resolver->ares_chann);
        free(resolver);
    }
}

/* Update DM ACSIPLIST */
static void cwmp_dns_update_dm() {
    //update data-model
    amxc_llist_it_t* addr = amxc_llist_get_first(&ainfo_list);
    amxc_string_t addr_list_str;
    amxc_string_init(&addr_list_str, 0);
    while(addr != NULL) {
        addr_info_t* value = amxc_llist_it_get_data(addr, addr_info_t, it);
        amxc_string_append(&addr_list_str, value->ip, strlen(value->ip));
        addr = amxc_llist_it_get_next(addr);
        if(addr) {
            amxc_string_append(&addr_list_str, ",", 1);
        }
    }

    if(DM_ENG_SetManagementServerValue(DM_ENG_EntityType_SYSTEM,
                                       DM_ENG_ACSIPLIST,
                                       amxc_string_get(&addr_list_str, 0)) != 0) {
        SAH_TRACEZ_ERROR("CWMPD", "failed to update data model ACSIPLIST");
    }
    amxc_string_clean(&addr_list_str);
    amxc_llist_clean(&ainfo_list, ainfo_list_clean);
}

//3.1 ACS Discovery
//...........
//The CPE SHOULD continue to perform DNS queries as normal, but SHOULD continue using the same IP address
//for as long as it can contact the ACS and for as long as the list of IP addresses returned by the DNS does
//not change. The CPE SHOULD select a new IP address whenever the list of IP addresses changes or when
//it cannot contact the ACS. This provides an opportunity for service providers to reconfigure their network.
static void cwmp_dns_schedule() {
    if((dns_ttl_val <= 0) || (dns_ttl_val == INT_MAX)) {
        //TTL=0 means the IP will never expire, but we will keep performing
        //DNS resolution each periodicinform interval, in case network configuration
        //has changed
        dns_ttl_val = 300;//default to 300 sec
        char* periodic_infor_interval = NULL;
        if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_PERIODICINFORMINTERVAL, &periodic_infor_interval) == 0) {
            if(periodic_infor_interval) {
                dns_ttl_val = atoi(periodic_infor_interval);
                free(periodic_infor_interval);
            }
        }
    }
    SAH_TRACEZ_INFO("CWMPD", "Next DNS lookup in [%d] sec", dns_ttl_val);
    amxp_timer_start(dns_ttl_timer, dns_ttl_val * 1000);
}

/* parse DNS result */
static void cwmp_dns_read_ai(struct ares_addrinfo* ai) {
    //clean any old data
    amxc_llist_clean(&ainfo_list, ainfo_list_clean);
    amxc_llist_init(&ainfo_list);
    dns_ttl_val = INT_MAX;
    const struct ares_addrinfo_node* ai_cur;
    char ip[46] = "";
    uint8_t* nip = NULL;
    for(ai_cur = ai->nodes; ai_cur != NULL; ai_cur = ai_cur->ai_next) {
        addr_info_t* new_addrinfo = NULL;

        inet_ntop(ai_cur->ai_family, ai_cur->ai_addr->sa_data, ip, 46);
        switch(ai_cur->ai_family) {
        case AF_INET:
            nip = (uint8_t*) &((struct sockaddr_in*) ai_cur->ai_addr)->sin_addr;
            break;
        case AF_INET6:
            nip = (uint8_t*) &((struct sockaddr_in6*) ai_cur->ai_addr)->sin6_addr;
            break;
        }
        ares_inet_ntop(ai_cur->ai_family, nip, ip, 46);

        if((strlen(ip) == 0)) {
            continue;
        }

        new_addrinfo = (addr_info_t*) calloc(1, sizeof(addr_info_t));
        new_addrinfo->ip = strdup(ip);
        new_addrinfo->ttl = ai_cur->ai_ttl;

        SAH_TRACEZ_INFO("CWMPD", "DNS lookup found IPv%d [%s], TTL [%d]",
                        ai_cur->ai_family == PF_INET6 ? 6 : 4,
                        ip,
                        ai_cur->ai_ttl);

        if(dns_ttl_val > ai_cur->ai_ttl) {
            //use the lowest TTL
            dns_ttl_val = ai_cur->ai_ttl;
        }
        amxc_llist_it_init(&new_addrinfo->it);
        amxc_llist_append(&ainfo_list, &new_addrinfo->it);
    }
}

/* clean resolver callback */
static void cwmp_dns_clean(UNUSED amxp_timer_t* timer, void* priv) {
    SAH_TRACEZ_INFO("CWMPD", "DNS resolver clean");
    amxp_timer_delete(&dns_clean_timer);
    dns_clean_timer = NULL;
    amxp_timer_delete(&dns_timeout_timer);
    dns_timeout_timer = NULL;
    cwmp_dns_resolver_destroy((resolver_context_t*) priv);
}

/* DNS TTL expired callback */
static void cwmp_dns_expired(UNUSED amxp_timer_t* timer, UNUSED void* priv) {
    cwmp_dns_resolve(false);
}

/* fd callback */
static void cwmp_dns_fd_event_cb(int fd, short flags, void* arg) {
    resolver_context_t* resolver = (resolver_context_t*) arg;
    int write = ARES_SOCKET_BAD;
    int read = ARES_SOCKET_BAD;

    if(flags & EV_READ) {
        read = fd;
    }
    if(flags & EV_WRITE) {
        write = fd;
    }
    ares_process_fd(resolver->ares_chann, read, write);
}

static void cwmp_dns_timeout_cb(UNUSED amxp_timer_t* timer, void* priv) {
    resolver_context_t* resolver = (resolver_context_t*) priv;
    if(resolver) {
        ares_process_fd(resolver->ares_chann, ARES_SOCKET_BAD, ARES_SOCKET_BAD);
    }
}

static void cwmp_dns_fd_poll_stop(resolver_context_t* resolver, int fd) {
    event_pair_t* fd_poll_event = event_list_findby_fd(&resolver->event_list, fd);

    if(!fd_poll_event) {
        return;
    }

    amxc_llist_it_take(&fd_poll_event->it);
    event_del(fd_poll_event->fd_event);
    event_free(fd_poll_event->fd_event);
    free(fd_poll_event);
    fd_poll_event = NULL;
}

static void cwmp_dns_fd_poll_start(resolver_context_t* resolver, short events, int fd) {
    event_pair_t* fd_poll_event = event_list_findby_fd(&resolver->event_list, fd);

    if(!fd_poll_event) {
        fd_poll_event = malloc(sizeof(event_pair_t));
        if(!fd_poll_event) {
            SAH_TRACEZ_INFO("CWMPD", "malloc fail, can't create a new event\n");
            return;
        }
        fd_poll_event->fd = fd;
        fd_poll_event->fd_event = NULL;
        amxc_llist_it_init(&fd_poll_event->it);
        amxc_llist_append(&resolver->event_list, &fd_poll_event->it);
    } else if(fd_poll_event->fd_event) {
        event_del(fd_poll_event->fd_event);
        event_free(fd_poll_event->fd_event);
    }

    fd_poll_event->fd_event = event_new(cwmp_evlp_get(), fd, events,
                                        cwmp_dns_fd_event_cb, (void*) resolver);

    event_add(fd_poll_event->fd_event, NULL);
}

/* socket callback, called twice per query if ok, multiple times if there is a timeout */
static void cwmp_dns_ares_sock_cb(UNUSED void* data, int fd, int read, int write) {
    short events = 0;
    resolver_context_t* resolver = (resolver_context_t*) data;

    SAH_TRACEZ_INFO("CWMPD", "ares_fd [%d] state [read:%d] [write:%d]", fd, read, write);

    if((read + write) == 0) {
        cwmp_dns_fd_poll_stop(resolver, fd);
        if(amxc_llist_is_empty(&resolver->event_list)) {
            amxp_timer_stop(dns_timeout_timer);
            if(!dns_clean_timer) {
                amxp_timer_new(&dns_clean_timer, cwmp_dns_clean, (void*) resolver);
                // the callback must finish before we can destroy ares channels
                amxp_timer_start(dns_clean_timer, 2000);
            }
        }
    } else {
        if(read) {
            events |= EV_READ;
        }
        if(write) {
            events |= EV_WRITE;
        }

        cwmp_dns_fd_poll_start(resolver, events, fd);

        if(!dns_timeout_timer) {
            amxp_timer_new(&dns_timeout_timer, cwmp_dns_timeout_cb, (void*) resolver);
            amxp_timer_set_interval(dns_timeout_timer, 2000);
            amxp_timer_start(dns_timeout_timer, 2000);
        }
    }
}

/* main resolver callback */
static void cwmp_dns_resolve_cb(void* data, int status, UNUSED int timeouts, struct ares_addrinfo* ai) {
    resolver_context_t* resolver = (resolver_context_t*) data;

    amxp_timer_stop(dns_timeout_timer);

    if((status == ARES_SUCCESS) && ai && ai->nodes) {
        cwmp_dns_read_ai(ai);
        cwmp_dns_update_dm();
        cwmp_dns_schedule();

        // DNS is resolved - retry to send pending inform messages immediatly
        if(resolver->send_boot_strap == false) {
            DM_ENG_NotificationInterface_timerStart("Retry-timer", 0, 0, DM_ENG_InformMessageScheduler_sendMessage);
        }
    } else {
        //DNS resolution failed, retry in 5 sec
        SAH_TRACEZ_ERROR("CWMPD", "DNS lookup Failed next in 5 sec,ares_error [%s]", ares_strerror(status));
        amxp_timer_start(dns_ttl_timer, 5 * 1000);
    }

    if(ai) {
        ares_freeaddrinfo(ai);
    }

    //even if DNS resolution is failed, try to send the bootstrap
    //this will update dmengine, so it send a bootstrap next time
    //DNS is OK
    if(resolver->send_boot_strap) {
        DM_ENG_InformMessageScheduler_bootstrapInform();
    }
}

static int cwmp_dns_get_aifamily() {
    int ai_family = AF_UNSPEC;
    char* ai_family_str = NULL;
    if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM,
                                       DM_ENG_ACSADDRFAMILY,
                                       &ai_family_str) == 0) {
        if(ai_family_str) {
            if(atoi(ai_family_str) == 4) {
                ai_family = AF_INET;
            } else if(atoi(ai_family_str) == 6) {
                ai_family = AF_INET6;
            }
            free(ai_family_str);
        }
    }
    return ai_family;
}

static resolver_context_t* cwmp_dns_resolver_create() {
    int optmask = 0;
    int status = 0;
    resolver_context_t* resolver_ctx = calloc(1, sizeof(resolver_context_t));

    if(!resolver_ctx) {
        SAH_TRACEZ_ERROR("CWMPD", "malloc failed, cannot create a new dns resolver");
        return NULL;
    }
    amxc_llist_init(&resolver_ctx->event_list);

    //cares init options
    resolver_ctx->ares_opts.sock_state_cb = cwmp_dns_ares_sock_cb;
    resolver_ctx->ares_opts.tries = 1;
    resolver_ctx->ares_opts.sock_state_cb_data = resolver_ctx;
    optmask = ARES_OPT_SOCK_STATE_CB | ARES_OPT_TRIES;

    status = ares_init_options(&resolver_ctx->ares_chann,
                               &resolver_ctx->ares_opts,
                               optmask);

    if(status != ARES_SUCCESS) {
        SAH_TRACEZ_ERROR("CWMPD", "ares init_options error [%s]", ares_strerror(status));
        free(resolver_ctx);
        return NULL;
    }

    //setup hints
    resolver_ctx->hints.ai_family = cwmp_dns_get_aifamily();
    resolver_ctx->hints.ai_socktype = SOCK_DGRAM;
    resolver_ctx->hints.ai_flags = ARES_AI_CANONNAME | ARES_AI_ENVHOSTS | ARES_AI_NOSORT;

    return resolver_ctx;
}

static const char* cwmp_dns_get_host(char* acsurl) {
    const char* host = NULL;
    const char* scheme = NULL;
    int port = 0;
    const char* path = NULL;

    if(lws_parse_uri(acsurl, &scheme, &host, &port, &path)) {
        SAH_TRACEZ_INFO("CWMPD", "Couldn't parse URL (%s)", acsurl);
        host = NULL;
    }
    return (host);
}

static void cwmp_dns_set_static_ip(const char* host) {
    addr_info_t* new_addrinfo = (addr_info_t*) calloc(1, sizeof(addr_info_t));

    new_addrinfo->ip = strdup(host);
    new_addrinfo->ttl = 0;
    amxc_llist_clean(&ainfo_list, ainfo_list_clean);//clean old ips
    amxc_llist_it_init(&new_addrinfo->it);
    amxc_llist_append(&ainfo_list, &new_addrinfo->it);

    if(DM_ENG_SetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_ACSIPLIST, host) != 0) {
        SAH_TRACEZ_ERROR("CWMPD", "ACSIP : failed to update data model");
    }
}

static cwmp_status_t cwmp_dns_ares_resolution(bool send_boot_strap, const char* host) {
    resolver_context_t* resolver = cwmp_dns_resolver_create();
    if(!resolver) {
        return cwmp_status_ko;
    }
    //keep track of the url changed event
    resolver->send_boot_strap = send_boot_strap;

    // initiate a new DNS query
    ares_getaddrinfo(resolver->ares_chann,
                     host, NULL,
                     &resolver->hints,
                     cwmp_dns_resolve_cb,
                     (void*) resolver);
    return cwmp_status_ok;
}

/* start dns resolution for ACS host */
cwmp_status_t cwmp_dns_resolve(bool send_boot_strap) {
    cwmp_status_t ret = cwmp_status_ko;
    char* acsurl = NULL;
    const char* host = NULL;

    if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_URL, &acsurl) != 0) {
        SAH_TRACEZ_ERROR("CWMPD", "Cannot fetch the ACS SERVER URL");
        goto error;
    }
    host = cwmp_dns_get_host(acsurl);
    if(!host) {
        SAH_TRACEZ_ERROR("CWMPD", "ACS HOST is NULL, exit");
        goto error;
    }
    //stop any scheduled DNS
    if(amxp_timer_remaining_time(dns_ttl_timer) > 0) {
        amxp_timer_stop(dns_ttl_timer);
    }

    if(is_ipaddr(host)) {
        cwmp_dns_set_static_ip(host);
        if(send_boot_strap) {
            //URL changed tell dmengine to send a bootstrap
            DM_ENG_InformMessageScheduler_bootstrapInform();
        }
    } else {
        SAH_TRACEZ_INFO("CWMPD", "Starting NEW DNS lookup [%s]", host);
        if(cwmp_dns_ares_resolution(send_boot_strap, host) == cwmp_status_ko) {
            SAH_TRACEZ_ERROR("CWMPD", "Failed to send DNS query");
            goto error;
        }
    }
    ret = cwmp_status_ok;
error:
    if(acsurl) {
        free(acsurl);
    }
    return ret;
}

cwmp_status_t cwmp_dns_init() {

    int status = ares_library_init(ARES_LIB_INIT_ALL);

    if(status != ARES_SUCCESS) {
        SAH_TRACEZ_ERROR("CWMPD", "ares library_init error [%s]", ares_strerror(status));
        return cwmp_status_ko;
    }

    amxp_timer_new(&dns_ttl_timer, cwmp_dns_expired, NULL);

    return cwmp_status_ok;
}

cwmp_status_t cwmp_dns_stop() {

    SAH_TRACEZ_INFO("CWMPD", "Stop DNS resolver");
    if(dns_ttl_timer) {
        amxp_timer_stop(dns_ttl_timer);
        amxp_timer_delete(&dns_ttl_timer);
    }
    //force clean resolver
    if(dns_clean_timer) {
        amxp_timer_start(dns_clean_timer, 0);
    }

    ares_library_cleanup();
    amxc_llist_clean(&ainfo_list, ainfo_list_clean);

    return cwmp_status_ok;
}

// the CPE SHOULD randomly choose an IP address from the list. When the CPE is unable to reach the ACS,
// it SHOULD randomly select a different IP address from the list and attempt to contact the ACS at the
// new IP address. This behavior ensures that CPEs will balance their requests between different ACSs
// if multiple IP addresses represent different ACSs.
void cwmp_dns_get_random_ip(char** ip) {
    char* acsiplist = NULL;
    int64_t count = 0;
    amxc_string_t addr_list_str;
    amxc_var_t ip_list;
    amxc_string_init(&addr_list_str, 0);
    amxc_var_init(&ip_list);

    if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_ACSIPLIST, &acsiplist) != 0) {
        SAH_TRACEZ_ERROR("CWMPD", "Cannot fetch the ACSIP list");
    }
    if((acsiplist != NULL) && (strlen(acsiplist) > 1)) {
        amxc_string_set(&addr_list_str, acsiplist);
        amxc_string_csv_to_var(&addr_list_str, &ip_list, NULL);
        amxc_var_for_each(var, &ip_list) {
            count++;
        }
        if(count >= 1) {
            int rand_index = (rand() % count);
            amxc_var_t* ip_var = amxc_var_get_index(&ip_list, rand_index, AMXC_VAR_FLAG_DEFAULT);
            const char* ipaddr = amxc_var_constcast(cstring_t, ip_var);
            if(ipaddr != NULL) {
                SAH_TRACEZ_ERROR("CWMPD", "Select  ip [%s] from list", ipaddr);
                *ip = strdup(ipaddr);
            }
        }
    } else {
        SAH_TRACEZ_INFO("CWMPD", "There is no ip on datamodel");
        if(dns_clean_timer == NULL) {
            //No DNS cache, start a new DNS query
            SAH_TRACEZ_ERROR("CWMPD", "Start a new dsn resolution");
            cwmp_dns_resolve(false);
        }
    }
}
