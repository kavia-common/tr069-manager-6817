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

#define _GNU_SOURCE
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <setjmp.h>
#include <stdarg.h>
#include <ctype.h>
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
#include "dummy_be.h"
#include "test_dm_adapter.h"
#include <amxc/amxc_macros.h>
#include <dmengine/DM_ENG_Device.h>

#define DM_ENG_METHOD_NOT_SUPPORTED          (9000)
#define DM_ENG_REQUEST_DENIED                (9001)
#define DM_ENG_INTERNAL_ERROR                (9002)
#define DM_ENG_INVALID_ARGUMENTS             (9003)
#define DM_ENG_RESOURCES_EXCEEDED            (9004)
#define DM_ENG_INVALID_PARAMETER_NAME        (9005)
#define DM_ENG_INVALID_PARAMETER_TYPE        (9006)
#define DM_ENG_INVALID_PARAMETER_VALUE       (9007)
#define DM_ENG_READ_ONLY_PARAMETER           (9008)
#define DM_ENG_NOTIFICATION_REQUEST_REJECTED (9009)
#define DM_ENG_DOWNLOAD_FAILURE              (9010)
#define DM_ENG_UPLOAD_FAILURE                (9011)
#define DM_ENG_AUTHENTICATION_FAILURE        (9012)
#define DM_ENG_UNSUPPORTED_PROTOCOL          (9013)


#define HOST_1_MAC "DD:DD:DD:FF:EE:DD"
#define HOST_1_IP  "192.168.0.220"
#define HOST_2_MAC "DD:DD:DD:FF:FF:FF"
#define HOST_2_IP  "192.168.0.221"


#define PRINT_RESULT 0

static const char* dm_dev_adapter_path = "../../output/x86_64-linux-gnu/libdmda_amx/libdmda_amx.so";
static const char* acache_file = "/tmp/cwmp_acache.txt";
static const char* acl_file = "./acl_test.json";
char* g_randomCpeUrl = NULL;

/* Some functions needed to implement tests not really used */
int inform(DM_ENG_DeviceIdStruct* id,
           DM_ENG_EventStruct* events[],
           DM_ENG_ParameterValueStruct* parameterList[],
           time_t currentTime, unsigned int retryCount) {
    (void) id;
    (void) events;
    (void) parameterList;
    (void) currentTime;
    (void) retryCount;
    return 0;
}

int transferComplete(DM_ENG_TransferCompleteStruct* tcs) {
    (void) tcs;
    return 0;
}

int requestDownload(const char* fileType, DM_ENG_ArgStruct* args[]) {
    (void) fileType;
    (void) args;
    return 0;
}

int getRPCMethods() {
    return 0;
}

int timerStart(const char* name, int waitTime, int intervalTime, timerHandler handler) {
    (void) name;
    (void) waitTime;
    (void) intervalTime;
    (void) handler;
    return 0;
}

int timerStop(const char* name) {
    (void) name;
    return 0;
}

unsigned int timerTimeRemaining(const char* name) {
    (void) name;
    return 0;
}

int engineEvent(const char* eventType) {
    (void) eventType;
    return 0;
}

int DM_SendHttpMessage(const char* msgToSendStr) {
    (void) msgToSendStr;
    return 0;
}
int client_startSession() {
    return 0;
}
int DM_CloseHttpSession(int closeMode) {
    (void) closeMode;
    return 0;
}

static amxb_bus_ctx_t* acs_bus_ctx = NULL;
static amxb_bus_ctx_t* system_bus_ctx = NULL;
static amxd_dm_t dm;
static amxo_parser_t parser;


static void handle_events(void) {
    printf("Handling events ");
    while(amxp_signal_read() == 0) {
        printf(".");
    }
    printf("\n");
}

static int check_parameter_value(DM_ENG_ParameterValueStruct** pv, char* path, char* val, int type) {
    int nb_param = DM_ENG_tablen((void**) pv);
    for(int i = 0; i < nb_param; i++) {
        if(( strcmp(pv[i]->parameterName, path) == 0 )
           && ( strcmp(pv[i]->value, val) == 0 )
           && ((int) pv[i]->type == type )) {
            return 0;
        }
    }
    return -1;
}


static int check_parameter_name(DM_ENG_ParameterInfoStruct** pI, char* path, bool writable) {
    int nb_param = DM_ENG_tablen((void**) pI);
    for(int i = 0; i < nb_param; i++) {
        if(( strcmp(pI[i]->parameterName, path) == 0 )
           && ((int) pI[i]->writable == writable )) {
            return 0;
        }
    }
    return -1;
}

//Add Host instance
static void add_host_instance(const char* mac, const char* ip) {
    int rv = 0;
    const char* object = "Device.Hosts.Host.";
    amxc_var_t ret;
    amxc_var_t values;

    amxc_var_init(&ret);
    amxc_var_init(&values);
    amxc_var_set_type(&values, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &values, "MACAddress", mac);
    amxc_var_add_key(cstring_t, &values, "IPAddress", ip);
    rv = amxb_add(acs_bus_ctx, object, 0, NULL, &values, &ret, 5);
    //amxc_var_dump(&ret, STDOUT_FILENO);
    if((rv != 0) || amxc_var_is_null(&ret)) {
        fprintf(stderr, "failed to add test object [%s] error %d \n", object, rv);
    }
    amxc_var_clean(&ret);
    amxc_var_clean(&values);
}

int test_dmadapter_setup(UNUSED void** state) {
    amxd_object_t* root_obj = NULL;

    test_register_dummy_be();

    assert_int_equal(amxd_dm_init(&dm), amxd_status_ok);
    assert_int_equal(amxo_parser_init(&parser), 0);

    root_obj = amxd_dm_get_root(&dm);
    assert_non_null(root_obj);

    assert_int_equal(amxo_parser_parse_file(&parser, "./test.odl", root_obj), 0);

    handle_events();
    // override the connection URI
    setenv("AMXB_URI", "dummy:/tmp/dummy.sock", 1);
    return 0;
}

int test_dmadapter_teardown(UNUSED void** state) {

    unsetenv("AMXB_URI");
    DM_ENG_DeactivateNotification(DM_ENG_EntityType_ANY);
    DM_ENG_Device_Unload();

    handle_events();
    amxb_be_remove_all();

    amxo_parser_clean(&parser);
    amxd_dm_clean(&dm);

    test_unregister_dummy_be();

    return 0;
}

void test_dmadapter_load(UNUSED void** state) {
    int rv = DM_ENG_Device_Load(dm_dev_adapter_path);
    assert_int_not_equal(rv, 0);
}

