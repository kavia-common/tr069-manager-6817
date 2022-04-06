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
    struct event* timeout_event;
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
static int dns_ttl_val = 0;

static amxp_timer_t* dns_clean_timer = NULL;
// list of ACS addresses with ttl.
amxc_llist_t ainfo_list;
int ainfo_count = 0;
int last_ip_index = -1;

static bool is_ipaddr(const char* ip) {
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
        event_free(ev_pair->fd_event);
        ev_pair->fd_event = NULL;
    }
    free(ev_pair);
}

static void event_list_findby_fd(amxc_llist_t* event_list, int fd, event_pair_t** ev_pair) {
    amxc_llist_it_t* ev_pair_it = amxc_llist_get_first(event_list);
    bool found = false;
    while(ev_pair_it) {
        *ev_pair = amxc_llist_it_get_data(ev_pair_it, event_pair_t, it);
        if(*ev_pair && ((*ev_pair)->fd == fd)) {
            found = true;
            break;
        }
        ev_pair_it = amxc_llist_it_get_next(ev_pair_it);
    }

    if(!found) {
        *ev_pair = NULL;
    }
}

static void event_list_delete_item(event_pair_t* ev_pair) {
    if(ev_pair) {
        //remove from list
        amxc_llist_it_take(&ev_pair->it);
        //free ressources
        event_del(ev_pair->fd_event);
        event_free(ev_pair->fd_event);
        free(ev_pair);
        ev_pair = NULL;
    }
}

/* clean up DNS resolver */
static void cwmp_dns_resolver_destroy(resolver_context_t* resolver) {
    SAH_TRACEZ_INFO("CWMPD", "DNS resolver clean");
    if(!resolver) {
        return;
    }

    if(resolver->timeout_event) {
        event_del(resolver->timeout_event);
        event_free(resolver->timeout_event);
        resolver->timeout_event = NULL;
    }

    amxc_llist_clean(&resolver->event_list, event_list_clean);
    ares_destroy(resolver->ares_chann);
    amxp_timer_delete(&dns_clean_timer);
    dns_clean_timer = NULL;
    free(resolver);
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
}

/*schedule next dns query*/
static void cwmp_dns_schedule() {
    //3.1 ACS Discovery
    //...........
    //The CPE SHOULD continue to perform DNS queries as normal, but SHOULD continue using the same IP address
    //for as long as it can contact the ACS and for as long as the list of IP addresses returned by the DNS does
    //not change. The CPE SHOULD select a new IP address whenever the list of IP addresses changes or when
    //it cannot contact the ACS. This provides an opportunity for service providers to reconfigure their network.

    if((dns_ttl_val <= 0) || (dns_ttl_val == INT_MAX)) {
        //TTL=0 mean the IP will never expire, but we will keep performing
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
    ainfo_count = 0;
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
        ainfo_count++;
    }
}

/* clean resolver callback */
static void cwmp_dns_clean(UNUSED amxp_timer_t* timer, void* priv) {
    resolver_context_t* resolver = (resolver_context_t*) priv;
    cwmp_dns_resolver_destroy(resolver);
}

/* DNS TTL expired callback */
static void cwmp_dns_expired(UNUSED amxp_timer_t* timer, UNUSED void* priv) {
    // TTL expired, start New DNS query
    cwmp_dns_resolve(false);
}

//set resolver timeout
static void cwmp_dns_resolver_settimeout(resolver_context_t* resolver, int timeout);

static void cwmp_dns_timeout_cb(UNUSED int fd, UNUSED short events, void* arg) {
    resolver_context_t* resolver = (resolver_context_t*) arg;
    //ares_process_fd may send a new request to the next dns server
    //reset the timeout event, otherwise this will block forever
    //if other DNS servers dosen't respond
    cwmp_dns_resolver_settimeout(resolver, 10);
    ares_process_fd(resolver->ares_chann, ARES_SOCKET_BAD, ARES_SOCKET_BAD);
}

static void cwmp_dns_resolver_settimeout(resolver_context_t* resolver, int timeout) {
    struct timeval tv, tvmax, * tvp;
    //setup a DNS timeout event
    tvmax.tv_sec = timeout;
    tvmax.tv_usec = 0;
    tvp = ares_timeout(resolver->ares_chann, &tvmax, &tv);
    resolver->timeout_event = event_new(cwmp_evlp_get(), -1, EV_TIMEOUT,
                                        cwmp_dns_timeout_cb,
                                        resolver);
    event_add(resolver->timeout_event, tvp);
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
    SAH_TRACEZ_INFO("CWMPD", "DNS lookup process_fd");
    ares_process_fd(resolver->ares_chann, read, write);
}

