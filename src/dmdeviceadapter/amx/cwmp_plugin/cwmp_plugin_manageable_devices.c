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
#include <v4v6option.h>

#define HOSTS_PATH                      "Hosts.Host."
#define DHCP4_CLIENTS_PATH              "DHCPv4Server.Pool.1.Client."
#define DHCP6_CLIENTS_PATH              "DHCPv6Server.Pool.1.Client."
#define MANAGEABLE_DEVICE_PATH          "ManagementServer.ManageableDevice."

#define FILTER_INSTANCE_ADDED  "notification in ['dm:instance-added']"
#define FILTER_OBJECT_CHANGED  "notification in ['dm:object-changed']"

static int cwmp_plugin_search_for_manageable_device(const char* host_path, char** manageable_device_path) {
    amxc_var_t get;
    amxc_var_init(&get);
    int ret = -1;
    const amxc_htable_t* htable = NULL;
    amxb_bus_ctx_t* bus_ctx = amxb_be_who_has("ManagementServer.");
    when_null(bus_ctx, stop);
    ret = amxb_get(bus_ctx, "ManagementServer.ManageableDevice.*.", 1, &get, 10);

    if((ret != AMXB_STATUS_OK) || amxc_var_is_null(&get)) {
        SAH_TRACEZ_ERROR(ME, "Failed to get ManageableDevices error [%d] ", ret);
    } else {
        htable = amxc_var_constcast(amxc_htable_t, GETI_ARG(&get, 0));
        amxc_htable_iterate(hit, htable) {
            const char* key = amxc_htable_it_get_key(hit);
            amxc_var_t* host = amxc_var_from_htable_it(hit);
            const char* path = GETP_CHAR(host, "Host");

            if(key && path && strstr(path, host_path)) {
                *manageable_device_path = strdup(key);
                ret = 0;
                goto stop;
            }
        }
    }
stop:
    amxc_var_clean(&get);
    return ret;
}

static int manageable_device_dhclient_search(const char* host_path, char** dhclient_path) {
    const amxc_htable_t* htable = NULL;
    amxc_var_t dhclient_objects;
    amxc_var_init(&dhclient_objects);
    amxc_var_t mac_objects;
    amxc_var_init(&mac_objects);
    const char* dhcp_clients[3] = { "DHCPv4Server.Pool.1.Client.*.", "DHCPv6Server.Pool.1.Client.*.", NULL };
    int ret = -1;
    int i = 0;
    const char* host_mac = NULL;
    amxc_string_t mac_path;
    amxc_string_init(&mac_path, 0);
    amxb_bus_ctx_t* host_bus = amxb_be_who_has("Hosts.");

    //lookup for mac
    amxc_string_setf(&mac_path, "%sPhysAddress", host_path);
    when_failed(amxb_get(host_bus, amxc_string_get(&mac_path, 0), 1, &mac_objects, 10), stop);
    when_true(amxc_var_is_null(&mac_objects), stop);


    amxc_string_clean(&mac_path);
    amxc_string_setf(&mac_path, "0.'%s'.PhysAddress", host_path);
    host_mac = GETP_CHAR(&mac_objects, amxc_string_get(&mac_path, 0));
    when_null(host_mac, stop);

    while(dhcp_clients[i] != NULL) {
        char* parent = NULL;
        amxb_bus_ctx_t* bus_ctx = NULL;
        bus_ctx = get_bus_ctx(dhcp_clients[i]);
        when_null(bus_ctx, next);

        when_failed(amxb_get(bus_ctx, dhcp_clients[i], 1, &dhclient_objects, 10), stop);
        when_true(amxc_var_is_null(&dhclient_objects), stop);

        htable = amxc_var_constcast(amxc_htable_t, GETI_ARG(&dhclient_objects, 0));

        amxc_htable_iterate(hit, htable) {
            const char* key = amxc_htable_it_get_key(hit);
            amxc_var_t* host = amxc_var_from_htable_it(hit);
            amxc_var_log(host);
            const char* mac = GETP_CHAR(host, "Chaddr");
            if((mac != NULL) && (strcasecmp(host_mac, mac) == 0)) {
                *dhclient_path = strdup(key);
                ret = 0;
                goto stop;
            }
        }
next:
        free(parent);
        i++;
    }
stop:
    amxc_var_clean(&dhclient_objects);
    return ret;
}