/* test suite */
void test_dmadapter_connection(UNUSED void** state) {

    void* systemctx = NULL;
    void* acsctx = NULL;
    int rv = DM_ENG_DataModelConnect(acache_file, acl_file, &systemctx, &acsctx);
    // connect to bus
    rv += DM_ENG_ActivateNotification(DM_ENG_EntityType_SYSTEM, inform, transferComplete, requestDownload,
                                      getRPCMethods, timerStart, timerStop, timerTimeRemaining, engineEvent);
    assert_int_equal(rv, 0);
    system_bus_ctx = (amxb_bus_ctx_t*) systemctx;
    acs_bus_ctx = (amxb_bus_ctx_t*) acsctx;
    assert_non_null(system_bus_ctx);
    assert_non_null(acs_bus_ctx);
    assert_int_equal(amxb_register(system_bus_ctx, &dm), 0);
    assert_int_equal(amxb_register(acs_bus_ctx, &dm), 0);
}


void test_dmadapter_ManagementServer_GetParameterValue(UNUSED void** state) {
    char* username = NULL;
    char* enable_cwmp = NULL;
    char* conn_request_host = NULL;
    char* periodic_inform_interval = NULL;

    assert_int_equal(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_USERNAME, &username), 0);
    assert_int_equal(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_ENABLECWMP, &enable_cwmp), 0);
    assert_int_equal(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_CONNECTIONREQUESTHOST, &conn_request_host), 0);
    assert_int_equal(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_PERIODICINFORMINTERVAL, &periodic_inform_interval), 0);

    assert_string_equal(username, "cdrouter");
    assert_string_equal(enable_cwmp, "true");
    assert_string_equal(conn_request_host, "0.0.0.0");
    assert_int_equal(atoi(periodic_inform_interval), 10000);

    free(username);
    free(enable_cwmp);
    free(conn_request_host);
    free(periodic_inform_interval);
}

void test_dmadapter_ManagementServer_SetParameterValue(UNUSED void** state) {
    char* username = NULL;
    char* reboot_by_acs = NULL;
    char* periodic_inform_interval = NULL;

    assert_int_equal(DM_ENG_SetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_REBOOTBYACS, "1"), 0);
    assert_int_equal(DM_ENG_SetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_USERNAME, "testUsername"), 0);
    assert_int_equal(DM_ENG_SetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_PERIODICINFORMINTERVAL, "9999"), 0);
    assert_int_equal(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_REBOOTBYACS, &reboot_by_acs), 0);
    assert_int_equal(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_USERNAME, &username), 0);
    assert_int_equal(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_PERIODICINFORMINTERVAL, &periodic_inform_interval), 0);
    assert_string_equal(username, "testUsername");
    assert_string_equal(reboot_by_acs, "true");
    assert_int_equal(atoi(periodic_inform_interval), 9999);

    free(username);
    free(reboot_by_acs);
    free(periodic_inform_interval);
}

void test_dmadapter_GetParameterNames_Parameter(UNUSED void** state) {
    // Setup test Data
    add_host_instance(HOST_1_MAC, HOST_1_IP);
    add_host_instance(HOST_2_MAC, HOST_2_IP);
    // writable parameter
    char* rw_string = "Device.GetParameterNames_Parameter.writablestring";
    // read-only parameter
    char* r_only_string = "Device.GetParameterNames_Parameter.readonlystring";
    // root parameter
    char* root_parameter = "Device.RootDataModelVersion";
    int rv = 0;
    int num_param = 0;
    bool next_level = true;
    DM_ENG_ParameterInfoStruct** param_info_st = NULL;

    rv = DM_ENG_GetParameterNames(DM_ENG_EntityType_ACS, (char*) rw_string, next_level, &param_info_st);
    assert_int_equal(rv, DM_ENG_INVALID_ARGUMENTS);

    if(param_info_st) {
        DM_ENG_deleteAllParameterInfoStruct(param_info_st);
        free(param_info_st);
    }
    param_info_st = NULL;

    next_level = false;
    rv = DM_ENG_GetParameterNames(DM_ENG_EntityType_ACS, (char*) rw_string, next_level, &param_info_st);
    assert_int_equal(rv, 0);
    num_param = DM_ENG_tablen((void**) param_info_st);
    assert_int_equal(num_param, 1);
    assert_string_equal(param_info_st[0]->parameterName, rw_string);
    assert_int_equal((int) param_info_st[0]->writable, 1);

    if(param_info_st) {
        DM_ENG_deleteAllParameterInfoStruct(param_info_st);
        free(param_info_st);
    }
    param_info_st = NULL;

    rv = DM_ENG_GetParameterNames(DM_ENG_EntityType_ACS, (char*) r_only_string, next_level, &param_info_st);
    assert_int_equal(rv, 0);
    num_param = DM_ENG_tablen((void**) param_info_st);
    assert_int_equal(num_param, 1);
    assert_string_equal(param_info_st[0]->parameterName, r_only_string);
    assert_int_equal((int) param_info_st[0]->writable, 0);

    if(param_info_st) {
        DM_ENG_deleteAllParameterInfoStruct(param_info_st);
        free(param_info_st);
    }
    param_info_st = NULL;

    rv = DM_ENG_GetParameterNames(DM_ENG_EntityType_ACS, root_parameter, next_level, &param_info_st);
    assert_int_equal(rv, 0);
    num_param = DM_ENG_tablen((void**) param_info_st);
    assert_int_equal(num_param, 1);
    assert_string_equal(param_info_st[0]->parameterName, root_parameter);
    assert_int_equal((int) param_info_st[0]->writable, 0);

    if(param_info_st) {
        DM_ENG_deleteAllParameterInfoStruct(param_info_st);
        free(param_info_st);
    }
    param_info_st = NULL;
}

void test_dmadapter_GetParameterNames_EmptyPath_NextLevel_True(UNUSED void** state) {
    char* empty = "";
    int num_param = 0;
    int rv = 0;
    bool next_level = true;
    DM_ENG_ParameterInfoStruct** param_info_st = NULL;

    rv = DM_ENG_GetParameterNames(DM_ENG_EntityType_ACS, empty, next_level, &param_info_st);
    assert_int_equal(rv, 0);
    num_param = DM_ENG_tablen((void**) param_info_st);
    assert_int_equal(num_param, 1);
    assert_string_equal(param_info_st[0]->parameterName, "Device.");
    assert_int_equal(param_info_st[0]->writable, 0);

    if(param_info_st) {
        DM_ENG_deleteAllParameterInfoStruct(param_info_st);
        free(param_info_st);
    }
    param_info_st = NULL;
}

