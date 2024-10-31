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

#include "cwmp_plugin.h"
#include <amxc/amxc_macros.h>
#include <amxc/amxc_string.h>
#include <amxc/amxc_variant.h>
#include <amxd/amxd_path.h>
#include <debug/sahtrace.h>
#include <debug/sahtrace_macros.h>
#include <stdlib.h>
#include <string.h>
#include <gmap/gmap.h>

#define MANAGEMENTSERVER_PATH           "ManagementServer."
#define MANAGEABLE_DEVICE_PATH          MANAGEMENTSERVER_PATH "ManageableDevice."
#define DEVICES_PATH                    "Devices."
#define DEVICES_DEVICE_PATH             DEVICES_PATH "Device."
#define HOSTS_PATH                      "Hosts."
#define HOST_HOSTS_PATH                 HOSTS_PATH "Host."
#define DM_FILTER_HOST_PHYS_CHANGED     "(notification in ['dm:object-changed']) and contains('parameters.PhysAddress')"

#define GMAP_QUERY_NAME                 "tr069-manager-active"
#define GMAP_QUERY_TAG                  "manageable && .Active != 0"

static gmap_query_t* manageable_gmap_query = NULL;
static amxp_timer_t* gmap_retry_timer = NULL;
static amxp_timer_t* host_retry_timer = NULL;
static amxc_llist_t md_requests;

typedef struct {
    amxc_llist_it_t it;
    amxc_var_t* deviceInfo;
} md_cache_context_t;

static char* cwmp_plugin_get_matching_path(const amxc_htable_t* htable, const char* host_path) {
    char* output_key = NULL;

    amxc_htable_iterate(hit, htable) {
        const char* key = amxc_htable_it_get_key(hit);
        amxc_var_t* host = amxc_var_from_htable_it(hit);
        const char* path = GET_CHAR(host, "Host");

        if(key && path && strstr(path, host_path)) {
            output_key = strdup(key);
            break;
        }
    }
    return output_key;
}

static int cwmp_plugin_search_for_manageable_device(const char* host_path, char** manageable_device_path) {
    int ret = -1;
    amxb_bus_ctx_t* bus_ctx = NULL;
    amxc_var_t get;
    const amxc_htable_t* htable = NULL;

    amxc_var_init(&get);
    bus_ctx = amxb_be_who_has(MANAGEMENTSERVER_PATH);
    when_null(bus_ctx, stop);

    when_failed_trace(amxb_get(bus_ctx, MANAGEABLE_DEVICE_PATH "*.", 1, &get, 10), stop, ERROR, "Failed to get " MANAGEABLE_DEVICE_PATH);

    htable = amxc_var_constcast(amxc_htable_t, GETI_ARG(&get, 0));
    *manageable_device_path = cwmp_plugin_get_matching_path(htable, host_path);

    ret = 0;
stop:
    amxc_var_clean(&get);
    return ret;
}

static int cwmp_plugin_set_manageable_host_param(amxd_object_t* inst, const char* host_list) {
    int ret = -1;
    amxd_trans_t trans;
    amxd_trans_init(&trans);

    amxd_trans_select_object(&trans, inst);
    amxd_trans_set_attr(&trans, amxd_tattr_change_ro, true);
    amxd_trans_set_value(cstring_t, &trans, "Host", host_list);
    when_failed_trace(amxd_trans_apply(&trans, cwmp_plugin_get_dm()), stop, ERROR, "Failed to Update ManageableDevice.Host");
    ret = 0;
stop:
    amxd_trans_clean(&trans);
    return ret;
}

