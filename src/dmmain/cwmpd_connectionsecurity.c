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

// 3.2.2: The CPE SHOULD restrict the number of Connection Requests it accepts during a given period of
//        time in order to further reduce the possibility of a denial of service attack. If the CPE chooses to reject
//        a Connection Request for this reason, the CPE MUST respond to that Connection Request with an
//        HTTP 503 status code (Service Unavailable). In this case, the CPE SHOULD NOT include the HTTP
//        Retry-After header in the response.
bool cwmp_server_maxConnectionsReached(void) {
    time_t now;
    time(&now);
    bool ret = false;
    unsigned int mc = DEFAULT_MAX_CONNECTIONREQUEST;
    unsigned int fc = DEFAULT_FREQ_CONNECTION_REQUEST;
    amxc_llist_it_t* cur = NULL;
    amxc_llist_it_t* next = NULL;
    const char* prefix = cwmp_app_getconf().prefix;
    amxc_string_t maxconnectionrequestPath;
    amxc_string_t freqconnectionrequestPath;
    DM_ENG_ParameterValueStruct** params_values_st = NULL;
    char* paramsArray[3];
    amxc_string_init(&maxconnectionrequestPath, 0);
    amxc_string_init(&freqconnectionrequestPath, 0);

    amxc_string_setf(&maxconnectionrequestPath, "ManagementServer.%sMaxConnectionRequest", prefix);
    amxc_string_setf(&freqconnectionrequestPath, "ManagementServer.%sFreqConnectionRequest", prefix);

    paramsArray[0] = strdup(amxc_string_get(&maxconnectionrequestPath, 0));
    paramsArray[1] = strdup(amxc_string_get(&freqconnectionrequestPath, 0));
    paramsArray[2] = NULL;

    if(DM_ENG_GetParameterValues(DM_ENG_EntityType_SYSTEM, (char**) paramsArray, &params_values_st) != 0) {
        SAH_TRACEZ_ERROR("CWMPD", "Failed to fetch MaxConnectionRequest/FreqConnectionRequest");
    }

    if(DM_ENG_tablen((void**) params_values_st) < 2) {
        SAH_TRACEZ_ERROR("CWMPD", "Failed to fetch MaxConnectionRequest/FreqConnectionRequest");
    } else {
        mc = atoi(params_values_st[0]->value);
        fc = atoi(params_values_st[1]->value);
    }

    cur = amxc_llist_get_first(&connection_timestamp_list);
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
        ret = true;
    }

    amxc_string_clean(&maxconnectionrequestPath);
    amxc_string_clean(&freqconnectionrequestPath);
    if(params_values_st) {
        DM_ENG_deleteAllParameterValueStruct(params_values_st);
        free(params_values_st);
    }
    free(paramsArray[0]);
    free(paramsArray[1]);
    return ret;
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