void test_dmadapter_GetParameterNames_Object_NextLevel_True(UNUSED void** state) {

    char* objtest_path = "Device.GetParameterNames_Object_NextLevel_True.";
    char* objtest_param1_path = "Device.GetParameterNames_Object_NextLevel_True.writablestring";
    char* object_param2_path = "Device.GetParameterNames_Object_NextLevel_True.readonlystring";
    char* objtest_child1_path = "Device.GetParameterNames_Object_NextLevel_True.GetParameterNames_Object1_NextLevel_True.";
    char* object_child2_path = "Device.GetParameterNames_Object_NextLevel_True.GetParameterNames_Object2_NextLevel_True.";
    int rv = 0;
    int num_param = 0;

    DM_ENG_ParameterInfoStruct** param_info_st = NULL;
    rv = DM_ENG_GetParameterNames(DM_ENG_EntityType_ACS, (char*) objtest_path, true, &param_info_st);
    assert_int_equal(rv, 0);
    num_param = DM_ENG_tablen((void**) param_info_st);
    assert_int_equal(num_param, 4);

    /*
     * Expected Result:
     * [Device.GetParameterNames_Object_NextLevel_True.readonlystring] , Writable= 0
     * [Device.GetParameterNames_Object_NextLevel_True.writablestring] , Writable= 1
     * [Device.GetParameterNames_Object_NextLevel_True.GetParameterNames_Object1_NextLevel_True.] , Writable= 0
     * [Device.GetParameterNames_Object_NextLevel_True.GetParameterNames_Object2_NextLevel_True.] , Writable= 0
     */
#if PRINT_RESULT
    for(int i = 0; i < num_param; i++) {
        printf("-> ParameterName [%s] , Writable= %d\n", param_info_st[i]->parameterName, param_info_st[i]->writable);
    }
#endif

    assert_string_equal(param_info_st[0]->parameterName, object_param2_path);
    assert_int_equal(param_info_st[0]->writable, 0);
    assert_string_equal(param_info_st[1]->parameterName, objtest_param1_path);
    assert_int_equal(param_info_st[1]->writable, 1);
    assert_string_equal(param_info_st[2]->parameterName, objtest_child1_path);
    assert_int_equal(param_info_st[2]->writable, 0);
    assert_string_equal(param_info_st[3]->parameterName, object_child2_path);
    assert_int_equal(param_info_st[3]->writable, 0);

    if(param_info_st) {
        DM_ENG_deleteAllParameterInfoStruct(param_info_st);
        free(param_info_st);
    }
    param_info_st = NULL;
}

void test_dmadapter_GetParameterNames_Object_NextLevel_False(UNUSED void** state) {

    char* objpath = "Device.GetParameterNames_Object_NextLevel_False.";
    char* param1_path = "Device.GetParameterNames_Object_NextLevel_False.writableInteger";
    char* param2_path = "Device.GetParameterNames_Object_NextLevel_False.writablestring";
    char* param3_path = "Device.GetParameterNames_Object_NextLevel_False.readonlystring";
    char* child1_path = "Device.GetParameterNames_Object_NextLevel_False.GetParameterNames_Object1.";
    char* child2_path = "Device.GetParameterNames_Object_NextLevel_False.GetParameterNames_Object2.";
    char* child1_param_path = "Device.GetParameterNames_Object_NextLevel_False.GetParameterNames_Object1.test1";
    char* child2_param_path = "Device.GetParameterNames_Object_NextLevel_False.GetParameterNames_Object2.test1";
    int rv = 0;
    int num_param = 0;

    DM_ENG_ParameterInfoStruct** param_info_st = NULL;
    rv = DM_ENG_GetParameterNames(DM_ENG_EntityType_ACS, (char*) objpath, false, &param_info_st);
    assert_int_equal(rv, 0);
    /*
     * Expected Result:
     * [Device.GetParameterNames_Object_NextLevel_False.] , Writable= 0
     * [Device.GetParameterNames_Object_NextLevel_False.writableInteger] , Writable= 1
     * [Device.GetParameterNames_Object_NextLevel_False.readonlystring] , Writable= 0
     * [Device.GetParameterNames_Object_NextLevel_False.writablestring] , Writable= 1
     * [Device.GetParameterNames_Object_NextLevel_False.GetParameterNames_Object1.] , Writable= 0
     * [Device.GetParameterNames_Object_NextLevel_False.GetParameterNames_Object1.test1] , Writable= 1
     * [Device.GetParameterNames_Object_NextLevel_False.GetParameterNames_Object2.] , Writable= 0
     * [Device.GetParameterNames_Object_NextLevel_False.GetParameterNames_Object2.test1] , Writable= 1
     */

    num_param = DM_ENG_tablen((void**) param_info_st);
    assert_int_equal(num_param, 8);

#if PRINT_RESULT
    for(int i = 0; i < num_param; i++) {
        printf("-> ParameterName [%s] , Writable= %d\n", param_info_st[i]->parameterName, param_info_st[i]->writable);
    }
#endif

    assert_string_equal(param_info_st[0]->parameterName, objpath);
    assert_int_equal(param_info_st[0]->writable, 0);
    assert_string_equal(param_info_st[1]->parameterName, param1_path);
    assert_int_equal(param_info_st[1]->writable, 1);
    assert_string_equal(param_info_st[2]->parameterName, param3_path);
    assert_int_equal(param_info_st[2]->writable, 0);
    assert_string_equal(param_info_st[3]->parameterName, param2_path);
    assert_int_equal(param_info_st[3]->writable, 1);
    assert_string_equal(param_info_st[4]->parameterName, child1_path);
    assert_int_equal(param_info_st[4]->writable, 0);
    assert_string_equal(param_info_st[5]->parameterName, child1_param_path);
    assert_int_equal(param_info_st[5]->writable, 1);
    assert_string_equal(param_info_st[6]->parameterName, child2_path);
    assert_int_equal(param_info_st[6]->writable, 0);
    assert_string_equal(param_info_st[7]->parameterName, child2_param_path);
    assert_int_equal(param_info_st[7]->writable, 1);

    if(param_info_st) {
        DM_ENG_deleteAllParameterInfoStruct(param_info_st);
        free(param_info_st);
    }
    param_info_st = NULL;
}