static int manageable_device_get_dhclient(const char* host_path, char** dhclient_path) {
    amxb_bus_ctx_t* ctx = amxb_be_who_has("Hosts.");
    amxc_var_t dhclient_object;
    amxc_var_init(&dhclient_object);
    amxc_llist_t string_list;
    amxc_llist_init(&string_list);
    amxc_string_t dhclient;
    amxc_string_init(&dhclient, 0);
    const char* tmp_str = NULL;
    int ret = -1;
    amxc_string_setf(&dhclient, "%sDHCPClient", host_path);
    when_failed(amxb_get(ctx, amxc_string_get(&dhclient, 0), 1, &dhclient_object, 10), stop);

    amxc_string_setf(&dhclient, "0.'%s'.DHCPClient", host_path);
    tmp_str = GETP_CHAR(&dhclient_object, amxc_string_get(&dhclient, 0));

    when_str_empty(tmp_str, search);
    amxc_string_setf(&dhclient, "%s", tmp_str);
    when_failed(amxc_string_split_to_llist(&dhclient, &string_list, ','), stop);
    amxc_llist_iterate(it, &string_list) {
        const char* val = amxc_string_get(amxc_string_from_llist_it(it), 0);
        if(val && *val) {
            *dhclient_path = strdup(val);
            ret = 0;
            SAH_TRACEZ_INFO(ME, "ManageableDevice dhcp-client found at %s", *dhclient_path);
            goto stop;
        }
    }

search:
    if(manageable_device_dhclient_search(host_path, dhclient_path) < 0) {
        SAH_TRACEZ_ERROR(ME, "Couldnt found any dhclient for %s", host_path);
    }

stop:
    amxc_string_clean(&dhclient);
    return ret;
}

static int manageable_device_get_dhclient_option(const char* client_path, char** opt) {
    amxc_string_t options_path;
    amxc_string_init(&options_path, 0);
    amxc_var_t get;
    amxc_var_init(&get);
    uint32_t option_tag = -1;
    const amxc_htable_t* htable = NULL;
    amxb_bus_ctx_t* bus_ctx = NULL;
    int ret = 0;

    if(strstr(client_path, "DHCPv6")) {
        option_tag = 17;
        bus_ctx = amxb_be_who_has("DHCPv6Server.");
    } else {
        option_tag = 125;
        bus_ctx = amxb_be_who_has("DHCPv4Server.");
    }
    when_null(bus_ctx, stop);

    amxc_string_setf(&options_path, "%sOption.*.", client_path);
    ret = amxb_get(bus_ctx, amxc_string_get(&options_path, 0), 1, &get, 10);

    if((ret != AMXB_STATUS_OK) || amxc_var_is_null(&get)) {
        SAH_TRACEZ_INFO(ME, "Failed to get DHCP Options error [%d], path [%s] ", ret, amxc_string_get(&options_path, 0));
    }

    htable = amxc_var_constcast(amxc_htable_t, GETI_ARG(&get, 0));
    amxc_htable_iterate(hit, htable) {
        amxc_var_t* option = amxc_var_from_htable_it(hit);
        uint32_t tag = GET_UINT32(option, "Tag");

        if(tag == option_tag) {
            const char* option_value = GETP_CHAR(option, "Value");
            when_null(option_value, stop);
            *opt = strdup(option_value);
            ret = 0;
            goto stop;
        }
    }

stop:
    amxc_string_clean(&options_path);
    amxc_var_clean(&get);
    return ret;
}

static void manageable_device_set_dhcp_options(amxc_var_t* manageableDeviceInfo, const char* data, uint32_t option_tag) {
    amxc_var_t dhcp_info;
    amxc_var_init(&dhcp_info);
    uint32_t length = 0;
    unsigned char* data_to_send_bin = dhcpoption_option_convert2bin(data, &length);

    if(option_tag == 125) {
        dhcpoption_v4parse(&dhcp_info, option_tag, length, data_to_send_bin);
    } else if(option_tag == 17) {
        dhcpoption_v6parse(&dhcp_info, option_tag, length, data_to_send_bin);
    }

    const amxc_var_t* newData = GETP_ARG(&dhcp_info, "0.Data");
    amxc_var_for_each(param, newData) {

        int32_t code = GET_INT32(param, "Code");
        const char* value = GETP_CHAR(param, "Data");
        SAH_TRACEZ_INFO(ME, "Found subopt -> [Code %d : value %s]", code, value);
        switch(code) {
        case 1:
        case 11:
            amxc_var_add_key(cstring_t, manageableDeviceInfo, "ManufacturerOUI", value);
            break;
        case 2:
        case 12:
            amxc_var_add_key(cstring_t, manageableDeviceInfo, "SerialNumber", value);
            break;
        case 3:
        case 13:
            amxc_var_add_key(cstring_t, manageableDeviceInfo, "ProductClass", value);
            break;
        default:
            SAH_TRACEZ_WARNING(ME, "Wrong DHCP suboption [tag : %d] [code : %d] [value : %s] ", option_tag, code, value);
            break;
        }
    }
    amxc_var_clean(&dhcp_info);
}

