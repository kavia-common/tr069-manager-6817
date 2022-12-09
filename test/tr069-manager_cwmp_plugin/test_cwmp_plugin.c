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

#define _GNU_SOURCE
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <setjmp.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>

#include <cmocka.h>
#include <amxc/amxc.h>
#include <amxp/amxp.h>
#include <amxd/amxd_dm.h>
#include <amxd/amxd_object.h>
#include <amxd/amxd_transaction.h>
#include <unistd.h>
#include <amxd/amxd_dm.h>
#include <amxo/amxo.h>
#include <amxb/amxb.h>
#include <amxb/amxb_register.h>
#include <amxc/amxc_macros.h>

#include "test_cwmp_plugin.h"
#include "cwmp_plugin.h"

void __wrap_start_cwmpd();
void __wrap_stop_cwmpd();

void __wrap_start_cwmpd() {
    //Does nothing
}

void __wrap_stop_cwmpd() {
    //Does nothing
}

bool __wrap_netmodel_initialize(void) {
    return true;
}

void __wrap_netmodel_cleanup(void) {
    return;
}

void __wrap_cwmp_plugin_transfer_init() {
    //Does nothing
}

netmodel_query_t* __wrap_netmodel_openQuery_luckyAddrAddress(const char* intf,
                                                             const char* subscriber,
                                                             const char* flag,
                                                             const char* traverse,
                                                             amxp_slot_fn_t handler,
                                                             UNUSED void* userdata) {
    assert_non_null(intf);
    assert_non_null(subscriber);
    assert_non_null(flag);
    assert_non_null(traverse);
    assert_non_null(handler);
    assert_true(strncmp(intf, "Device.IP.Interface.", 20) == 0);

    amxc_var_t result;
    assert_int_equal(amxc_var_init(&result), 0);
    assert_int_equal(amxc_var_set_type(&result, AMXC_VAR_ID_CSTRING), 0);
    amxc_var_set_cstring_t(&result, "172.17.0.2");

    handler(NULL, &result, NULL);

    amxc_var_clean(&result);
    return NULL;
}

void __wrap_netmodel_closeQuery(netmodel_query_t* query) {
    assert_non_null(query);
    free(query);
}


static const char* odl_defs = "test.odl";

static amxd_dm_t dm;
static amxo_parser_t parser;


static void handle_events(void) {
    printf("Handling events ");
    while(amxp_signal_read() == 0) {
        printf(".");
    }
    printf("\n");
}

int test_cwmp_plugin_setup(UNUSED void** state) {
    amxd_object_t* root_obj = NULL;

    assert_int_equal(amxd_dm_init(&dm), amxd_status_ok);
    assert_int_equal(amxo_parser_init(&parser), 0);

    root_obj = amxd_dm_get_root(&dm);
    assert_non_null(root_obj);

    assert_int_equal(amxo_parser_parse_file(&parser, odl_defs, root_obj), 0);

    handle_events();

    return 0;

}

int test_cwmp_plugin_teardown(UNUSED void** state) {
    amxo_parser_clean(&parser);
    amxd_dm_clean(&dm);
    return 0;
}

void test_cwmp_plugin_start(UNUSED void** state) {
    assert_int_equal(_cwmp_plugin_main(0, &dm, &parser), 0);

    assert_non_null(cwmp_plugin_get_parser());
    assert_ptr_equal(cwmp_plugin_get_parser(), &parser);

    assert_non_null(cwmp_plugin_get_config());
    assert_ptr_equal(cwmp_plugin_get_config(), &parser.config);

}

void test_cwmp_plugin_parameters(UNUSED void** state) {
    amxd_dm_t* dm = cwmp_plugin_get_dm();
    assert_non_null(dm);

    amxd_object_t* management_server = NULL;
    management_server = amxd_dm_findf(dm, "ManagementServer.");
    assert_non_null(management_server);

    amxd_object_t* conn_request = amxd_dm_findf(cwmp_plugin_get_dm(), "ManagementServer.ConnRequest");
    assert_non_null(conn_request);

    amxd_object_t* internal_settings = amxd_dm_findf(cwmp_plugin_get_dm(), "ManagementServer.InternalSettings");
    assert_non_null(internal_settings);

    amxd_object_t* managementServer_state = amxd_dm_findf(cwmp_plugin_get_dm(), "ManagementServer.State");
    assert_non_null(managementServer_state);

    amxd_object_t* managementServer_stats = amxd_dm_findf(cwmp_plugin_get_dm(), "ManagementServer.Stats");
    assert_non_null(managementServer_stats);
}

void test_cwmp_plugin_write_interface(UNUSED void** state) {
    amxc_var_t data;
    amxc_var_t* values = NULL;
    amxc_var_t* parameters = NULL;
    amxd_status_t ret = amxd_status_ok;

    amxc_var_init(&data);
    amxc_var_set_type(&data, AMXC_VAR_ID_HTABLE);
    parameters = amxc_var_add_key(amxc_htable_t, &data, "parameters", NULL);
    values = amxc_var_add_key(amxc_htable_t, parameters, "Interface", NULL);
    amxc_var_add_key(cstring_t, values, "to", "Device.IP.Interface.2.");
    _writeInterface(NULL, &data, NULL);

    amxd_object_t* conn_request = amxd_dm_findf(cwmp_plugin_get_dm(), "ManagementServer.ConnRequest");
    char* local_ip = amxd_object_get_cstring_t(conn_request, "LocalIPAddress", &ret);
    assert_non_null(local_ip);
    assert_string_equal(local_ip, "172.17.0.2");

    amxc_var_clean(&data);
    free(local_ip);
}

void test_cwmp_plugin_updateConnectionRequestURL(UNUSED void** state) {
    amxd_status_t ret = amxd_status_ok;
    char* connectionRequestURL = NULL;

    amxd_object_t* management_server = amxd_dm_findf(cwmp_plugin_get_dm(), "ManagementServer");
    amxd_object_t* conn_request = amxd_dm_findf(cwmp_plugin_get_dm(), "ManagementServer.ConnRequest");

    amxd_object_set_cstring_t(management_server, "ConnectionRequestURL", "http://0.0.0.0:50805/default-path");

    amxd_object_set_cstring_t(conn_request, "ConnRequestHost", "172.17.0.2");
    amxd_object_set_cstring_t(conn_request, "ConnRequestPort", "5000");
    amxd_object_set_cstring_t(conn_request, "ConnRequestPath", "test");

    _updateConnectionRequestURL(NULL, NULL, NULL);
    // check ConnectionRequestURL is updated
    connectionRequestURL = amxd_object_get_cstring_t(management_server, "ConnectionRequestURL", &ret);
    assert_string_equal(connectionRequestURL, "http://172.17.0.2:5000/test");
    free(connectionRequestURL);
}

void test_cwmp_plugin_stop(UNUSED void** state) {
    assert_int_equal(_cwmp_plugin_main(1, &dm, &parser), 0);
}
