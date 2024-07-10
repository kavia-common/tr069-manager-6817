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

#include <stdio.h>
#include <debug/sahtrace.h>
#include "dmmain/cwmpd.h"

typedef struct _timer_list_item {
    amxp_timer_t* timer;
    amxc_string_t* name;
    timerHandler handler;
    bool interval_timer;
    struct _timer_list_item* next;
} timer_list_item;

static timer_list_item* timer_list = NULL;

/**
 * Find the specified timer and return the struct using name
 */
static timer_list_item* timer_find(const char* name) {
    if((name == NULL) || (*name == 0)) {
        return NULL;
    }
    for(timer_list_item* timer = timer_list; name && timer; timer = timer->next) {
        if(timer->name && (amxc_string_get(timer->name, 0) != NULL) && (strcmp(amxc_string_get(timer->name, 0), name) == 0)) {
            return timer;
        }
    }
    return NULL;
}

/**
 * Find the specified timer and return the struct using amxp_timer_t* addr
 */
static timer_list_item* timer_find_by_addr(amxp_timer_t* t) {
    for(timer_list_item* timer = timer_list; t && timer; timer = timer->next) {
        if(t == timer->timer) {
            return timer;
        }
    }
    return NULL;
}

/**
 * Cleanup the specified timer struct
 */
static void timer_free(timer_list_item* item) {
    amxp_timer_stop(item->timer);
    amxc_string_delete(&item->name);
    amxp_timer_delete(&item->timer);
    free(item);
}

/**
 * Remove the specified timer from the list
 */
static void timer_remove(const char* name) {
    timer_list_item* item = NULL;
    timer_list_item* prev = NULL;

    for(item = timer_list; name && item; item = item->next) {
        if(item->name && (strcmp(name, item->name->buffer) == 0)) {
            if(item == timer_list) {
                timer_list = item->next;
            }
            if(prev) {
                prev->next = item->next;
            }
            timer_free(item);
            break;
        }
        prev = item;
    }
    return;
}


/**
 * This function is the main timeout handler, this one will calls the corresponding
 * callback routing in the DM_ENGINE
 * If it's a single shot timer, the timer will be removed from memory
 */
static void timer_timeout_handler(amxp_timer_t* timer, void* priv) {
    amxc_string_t* name = (amxc_string_t*) priv;
    char* arg = NULL;
    timer_list_item* item;
    timerHandler handler;

    SAH_TRACEZ_INFO("CWMPD", "Timer callback %s (%p)", name->buffer, timer);
    item = timer_find_by_addr(timer);
    if(!item) {
        SAH_TRACEZ_WARNING("CWMPD", "Timer timeout for inexisting timer %s", name->buffer);
        return;
    }
    arg = amxc_string_dup(name, 0, amxc_string_text_length(name));
    if(!item->handler) {
        return;
    }
    handler = item->handler;
    if(item->interval_timer == false) {
        timer_remove(name->buffer);
    }
    (*handler)(arg);
    free(arg);
}

/**
 * Find the specified timer and return the struct
 */
static void timer_append(timer_list_item* item_to_add) {
    if(item_to_add) {
        item_to_add->next = NULL;
    } else {
        return;
    }
    if(timer_list) {
        for(timer_list_item* item = timer_list; item; item = item->next) {
            if(item->next == NULL) {
                item->next = item_to_add;
                break;
            }
        }
    } else {
        timer_list = item_to_add;
    }
}

int cwmp_timer_start(const char* name, int waitTime, int intervalTime, timerHandler handler) {
    timer_list_item* timer = NULL;
    if((name == NULL) || (*name == 0)) {
        return cwmp_status_ko;
    }
    timer = timer_find(name);

    if(!timer) {
        timer = (timer_list_item*) malloc(sizeof(*timer));
        if(!timer) {
            return cwmp_status_ko;
        }
        timer->timer = NULL;
        amxc_string_new(&timer->name, strlen(name));
        amxc_string_append(timer->name, name, strlen(name));
        amxp_timer_new(&timer->timer, timer_timeout_handler, timer->name);
        if(intervalTime) {
            amxp_timer_set_interval(timer->timer, intervalTime * 1000);
            timer->interval_timer = true;
        } else {
            timer->interval_timer = false;
        }
        timer->handler = handler;
        timer_append(timer);
    } else {
        SAH_TRACEZ_INFO("CWMPD", "Stopping timer %s", name);
        amxp_timer_stop(timer->timer);
    }
    SAH_TRACEZ_INFO("CWMPD", "Starting timer %s with %d seconds", name, waitTime);
    amxp_timer_start(timer->timer, waitTime * 1000);
    return cwmp_status_ok;
}

int cwmp_timer_stop(const char* name) {
    timer_list_item* item = NULL;
    if((name == NULL) || (*name == 0)) {
        return cwmp_status_ko;
    }
    item = timer_find(name);

    if(item) {
        amxp_timer_stop(item->timer);
        timer_remove(name);
    } else {
        return cwmp_status_ko;
    }
    SAH_TRACEZ_INFO("CWMPD", "timer stop: %s", name);
    return cwmp_status_ok;
}

unsigned int cwmp_timer_remainingTime(const char* name) {
    timer_list_item* item = timer_find(name);

    return (item) ? amxp_timer_remaining_time(item->timer) : 0;
}