static int cwmp_plugin_add_manageable_host(amxd_object_t* inst, const char* host) {
    int ret = -1;
    char* hosts = NULL;
    amxc_string_t new_host_val;
    amxc_string_init(&new_host_val, 0);

    hosts = amxd_object_get_cstring_t(inst, "Host", NULL);
    when_null(hosts, stop);

    when_false((strstr(hosts, host) == NULL), exit);
    amxc_string_setf(&new_host_val, "%s%s,", hosts, host);
    when_failed_trace(cwmp_plugin_set_manageable_host_param(inst, amxc_string_get(&new_host_val, 0)), stop, ERROR, "Failed to Update ManageableDevice.Host");
exit:
    ret = 0;
stop:
    free(hosts);
    amxc_string_clean(&new_host_val);
    return ret;
}

static int cwmp_plugin_create_manageable_device(const char* moui, const char* sn, const char* pc, const char* host) {
    int ret = -1;
    amxd_trans_t trans;
    amxc_string_t new_host_val;
    amxd_trans_init(&trans);
    amxc_string_init(&new_host_val, 0);

    amxc_string_setf(&new_host_val, "%s,", host);
    amxd_trans_select_pathf(&trans, MANAGEABLE_DEVICE_PATH);
    amxd_trans_set_attr(&trans, amxd_tattr_change_ro, true);
    amxd_trans_add_inst(&trans, 0, NULL);

    amxd_trans_set_value(cstring_t, &trans, "ManufacturerOUI", moui);
    amxd_trans_set_value(cstring_t, &trans, "ProductClass", pc);
    amxd_trans_set_value(cstring_t, &trans, "SerialNumber", sn);
    amxd_trans_set_value(cstring_t, &trans, "Host", amxc_string_get(&new_host_val, 0));
    when_failed_trace(amxd_trans_apply(&trans, cwmp_plugin_get_dm()), stop, ERROR, "Failed to add a new ManageableDevice");
    ret = 0;
stop:
    amxd_trans_clean(&trans);
    amxc_string_clean(&new_host_val);
    return ret;
}

static int cwmp_plugin_del_manageable_device(int index) {
    int ret = -1;
    amxd_trans_t trans;
    amxd_trans_init(&trans);

    amxd_trans_select_pathf(&trans, MANAGEABLE_DEVICE_PATH);
    amxd_trans_set_attr(&trans, amxd_tattr_change_ro, true);
    amxd_trans_del_inst(&trans, index, NULL);
    when_failed_trace(amxd_trans_apply(&trans, cwmp_plugin_get_dm()), stop, ERROR, "Failed to delete Manageable Device Instance [%d]", index);
    ret = 0;
stop:
    amxd_trans_clean(&trans);
    return ret;
}

static int cwmp_plugin_update_manageable_device(const char* manageable, const char* host) {
    int ret = -1;
    char* hosts = NULL;
    amxd_object_t* inst = NULL;
    amxc_string_t manageable_hosts, new_manageable_hosts;
    amxc_llist_t string_list;
    amxc_string_init(&manageable_hosts, 0);
    amxc_string_init(&new_manageable_hosts, 0);
    amxc_llist_init(&string_list);

    inst = amxd_dm_findf(cwmp_plugin_get_dm(), "%s", manageable);
    when_null(inst, stop);

    hosts = amxd_object_get_cstring_t(inst, "Host", NULL);
    when_null(hosts, stop);
    amxc_string_setf(&manageable_hosts, "%s", hosts);
    when_failed(amxc_string_split_to_llist(&manageable_hosts, &string_list, ','), stop);

    amxc_llist_iterate(it, &string_list) {
        const char* val = amxc_string_get(amxc_string_from_llist_it(it), 0);
        if(val && (strlen(val) > 0) && (strcmp(host, val) != 0)) {
            amxc_string_append(&new_manageable_hosts, val, strlen(val));
            amxc_string_append(&new_manageable_hosts, ",", 1);
        }
    }

    if(strstr(amxc_string_get(&new_manageable_hosts, 0), "Host") == NULL) {
        when_failed_trace(cwmp_plugin_del_manageable_device(inst->index), stop, ERROR,
                          "Failed to delete manageable device [%s]", manageable);
    } else {
        when_failed_trace(cwmp_plugin_set_manageable_host_param(inst, amxc_string_get(&new_manageable_hosts, 0)), stop, ERROR,
                          "Failed to remove host from manageable device [%s] - Host:[%s]", manageable, host);
    }
    ret = 0;
stop:
    free(hosts);
    amxc_llist_clean(&string_list, amxc_string_list_it_free);
    amxc_string_clean(&new_manageable_hosts);
    amxc_string_clean(&manageable_hosts);
    return ret;
}