/* socket callback, called twice per query if ok, multiple times if there is a timeout */
static void cwmp_dns_ares_sock_cb(UNUSED void* data, int fd, int read, int write) {
    short events = 0;
    resolver_context_t* resolver = (resolver_context_t*) data;
    event_pair_t* ev_pair = NULL;

    SAH_TRACEZ_INFO("CWMPD", "ares_fd [%d] state [read:%d] [write:%d]", fd, read, write);

    event_list_findby_fd(&resolver->event_list, fd, &ev_pair);

    //no more event on this fd
    if((read + write) == 0) {
        event_list_delete_item(ev_pair);
        if(amxc_llist_is_empty(&resolver->event_list)) {
            if(!dns_clean_timer) {
                amxp_timer_new(&dns_clean_timer, cwmp_dns_clean, (void*) resolver);
                // the callback must finish before we can destroy ares channels
                amxp_timer_start(dns_clean_timer, 2000);
            }
        }
        return;
    }
    if(read) {
        events |= EV_READ;
    }
    if(write) {
        events |= EV_WRITE;
    }

    if(!ev_pair) {
        ev_pair = malloc(sizeof(event_pair_t));
        if(!ev_pair) {
            SAH_TRACEZ_ERROR("CWMPD", "malloc fail, can't create a new event");
            return;
        }
        ev_pair->fd = fd;
        ev_pair->fd_event = NULL;
        amxc_llist_it_init(&ev_pair->it);
        amxc_llist_append(&resolver->event_list, &ev_pair->it);
    }

    ev_pair->fd_event = event_new(cwmp_evlp_get(), fd, events,
                                  cwmp_dns_fd_event_cb,
                                  (void*) resolver);

    if(!ev_pair->fd_event) {
        SAH_TRACEZ_ERROR("CWMPD", "event_new failed");
        return;
    }
    if(event_add(ev_pair->fd_event, NULL) != 0) {
        SAH_TRACEZ_ERROR("CWMPD", "dns service event_add failed");
    }
}

/* main resolver callback */
static void cwmp_dns_resolve_cb(void* data, int status, int timeouts, struct ares_addrinfo* ai) {
    resolver_context_t* resolver = (resolver_context_t*) data;

    if(timeouts > 0) {
        //some times this callback is never called until we reach the timeout
        //even with a valid result (maybe cares bug)
        SAH_TRACEZ_INFO("CWMPD", "some queries had a timeout");
    } else {
        SAH_TRACEZ_INFO("CWMPD", "deleting timeout event %p", resolver->timeout_event);
        event_del(resolver->timeout_event);
        event_free(resolver->timeout_event);
        resolver->timeout_event = NULL;
    }

    if((status == ARES_SUCCESS) && ai && ai->nodes) {
        //parse result
        cwmp_dns_read_ai(ai);
        //update ACSIPList
        cwmp_dns_update_dm();
        cwmp_dns_schedule();
        ares_freeaddrinfo(ai);
    } else {
        //DNS resolution failed, retry in 60 sec
        SAH_TRACEZ_ERROR("CWMPD", "DNS lookup Failed next in 30sec,ares_error [%s]", ares_strerror(status));
        amxp_timer_start(dns_ttl_timer, 30 * 1000);
    }
    //even if DNS resolution is failed, try to send the bootstrap
    //this will update dmengine, so it send a bootstrap next time
    //DNS is OK
    if(resolver->send_boot_strap) {
        DM_ENG_InformMessageScheduler_bootstrapInform();
    }
}

static resolver_context_t* cwmp_dns_resolver_create() {
    int ai_family = AF_UNSPEC;
    char* ai_family_str = NULL;
    int optmask = 0;
    int status = 0;
    resolver_context_t* resolver_ctx = malloc(sizeof(resolver_context_t));

    if(!resolver_ctx) {
        SAH_TRACEZ_ERROR("CWMPD", "malloc failed, cannot create a new dns resolver");
        return NULL;
    }
    //init resolver
    memset(resolver_ctx, 0, sizeof(resolver_context_t));
    amxc_llist_init(&resolver_ctx->event_list);

    //cares init options
    resolver_ctx->ares_opts.sock_state_cb = cwmp_dns_ares_sock_cb;
    resolver_ctx->ares_opts.tries = 1;
    resolver_ctx->ares_opts.sock_state_cb_data = resolver_ctx;
    resolver_ctx->ares_opts.timeout = 10;//10sec timeout
    optmask = ARES_OPT_SOCK_STATE_CB | ARES_OPT_TIMEOUTMS | ARES_OPT_TRIES;

    status = ares_init_options(&resolver_ctx->ares_chann,
                               &resolver_ctx->ares_opts,
                               optmask);

    if(status != ARES_SUCCESS) {
        SAH_TRACEZ_ERROR("CWMPD", "ares init_options error [%s]", ares_strerror(status));
        free(resolver_ctx);
        return NULL;
    }

    //setup hints
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
    resolver_ctx->hints.ai_family = ai_family;
    resolver_ctx->hints.ai_socktype = SOCK_DGRAM;
    resolver_ctx->hints.ai_flags = ARES_AI_CANONNAME | ARES_AI_ENVHOSTS | ARES_AI_NOSORT;

    return resolver_ctx;
}

