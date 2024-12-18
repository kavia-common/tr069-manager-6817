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

#if !defined(__cwmp_plugin_H__)
#define __cwmp_plugin_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <amxc/amxc.h>
#include <amxc/amxc_macros.h>
#include <amxp/amxp.h>
#include <amxd/amxd_dm.h>
#include <amxd/amxd_object.h>
#include <amxd/amxd_object_event.h>
#include <amxd/amxd_transaction.h>
#include <amxd/amxd_action.h>

#include <amxb/amxb.h>

#include <amxo/amxo.h>
#include <amxo/amxo_save.h>

#include <netmodel/common_api.h>
#include <netmodel/client.h>

#define STRING_EMPTY(TEXT) ((TEXT == NULL) || (*TEXT == 0))
#define FAULTCODE_INTERNAL_ERROR 9002

#define ME "CWMP_PLUGIN"

typedef struct _cwmp_plugin_app {
    amxd_dm_t* dm;
    amxo_parser_t* parser;
    bool init_done;
} cwmp_plugin_app_t;

typedef void (* proc_ctrl_cb_t)(void* priv);
typedef void (* proc_ctrl_clean_cb_t)(void* priv);

typedef struct _cwmp_proc_ctx {
    amxp_proc_ctrl_t* proc;
    proc_ctrl_cb_t cb;
    proc_ctrl_clean_cb_t clean_cb;
    void* priv;
} cwmp_proc_ctx_t;

int _cwmp_plugin_main(int reason, amxd_dm_t* dm, amxo_parser_t* parser);

void cwmp_plugin_set_app(amxd_dm_t* dm, amxo_parser_t* parser);
amxd_dm_t* PRIVATE cwmp_plugin_get_dm(void);
amxo_parser_t* PRIVATE cwmp_plugin_get_parser(void);
amxc_var_t* PRIVATE cwmp_plugin_get_config(void);



//Dm functions
amxd_status_t _ManagementServer_updateConnectionRequestURL(amxd_object_t* object,
                                                           amxd_function_t* func,
                                                           amxc_var_t* args,
                                                           amxc_var_t* ret);

amxd_status_t _ManagementServer_updateParameterKey(amxd_object_t* object,
                                                   amxd_function_t* func,
                                                   amxc_var_t* args,
                                                   amxc_var_t* ret);

amxd_status_t _AddTransfer(amxd_object_t* object,
                           amxd_function_t* func,
                           amxc_var_t* args,
                           amxc_var_t* ret);

void _updateConnectionRequestURL(const char* const sig_name,
                                 const amxc_var_t* const data,
                                 void* const priv);

void _updateConnectionRequestPath(UNUSED const char* const sig_name,
                                  UNUSED const amxc_var_t* const data,
                                  UNUSED void* const priv);

void _updateConnectionRequestPort(UNUSED const char* const sig_name,
                                  UNUSED const amxc_var_t* const data,
                                  UNUSED void* const priv);

void _writeInterface(const char* const sig_name,
                     const amxc_var_t* const data,
                     void* const priv);

void _writePreferredIPVersion(const char* const sig_name,
                              const amxc_var_t* const data,
                              void* const priv);

void start_cwmpd(void);

void stop_cwmpd(void);

bool isAddressIpV6(const char* address);

void proc_finished_cb(const char* const event_name,
                      const amxc_var_t* const event_data,
                      void* const priv);

//amxp_proc_ctrl_t* get_cwmpd_proc(void);
amxp_subproc_t* get_cwmpd_subproc(void);

void cwmp_plugin_netmodel_open_queries(const char* intf_path);

void cwmp_plugin_netmodel_close_queries(void);

void cwmp_plugin_netmodel_init(void);

void cwmp_plugin_netmodel_find_ip(void);

void cwmp_plugin_netmodel_cleanup(void);

void open_cwmpd_listening_port(void);

void close_cwmpd_listening_port(void);

void cwmp_plugin_update_conreq_host(const char* ip);

void cwmp_plugin_update_conreq_port(uint16_t port);

void cwmp_plugin_transfer_init(void);

void cwmp_plugin_transfer_clean(void);

void cwmp_plugin_manageabledevice_init(void);

void cwmp_plugin_manageabledevice_clean(void);

int cwmp_proc_ctx_new(cwmp_proc_ctx_t** ctx,
                      amxp_proc_ctrl_t* proc,
                      proc_ctrl_cb_t cb,
                      proc_ctrl_clean_cb_t clean_cb,
                      void* priv);

const char* cwmp_plugin_getFaultString(int code);

amxb_bus_ctx_t* get_bus_ctx(const char* inpath);

int cwmp_plugin_add_subscription(const char* path, const char* filter, amxp_slot_fn_t cb);

int cwmp_plugin_del_subscription(const char* path, amxp_slot_fn_t cb);

amxd_status_t _sendInformMessage(amxd_object_t* object,
                                 amxd_function_t* func,
                                 amxc_var_t* args,
                                 amxc_var_t* ret);

int cwmp_plugin_xmpp_init(void);
void _connreq_xmpp_connection_changed(const char* const sig_name,
                                      const amxc_var_t* const data,
                                      void* const priv);

void cwmp_plugin_dm_save(void);

bool cwmp_plugin_init_done(void);
bool cwmp_plugin_cwmpd_start_avoided(void);

#ifdef __cplusplus
}
#endif

#endif // __cwmp_plugin_H__