static int cwmp_plugin_add_manageable_device(amxc_var_t* manageableDeviceInfo) {
    int ret = -1;
    amxd_object_t* inst = NULL;
    when_null(manageableDeviceInfo, stop);

    const char* moui = GET_CHAR(manageableDeviceInfo, "ManufacturerOUI");
    const char* sn = GET_CHAR(manageableDeviceInfo, "SerialNumber");
    const char* pc = GET_CHAR(manageableDeviceInfo, "ProductClass");
    const char* host = GET_CHAR(manageableDeviceInfo, "Host");
    when_false_trace((moui && sn && pc && host), stop,
                     ERROR, "Incomplete manageable device moui:[%s] sn:[%s] pc:[%s] host:[%s]", moui, sn, pc, host);

    inst = amxd_dm_findf(cwmp_plugin_get_dm(), "%s[ManufacturerOUI == '%s' && SerialNumber == '%s' && ProductClass == '%s'].",
                         MANAGEABLE_DEVICE_PATH, moui, sn, pc);
    if(inst) {
        when_failed_trace(cwmp_plugin_add_manageable_host(inst, host), stop, ERROR,
                          "Failed to add host to manageable device - OUI:[%s] SN:[%s] PC:[%s] Host:[%s]", moui, sn, pc, host);
    } else {
        when_failed_trace(cwmp_plugin_create_manageable_device(moui, sn, pc, host), stop, ERROR,
                          "Failed to create manageable device - OUI:[%s] SN:[%s] PC:[%s] Host:[%s]", moui, sn, pc, host);
    }
    ret = 0;
stop:
    return ret;
}

static int cwmp_plugin_search_for_hosts(const char* mac, amxc_llist_t* host_paths) {
    int ret = -1;
    amxb_bus_ctx_t* host_bus_ctx = NULL;
    const amxc_htable_t* htable = NULL;
    amxc_var_t* host = NULL;
    amxc_var_t hosts_object;
    amxc_string_t* host_string = NULL;

    amxc_var_init(&hosts_object);
    when_null(mac, stop);
    when_null(host_paths, stop);

    host_bus_ctx = amxb_be_who_has(HOSTS_PATH);
    when_null(host_bus_ctx, stop);
    when_failed(amxb_get(host_bus_ctx, HOST_HOSTS_PATH "*.", 1, &hosts_object, 10), stop);
    when_true(amxc_var_is_null(&hosts_object), stop);

    htable = amxc_var_constcast(amxc_htable_t, GETI_ARG(&hosts_object, 0));
    amxc_htable_iterate(hit, htable) {
        const char* key = amxc_htable_it_get_key(hit);
        host = amxc_var_from_htable_it(hit);
        amxc_var_log(host);
        const char* host_mac = GET_CHAR(host, "PhysAddress");
        if(key && host_mac && (strcasecmp(host_mac, mac) == 0)) {
            amxc_string_new(&host_string, 0);
            amxc_string_setf(host_string, "Device.%s", key);
            amxc_llist_append(host_paths, &host_string->it);
            ret = 0;
        }
    }
stop:
    amxc_var_clean(&hosts_object);
    return ret;
}