static void cwmp_plugin_del_manageable_device(int index) {
    amxd_trans_t trans;
    amxd_trans_init(&trans);
    amxd_trans_select_pathf(&trans, MANAGEABLE_DEVICE_PATH);
    amxd_trans_set_attr(&trans, amxd_tattr_change_ro, true);
    amxd_trans_del_inst(&trans, index, NULL);

    if(amxd_trans_apply(&trans, cwmp_plugin_get_dm()) != 0) {
        SAH_TRACEZ_ERROR(ME, "Failed to delete Manageable Device Instance [%d]", index);
    }
    amxd_trans_clean(&trans);
}

static void cwmp_plugin_update_manageable_device(const char* manageable, const char* host) {
    SAH_TRACEZ_INFO(ME, "Update manageable device [%s]", manageable);
    amxc_var_t ret;
    amxc_var_init(&ret);
    amxd_object_t* inst = NULL;
    amxc_string_t manageable_hosts;
    amxc_string_init(&manageable_hosts, 0);
    amxc_llist_t string_list;
    amxc_llist_init(&string_list);
    amxc_string_t new_manageable_hosts;
    amxc_string_init(&new_manageable_hosts, 0);

    inst = amxd_dm_findf(cwmp_plugin_get_dm(), "%s", manageable);
    when_null(inst, stop);

    const char* hosts = amxd_object_get_cstring_t(inst, "Host", NULL);
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
        SAH_TRACEZ_INFO(ME, "Deleting manageable device [%s]", manageable);
        cwmp_plugin_del_manageable_device(inst->index);
    } else {
        amxd_trans_t trans;
        amxd_trans_init(&trans);
        amxd_trans_select_object(&trans, inst);
        amxd_trans_set_attr(&trans, amxd_tattr_change_ro, true);
        amxd_trans_set_value(cstring_t, &trans, "Host", amxc_string_get(&new_manageable_hosts, 0));
        amxd_trans_apply(&trans, cwmp_plugin_get_dm());
        amxd_trans_clean(&trans);
    }

stop:
    amxc_string_clean(&manageable_hosts);
}

static int cwmp_plugin_add_manageable_device(amxc_var_t* manageableDeviceInfo) {
    int ret = -1;
    amxd_object_t* inst = NULL;
    amxd_trans_t trans;
    amxd_trans_init(&trans);
    amxc_string_t new_host_val;
    amxc_string_init(&new_host_val, 0);
    when_null(manageableDeviceInfo, stop);
    const char* ManufacturerOUI = GETP_CHAR(manageableDeviceInfo, "ManufacturerOUI");
    const char* SerialNumber = GETP_CHAR(manageableDeviceInfo, "SerialNumber");
    const char* ProductClass = GETP_CHAR(manageableDeviceInfo, "ProductClass");
    const char* Host = GETP_CHAR(manageableDeviceInfo, "Host");
    when_false((ManufacturerOUI && SerialNumber && ProductClass && Host), stop);

    inst = amxd_dm_findf(cwmp_plugin_get_dm(), "%s[ManufacturerOUI == '%s' && SerialNumber == '%s' && ProductClass == '%s'].", MANAGEABLE_DEVICE_PATH, ManufacturerOUI, SerialNumber, ProductClass);

    SAH_TRACEZ_INFO(ME, "Manageable device connected [%s] [%s] [%s] [%s]", ManufacturerOUI, SerialNumber, ProductClass, Host);
    if(inst) {
        const char* hosts = amxd_object_get_cstring_t(inst, "Host", NULL);
        when_null(hosts, stop);
        when_false((strstr(hosts, Host) == NULL), stop);
        amxc_string_setf(&new_host_val, "%s%s,", hosts, Host);
        amxd_trans_select_object(&trans, inst);
        amxd_trans_set_attr(&trans, amxd_tattr_change_ro, true);
        amxd_trans_set_value(cstring_t, &trans, "Host", amxc_string_get(&new_host_val, 0));
        when_failed_trace(amxd_trans_apply(&trans, cwmp_plugin_get_dm()), stop, ERROR, "Failed to Update ManageableDevice.Host");
    } else {
        //Create a new Manageable device
        amxc_string_setf(&new_host_val, "%s,", Host);
        amxd_trans_select_pathf(&trans, MANAGEABLE_DEVICE_PATH);
        amxd_trans_set_attr(&trans, amxd_tattr_change_ro, true);
        amxd_trans_add_inst(&trans, 0, NULL);

        amxd_trans_set_value(cstring_t, &trans, "ManufacturerOUI", ManufacturerOUI);
        amxd_trans_set_value(cstring_t, &trans, "ProductClass", ProductClass);
        amxd_trans_set_value(cstring_t, &trans, "SerialNumber", SerialNumber);
        amxd_trans_set_value(cstring_t, &trans, "Host", amxc_string_get(&new_host_val, 0));
        when_failed_trace(amxd_trans_apply(&trans, cwmp_plugin_get_dm()), stop, ERROR, "Failed to add a new ManageableDevice");
    }

    ret = 0;
stop:
    amxd_trans_clean(&trans);
    amxc_string_clean(&new_host_val);
    return ret;
}

