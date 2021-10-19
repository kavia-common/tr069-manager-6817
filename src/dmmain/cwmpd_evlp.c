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
#include "dmmain/cwmpd.h"
#include <amxp/amxp.h>
#include <stdio.h>

static struct event_base* main_loop = NULL;
static struct event* sig_alarm = NULL;
static struct event* amxp_sig = NULL;
static struct event* acs_bus_ev = NULL;
static struct event* sys_bus_ev = NULL;

static void cwmp_evlp_timers_cb(UNUSED evutil_socket_t fd,
                                UNUSED short event,
                                UNUSED void* arg) {
    amxp_timers_calculate();
    amxp_timers_check();
}

static void cwmp_evlp_bus_event_cb(UNUSED evutil_socket_t fd,
                                   UNUSED short flags,
                                   void* arg) {
    amxb_bus_ctx_t* bus_ctx = (amxb_bus_ctx_t*) arg;
    amxb_read(bus_ctx);
}

static void cwmp_evlp_amxp_sig_cb(UNUSED evutil_socket_t fd,
                                  UNUSED short flags,
                                  UNUSED void* arg) {
    amxp_signal_read();
}

struct event_base* cwmp_evlp_get() {
    if(!main_loop) {
        SAH_TRACE_ERROR("CWMPD No evlp is initiaized");
    }

    return main_loop;
}

cwmp_status_t cwmp_evlp_create(amxb_bus_ctx_t* acs_bus_ctx, amxb_bus_ctx_t* sys_bus_ctx) {

    main_loop = event_base_new();

    if(main_loop == NULL) {
        SAH_TRACE_ERROR("CWMPD Creation of event base failed");
        return cwmp_status_ko;
    }

    int acsfd = amxb_get_fd(acs_bus_ctx);
    int sysfd = amxb_get_fd(sys_bus_ctx);

    // Signals
    sig_alarm = evsignal_new(main_loop,
                             SIGALRM,
                             cwmp_evlp_timers_cb,
                             NULL);
    event_add(sig_alarm, NULL);

    amxp_sig = event_new(main_loop,
                         amxp_signal_fd(),
                         EV_READ | EV_PERSIST,
                         cwmp_evlp_amxp_sig_cb,
                         NULL);
    event_add(amxp_sig, NULL);

    // bus events
    acs_bus_ev = event_new(main_loop,
                           acsfd,
                           EV_READ | EV_PERSIST,
                           cwmp_evlp_bus_event_cb,
                           acs_bus_ctx);
    event_add(acs_bus_ev, NULL);

    sys_bus_ev = event_new(main_loop,
                           sysfd,
                           EV_READ | EV_PERSIST,
                           cwmp_evlp_bus_event_cb,
                           sys_bus_ctx);
    event_add(sys_bus_ev, NULL);

    return cwmp_status_ok;
}

//Start event dispatching
cwmp_status_t cwmp_evlp_start(void) {

    if(( main_loop != NULL) &&
       (event_base_dispatch(main_loop) == 0)) {
        return cwmp_status_ok;
    }
    return cwmp_status_ko;
}

//Stop dispatching events
cwmp_status_t cwmp_evlp_stop(void) {

    if(( main_loop != NULL) &&
       (event_base_loopbreak(main_loop) == 0)) {
        return cwmp_status_ok;
    }
    return cwmp_status_ko;
}

//Cleanup memory
cwmp_status_t cwmp_evlp_clean(void) {

    if(acs_bus_ev) {
        event_del(acs_bus_ev);
        free(acs_bus_ev);
        acs_bus_ev = NULL;
    }

    if(sys_bus_ev) {
        event_del(sys_bus_ev);
        free(sys_bus_ev);
        sys_bus_ev = NULL;
    }

    if(sig_alarm) {
        event_del(sig_alarm);
        free(sig_alarm);
        sig_alarm = NULL;
    }

    if(main_loop) {
        event_base_free(main_loop);
        main_loop = NULL;
    }

    return cwmp_status_ok;
}