static int cwmp_plugin_search_for_hosts_by_device_key(const char* device_key, amxc_llist_t* host_paths) {
    int ret = -1;
    const char* physaddress = NULL;
    amxb_bus_ctx_t* device_bus_ctx = NULL;
    amxc_var_t device;
    amxc_string_t device_path;
    amxc_var_init(&device);
    amxc_string_init(&device_path, 0);

    device_bus_ctx = amxb_be_who_has(DEVICES_PATH);
    when_null(device_bus_ctx, stop);

    amxc_string_setf(&device_path, DEVICES_DEVICE_PATH "%s.PhysAddress", device_key);
    when_failed(amxb_get(device_bus_ctx, amxc_string_get(&device_path, 0), 1, &device, 10), stop);
    when_true(amxc_var_is_null(&device), stop);

    physaddress = GETP_CHAR(&device, "0.0.PhysAddress");
    when_null(physaddress, stop);

    when_failed(cwmp_plugin_search_for_hosts(physaddress, host_paths), stop);
    ret = 0;
stop:
    amxc_string_clean(&device_path);
    amxc_var_clean(&device);
    return ret;
}

static int cwmp_plugin_set_manageable_deviceinfo(amxc_var_t* manageableDeviceInfo, amxc_var_t* device, const char* host_path) {
    int ret = -1;
    when_true(amxc_htable_is_empty(amxc_var_constcast(amxc_htable_t, device)), stop);

    amxc_var_add_key(cstring_t, manageableDeviceInfo, "Host", host_path);
    amxc_var_add_key(cstring_t, manageableDeviceInfo, "Key", GET_CHAR(device, "Key"));
    amxc_var_add_key(cstring_t, manageableDeviceInfo, "PhysAddress", GET_CHAR(device, "PhysAddress"));
    amxc_var_add_key(cstring_t, manageableDeviceInfo, "ManufacturerOUI", GET_CHAR(device, "OUI"));
    amxc_var_add_key(cstring_t, manageableDeviceInfo, "SerialNumber", GET_CHAR(device, "SerialNumber"));
    amxc_var_add_key(cstring_t, manageableDeviceInfo, "ProductClass", GET_CHAR(device, "ProductClass"));
    ret = 0;
stop:
    return ret;
}

static md_cache_context_t* cwmp_plugin_md_cache_find(const char* parameter, const char* value) {
    md_cache_context_t* md = NULL;
    amxc_llist_for_each(it, &md_requests) {
        md = amxc_container_of(it, md_cache_context_t, it);
        const char* param = GET_CHAR(md->deviceInfo, parameter);
        if(param && (0 == strcmp(param, value))) {
            return md;
        }
    }
    return NULL;
}

static void cwmp_plugin_md_cache_init(void) {
    amxc_llist_init(&md_requests);
}

static void cwmp_plugin_md_cache_free(amxc_llist_it_t* it) {
    md_cache_context_t* md = amxc_container_of(it, md_cache_context_t, it);
    amxc_var_delete(&md->deviceInfo);
    free(md);
}

static void cwmp_plugin_md_cache_cleanup(void) {
    amxc_llist_clean(&md_requests, cwmp_plugin_md_cache_free);
}

static int cwmp_plugin_md_cache_create(md_cache_context_t** md, amxc_var_t* deviceInfo) {
    int ret = -1;
    when_null(md, stop);
    *md = (md_cache_context_t*) calloc(1, sizeof(md_cache_context_t));
    when_null_trace(*md, stop, ERROR, "alloc failed");
    amxc_var_new(&(*md)->deviceInfo);
    amxc_var_copy((*md)->deviceInfo, deviceInfo);
    ret = 0;
stop:
    return ret;
}

static int cwmp_plugin_add_manageable_device_to_cache(amxc_var_t* manageableDeviceInfo) {
    int ret = -1;
    md_cache_context_t* md = NULL;
    when_failed(cwmp_plugin_md_cache_create(&md, manageableDeviceInfo), stop);
    amxc_llist_append(&md_requests, &md->it);
    ret = 0;
stop:
    return ret;
}

static void cwmp_plugin_clean_manageable_device_cache(const char* key) {
    md_cache_context_t* md = NULL;

    md = cwmp_plugin_md_cache_find("Key", key);
    when_null(md, exit);
    amxc_llist_it_take(&md->it);
    cwmp_plugin_md_cache_free(&md->it);
exit:
    return;
}