static void cwmp_plugin_host_event(UNUSED const char* const sig_name,
                                   const amxc_var_t* const data,
                                   UNUSED void* const priv) {
    const char* objpath = GETP_CHAR(data, "path");
    bool active = GETP_BOOL(data, "parameters.Active.to");
    amxc_string_t host_path;
    amxc_string_init(&host_path, 0);
    char* dhclient = NULL;
    char* opt = NULL;
    int opt_tag = 0;
    amxc_var_t manageableDevice;
    amxc_var_init(&manageableDevice);
    amxc_var_set_type(&manageableDevice, AMXC_VAR_ID_HTABLE);
    amxc_string_setf(&host_path, "Device.%s", objpath);

    if(active) {
        manageable_device_get_dhclient(objpath, &dhclient);
        when_null(dhclient, stop);

        opt_tag = (strstr(dhclient, "DHCPv6") != NULL) ? 17 : 125;
        manageable_device_get_dhclient_option(dhclient, &opt);
        when_null(opt, stop);

        manageable_device_set_dhcp_options(&manageableDevice, opt, opt_tag);
        amxc_var_add_key(cstring_t, &manageableDevice, "Host", amxc_string_get(&host_path, 0));
        cwmp_plugin_add_manageable_device(&manageableDevice);
    } else {
        char* manageable_device_path = NULL;
        cwmp_plugin_search_for_manageable_device(amxc_string_get(&host_path, 0), &manageable_device_path);
        when_null(manageable_device_path, stop);
        cwmp_plugin_update_manageable_device(manageable_device_path, amxc_string_get(&host_path, 0));
        free(manageable_device_path);
    }
stop:
    free(dhclient);
    free(opt);
    amxc_string_clean(&host_path);
    amxc_var_clean(&manageableDevice);
}

static int cwmp_plugin_search_for_host(const char* dhcp_path, char** host_path) {
    amxc_var_t dhcp_object;
    amxc_var_init(&dhcp_object);
    amxc_var_t hosts_object;
    amxc_var_init(&hosts_object);
    int ret = -1;
    const char* mac = NULL;
    char* tmp_path = NULL;
    const amxc_htable_t* htable = NULL;
    amxb_bus_ctx_t* dhcp_bus_ctx = NULL;
    amxb_bus_ctx_t* host_bus_ctx = NULL;
    amxc_string_t client_path;
    amxc_string_init(&client_path, 0);
    int path_len = 0;

    when_null(dhcp_path, stop);
    path_len = strlen(dhcp_path);
    dhcp_bus_ctx = get_bus_ctx(dhcp_path);
    host_bus_ctx = amxb_be_who_has("Hosts.");
    when_null(host_bus_ctx, stop);

    if(strncmp((dhcp_path + (path_len - 7)), "Client.", 7) == 0) {
        tmp_path = strdup(dhcp_path);
    } else if(strncmp((dhcp_path + (path_len - 7)), "Option.", 7) == 0) {
        tmp_path = strndup(dhcp_path, path_len - 7);
    }
    when_null(tmp_path, stop);
    amxc_string_setf(&client_path, "%sChaddr", tmp_path);
    when_failed(amxb_get(dhcp_bus_ctx, amxc_string_get(&client_path, 0), 1, &dhcp_object, 10), stop);
    when_true(amxc_var_is_null(&dhcp_object), stop);

    amxc_string_clean(&client_path);
    amxc_string_setf(&client_path, "0.'%s'.Chaddr", tmp_path);
    mac = GETP_CHAR(&dhcp_object, amxc_string_get(&client_path, 0));
    when_null(mac, stop);

    when_failed(amxb_get(host_bus_ctx, HOSTS_PATH "*.", 1, &hosts_object, 10), stop);
    when_true(amxc_var_is_null(&hosts_object), stop);

    htable = amxc_var_constcast(amxc_htable_t, GETI_ARG(&hosts_object, 0));

    amxc_htable_iterate(hit, htable) {
        const char* key = amxc_htable_it_get_key(hit);
        amxc_var_t* host = amxc_var_from_htable_it(hit);
        amxc_var_log(host);
        const char* host_mac = GETP_CHAR(host, "PhysAddress");
        if(key && host_mac && (strcasecmp(host_mac, mac) == 0)) {
            *host_path = strdup(key);
            ret = 0;
            goto stop;
        }
    }

stop:
    amxc_var_clean(&dhcp_object);
    amxc_var_clean(&hosts_object);
    amxc_string_clean(&client_path);
    free(tmp_path);
    return ret;
}