void test_dmadapter_GetParameterNames_Object_NextLevel_False_searchPath(UNUSED void** state) {

    // both notation '.*.' or '.*' at the end works the same way
    char* objtest_path = "Device.Hosts.Host.*.";
    int rv = 0;
    int num_param = 0;
    DM_ENG_ParameterInfoStruct** param_info_st = NULL;

    rv = DM_ENG_GetParameterNames(DM_ENG_EntityType_ACS, (char*) objtest_path, false, &param_info_st);
    assert_int_equal(rv, 0);
    /*
     * Expected Result:
     * Parameter/Object [Device.Hosts.Host.1.] , Writable= 1
     * Parameter/Object [Device.Hosts.Host.1.IPAddress] , Writable= 1
     * Parameter/Object [Device.Hosts.Host.1.MACAddress] , Writable= 1
     * Parameter/Object [Device.Hosts.Host.2.] , Writable= 1
     * Parameter/Object [Device.Hosts.Host.2.IPAddress] , Writable= 1
     * Parameter/Object [Device.Hosts.Host.2.MACAddress] , Writable= 1
     */
    num_param = DM_ENG_tablen((void**) param_info_st);
    assert_int_equal(num_param, 6);// result should include objects

    assert_int_equal(check_parameter_name(param_info_st, "Device.Hosts.Host.1.", true), 0);
    assert_int_equal(check_parameter_name(param_info_st, "Device.Hosts.Host.1.MACAddress", true), 0);
    assert_int_equal(check_parameter_name(param_info_st, "Device.Hosts.Host.1.IPAddress", true), 0);
    assert_int_equal(check_parameter_name(param_info_st, "Device.Hosts.Host.2.", true), 0);
    assert_int_equal(check_parameter_name(param_info_st, "Device.Hosts.Host.2.MACAddress", true), 0);
    assert_int_equal(check_parameter_name(param_info_st, "Device.Hosts.Host.2.IPAddress", true), 0);

#if PRINT_RESULT
    for(int i = 0; i < num_param; i++) {
        printf("-> Parameter/Object [%s] , Writable= %d\n", param_info_st[i]->parameterName, param_info_st[i]->writable);
    }
#endif
    if(param_info_st) {
        DM_ENG_deleteAllParameterInfoStruct(param_info_st);
        free(param_info_st);
    }
    param_info_st = NULL;
}

void test_dmadapter_GetParameterNames_Object_NextLevel_True_searchPath(UNUSED void** state) {
    // both notation '.*.' or '.*' at the end works the same way
    char* objtest_path = "Device.Hosts.Host.*.";
    int rv = 0;
    int num_param = 0;
    DM_ENG_ParameterInfoStruct** param_info_st = NULL;

    rv = DM_ENG_GetParameterNames(DM_ENG_EntityType_ACS, (char*) objtest_path, true, &param_info_st);
    assert_int_equal(rv, 0);
    /*
     * Expected Result:
     * Parameter/Object [Device.Hosts.Host.1.MACAddress] , Writable= 1
     * Parameter/Object [Device.Hosts.Host.1.IPAddress] , Writable= 1
     * Parameter/Object [Device.Hosts.Host.2.MACAddress] , Writable= 1
     * Parameter/Object [Device.Hosts.Host.2.IPAddress] , Writable= 1
     */
    num_param = DM_ENG_tablen((void**) param_info_st);
    assert_int_equal(num_param, 4);
    assert_int_equal(check_parameter_name(param_info_st, "Device.Hosts.Host.1.MACAddress", true), 0);
    assert_int_equal(check_parameter_name(param_info_st, "Device.Hosts.Host.1.IPAddress", true), 0);
    assert_int_equal(check_parameter_name(param_info_st, "Device.Hosts.Host.2.MACAddress", true), 0);
    assert_int_equal(check_parameter_name(param_info_st, "Device.Hosts.Host.2.IPAddress", true), 0);
#if PRINT_RESULT
    for(int i = 0; i < num_param; i++) {
        printf("-> Parameter/Object [%s] , Writable= %d\n", param_info_st[i]->parameterName, param_info_st[i]->writable);
    }
#endif
    if(param_info_st) {
        DM_ENG_deleteAllParameterInfoStruct(param_info_st);
        free(param_info_st);
    }
    param_info_st = NULL;
}

void test_dmadapter_GetParameterNames_Parameter_searchPath(UNUSED void** state) {
    char* objtest_path = "Device.Hosts.Host.*.IPAddress";
    int rv = 0;
    int num_param = 0;
    DM_ENG_ParameterInfoStruct** param_info_st = NULL;

    rv = DM_ENG_GetParameterNames(DM_ENG_EntityType_ACS, (char*) objtest_path, false, &param_info_st);
    assert_int_equal(rv, 0);
    /*
     * Expected Result:
     * Parameter/Object [Device.Hosts.Host.1.IPAddress] , Writable= 1
     * Parameter/Object [Device.Hosts.Host.2.IPAddress] , Writable= 1
     */
    num_param = DM_ENG_tablen((void**) param_info_st);
    assert_int_equal(num_param, 2);
    assert_int_equal(check_parameter_name(param_info_st, "Device.Hosts.Host.1.IPAddress", true), 0);
    assert_int_equal(check_parameter_name(param_info_st, "Device.Hosts.Host.2.IPAddress", true), 0);

#if PRINT_RESULT
    for(int i = 0; i < num_param; i++) {
        printf("-> Parameter/Object [%s] , Writable= %d\n", param_info_st[i]->parameterName, param_info_st[i]->writable);
    }
#endif
    if(param_info_st) {
        DM_ENG_deleteAllParameterInfoStruct(param_info_st);
        free(param_info_st);
    }
    param_info_st = NULL;
}

void test_dmadapter_GetParameterNames_DeviceObject_NextLevel_True(UNUSED void** state) {
    char* objpath = "Device.";
    int rv = 0;
    int num_param = 0;
    DM_ENG_ParameterInfoStruct** param_info_st = NULL;

    rv = DM_ENG_GetParameterNames(DM_ENG_EntityType_ACS, objpath, true, &param_info_st);
    assert_int_equal(rv, 0);
    num_param = DM_ENG_tablen((void**) param_info_st);
    assert_int_equal(num_param, 11);// 2 objects , 3 parameter

    assert_int_equal(check_parameter_name(param_info_st, "Device.RootDataModelVersion", 0), 0);
    assert_int_equal(check_parameter_name(param_info_st, "Device.InterfaceStackNumberOfEntries", 0), 0);
    assert_int_equal(check_parameter_name(param_info_st, "Device.ManagementServer.", 0), 0);
    assert_int_equal(check_parameter_name(param_info_st, "Device.DeviceInfo.", 0), 0);
    assert_int_not_equal(check_parameter_name(param_info_st, "Device.NoAccessRights.", 0), 0);
#if PRINT_RESULT
    for(int i = 0; i < num_param; i++) {
        printf("-> Parameter/Object [%s] , Writable= %d\n", param_info_st[i]->parameterName, param_info_st[i]->writable);
    }
#endif
    if(param_info_st) {
        DM_ENG_deleteAllParameterInfoStruct(param_info_st);
        free(param_info_st);
    }
    param_info_st = NULL;
}

