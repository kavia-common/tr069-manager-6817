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
#include <stdlib.h>
#include <stdio.h>
#include <setjmp.h>
#include <stdarg.h>
#include <cmocka.h>
#include <string.h>
#include <time.h>

#include <amxc/amxc_macros.h>
#include <amxc/amxc.h>
#include <amxp/amxp.h>

#include <amxd/amxd_dm.h>
#include <amxd/amxd_object.h>
#include <amxd/amxd_transaction.h>

#include <amxo/amxo.h>

#include <amxut/amxut_bus.h>
#include <amxut/amxut_dm.h>
#include <amxut/amxut_macros.h>
#include <amxut/amxut_sahtrace.h>
#include <amxut/amxut_timer.h>
#include <amxut/amxut_util.h>

#include <debug/sahtrace.h>
#include <debug/sahtrace_macros.h>

#include "test_get_parameter_attributes.h"

#include <dmengine/DM_ENG_Type.h>
#include <dmengine/DM_ENG_EntityType.h>
#include <dmengine/DM_ENG_RPCInterface.h>

#include "../../src/dmdeviceadapter/amx/adapter/DM_AmxParameterValues.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

static const char* definition_odl = "./test_data/device-definition.odl";

xmlDocPtr doc = NULL;
xmlNodePtr body_node = NULL;
dm_amx_env_t* acs_info = NULL;

int test_gpa_all_setup(void** state) {

    LIBXML_TEST_VERSION

        doc = xmlNewDoc(BAD_CAST "1.0");
    xmlNodePtr root_node = xmlNewNode(NULL, BAD_CAST "root");
    xmlDocSetRootElement(doc, root_node);
    body_node = xmlNewChild(root_node, NULL, BAD_CAST "body", NULL);

    amxut_bus_setup(state);
    sahTraceSetLevel(200);
    sahTraceAddZone(500, "TEST");
    sahTraceAddZone(200, "DM_DA");
    // sahTraceAddZone(500, "DM_ENGINE");

    amxut_dm_load_odl(definition_odl);

    const char* default_odl = (const char*) *state;

    if((default_odl != NULL) && (*default_odl != 0)) {
        SAH_TRACEZ_INFO("TEST", "Loading default: %s", default_odl);
        amxut_dm_load_odl(default_odl);
    }

    assert_non_null(amxb_be_who_has("Device."));

    amxut_bus_handle_events();

    acs_info = DM_ENG_Device_GetACSInfo();
    assert_non_null(acs_info);
    acs_info->bus_ctx = amxb_be_who_has("Device.");
    acs_info->acl_rules = amxa_parse_files("./test_data/aclfile.json");

    return 0;
}

int test_gpa_all_teardown(void** state) {

    amxc_var_delete(&acs_info->acl_rules);
    xmlFreeDoc(doc);
    xmlCleanupParser();

    return amxut_bus_teardown(state);
}

static void xml_doc_print(xmlDocPtr doc) {
    xmlChar* xmlBuffer;
    int bufferSize;
    xmlDocDumpFormatMemory(doc, &xmlBuffer, &bufferSize, 1);
    printf("%s", (char*) xmlBuffer);
    xmlFree(xmlBuffer);
}

void test_gpa_all_instance_mode(UNUSED void** state) {
    clock_t start, end;
    double cpu_time_used;
    acs_info->instanceAlias = false;
    start = clock();
    assert_int_equal(DM_ENG_Device_GetParameterAttributesAll(body_node), 0);
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    SAH_TRACEZ_INFO("TEST", "DM_ENG_Device_GetParameterAttributesAll [InstanceMode]: took %f seconds", cpu_time_used);
    xml_doc_print(doc);
}

void test_gpa_all_alias_mode(UNUSED void** state) {
    clock_t start, end;
    double cpu_time_used;
    acs_info->instanceAlias = true;
    start = clock();
    assert_int_equal(DM_ENG_Device_GetParameterAttributesAll(body_node), 0);
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    SAH_TRACEZ_INFO("TEST", "DM_ENG_Device_GetParameterAttributesAll [AliasMode]: took %f seconds", cpu_time_used);
    xml_doc_print(doc);
}