static void cwmp_plugin_dhclient_event(UNUSED const char* const sig_name,
                                       const amxc_var_t* const data,
                                       UNUSED void* const priv) {
    const amxc_var_t* parameters = GETP_ARG(data, "parameters");
    const char* objpath = GETP_CHAR(data, "path");
    uint32_t option_tag = 0;
    const char* opt = NULL;
    char* host_path = NULL;
    amxc_string_t host;
    amxc_string_init(&host, 0);
    amxc_var_t manageableDevice;
    amxc_var_init(&manageableDevice);
    amxc_var_set_type(&manageableDevice, AMXC_VAR_ID_HTABLE);

    option_tag = (strstr(objpath, "DHCPv6") != NULL) ? 17 : 125;

    if(strncmp((objpath + (strlen(objpath) - 7)), "Client.", 7) == 0) {
        //DHCPv4Server.Pool.[1].Client. when client added
        amxc_var_t* dhcp_options = GETP_ARG(parameters, "Option");
        when_true(amxc_var_is_null(dhcp_options), stop);
        amxc_var_for_each(option, dhcp_options) {
            uint32_t tag = GET_UINT32(option, "Tag");
            if(tag == option_tag) {
                opt = GETP_CHAR(option, "Value");
            }
        }
    } else if(strncmp((objpath + (strlen(objpath) - 7)), "Option.", 7) == 0) {
        //DHCPv4Server.Pool.[1].Client.Option. when option added
        uint32_t tag = GET_UINT32(parameters, "Tag");
        if(tag == option_tag) {
            opt = GETP_CHAR(parameters, "Value");
        }
    }
    when_null(opt, stop);

    when_failed(cwmp_plugin_search_for_host(objpath, &host_path), stop);
    amxc_string_setf(&host, "Device.%s", host_path);
    manageable_device_set_dhcp_options(&manageableDevice, opt, option_tag);
    amxc_var_add_key(cstring_t, &manageableDevice, "Host", amxc_string_get(&host, 0));
    when_failed(cwmp_plugin_add_manageable_device(&manageableDevice), stop);

    if(cwmp_plugin_add_subscription(host_path,
                                    "notification in ['dm:object-changed'] && contains('parameters.Active')",
                                    cwmp_plugin_host_event)) {
        SAH_TRACEZ_ERROR(ME, "Failed to subscribe on %s", host_path);
    }

stop:
    amxc_string_clean(&host);
    amxc_var_clean(&manageableDevice);
    free(host_path);
}

void cwmp_plugin_manageableDevice_init(void) {
    if(cwmp_plugin_add_subscription(DHCP4_CLIENTS_PATH, FILTER_INSTANCE_ADDED, cwmp_plugin_dhclient_event)) {
        SAH_TRACEZ_ERROR(ME, "Failed to create subscription for %s", DHCP4_CLIENTS_PATH);
    }

    if(cwmp_plugin_add_subscription(DHCP6_CLIENTS_PATH, FILTER_INSTANCE_ADDED, cwmp_plugin_dhclient_event)) {
        SAH_TRACEZ_ERROR(ME, "Failed to create subscription for %s", DHCP6_CLIENTS_PATH);
    }
}

void cwmp_plugin_manageableDevice_clean(void) {
    cwmp_plugin_del_subscription(DHCP4_CLIENTS_PATH, cwmp_plugin_dhclient_event);
    cwmp_plugin_del_subscription(DHCP6_CLIENTS_PATH, cwmp_plugin_dhclient_event);
}