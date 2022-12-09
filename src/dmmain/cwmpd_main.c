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
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <event2/event.h>
#include <arpa/inet.h>

#include "dmmain/cwmpd.h"
#include <dmcom/dm_com.h>
#include <dmengine/DM_ENG_Device.h>

#include <debug/sahtrace.h>
#include <debug/sahtrace_macros.h>
#include <amxc/amxc_macros.h>

/*PID FILE*/
#ifndef CFG_PID_FILE
#define CFG_PID_FILE "/var/run/cwmpd.pid"
#endif

static application_t cwmp_app;// app instance
static amxb_bus_ctx_t* sys_bus_ctx = NULL;
static amxb_bus_ctx_t* acs_bus_ctx = NULL;
static amxo_parser_t parser;
static amxd_object_t* root = NULL;

bool is_ipaddr(const char* ip) {
    struct in6_addr result;
    when_null(ip, exit);

    if(inet_pton(AF_INET, ip, &result) || inet_pton(AF_INET6, ip, &result)) {
        return true;
    }
exit:
    return false;
}

static void cwmp_app_handleSignal(int signal) {
    SAH_TRACEZ_WARNING("CWMPD", "handling signal %d", signal);
    if((signal == SIGINT) || (signal == SIGTERM)) {
        cwmp_app.state = EXIT;
        cwmp_evlp_stop();
    }
}

static void app_usage() {
    printf("cwmpd [-h] [-f] [-v] [-d] [-s]\n"
           "options:\n"
           "  -h        --help           this help screen\n"
           "  -o        --public-port    the port to listen on (public)\n"
           "  -f        --foreground     do not daemonize, log to stdout\n"
           "  -c        --odl-config     odl config file\n");
    exit(0);
}

void cwmp_add_sahtrace_zones(amxc_var_t* trace_zones) {
    SAH_TRACE_IN();
    if(amxc_var_type_of(trace_zones) == AMXC_VAR_ID_HTABLE) {
        const amxc_htable_t* zones = amxc_var_constcast(amxc_htable_t, trace_zones);
        amxc_htable_for_each(it, zones) {
            const char* zone_name = amxc_htable_it_get_key(it);
            amxc_var_t* var_level = amxc_var_from_htable_it(it);
            uint32_t zone_level = amxc_var_dyncast(uint32_t, var_level);
            sahTraceAddZone(zone_level, zone_name);
        }
    }
    SAH_TRACE_OUT();
}

static cwmp_status_t cwmp_app_parse_config(void) {
    cwmp_status_t ret = cwmp_status_ko;
    amxo_parser_init(&parser);
    amxd_object_new(&root, amxd_object_singleton, "root");
    int retval = amxo_parser_parse_file(&parser, cwmp_app.odl_config, root);
    when_false_trace(retval != -1, exit, ERROR, "CWMPD: ODL parsing failed - message = %s", amxo_parser_get_message(&parser));
    amxc_var_t* config = &parser.config;
    when_null_trace(config, exit, ERROR, "CWMPD: cwmpd configs should not be NULL");

    // tr069-service configs
    amxc_var_t* tr069_config = amxc_var_get_key(config, "tr069-service", AMXC_VAR_FLAG_DEFAULT);
    cwmp_app.da_path = GETP_CHAR(tr069_config, "cwmpd_adapter_path");
    cwmp_app.persistent_rpc_path = GETP_CHAR(tr069_config, "cwmpd_persistent_rpc_path");
    cwmp_app.trustedCA = GETP_CHAR(tr069_config, "cwmpd_certs_file");
    cwmp_app.pidFile = GETP_CHAR(tr069_config, "cwmpd_pid_file");

    // Set tracelevel
    amxc_var_t* trace = amxc_var_get_key(config, "sahtrace", AMXC_VAR_FLAG_DEFAULT);
    cwmp_app.traceLevel = GET_UINT32(trace, "level");
    sahTraceSetLevel(cwmp_app.traceLevel);

    // Add sahtrace zones
    amxc_var_t* trace_zones = amxc_var_get_key(config, "trace-zones", AMXC_VAR_FLAG_DEFAULT);
    if(trace_zones) {
        cwmp_add_sahtrace_zones(trace_zones);
    }

    // ODL parsing config OK
    ret = cwmp_status_ok;
exit:
    amxd_object_delete(&root);
    return ret;
}

static void cwmp_app_configureDefaults() {
    cwmp_app.name = "cwmpd";
    cwmp_app.daemonize = 1;

    cwmp_app.pidFile = getenv("CWMPD_PID_FILE");
    if(cwmp_app.pidFile == NULL) {
        cwmp_app.pidFile = CFG_PID_FILE;
    }
    /* defaults for sahtrace */
    cwmp_app.traceLevel = 200;
    cwmp_app.traceType = TRACE_TYPE_SYSLOG;
    cwmp_app.persistent_rpc_path = "/tmp/";
    /* ssl default */
#ifdef CONFIG_SAH_AMX_TR069_MANAGER_CERTIFICATE_NO_PEM
    cwmp_app.trustedCA = NULL;
    cwmp_app.ssl_priv_key = NULL;
#else
    cwmp_app.trustedCA = "/etc/ca.pem";
    cwmp_app.ssl_client_priv_key = NULL;
    cwmp_app.ssl_client_cert = NULL;
#endif
}