void test_dmadapter_GetParameterNames_EmptyPath_NextLevel_false(UNUSED void** state) {
    char* objpath = ""; // or Device. same result
    int rv = 0;
    int num_param = 0;
    DM_ENG_ParameterInfoStruct** param_info_st = NULL;

    rv = DM_ENG_GetParameterNames(DM_ENG_EntityType_ACS, (char*) objpath, false, &param_info_st);
    assert_int_equal(rv, 0);
    // result should at least include Device. at the very beggining
    assert_string_equal(param_info_st[0]->parameterName, "Device.");
    assert_int_equal(param_info_st[0]->writable, 0);
    num_param = DM_ENG_tablen((void**) param_info_st);
    assert_int_not_equal(num_param, 0);
    assert_int_not_equal(num_param, 1);

    assert_int_equal(check_parameter_name(param_info_st, "Device.RootDataModelVersion", 0), 0);
    assert_int_equal(check_parameter_name(param_info_st, "Device.InterfaceStackNumberOfEntries", 0), 0);
    assert_int_equal(check_parameter_name(param_info_st, "Device.ManagementServer.", 0), 0);
    assert_int_equal(check_parameter_name(param_info_st, "Device.DeviceInfo.", 0), 0);
    // here its hard to check all the data model
    // just print them
#if PRINT_RESULT
    for(int i = 0; i < num_param; i++) {
        printf("-> Parameter/Object [%s] , Writable= %d\n", param_info_st[i]->parameterName, param_info_st[i]->writable);
    }
#endif
    if(param_info_st) {
        DM_ENG_deleteAllParameterInfoStruct(param_info_st);
        free(param_info_st);
    }
    param_info_st = NULL;
}

void test_dmadapter_GetParameterNames_DeviceObject_NextLevel_False(UNUSED void** state) {
    char* objpath = "Device."; // or Device. same result
    int rv = 0;
    int num_param = 0;
    DM_ENG_ParameterInfoStruct** param_info_st = NULL;

    rv = DM_ENG_GetParameterNames(DM_ENG_EntityType_ACS, (char*) objpath, false, &param_info_st);
    assert_int_equal(rv, 0);
    // result should at least include Device. at the very beggining
    assert_string_equal(param_info_st[0]->parameterName, "Device.");
    assert_int_equal(param_info_st[0]->writable, 0);
    num_param = DM_ENG_tablen((void**) param_info_st);
    assert_int_not_equal(num_param, 0);
    assert_int_not_equal(num_param, 1);

    assert_int_equal(check_parameter_name(param_info_st, "Device.RootDataModelVersion", 0), 0);
    assert_int_equal(check_parameter_name(param_info_st, "Device.InterfaceStackNumberOfEntries", 0), 0);
    assert_int_equal(check_parameter_name(param_info_st, "Device.ManagementServer.", 0), 0);
    assert_int_equal(check_parameter_name(param_info_st, "Device.DeviceInfo.", 0), 0);
    // here its hard to check all the data model
    // just print them
#if PRINT_RESULT
    for(int i = 0; i < num_param; i++) {
        printf("-> Parameter/Object [%s] , Writable= %d\n", param_info_st[i]->parameterName, param_info_st[i]->writable);
    }
#endif
    if(param_info_st) {
        DM_ENG_deleteAllParameterInfoStruct(param_info_st);
        free(param_info_st);
    }
    param_info_st = NULL;
}

void test_dmadapter_GetParametersValues_Parameters(UNUSED void** state) {
    int num_param = 4;
    char* paramPath1 = "Device.DeviceInfo.SerialNumber";
    char* paramPath2 = "Device.DeviceInfo.DeviceStatus";
    char* paramPath3 = "Device.DeviceInfo.testuint";
    DM_ENG_ParameterValueStruct** params_values_st = NULL;
    char* paramsArray[num_param + 1];
    paramsArray[0] = paramPath1;
    paramsArray[1] = paramPath2;
    paramsArray[2] = paramPath3;
    paramsArray[3] = NULL;

    int rv = DM_ENG_GetParameterValues(DM_ENG_EntityType_ACS, (char**) paramsArray, &params_values_st);
    assert_int_equal(rv, 0);
    /*
     * Expected Result:
     * [Device.DeviceInfo.SerialNumber] : type= 5 , val= 000000123
     * [Device.DeviceInfo.DeviceStatus] : type= 5 , val= Up
     * [Device.DeviceInfo.testuint] : type= 0 , val= 10
     */
    int nb_param = DM_ENG_tablen((void**) params_values_st);
    assert_int_equal(nb_param, 3);

    assert_string_equal(params_values_st[0]->parameterName, paramPath1);
    assert_int_equal(params_values_st[0]->type, DM_ENG_ParameterType_STRING);
    assert_string_equal(params_values_st[0]->value, "000000123");

    assert_string_equal(params_values_st[1]->parameterName, paramPath2);
    assert_int_equal(params_values_st[1]->type, DM_ENG_ParameterType_STRING);
    assert_string_equal(params_values_st[1]->value, "Up");

    assert_string_equal(params_values_st[2]->parameterName, paramPath3);
    assert_int_equal(params_values_st[2]->type, DM_ENG_ParameterType_UINT);
    assert_string_equal(params_values_st[2]->value, "10");

    if(rv == 0) {
    #if PRINT_RESULT
        printf("-----------------------------test_amx_GetParametersValues_Parameters---------------------------------\n");
        for(int i = 0; i < nb_param; i++) {
            printf("--> Parameter [%s] : type= %d , val= %s \n", (char*) params_values_st[i]->parameterName,
                   params_values_st[i]->type, (char*) params_values_st[i]->value);
        }
        printf("-----------------------------test_amx_GetParametersValues_Parameters---------------------------------\n");
    #endif
        if(params_values_st) {
            DM_ENG_deleteAllParameterValueStruct(params_values_st);
            free(params_values_st);
        }
    }
}

void test_dmadapter_GetParametersValues_Parameter_noAccessRights(UNUSED void** state) {
    int num_param = 4;
    char* paramPath1 = "Device.NoAccessRights.Status";

    DM_ENG_ParameterValueStruct** params_values_st = NULL;
    char* paramsArray[num_param + 1];
    paramsArray[0] = paramPath1;
    paramsArray[1] = NULL;

    int rv = DM_ENG_GetParameterValues(DM_ENG_EntityType_ACS, (char**) paramsArray, &params_values_st);
    assert_int_not_equal(rv, 0);

    int nb_param = DM_ENG_tablen((void**) params_values_st);
    assert_int_equal(nb_param, 0);



    if(rv == 0) {
    #if PRINT_RESULT
        printf("-----------------------------test_amx_GetParametersValues_Parameters---------------------------------\n");
        for(int i = 0; i < nb_param; i++) {
            printf("--> Parameter [%s] : type= %d , val= %s \n", (char*) params_values_st[i]->parameterName,
                   params_values_st[i]->type, (char*) params_values_st[i]->value);
        }
        printf("-----------------------------test_amx_GetParametersValues_Parameters---------------------------------\n");
    #endif
        if(params_values_st) {
            DM_ENG_deleteAllParameterValueStruct(params_values_st);
            free(params_values_st);
        }
    }
}

