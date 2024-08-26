/****************************************************************************
**
** SPDX-License-Identifier: BSD-2-Clause-Patent
**
** SPDX-FileCopyrightText: Copyright (c) 2024 SoftAtHome
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

#include "cwmp_plugin.h"
#include <amxc/amxc_macros.h>
#include <amxc/amxc_string.h>
#include <amxc/amxc_variant.h>
#include <amxd/amxd_path.h>
#include <debug/sahtrace.h>
#include <debug/sahtrace_macros.h>

static const int _MAX_FAULT_MESSAGES = 20; // Index of the last element in the _FAULT_MESSAGES table (Vendor specific fault)

static const char* _FAULT_MESSAGES[] =
{
    "Method not supported",                             // 9000
    "Request denied (no reason specified)",             // 9001
    "Internal error",                                   // 9002
    "Invalid arguments",                                // 9003
    "Resources exceeded",                               // 9004
    "Invalid parameter name",                           // 9005
    "Invalid parameter type",                           // 9006
    "Invalid parameter value",                          // 9007
    "Attempt to set a non-writable parameter",          // 9008
    "Notification request rejected",                    // 9009
    "Download failure",                                 // 9010
    "Upload failure",                                   // 9011
    "File transfer server authentication failure",      // 9012
    "Unsupported protocol for file transfer",           // 9013
    "Download failure: unable to join multicast group", // 9014
    "Download failure: unable to contact file server",  // 9015
    "Download failure: unable to access file",          // 9016
    "Download failure: unable to complete download",    // 9017
    "Download failure: file corrupted",                 // 9018
    "Download failure: file authentication failure",    // 9019
    "Vendor specific fault"                             // 9800 - 9899
};

/**
 * @param code Fault code
 * @return The corresponding fault string
 */
const char* cwmp_plugin_getFaultString(int code) {
    const char* res = NULL;
    int i = code - 9000;
    if((i >= 0) && (i < _MAX_FAULT_MESSAGES)) {
        res = _FAULT_MESSAGES[i];
    } else if((i >= 800) && (i <= 899)) {
        res = _FAULT_MESSAGES[_MAX_FAULT_MESSAGES];
    }
    return res;
}

amxb_bus_ctx_t* get_bus_ctx(const char* inpath) {
    amxb_bus_ctx_t* bus_ctx = NULL;
    amxd_path_t path;
    char* fixed_part = NULL;
    when_str_empty(inpath, stop);
    amxd_path_init(&path, inpath);
    fixed_part = amxd_path_get_fixed_part(&path, false);
    when_null(fixed_part, stop);
    bus_ctx = amxb_be_who_has(fixed_part);

stop:
    free(fixed_part);
    amxd_path_clean(&path);
    return bus_ctx;
}

int cwmp_plugin_add_subscription(const char* path, const char* filter, amxp_slot_fn_t cb) {
    int ret = -1;
    amxb_bus_ctx_t* bus_ctx = NULL;

    bus_ctx = get_bus_ctx(path);
    when_null_trace(bus_ctx, stop, ERROR, "Could not find the context of [%s]", path);

    ret = amxb_subscribe(bus_ctx, path, filter, cb, NULL);
    when_failed_trace(ret, stop, ERROR, "Failed to subscribe on [%s]", path);
    ret = 0;
stop:
    return ret;
}

int cwmp_plugin_del_subscription(const char* path, amxp_slot_fn_t cb) {
    int ret = -1;
    amxb_bus_ctx_t* bus_ctx = NULL;

    bus_ctx = get_bus_ctx(path);
    when_null_trace(bus_ctx, stop, ERROR, "Could not find the context of [%s]", path);

    ret = amxb_unsubscribe(bus_ctx, path, cb, NULL);
    when_failed_trace(ret, stop, ERROR, "Failed to unsubscribe on [%s]", path);

    ret = 0;
stop:
    return ret;
}