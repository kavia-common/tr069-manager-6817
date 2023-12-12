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

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <debug/sahtrace.h>
#include <debug/sahtrace_macros.h>

#include <amxc/amxc.h>
#include <amxc/amxc_macros.h>
#include <amxp/amxp.h>
#include "cwmp_plugin.h"



int cwmp_proc_ctx_new(cwmp_proc_ctx_t** ctx,
                      amxp_proc_ctrl_t* proc,
                      proc_ctrl_cb_t cb,
                      proc_ctrl_clean_cb_t clean_cb,
                      void* priv) {
    int ret = -1;
    when_null(ctx, stop);
    when_null(cb, stop);

    *ctx = (cwmp_proc_ctx_t*) calloc(1, sizeof(cwmp_proc_ctx_t));
    when_null(*ctx, stop);

    (*ctx)->cb = cb;
    (*ctx)->clean_cb = clean_cb;
    (*ctx)->proc = proc;
    (*ctx)->priv = priv;
    ret = 0;
stop:
    if((ret != 0) && (*ctx != NULL)) {
        free(*ctx);
        *ctx = NULL;
    }
    return ret;
}

void proc_finished_cb(const char* const event_name,
                      UNUSED const amxc_var_t* const event_data,
                      void* const priv) {
    cwmp_proc_ctx_t* context = NULL;
    SAH_TRACEZ_INFO(ME, "proc signal [%s], private data [%p]", event_name, priv);

    when_null(priv, stop);
    context = (cwmp_proc_ctx_t*) priv;
    when_null(context, stop);
    when_null(context->cb, clean);
    context->cb(context->priv);
    when_null(context->clean_cb, clean);
    context->clean_cb(context->priv);

clean:
    if(context->proc) {
        amxp_proc_ctrl_delete(&context->proc);
    }
    free(context);
stop:
    return;
}


// GCOVR_EXCL_STOP