void test_dmadapter_GetParametersValues_RootParameters(UNUSED void** state) {
    int num_param = 2;
    char* paramPath1 = "Device.RootDataModelVersion";
    char* paramPath2 = "Device.InterfaceStackNumberOfEntries";
    DM_ENG_ParameterValueStruct** params_values_st = NULL;
    char* paramsArray[num_param + 1];
    paramsArray[0] = paramPath1;
    paramsArray[1] = paramPath2;
    paramsArray[2] = NULL;
    int rv = DM_ENG_GetParameterValues(DM_ENG_EntityType_ACS, (char**) paramsArray, &params_values_st);
    assert_int_equal(rv, 0);

    int nb_param = DM_ENG_tablen((void**) params_values_st);
    assert_int_equal(nb_param, 2);

    assert_string_equal(params_values_st[0]->parameterName, paramPath1);
    assert_int_equal(params_values_st[0]->type, DM_ENG_ParameterType_STRING);// 5 -> string
    assert_string_equal(params_values_st[0]->value, "2.14");

    assert_string_equal(params_values_st[1]->parameterName, paramPath2);
    assert_int_equal(params_values_st[1]->type, DM_ENG_ParameterType_UINT);// 0 -> uint
    assert_string_equal(params_values_st[1]->value, "0");

    if(rv == 0) {
    #if PRINT_RESULT
        printf("-----------------------------test_amx_GetParametersValues_Parameters---------------------------------\n");
        for(int i = 0; i < nb_param; i++) {
            printf("--> Parameter [%s] : type= %d , val= %s \n", (char*) params_values_st[i]->parameterName,
                   params_values_st[i]->type, (char*) params_values_st[i]->value);
        }
        printf("-----------------------------test_amx_GetParametersValues_Parameters---------------------------------\n");
    #endif
        if(params_values_st) {
            DM_ENG_deleteAllParameterValueStruct(params_values_st);
            free(params_values_st);
        }
    }
}

void test_dmadapter_GetParametersValues_Object(UNUSED void** state) {
    int num_param = 1;
    char* objPath = "Device.DeviceInfo.";
    DM_ENG_ParameterValueStruct** params_values_st = NULL;
    char* paramsArray[num_param + 1];
    paramsArray[0] = objPath;
    paramsArray[1] = NULL;

    int rv = DM_ENG_GetParameterValues(DM_ENG_EntityType_ACS, (char**) paramsArray, &params_values_st);
    assert_int_equal(rv, 0);
    /*
     * Expected Result:
     * [Device.DeviceInfo.SerialNumber] : type= 5 , val= 000000123
     * [Device.DeviceInfo.DeviceStatus] : type= 5 , val= Up
     * [Device.DeviceInfo.testInt] : type= 0 , val= 10
     */
    int nb_param = DM_ENG_tablen((void**) params_values_st);
    assert_int_equal(nb_param, 7);

    assert_int_equal(check_parameter_value(params_values_st, "Device.DeviceInfo.SerialNumber", "000000123", DM_ENG_ParameterType_STRING), 0);
    assert_int_equal(check_parameter_value(params_values_st, "Device.DeviceInfo.DeviceStatus", "Up", DM_ENG_ParameterType_STRING), 0);
    assert_int_equal(check_parameter_value(params_values_st, "Device.DeviceInfo.HardwareVersion", "1.1", DM_ENG_ParameterType_STRING), 0);
    assert_int_equal(check_parameter_value(params_values_st, "Device.DeviceInfo.SoftwareVersion", "2.7", DM_ENG_ParameterType_STRING), 0);
    assert_int_equal(check_parameter_value(params_values_st, "Device.DeviceInfo.testuint", "10", DM_ENG_ParameterType_UINT), 0);
    assert_int_equal(check_parameter_value(params_values_st, "Device.DeviceInfo.testint", "10", DM_ENG_ParameterType_INT), 0);
    assert_int_equal(check_parameter_value(params_values_st, "Device.DeviceInfo.testbool", "true", DM_ENG_ParameterType_BOOLEAN), 0);

    if(rv == 0) {
    #if PRINT_RESULT
        printf("------------------------------test_amx_GetParametersValues_Object--------------------------------\n");
        for(int i = 0; i < nb_param; i++) {
            printf("--> Parameter [%s] : type= %d , val= %s \n", (char*) params_values_st[i]->parameterName,
                   params_values_st[i]->type, (char*) params_values_st[i]->value);
        }
        printf("-------------------------------test_amx_GetParametersValues_Object-------------------------------\n");
    #endif

        if(params_values_st) {
            DM_ENG_deleteAllParameterValueStruct(params_values_st);
            free(params_values_st);
        }
    }
}

void test_dmadapter_GetParametersValues_searchPath_Parameter(UNUSED void** state) {
    int num_param = 1;
    char* object_path = "Device.Hosts.Host.*.IPAddress";
    DM_ENG_ParameterValueStruct** params_values_st = NULL;
    char* paramsArray[num_param + 1];
    paramsArray[0] = object_path;
    paramsArray[1] = NULL;

    int rv = DM_ENG_GetParameterValues(DM_ENG_EntityType_ACS, (char**) paramsArray, &params_values_st);
    assert_int_equal(rv, 0);
    int nb_param = DM_ENG_tablen((void**) params_values_st);
    assert_int_equal(nb_param, 2);

    /*
     * Expected Result:
     * [Device.Hosts.Host.1.IPAddress] : type= 5 , value = 192.168.0.220
     * [Device.Hosts.Host.2.IPAddress] : type= 5 , value = 192.168.0.221
     */
    assert_string_equal(params_values_st[0]->parameterName, "Device.Hosts.Host.1.IPAddress");
    assert_int_equal(params_values_st[0]->type, DM_ENG_ParameterType_STRING);// 5 -> string
    assert_string_equal(params_values_st[0]->value, HOST_1_IP);

    assert_string_equal(params_values_st[1]->parameterName, "Device.Hosts.Host.2.IPAddress");
    assert_int_equal(params_values_st[1]->type, DM_ENG_ParameterType_STRING);// 0 -> uint
    assert_string_equal(params_values_st[1]->value, HOST_2_IP);

    #if PRINT_RESULT
    if(rv == 0) {
        printf("------------------------------test_amx_GetParametersValues_searchPath--------------------------------\n");
        for(int i = 0; i < nb_param; i++) {
            printf("--> Parameter [%s] : type= %d , val= %s \n", (char*) params_values_st[i]->parameterName,
                   params_values_st[i]->type, (char*) params_values_st[i]->value);
        }
        printf("-------------------------------test_amx_GetParametersValues_searchPath-------------------------------\n");
    }
    #endif

    if(rv == 0) {
        if(params_values_st) {
            DM_ENG_deleteAllParameterValueStruct(params_values_st);
            free(params_values_st);
        }
    }
}