static void cwmp_app_configureOptions(int argc, char* argv[]) {
    while(1) {
        int option_index = 0;
        static struct option long_options[] = {
            {"", 1, 0, 0},
            {"foreground", 1, 0, 'f'},
            {"help", 1, 0, 'h'},
            {"odl-config", 1, 0, 'c'},
            {0, 0, 0, 0}
        };

        int c = getopt_long(argc, argv, "hDfit:c:", long_options, &option_index);
        if(c == -1) {
            break;
        }

        switch(c) {
        case 'f':
            cwmp_app.traceType = TRACE_TYPE_STDOUT;
            cwmp_app.daemonize = 0;
            break;
        case 'h':
            app_usage();
            break;
        case 'c':
            cwmp_app.odl_config = optarg;
            break;
        case 'D':
            //Do not deamonize cwmpd is
            //controlled by amxp_proc_t
            cwmp_app.traceType = TRACE_TYPE_SYSLOG;
            cwmp_app.daemonize = 0;
            break;
        }
    }
}

application_t cwmp_app_getconf(void) {
    return cwmp_app;
}

static cwmp_status_t cwmp_app_http_server_restart() {
    /* Stop http Server */
    if(cwmp_server_stop() != cwmp_status_ok) {
        SAH_TRACEZ_WARNING("CWMPD", "failed to stop HTTP server");
    }
    /* Init http Server */
    if(cwmp_server_init() != cwmp_status_ok) {
        SAH_TRACEZ_ERROR("CWMPD", "failed to initialize HTTP server");
        return cwmp_status_ko;
    }
    /* Start http Server */
    if(cwmp_server_start() != cwmp_status_ok) {
        SAH_TRACEZ_ERROR("CWMPD", "failed to start HTTP server");
        return cwmp_status_ko;
    }

    return cwmp_status_ok;
}

static cwmp_status_t cwmp_app_clean() {
    cwmp_status_t status = cwmp_status_ok;
    if(cwmp_client_stop() != cwmp_status_ok) {
        SAH_TRACEZ_ERROR("CWMPD", "failed to stop HTTP client");
        status = cwmp_status_ko;
    }
    if(cwmp_server_stop() != cwmp_status_ok) {
        SAH_TRACEZ_ERROR("CWMPD", "failed to stop HTTP server");
        status = cwmp_status_ko;
    }
    if(cwmp_dns_stop() != cwmp_status_ok) {
        SAH_TRACEZ_ERROR("CWMPD", "DNS cleanup failed");
        status = cwmp_status_ko;
    }
    if(cwmp_evlp_clean() != cwmp_status_ok) {
        SAH_TRACEZ_ERROR("CWMPD", "eventloop cleanup failed");
        status = cwmp_status_ko;
    }
    DM_COM_STOP();
    DM_ENG_Device_Unload();

    return status;
}

int cwmp_app_engineEventHandler(const char* eventType) {
    SAH_TRACEZ_INFO("CWMPD", "Engine event Handler [%s]", eventType);
    if(strcmp(eventType, EVENT_ENG_SRV_RESTART) == 0) {
        if(cwmp_app_http_server_restart() != cwmp_status_ok) {
            SAH_TRACEZ_ERROR("CWMPD", "Restarting HTTP server failed");
            //DIE HERE
            raise(SIGTERM);
        }
    } else if(strcmp(eventType, EVENT_ENG_SRV_STOP) == 0) {
        if(cwmp_server_stop() != cwmp_status_ok) {
            SAH_TRACEZ_ERROR("CWMPD", "Stopping HTTP server failed");
        }
    } else if(strcmp(eventType, EVENT_ENG_SRV_START) == 0) {
        if(cwmp_server_init() != cwmp_status_ok) {
            SAH_TRACEZ_WARNING("CWMPD", "HTTP server initialization failed");
        }
        /* Start http Server */
        if(cwmp_server_start() != cwmp_status_ok) {
            SAH_TRACEZ_ERROR("CWMPD", "Starting HTTP server failed");
            // if we reach this point propably cwmpd need a restart exit
            // the app as it's already dead and can't answer to ACS requests
            raise(SIGTERM);
        }
    } else if(strcmp(eventType, EVENT_ENG_CLEAR_ACS_IP) == 0) {
        cwmp_client_clear_ACSIP();// clear acs ip
    } else if(strcmp(eventType, EVENT_ENG_URL_CHANGED) == 0) {
        SAH_TRACEZ_ERROR("CWMPD", "ACS URL changed : resolve new address");
        cwmp_dns_resolve(true);
    } else {
        SAH_TRACEZ_INFO("CWMPD", "unhandled engine event [%s]", eventType);
    }
    return DM_ENG_COMPLETED;
}