static void cwmp_plugin_manageable_query(gmap_query_t* query UNUSED, const char* key, amxc_var_t* device, gmap_query_action_t action) {
    const char* host_path = NULL;
    amxc_llist_t host_paths;
    char* manageable_device_path = NULL;
    amxc_var_t manageableDevice;

    amxc_llist_init(&host_paths);
    amxc_var_init(&manageableDevice);
    amxc_var_set_type(&manageableDevice, AMXC_VAR_ID_HTABLE);
    when_null_trace(key, stop, ERROR, "key == NULL");

    if(action == gmap_query_expression_start_matching) {
        when_null_trace(device, stop, ERROR, "%s - device==NULL", key);
        if(0 == cwmp_plugin_search_for_hosts(GET_CHAR(device, "PhysAddress"), &host_paths)) {
            amxc_llist_for_each(it, &host_paths) {
                host_path = amxc_string_get(amxc_string_from_llist_it(it), 0);
                when_failed_trace(cwmp_plugin_set_manageable_deviceinfo(&manageableDevice, device, host_path), stop, ERROR,
                                  "Failed to set manageable deviceinfo - device:[%s] host_path:[%s]", GET_CHAR(device, "PhysAddress"), host_path);
                when_failed_trace(cwmp_plugin_add_manageable_device(&manageableDevice), stop, ERROR,
                                  "Failed to add manageable device - device:[%s] host_path:[%s]", GET_CHAR(device, "PhysAddress"), host_path);
                SAH_TRACEZ_INFO(ME, "Added manageable device - device:[%s] host_path:[%s]", GET_CHAR(device, "PhysAddress"), host_path);
                amxc_var_clean(&manageableDevice);
            }
        } else {
            when_failed_trace(cwmp_plugin_set_manageable_deviceinfo(&manageableDevice, device, NULL), stop, ERROR,
                              "Failed to set manageable deviceinfo - device:[%s]", GET_CHAR(device, "PhysAddress"));
            when_failed_trace(cwmp_plugin_add_manageable_device_to_cache(&manageableDevice), stop, ERROR,
                              "Failed to add manageable device to cache - device:[%s]", GET_CHAR(device, "PhysAddress"));
            SAH_TRACEZ_INFO(ME, "Added manageable device to cache - device:[%s]", GET_CHAR(device, "PhysAddress"));
        }
    } else if(action == gmap_query_expression_stop_matching) {
        cwmp_plugin_clean_manageable_device_cache(key);
        when_failed(cwmp_plugin_search_for_hosts_by_device_key(key, &host_paths), stop);
        amxc_llist_for_each(it, &host_paths) {
            host_path = amxc_string_get(amxc_string_from_llist_it(it), 0);
            when_failed(cwmp_plugin_search_for_manageable_device(host_path, &manageable_device_path), stop);
            when_failed(cwmp_plugin_update_manageable_device(manageable_device_path, host_path), stop);
            free(manageable_device_path);
            manageable_device_path = NULL;
        }
    }
stop:
    amxc_var_clean(&manageableDevice);
    free(manageable_device_path);
    amxc_llist_clean(&host_paths, amxc_string_list_it_free);
    return;
}

static void cwmp_plugin_try_open_gmap_query(UNUSED amxp_timer_t* timer, UNUSED void* priv) {
    static int timeout = 1000;
    when_not_null(manageable_gmap_query, stop);

    manageable_gmap_query = gmap_query_open(GMAP_QUERY_TAG, GMAP_QUERY_NAME, cwmp_plugin_manageable_query);
    if(manageable_gmap_query == NULL) {
        amxp_timer_start(gmap_retry_timer, timeout);
        if(timeout < 20000) {
            timeout = timeout * 2;
        }
    }
stop:
    return;
}