void test_dmadapter_GetParametersValues_searchPath_Object(UNUSED void** state) {
    int num_param = 1;
    char* object_path = "Device.Hosts.Host.*.";
    DM_ENG_ParameterValueStruct** params_values_st = NULL;
    char* paramsArray[num_param + 1];
    paramsArray[0] = object_path;
    paramsArray[1] = NULL;

    int rv = DM_ENG_GetParameterValues(DM_ENG_EntityType_ACS, (char**) paramsArray, &params_values_st);
    assert_int_equal(rv, 0);
    int nb_param = DM_ENG_tablen((void**) params_values_st);
    assert_int_equal(nb_param, 4);

    /*Expected Result:
     * [Device.Hosts.Host.1.IPAddress] : type= 5 , value = 192.168.0.220
     * [Device.Hosts.Host.1.MACAddress] : type= 5 , value = DD:DD:DD:FF:EE:DD
     * [Device.Hosts.Host.2.IPAddress] : type= 5 , value = 192.168.0.221
     * [Device.Hosts.Host.2.MACAddress] : type= 5 , value = DD:DD:DD:FF:FF:FF
     */
    assert_string_equal(params_values_st[0]->parameterName, "Device.Hosts.Host.1.IPAddress");
    assert_int_equal(params_values_st[0]->type, DM_ENG_ParameterType_STRING);// 5 -> string
    assert_string_equal(params_values_st[0]->value, HOST_1_IP);

    assert_string_equal(params_values_st[1]->parameterName, "Device.Hosts.Host.1.MACAddress");
    assert_int_equal(params_values_st[1]->type, DM_ENG_ParameterType_STRING);// 5 -> string
    assert_string_equal(params_values_st[1]->value, HOST_1_MAC);

    assert_string_equal(params_values_st[2]->parameterName, "Device.Hosts.Host.2.IPAddress");
    assert_int_equal(params_values_st[2]->type, DM_ENG_ParameterType_STRING);// 0 -> uint
    assert_string_equal(params_values_st[2]->value, HOST_2_IP);

    assert_string_equal(params_values_st[3]->parameterName, "Device.Hosts.Host.2.MACAddress");
    assert_int_equal(params_values_st[3]->type, DM_ENG_ParameterType_STRING);// 5 -> string
    assert_string_equal(params_values_st[3]->value, HOST_2_MAC);

    #if PRINT_RESULT
    if(rv == 0) {
        printf("------------------------------test_amx_GetParametersValues_searchPath--------------------------------\n");
        for(int i = 0; i < nb_param; i++) {
            printf("--> Parameter [%s] : type= %d , val= %s \n", (char*) params_values_st[i]->parameterName,
                   params_values_st[i]->type, (char*) params_values_st[i]->value);
        }
        printf("-------------------------------test_amx_GetParametersValues_searchPath-------------------------------\n");
    }
    #endif
    if(rv == 0) {
        if(params_values_st) {
            DM_ENG_deleteAllParameterValueStruct(params_values_st);
            free(params_values_st);
        }
    }
}

void test_dmadapter_SetParameterValues(UNUSED void** state) {
    char* writableInteger = "Device.SetParameterValues_Test_Object.writableInteger";
    char* writableString = "Device.SetParameterValues_Test_Object.writablestring";
    int NPARAMS = 2;
    DM_ENG_SetParameterValuesFault** pFaults = NULL;
    DM_ENG_ParameterStatus status;
    DM_ENG_ParameterValueStruct** pParameterList = NULL;

    pParameterList = DM_ENG_newTabParameterValueStruct(NPARAMS + 1);

    pParameterList[0] = DM_ENG_newParameterValueStruct(writableInteger,
                                                       DM_ENG_ParameterType_INT,
                                                       "1000");

    pParameterList[1] = DM_ENG_newParameterValueStruct(writableString,
                                                       DM_ENG_ParameterType_STRING,
                                                       "test string");
    pParameterList[2] = NULL;




    int rv = DM_ENG_SetParameterValues(DM_ENG_EntityType_ACS,
                                       pParameterList,
                                       "",
                                       &status,
                                       &pFaults);
    assert_int_equal(rv, 0);
    assert_int_equal(status, 0);

    #if PRINT_RESULT
    printf("------------------------------test_amx_SetParameterValues--------------------------------\n");
    if(rv == 0) {
        printf("SetParameterValues OK\n");
    } else {
        printf("SetParameterValues NOK: ret     = %d\n", rv);
        printf("                        status  = %d\n", status);
        int len = DM_ENG_tablen((void**) pFaults);
        int j;
        for(j = 0; j < len; j++) {
            printf(" ERROR param=%s, ERROR code=%d\n", pFaults[j]->parameterName, pFaults[j]->faultCode);
        }
    }
    printf("--------------------------------test_amx_SetParameterValues------------------------------\n");
    #endif
    if(pFaults) {
        DM_ENG_deleteAllSetParameterValuesFault(pFaults);
        free(pFaults);
    }

    if(pParameterList) {
        DM_ENG_deleteTabParameterValueStruct(pParameterList);
    }
}

void test_dmadapter_SetParameterValues_Faults(UNUSED void** state) {
    // TODO! ADD more fault code
    char* readOnlyString = "Device.SetParameterValues_Test_Object.readonlystring";
    int NPARAMS = 1;
    DM_ENG_SetParameterValuesFault** pFaults = NULL;
    DM_ENG_ParameterStatus status;
    DM_ENG_ParameterValueStruct** pParameterList = NULL;

    pParameterList = DM_ENG_newTabParameterValueStruct(NPARAMS + 1);

    pParameterList[0] = DM_ENG_newParameterValueStruct(readOnlyString,
                                                       DM_ENG_ParameterType_STRING,
                                                       "error");
    pParameterList[1] = NULL;

    int rv = DM_ENG_SetParameterValues(DM_ENG_EntityType_ACS,
                                       pParameterList,
                                       "",
                                       &status,
                                       &pFaults);
    assert_int_equal(rv, DM_ENG_INVALID_ARGUMENTS);
    assert_int_equal(status, -1);

    int nb_fault = DM_ENG_tablen((void**) pFaults);
    assert_int_equal(nb_fault, 1);
    assert_string_equal(pFaults[0]->parameterName, "Device.SetParameterValues_Test_Object.readonlystring");
    // did we get the right fault code
    assert_int_equal(pFaults[0]->faultCode, DM_ENG_READ_ONLY_PARAMETER);

    #if PRINT_RESULT
    printf("------------------------------test_amx_SetParameterValues--------------------------------\n");
    if(rv == 0) {
        printf("SetParameterValues OK\n");
    } else {
        printf("SetParameterValues NOK: ret     = %d\n", rv);
        printf("                        status  = %d\n", status);

        int j;
        for(j = 0; j < nb_fault; j++) {
            printf(" ERROR param=%s, ERROR code=%d\n", pFaults[j]->parameterName, pFaults[j]->faultCode);
        }
    }
    printf("--------------------------------test_amx_SetParameterValues------------------------------\n");
    #endif
    if(pFaults) {
        DM_ENG_deleteAllSetParameterValuesFault(pFaults);
        free(pFaults);
    }

    if(pParameterList) {
        DM_ENG_deleteTabParameterValueStruct(pParameterList);
    }
}

