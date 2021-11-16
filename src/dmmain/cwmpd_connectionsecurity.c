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

#include <amxc/amxc.h>
#include <amxp/amxp.h>
#include <debug/sahtrace.h>

#include "dmmain/cwmpd.h"
#include <dmengine/DM_ENG_RPCInterface.h>
#include <dmcommon/DM_GlobalDefs.h>
#include <dmcom/dm_com.h>

typedef struct _time_list_item {
    time_t t;
    amxc_llist_it_t it;
} time_list_item_t;

static amxc_llist_t connection_timestamp_list;

void cwmp_server_initConnectionTimestampList(void) {
    amxc_llist_init(&connection_timestamp_list);
}

void cwmp_server_maxConnectionsAdd(void) {
    // add a new timestamp to the linked list
    time_list_item_t* item = (time_list_item_t*) calloc(1, sizeof(time_list_item_t));
    if(!item) {
        SAH_TRACEZ_ERROR("CWMPD", "Cannot allocate memory");
        return;
    }
    amxc_llist_it_init(&item->it);
    time(&item->t);
    amxc_llist_append(&connection_timestamp_list, &item->it);
}

bool cwmp_server_maxConnectionsReached(void) {
    time_t now;
    time(&now);

    // get the DM_ENG_MAXCONNECTIONREQUEST
    char* maxconnectionrequest = NULL;
    unsigned int mc = DEFAULT_MAX_CONNECTIONREQUEST;
    if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_MAXCONNECTIONREQUEST, &maxconnectionrequest) != 0) {
        SAH_TRACEZ_ERROR("CWMPD", "Cannot fetch the MaxConnectionRequest");
    }
    if(maxconnectionrequest != NULL) {
        mc = atoi(maxconnectionrequest);
        free(maxconnectionrequest);
    }

    // get the DM_ENG_FREQCONNECTIONREQUEST
    char* freqconnectionrequest = NULL;
    unsigned int fc = DEFAULT_FREQ_CONNECTION_REQUEST;
    if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_FREQCONNECTIONREQUEST, &freqconnectionrequest) != 0) {
        SAH_TRACEZ_ERROR("CWMPD", "Cannot fetch the FreqConnectionRequest");
    }
    if(freqconnectionrequest != NULL) {
        fc = atoi(freqconnectionrequest);
        free(freqconnectionrequest);
    }

    // remove all outdated and invalid timestamps (in case the system clock has been changed)
    amxc_llist_it_t* cur = amxc_llist_get_first(&connection_timestamp_list);
    amxc_llist_it_t* next = NULL;
    while(cur != NULL) {
        next = amxc_llist_it_get_next(cur);
        time_list_item_t* value = amxc_llist_it_get_data(cur, time_list_item_t, it);
        if(((now > value->t) && (now - value->t > (int) fc)) || (value->t > now)) {
            amxc_llist_it_take(cur);
            free(value);
        }
        cur = next;
    }
    // 2*mc: due to the basic/digest authentication, 2 "physical"  connection requests are send per "logical" connection request
    if(amxc_llist_size(&connection_timestamp_list) > 2 * mc) {
        SAH_TRACEZ_INFO("CWMPD", "The maximum number of connections per period is reached.");
        return true;
    }

    return false;
}

void cwmp_server_maxConnectionsCleanup(void) {
    SAH_TRACEZ_INFO("CWMPD", "Clean up max connections.");
    amxc_llist_it_t* cur = amxc_llist_get_first(&connection_timestamp_list);
    amxc_llist_it_t* next = NULL;
    while(cur != NULL) {
        next = amxc_llist_it_get_next(cur);
        time_list_item_t* value = amxc_llist_it_get_data(cur, time_list_item_t, it);
        amxc_llist_it_take(cur);
        free(value);
        cur = next;
    }

    amxc_llist_clean(&connection_timestamp_list, NULL);
}