static cwmp_status_t cwmp_app_settings(int argc, char** argv) {
    cwmp_status_t rc = cwmp_status_ko;
    signal(SIGINT, cwmp_app_handleSignal);
    signal(SIGTERM, cwmp_app_handleSignal);

    /* Configure APP*/
    cwmp_app_configureDefaults();
    sahTraceOpen("cwmpd", cwmp_app.traceType);
    sahTraceSetLevel(cwmp_app.traceLevel);
    cwmp_app_configureOptions(argc, argv);

    if(cwmp_status_ko == cwmp_app_parse_config()) {
        SAH_TRACEZ_ERROR("CWMPD", "Failed to parse cwmpd config");
        amxo_parser_clean(&parser);
        return rc;
    }
    /* Daemonize if needed */
    if(cwmp_app.daemonize) {
        if(daemon(0, 0) < 0) {
            SAH_TRACEZ_ERROR("CWMPD", "unable to daemonize: %s", strerror(errno));
            return rc;
        }
    }
    rc = cwmp_status_ok;
    return rc;
}

static cwmp_status_t cwmp_app_init_dmengine() {
    cwmp_status_t rc = cwmp_status_ko;
    if(!DM_ENG_Device_Load(cwmp_app.da_path)) {
        SAH_TRACEZ_ERROR("CWMPD", "Failed to load adapter plugin");
        return rc;
    }
    //Connect to Data-model
    if(DM_COM_DMCONNECT(cwmp_app.persistent_rpc_path, (void**) &sys_bus_ctx, (void**) &acs_bus_ctx) != 0) {
        SAH_TRACEZ_ERROR("CWMPD", "Failed to initialize DM_COM");
        return rc;
    }

    //Init evlp
    if(cwmp_evlp_create(acs_bus_ctx, sys_bus_ctx) != cwmp_status_ok) {
        SAH_TRACEZ_ERROR("CWMPD", "Failed to initialize evlp");
        return rc;
    }
    rc = cwmp_status_ok;
    return rc;
}

static cwmp_status_t cwmp_app_init_services() {
    cwmp_status_t rc = cwmp_status_ko;
    /* Init http Server */
    if(cwmp_server_init() != cwmp_status_ok) {
        SAH_TRACEZ_ERROR("CWMPD", "Failed to initialize HTTP server");
        goto error;
    }

    /* Start http Server */
    if(cwmp_server_start() != cwmp_status_ok) {
        SAH_TRACEZ_ERROR("CWMPD", "Failed to start HTTP server");
        goto error;
    }
    /* Init DNS service */
    if(cwmp_dns_init() != cwmp_status_ok) {
        SAH_TRACEZ_ERROR("CWMPD", "Failed to initialize DNS resolver");
        goto error;
    }

    /* Resolve ACS URL before starting the cwmp_client*/
    if(cwmp_dns_resolve(false) != cwmp_status_ok) {
        SAH_TRACEZ_INFO("CWMPD", "Failed to start DNS resolution");
        goto error;
    }

    /* Init http Client */
    if(cwmp_client_init() != cwmp_status_ok) {
        SAH_TRACEZ_ERROR("CWMPD", "HTTP client initialization failed");
        goto error;
    }
    rc = cwmp_status_ok;
error:
    return rc;
}

static cwmp_status_t cwmp_app_enable_notifications() {
    cwmp_status_t rc = cwmp_status_ok;

    if(DM_COM_INIT(cwmp_timer_start,
                   cwmp_timer_stop,
                   cwmp_timer_remainingTime,
                   cwmp_app_engineEventHandler) != 0) {
        SAH_TRACEZ_ERROR("CWMPD", "Failed to initialize DM_COM");
        rc = cwmp_status_ko;
    }

    /* Start the main loop */
    SAH_TRACEZ_INFO("CWMPD", "starting cwmpd main loop");

    /* Start the main event loop and wait for events*/
    if(cwmp_evlp_start() != cwmp_status_ok) {
        SAH_TRACEZ_ERROR("CWMPD", "eventloop start failed going to exit");
        rc = cwmp_status_ko;
    }
    return rc;
}

int main(int argc, char* argv[]) {
    int rc = 1;

    if(cwmp_status_ko == cwmp_app_settings(argc, argv)) {
        return rc;
    }

    if(cwmp_status_ko == cwmp_app_init_dmengine()) {
        return rc;
    }

    if(cwmp_app_init_services() != cwmp_status_ok) {
        goto error;
    }

    if(cwmp_app_enable_notifications() != cwmp_status_ok) {
        goto error;
    }

error:

    if(cwmp_app_clean() != cwmp_status_ok) {
        cwmp_app.state = ERROR;
    }
    /* exit the app*/
    rc = (cwmp_app.state == ERROR);
    SAH_TRACE_APP_INFO("CWMPD is exiting with code [%d]", rc);
    return rc;
}

