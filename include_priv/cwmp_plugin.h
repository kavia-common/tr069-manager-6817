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

#if !defined(__cwmp_plugin_H__)
#define __cwmp_plugin_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <amxc/amxc.h>
#include <amxp/amxp.h>
#include <amxd/amxd_dm.h>
#include <amxd/amxd_object.h>
#include <amxd/amxd_object_event.h>
#include <amxd/amxd_transaction.h>
#include <amxd/amxd_action.h>

#include <amxb/amxb.h>

#include <amxo/amxo.h>
#include <amxo/amxo_save.h>

#define PRIVATE __attribute__ ((visibility("hidden")))
#define UNUSED __attribute__((unused))

#define when_null(x, l) if(x == NULL) { goto l; }
#define when_failed(x, l) if(x != 0) { goto l; }

#define ME "CWMP_PLUGIN"

typedef struct _cwmp_plugin_app {
    amxd_dm_t* dm;
    amxo_parser_t* parser;
    amxb_bus_ctx_t* amxb_bus_ctx;
} cwmp_plugin_app_t;

int _cwmp_plugin_main(int reason, amxd_dm_t* dm, amxo_parser_t* parser);

amxd_dm_t* PRIVATE cwmp_plugin_get_dm(void);
amxo_parser_t* PRIVATE cwmp_plugin_get_parser(void);
amxc_var_t* PRIVATE cwmp_plugin_get_config(void);
amxb_bus_ctx_t* PRIVATE cwmp_plugin_get_bus(void);

//Dm functions
amxd_status_t _ManagementServer_save(amxd_object_t* object,
                                     amxd_function_t* func,
                                     amxc_var_t* args,
                                     amxc_var_t* ret);

amxd_status_t _ManagementServer_load(amxd_object_t* object,
                                     amxd_function_t* func,
                                     amxc_var_t* args,
                                     amxc_var_t* ret);

amxd_status_t _ManagementServer_updateConnectionRequestURL(amxd_object_t* object,
                                                           amxd_function_t* func,
                                                           amxc_var_t* args,
                                                           amxc_var_t* ret);

void _manageCwmpd(UNUSED const char* const sig_name,
                  UNUSED const amxc_var_t* const data,
                  UNUSED void* const priv);

void _updateConnectionRequestURL(const char* const sig_name,
                                 const amxc_var_t* const data,
                                 void* const priv);

void _writeInterface(const char* const sig_name,
                     const amxc_var_t* const data,
                     void* const priv);

amxd_status_t _getACSIPTTL(amxd_object_t* object,
                           amxd_param_t* param,
                           amxd_action_t reason,
                           const amxc_var_t* const args,
                           amxc_var_t* const retval,
                           void* priv);

void findWanInterface(void);

void wanIPAddressChanged(const char* const sig_name,
                         const amxc_var_t* const data,
                         void* const priv);

void start_cwmpd(void);

void stop_cwmpd(void);

void cwmpd_proc_stopped(const char* const event_name,
                        UNUSED const amxc_var_t* const event_data,
                        UNUSED void* const priv);

#ifdef __cplusplus
}
#endif

#endif // __cwmp_plugin_H__