void test_dmadapter_AddDeleteObject(UNUSED void** state) {
    char* object_path = "Device.Hosts.Host.";
    // char* path=NULL;
    amxc_string_t path;
    amxc_string_init(&path, 0);
    unsigned int pInstanceNumber;
    DM_ENG_ParameterStatus pStatus;

    int rv = DM_ENG_AddObject(DM_ENG_EntityType_ACS,
                              object_path,
                              "",
                              &pInstanceNumber,
                              &pStatus);
    assert_int_equal(rv, 0);
    assert_int_equal(pInstanceNumber, 3);
    assert_int_equal((int) pStatus, 0);
    #if PRINT_RESULT
    printf("------------------------------test_amx_AddDeleteObject--------------------------------\n");
    if(rv == 0) {
        printf("addobject : OK - instancenumber=%d, pStatus=%d\n", pInstanceNumber, pStatus);
    } else {
        printf("addobject : NOK (%d)\n", rv);
    }
    #endif
    amxc_string_setf(&path, "%s%d.", object_path, pInstanceNumber);

    // try to delete the added object
    rv = DM_ENG_DeleteObject(DM_ENG_EntityType_ACS,
                             (char* ) amxc_string_get(&path, 0),
                             "",
                             &pStatus);
    assert_int_equal(rv, 0);
    assert_int_equal((int) pStatus, 0);
    #if PRINT_RESULT
    if(rv == 0) {
        printf("deleteobject : OK status=%d\n", pStatus);
    } else {
        printf("deleteobject : NOK (%d)\n", rv);
    }
    printf("-------------------------------test_amx_AddDeleteObject-------------------------------\n");
    #endif
    amxc_string_clean(&path);
}


void test_dmadapter_SetParameterAttributes(UNUSED void** state) {
    DM_ENG_ParameterAttributesStruct** pParameterList = NULL;
    int rv = -1;
    char* usrName = "Device.ManagementServer.Username";
    char* deviceStatus = "Device.DeviceInfo.DeviceStatus";
    // Add some passive notification (PASSIVE == 1) and active notification
    // we will read them on test_dmadapter_GetParameterAttributes
    pParameterList = DM_ENG_newTabParameterAttributesStruct(3);
    pParameterList[0] = DM_ENG_newParameterAttributesStruct(usrName, 1, NULL);
    pParameterList[1] = DM_ENG_newParameterAttributesStruct(deviceStatus, 2, NULL);
    pParameterList[2] = NULL;

    rv = DM_ENG_SetParameterAttributes(DM_ENG_EntityType_ACS, pParameterList);
    assert_int_equal(rv, 0);
    if(pParameterList) {
        DM_ENG_deleteTabParameterAttributesStruct(pParameterList);
    }

    // Set the notification mode for deviceStatus parameter to OFF
    // to test the notification remove and delete the subscription
    // We will check this mode on test_dmadapter_GetParameterAttributes
    pParameterList = DM_ENG_newTabParameterAttributesStruct(2);
    pParameterList[0] = DM_ENG_newParameterAttributesStruct(deviceStatus, 0, NULL);
    pParameterList[1] = NULL;

    rv = DM_ENG_SetParameterAttributes(DM_ENG_EntityType_ACS, pParameterList);
    assert_int_equal(rv, 0);
    if(pParameterList) {
        DM_ENG_deleteTabParameterAttributesStruct(pParameterList);
    }
}

void test_dmadapter_GetParameterAttributes(UNUSED void** state) {
    char* paramArray[100];
    DM_ENG_ParameterAttributesStruct** pResult = NULL;
    int rv = -1;
    int rs = 0;

    char* hdrversion = "Device.DeviceInfo.HardwareVersion";
    char* sfrversion = "Device.DeviceInfo.SoftwareVersion";
    char* usrName = "Device.ManagementServer.Username";
    char* deviceStatus = "Device.DeviceInfo.DeviceStatus";
    paramArray[0] = hdrversion;
    paramArray[1] = sfrversion;
    paramArray[2] = usrName;
    paramArray[3] = deviceStatus;
    paramArray[4] = NULL;

    rv = DM_ENG_GetParameterAttributes(DM_ENG_EntityType_ACS, (char**) paramArray, &pResult);
    assert_int_equal(rv, 0);

    rs = DM_ENG_tablen((void**) pResult);
    assert_int_equal(rs, 4);
    //(FORCED == 3), (ACTIVE == 2), (PASSIVE == 1) and (OFF == 0)
    assert_string_equal(pResult[0]->parameterName, hdrversion);
    assert_int_equal(pResult[0]->notification, 2);
    assert_string_equal(pResult[1]->parameterName, sfrversion);
    assert_int_equal(pResult[1]->notification, 2);
    assert_string_equal(pResult[2]->parameterName, usrName);
    assert_int_equal(pResult[2]->notification, 1);
    assert_string_equal(pResult[3]->parameterName, deviceStatus);
    assert_int_equal(pResult[3]->notification, 0);
#if PRINT_RESULT
    for(int i = 0; i < rs; i++) {
        printf("param=%s, Notification Mode=%d\n", pResult[i]->parameterName, pResult[i]->notification);
    }
#endif
    //Clean mem
    if(pResult) {
        DM_ENG_deleteTabParameterAttributesStruct(pResult);
    }
}

void test_dmadapter_Open_Close_Session(UNUSED void** state) {
    char* session_status;
    DM_ENG_SessionOpened(DM_ENG_EntityType_ACS);
    assert_int_equal(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_SESSIONSTATUS, &session_status), 0);
    assert_string_equal(session_status, "Busy");
    free(session_status);
    DM_ENG_SessionClosed(DM_ENG_EntityType_ACS, true);
    assert_int_equal(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_SESSIONSTATUS, &session_status), 0);
    assert_string_equal(session_status, "Idle");
    free(session_status);
}

void test_dmadapter_Reboot(UNUSED void** state) {
    DM_ENG_SessionOpened(DM_ENG_EntityType_ACS);
    int rv = DM_ENG_Reboot(DM_ENG_EntityType_ACS, "My reboot command");
    assert_int_equal(rv, 0);
    DM_ENG_SessionClosed(DM_ENG_EntityType_ACS, true);
}

void test_dmadapter_FactoryReset(UNUSED void** state) {
    DM_ENG_SessionOpened(DM_ENG_EntityType_ACS);
    int rv = DM_ENG_FactoryReset(DM_ENG_EntityType_ACS);
    assert_int_equal(rv, 0);
    DM_ENG_SessionClosed(DM_ENG_EntityType_ACS, true);
}