static void cwmp_plugin_host_phys_changed(UNUSED const char* const sig_name, const amxc_var_t* const data, UNUSED void* const priv) {
    amxc_var_t* parameters = NULL;
    amxc_var_t* phys_var = NULL;
    const char* physaddress = NULL;
    md_cache_context_t* md_cache = NULL;
    const char* path = NULL;
    amxc_var_t* host = NULL;
    amxc_string_t host_string;
    amxc_string_init(&host_string, 0);

    when_null_trace(data, stop, ERROR, "data == NULL");
    parameters = GET_ARG(data, "parameters");
    when_null_trace(parameters, stop, ERROR, "data.parameters == NULL");
    phys_var = GET_ARG(parameters, "PhysAddress");
    when_null_trace(phys_var, stop, ERROR, "No PhysAddress parameter found");
    physaddress = GET_CHAR(phys_var, "to");
    when_str_empty(physaddress, stop);
    SAH_TRACEZ_INFO(ME, "Host physical address updated - device:[%s]", physaddress);
    md_cache = cwmp_plugin_md_cache_find("PhysAddress", physaddress);
    when_null(md_cache, stop);
    SAH_TRACEZ_INFO(ME, "Found device in cache - device:[%s]", physaddress);

    path = GET_CHAR(data, "path");
    when_str_empty(path, stop);

    amxc_string_setf(&host_string, "Device.%s", path);
    host = amxc_var_add_new_key(md_cache->deviceInfo, "Host");
    amxc_var_push(cstring_t, host, amxc_string_take_buffer(&host_string));

    when_failed_trace(cwmp_plugin_add_manageable_device(md_cache->deviceInfo), stop, ERROR,
                      "Failed to add manageable device - device:[%s] host_path:[%s]", physaddress, GET_CHAR(md_cache->deviceInfo, "Host"));
    SAH_TRACEZ_INFO(ME, "Added manageable device - device:[%s] host_path:[%s]", physaddress, GET_CHAR(md_cache->deviceInfo, "Host"));
    amxc_llist_it_take(&md_cache->it);
    cwmp_plugin_md_cache_free(&md_cache->it);
stop:
    amxc_string_clean(&host_string);
    return;
}

static void cwmp_plugin_try_add_manageabledevice_subscriptions(UNUSED amxp_timer_t* timer, UNUSED void* priv) {
    static int host_timeout = 1000;

    int retval = cwmp_plugin_add_subscription(HOST_HOSTS_PATH, DM_FILTER_HOST_PHYS_CHANGED, cwmp_plugin_host_phys_changed);
    if(retval != 0) {
        SAH_TRACEZ_INFO(ME, "Could not create Host subscription - retrying");
        amxp_timer_start(host_retry_timer, host_timeout);
        if(host_timeout < 20000) {
            host_timeout = host_timeout * 2;
        }
        goto stop;
    }

    cwmp_plugin_try_open_gmap_query(NULL, NULL);
stop:
    return;
}

void cwmp_plugin_manageabledevice_init(void) {
    amxb_bus_ctx_t* gmap_ctx = amxb_be_who_has("Devices.Device");
    when_null_trace(gmap_ctx, stop, ERROR, "gMap data model not found 'Devices.Device' - is gmap-server running?");
    gmap_client_init(gmap_ctx);
    cwmp_plugin_md_cache_init();
    amxp_timer_new(&host_retry_timer, cwmp_plugin_try_add_manageabledevice_subscriptions, NULL);
    amxp_timer_new(&gmap_retry_timer, cwmp_plugin_try_open_gmap_query, NULL);

    cwmp_plugin_try_add_manageabledevice_subscriptions(NULL, NULL);
stop:
    return;
}

void cwmp_plugin_manageabledevice_clean(void) {
    gmap_query_close(manageable_gmap_query);
    manageable_gmap_query = NULL;
    cwmp_plugin_del_subscription(HOST_HOSTS_PATH, NULL);
    cwmp_plugin_md_cache_cleanup();
}