/* start dns resolution for ACS host */
cwmp_status_t cwmp_dns_resolve(bool send_boot_strap) {
    cwmp_status_t ret = cwmp_status_ko;
    char* acsurl = NULL;
    const char* host = NULL;
    const char* scheme = NULL;
    int port = 0;
    const char* path = NULL;

    if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_URL, &acsurl) != 0) {
        SAH_TRACEZ_ERROR("CWMPD", "Cannot fetch the ACS SERVER URL");
        goto error;
    }

    if(lws_parse_uri(acsurl, &scheme, &host, &port, &path)) {
        SAH_TRACEZ_ERROR("CWMPD", "Couldn't parse URL (%s)", acsurl);
        goto error;
    }

    if(!host) {
        SAH_TRACEZ_ERROR("CWMPD", "ACS HOST is NULL, exit");
        goto error;
    }

    //stop any scheduled DNS
    if(amxp_timer_remaining_time(dns_ttl_timer) > 0) {
        amxp_timer_stop(dns_ttl_timer);
    }

    if(is_ipaddr(host)) {
        amxc_llist_clean(&ainfo_list, ainfo_list_clean);//clean old ips
        addr_info_t* new_addrinfo = (addr_info_t*) calloc(1, sizeof(addr_info_t));
        new_addrinfo->ip = strdup(host);
        new_addrinfo->ttl = 0;
        amxc_llist_it_init(&new_addrinfo->it);
        amxc_llist_append(&ainfo_list, &new_addrinfo->it);
        ainfo_count++;
        if(DM_ENG_SetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_ACSIPLIST, host) != 0) {
            SAH_TRACEZ_ERROR("CWMPD", "ACSIP : failed to update data model");
        }
        if(send_boot_strap) {
            //URL chenged tell dmengine to send a bootstrap
            DM_ENG_InformMessageScheduler_bootstrapInform();
        }
    } else {
        SAH_TRACEZ_INFO("CWMPD", "Starting NEW DNS lookup [%s]", host);
        resolver_context_t* resolver = cwmp_dns_resolver_create();

        if(!resolver) {
            goto error;
        }
        //keep track of the url changed event
        resolver->send_boot_strap = send_boot_strap;

        cwmp_dns_resolver_settimeout(resolver, 10);
        // initiate a new DNS query
        ares_getaddrinfo(resolver->ares_chann,
                         host, NULL,
                         &resolver->hints,
                         cwmp_dns_resolve_cb,
                         (void*) resolver);
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

void cwmp_dns_getRandomIP(char** ip) {

    if(ainfo_count <= 0) {
        if(dns_clean_timer == NULL) {
            //No DNS cache, start a new DNS query
            cwmp_dns_resolve(false);
        }
    } else {
        // the CPE SHOULD randomly choose an IP address from the list. When the CPE is unable to reach the ACS,
        // it SHOULD randomly select a different IP address from the list and attempt to contact the ACS at the
        // new IP address. This behavior ensures that CPEs will balance their requests between different ACSs
        // if multiple IP addresses represent different ACSs.
        int count = 0;
        addr_info_t* random_ip = NULL;
        amxc_llist_it_t* addr = NULL;
        srand(time(0));
        int rand_ip = (rand() % ainfo_count);//stupid, but enought for our use case
        if((rand_ip == last_ip_index) && (ainfo_count > 1)) {
            //-1
            if((rand_ip + 1) >= ainfo_count) {
                rand_ip -= 1;
            } else {//+1
                rand_ip += 1;
            }

            if(rand_ip < 0) {
                rand_ip = 0;
            }
        }
        last_ip_index = rand_ip;
        //find a new IP
        addr = amxc_llist_get_first(&ainfo_list);
        while(count != rand_ip) {
            addr = amxc_llist_it_get_next(addr);
            count++;
        }
        random_ip = amxc_llist_it_get_data(addr, addr_info_t, it);
        if(!addr || !random_ip) {
            SAH_TRACEZ_ERROR("CWMPD", "Failed to get new ACS IP");
            return;
        }
        SAH_TRACEZ_INFO("CWMPD", "trying ACS ip [%s]", random_ip->ip);
        *ip = strdup(random_ip->ip);
    }
}
