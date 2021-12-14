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
#include <string.h>

#include "dmmain/cwmpd.h"

static int cwmp_client_getACSAddrFamily() {
    uint8_t addrFamily = 0;
    char* tmp = NULL;
    if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_ACSADDRFAMILY, &tmp) == 0) {
        if(tmp) {
            addrFamily = atoi(tmp);
            free(tmp);
        }
    }
    SAH_TRACEZ_INFO("CWMPD", "ACSADDFamily = %d", addrFamily);
    if(addrFamily == 4) {
        return AF_INET;
    } else if(addrFamily == 6) {
        return AF_INET6;
    }
    return AF_UNSPEC;
}

static void cwmp_dns_state_cb(UNUSED void* data, int fd, int read, int write) {
    SAH_TRACEZ_INFO("CWMPD", "Change state fd %d read:%d write:%d", fd, read, write);
}

static void cwmp_dns_resolve_cb(void* data, int status, UNUSED int timeouts, struct ares_addrinfo* ai) {
    if(!ai || (status != ARES_SUCCESS)) {
        SAH_TRACEZ_INFO("CWMPD", "Failed to lookup %s", ares_strerror(status));
        return;
    }
    SAH_TRACEZ_INFO("CWMPD", "DNS lookup OK");
    struct ares_addrinfo** dns_cache = (struct ares_addrinfo**) data;
    (*dns_cache) = ai;// pass result to client
}

static void
wait_ares(ares_channel channel) {
    // this will block until we resolve DNS or timeout
    // this is the behavior we want, dont use
    // the evlp here?
    for(;;) {
        struct timeval* tvp, tv;
        fd_set read_fds, write_fds;
        int nfds;

        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);
        nfds = ares_fds(channel, &read_fds, &write_fds);
        if(nfds == 0) {
            break;
        }
        tvp = ares_timeout(channel, NULL, &tv);
        select(nfds, &read_fds, &write_fds, NULL, tvp);
        ares_process(channel, &read_fds, &write_fds);
    }
}

cwmp_status_t cwmp_dns_resolve(const char* hostname, struct ares_addrinfo** dns_cache) {
    ares_channel channel = NULL;
    struct ares_options ares_opts;
    int status, optmask = 0;
    memset(&ares_opts, 0, sizeof(ares_opts));

    status = ares_library_init(ARES_LIB_INIT_ALL);
    if(status != ARES_SUCCESS) {
        SAH_TRACEZ_ERROR("CWMPD", "ares init failed with error [%s]", ares_strerror(status));
        return cwmp_status_ko;
    }

    ares_opts.sock_state_cb = cwmp_dns_state_cb;
    optmask |= ARES_OPT_SOCK_STATE_CB;

    status = ares_init_options(&channel, &ares_opts, optmask);
    if(status != ARES_SUCCESS) {
        SAH_TRACEZ_ERROR("CWMPD", "ares set options failed error [%s]", ares_strerror(status));
        return cwmp_status_ko;
    }

    struct ares_addrinfo_hints hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = cwmp_client_getACSAddrFamily();
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = ARES_AI_CANONNAME | ARES_AI_ENVHOSTS | ARES_AI_NOSORT;

    SAH_TRACEZ_INFO("CWMPD", "Send DNS query [URI=%s]", hostname);
    ares_getaddrinfo(channel, hostname, NULL, &hints, cwmp_dns_resolve_cb, (void*) dns_cache);
    wait_ares(channel);
    ares_destroy(channel);
    ares_library_cleanup();
    return cwmp_status_ok;
}