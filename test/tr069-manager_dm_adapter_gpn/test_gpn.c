/****************************************************************************
**
** SPDX-License-Identifier: BSD-2-Clause-Patent
**
** SPDX-FileCopyrightText: Copyright (c) 2025 SoftAtHome
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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <setjmp.h>
#include <cmocka.h>
#include <amxut/amxut_bus.h>
#include <amxut/amxut_dm.h>
#include <debug/sahtrace.h>

#include "test_gpn.h"
#include "../../src/dmdeviceadapter/amx/adapter/DM_AmxParameter.h"

#define TEST_DEBUG_LOGS 0

#define MAX_EXPECTED_PARAMETERS 20000

typedef struct pn_t {
    const char* name;
    bool writable;
} pn_t;

static const char* device_odl = "./test_data/device.odl";

dm_amx_env_t* acs_info = NULL;
dm_amx_env_t* system_info = NULL;

int test_gpn_setup(UNUSED void** state) {
    amxut_bus_setup(state);
#if TEST_DEBUG_LOGS
    sahTraceSetLevel(500);
    sahTraceAddZone(500, "DM_DA");
#endif
    amxut_dm_load_odl(device_odl);
    assert_non_null(amxb_be_who_has("Device."));
    system_info = DM_ENG_Device_GetSystemInfo();
    system_info->bus_ctx = amxb_be_who_has("Device.");
    acs_info = DM_ENG_Device_GetACSInfo();
    acs_info->prefix = "Device.";
    assert_non_null(acs_info);
    acs_info->bus_ctx = amxb_be_who_has("Device.");
    acs_info->acl_rules = amxa_parse_files("./test_data/aclfile.json");
    return 0;
}

int test_gpn_teardown(void** state) {
    amxc_var_delete(&acs_info->acl_rules);
    return amxut_bus_teardown(state);
}

static size_t get_result_from_pn(dm_eng_pn_list_t* list, pn_t result[], size_t max) {
    size_t count = 0;
    amxc_htable_for_each(hit, list) {
        dm_eng_pn_t* pn = amxc_htable_it_get_data(hit, dm_eng_pn_t, hit);
#if TEST_DEBUG_LOGS
        print_message("{\"%s\", \"%d\"},\n", pn->name, pn->writable);
#endif
        result[count].name = pn->name;
        result[count].writable = pn->writable;
        count++;
        if(count >= max) {
            break;
        }
    }
    return count;
}

static void verify_parameter_list(
    uint8_t test_number,
    pn_t expected[],
    size_t expected_count,
    pn_t result[],
    size_t actual_count
    ) {

#if TEST_DEBUG_LOGS
    print_message("Case: %d, count [expected:%ld - actual %ld]\n", test_number, expected_count, actual_count);
#endif
    assert_non_null(expected);
    assert_int_equal(expected_count, actual_count);
    size_t i = 0;
    while(expected[i].name != NULL) {
        i++;
    }
    assert_int_equal(expected_count, i);

    for(i = 0; i < expected_count; ++i) {
        size_t j = 0;
        bool found = false;
        for(; j < actual_count; ++j) {
            if(strcmp(expected[i].name, result[j].name) == 0) {
                found = true;
                break;
            }
        }
        if(!found) {
            fail_msg("Expected '%s' not found in actual list", expected[i].name);
        } else {
#if TEST_DEBUG_LOGS
            print_message("Expected: %s, %d\n", expected[i].name, expected[i].writable);
            print_message("Result  : %s, %d\n", result[j].name, result[j].writable);
#endif
            assert_int_equal(expected[i].writable, result[j].writable);
        }
    }

    print_message("Test [Case %d] is OK\n", test_number);
}

void test_gpn(UNUSED void** state) {
    struct {
        bool enable;
        bool nextlevel;
        const char* input_path;
        pn_t expected[MAX_EXPECTED_PARAMETERS];
        size_t expected_count;
    } test_cases[] = {
        {
            true,
            false,        // nextlevel set to false
            "Device.IP.", // requested parial path
            {             // expected
                {"Device.IP.", false},
                {"Device.IP.ActivePortNumberOfEntries", false},
                {"Device.IP.ActivePort.", true},
                {"Device.IP.Diagnostics.", false},
                {"Device.IP.Diagnostics.Controller", true},
                {"Device.IP.Diagnostics.DownloadDiagnosticsMaxConnections", true},
                {"Device.IP.Interface.", true},
                {"Device.IP.Interface.1.", true},
                {"Device.IP.Interface.1.Alias", true},
                {"Device.IP.Interface.1.Enable", true},
                {"Device.IP.Interface.1.IPv4Address.", true},
                {"Device.IP.Interface.1.IPv4Address.1.", true},
                {"Device.IP.Interface.1.IPv4Address.1.Alias", true},
                {"Device.IP.Interface.1.IPv4Address.1.IPAddress", true},
                {"Device.IP.Interface.2.", true},
                {"Device.IP.Interface.2.Alias", true},
                {"Device.IP.Interface.2.Enable", true},
                {"Device.IP.Interface.2.IPv4Address.", true},
                {"Device.IP.Interface.2.IPv4Address.1.", true},
                {"Device.IP.Interface.2.IPv4Address.1.Alias", true},
                {"Device.IP.Interface.2.IPv4Address.1.IPAddress", true},
                {NULL, false}
            },
            21
        },
        {
            true,
            true,         // nextlevel set to true
            "Device.IP.", // requested parial path
            {             // expected
                {"Device.IP.ActivePortNumberOfEntries", false},
                {"Device.IP.ActivePort.", true},
                {"Device.IP.Diagnostics.", false},
                {"Device.IP.Interface.", true},
                {NULL, false}
            },
            4
        },
        {
            true,
            true,                                   // nextlevel set to true
            "Device.IP.Interface.*.IPv4Address.*.", // requested wildcard path
            {                                       // expected
                {"Device.IP.Interface.1.IPv4Address.1.", true},
                {"Device.IP.Interface.1.IPv4Address.1.Alias", true},
                {"Device.IP.Interface.1.IPv4Address.1.IPAddress", true},
                {"Device.IP.Interface.2.IPv4Address.1.", true},
                {"Device.IP.Interface.2.IPv4Address.1.Alias", true},
                {"Device.IP.Interface.2.IPv4Address.1.IPAddress", true},
                {NULL, false}
            },
            6
        },
        {
            true,
            false,                              // nextlevel set to false
            "Device.IP.Diagnostics.Controller", // requested parameter path
            {                                   // expected
                {"Device.IP.Diagnostics.Controller", true},
                {NULL, false}
            },
            1
        },
    };

    static const size_t test_case_count = sizeof(test_cases) / sizeof(test_cases[0]);

    for(size_t i = 0; i < test_case_count; ++i) {
        if(!test_cases[i].enable) {
            print_message("Case %ld skipped\n", i);
            continue;
        }

        dm_eng_pn_list_t pn_list;
        dm_eng_pn_list_init(&pn_list);

        clock_t start, end;
        double cpu_time_used;
        start = clock();

        assert_int_equal(DM_ENG_Device_GPN(test_cases[i].input_path, test_cases[i].nextlevel, &pn_list), 0);

        end = clock();
        cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
        print_message("DM_ENG_Device_GPN: took %f seconds\n", cpu_time_used);

        pn_t result[MAX_EXPECTED_PARAMETERS];
        size_t result_count = get_result_from_pn(&pn_list, result, MAX_EXPECTED_PARAMETERS);

        verify_parameter_list(i,
                              test_cases[i].expected,
                              test_cases[i].expected_count,
                              result,
                              result_count);

        dm_eng_pn_list_clean(&pn_list);
    }
}