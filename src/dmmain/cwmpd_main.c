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

/* open ssl header */
#ifdef OPEN_SSL_SUPPORT
#include <openssl/rand.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/evp.h>
#include <openssl/bio.h>

#include <tr069key/tr069key.h>
#endif
#include <event2/event.h>
#include <amxc/amxc.h>
#include <amxp/amxp.h>
#include <amxd/amxd_dm.h>
#include <amxb/amxb.h>
#include "dmmain/cwmpd.h"
#include "dmcom/dm_com.h"
#include "dmengine/DM_ENG_Device.h"
/*PID FILE*/
#ifndef CFG_PID_FILE
#define CFG_PID_FILE "/var/run/cwmpd.pid"
#endif

application_t cwmp_app;// app instance
static struct lws_context_creation_info server_info;
static struct lws_context_creation_info client_info;
static struct lws_context* server_ctx = NULL;
static struct lws_context* client_ctx = NULL;
amxb_bus_ctx_t* sys_bus_ctx = NULL;
amxb_bus_ctx_t* acs_bus_ctx = NULL;


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
           "  -t        --trustedCA      Trusted CA certificates\n"
           "  -d        --da_path        device adapter path\n"
           "  -s        --sahtracelvl    set sahtrace level of log\n");
    exit(0);
}

static void cwmp_app_configureDefaults() {
    cwmp_app.name = (char*) "cwmpd";
    cwmp_app.daemonize = 1;

    cwmp_app.pidFile = (char*) getenv("CWMPD_PID_FILE");
    if(cwmp_app.pidFile == NULL) {
        cwmp_app.pidFile = (char*) CFG_PID_FILE;
    }
    /* defaults for sahtrace */
    cwmp_app.traceLevel = 200;
    cwmp_app.traceType = TRACE_TYPE_SYSLOG;
    cwmp_app.cacheFile = (char*) "/tmp/cwmpd_cache.txt";
    /* ssl default */
#ifdef CONFIG_SAH_SERVICES_TR069_CERTIFICATE_NO_PEM
    cwmp_app.trustedCA = NULL;
    cwmp_app.ssl_priv_key = NULL;
#else
    cwmp_app.trustedCA = (char*) "/etc/ca.pem";
    cwmp_app.ssl_priv_key = (char*) "/etc/ssl_priv_key.key";
#endif
}


static void cwmp_app_configureOptions(int argc, char* argv[]) {
    while(1) {
        int option_index = 0;
        static struct option long_options[] = {
            {"", 1, 0, 0},
            {"foreground", 1, 0, 'f'},
            {"help", 1, 0, 'h'},
            {"trustedCA", 1, 0, 't'},
            {"da_path", 1, 0, 'd'},
            {"sahtracelvl", 1, 0, 's'},
            {"pidfile", 1, 0, 'p'},
            {0, 0, 0, 0}
        };

        int c = getopt_long(argc, argv, "hDfit:a:s:p:q:", long_options, &option_index);
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
        case 't':
            cwmp_app.trustedCA = optarg;
            break;
        case 'a':
            cwmp_app.da_path = optarg;
            break;
        case 'q':
            cwmp_app.cacheFile = optarg;
            break;
        case 'p':
            cwmp_app.pidFile = optarg;
            break;
        case 's':
            cwmp_app.traceLevel = atoi(optarg);
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
    // Since lws dosen't support dynamic vhost creation/deletion so we
    // need to destroy the whole server context and then create a new one

    /* Stop http Server */
    if(cwmp_server_stop(server_ctx) != cwmp_status_ok) {
        SAH_TRACEZ_WARNING("CWMPD", "failed to stop HTTP server");
    }
    // configuration maybe updatet, reinitialize
    memset(&server_info, 0, sizeof server_info);
    /* Init http Server */
    if(cwmp_server_init(&server_info) != cwmp_status_ok) {
        SAH_TRACEZ_ERROR("CWMPD", "failed to initialize HTTP server");
        return cwmp_status_ko;
    }

    void* foreign_loops[1] = { cwmp_evlp_get() };
    /* Start http Server */
    if(cwmp_server_start(&server_info, foreign_loops, server_ctx) != cwmp_status_ok) {
        SAH_TRACEZ_ERROR("CWMPD", "failed to start HTTP server");
        return cwmp_status_ko;
    }

    return cwmp_status_ok;
}

static cwmp_status_t cwmp_app_clean() {
    cwmp_status_t status = cwmp_status_ok;
    if(cwmp_client_stop(client_ctx) != cwmp_status_ok) {
        SAH_TRACEZ_ERROR("CWMPD", "failed to stop HTTP client");
        status = cwmp_status_ko;
    }
    if(cwmp_server_stop(server_ctx) != cwmp_status_ok) {
        SAH_TRACEZ_ERROR("CWMPD", "failed to stop HTTP server");
        status = cwmp_status_ko;
    }
    if(cwmp_evlp_clean() != cwmp_status_ok) {
        SAH_TRACEZ_ERROR("CWMPD", "eventloop cleanup failed");
        status = cwmp_status_ko;
    }
    // return no value ?
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
        if(cwmp_server_stop(server_ctx) != cwmp_status_ok) {
            SAH_TRACEZ_ERROR("CWMPD", "Stopping HTTP server failed");
        }
    } else if(strcmp(eventType, EVENT_ENG_SRV_START) == 0) {
        if(server_ctx != NULL) {
            SAH_TRACEZ_WARNING("CWMPD", "Attemp to Start a new HTTP server while another is running");
            // Stop the other first , otherwise we will fail to bind to ip:port
            if(cwmp_server_stop(server_ctx) != cwmp_status_ok) {
                SAH_TRACEZ_ERROR("CWMPD", "Stopping HTTP server failed");
            }
        }
        // Configuration maybe changed, reinitialize
        memset(&server_info, 0, sizeof server_info);
        if(cwmp_server_init(&server_info) != cwmp_status_ok) {
            SAH_TRACEZ_WARNING("CWMPD", "HTTP server initialization failed");
        }
        void* foreign_loops[1] = { cwmp_evlp_get() };
        /* Start http Server */
        if(cwmp_server_start(&server_info, foreign_loops, server_ctx) != cwmp_status_ok) {
            SAH_TRACEZ_ERROR("CWMPD", "Starting HTTP server failed");
            // if we reach this point propably cwmpd need a restart exit
            // the app as it's already dead and can't answer to ACS requests
            raise(SIGTERM);
        }
    } else if(strcmp(eventType, EVENT_ENG_CLEAR_ACS_IP) == 0) {
        cwmp_client_clear_ACSIP();// clear acs ip
    } else {
        SAH_TRACEZ_INFO("CWMPD", "unhandled engine event [%s]", eventType);
    }
    return DM_ENG_COMPLETED;
}

void init_sahtrace() {
    sahTraceOpen("cwmpd", cwmp_app.traceType);
    sahTraceSetLevel(cwmp_app.traceLevel);
    //TODO! set zone level according to odl?
    sahTraceAddZone(cwmp_app.traceLevel, "CWMPD");
    sahTraceAddZone(cwmp_app.traceLevel, "DM_DA");
    sahTraceAddZone(cwmp_app.traceLevel, "DM_ENGINE");
    sahTraceAddZone(cwmp_app.traceLevel, "DM_COM");
    sahTraceAddZone(cwmp_app.traceLevel, "DM_COMMON");
    sahTraceAddZone(cwmp_app.traceLevel, "DM_HS");
}

int main(int argc, char* argv[]) {
    int rc = 1;

    signal(SIGINT, cwmp_app_handleSignal);
    signal(SIGTERM, cwmp_app_handleSignal);

    /* Configure APP*/
    cwmp_app_configureDefaults();
    cwmp_app_configureOptions(argc, argv);

    init_sahtrace();

    if(!DM_ENG_Device_Load(cwmp_app.da_path)) {
        SAH_TRACEZ_ERROR("CWMPD", "Failed to load adapter plugin");
        return rc;
    }

    if(DM_COM_INIT(cwmp_timer_start, cwmp_timer_stop, cwmp_timer_remainingTime,
                   cwmp_app_engineEventHandler, cwmp_app.cacheFile,
                   (void**) &sys_bus_ctx, (void**) &acs_bus_ctx) != 0) {
        SAH_TRACEZ_ERROR("CWMPD", "Failed to initialize DM_COM");
        return rc;
    }

    //Init evlp
    if(cwmp_evlp_create(acs_bus_ctx, sys_bus_ctx) != cwmp_status_ok) {
        SAH_TRACEZ_ERROR("CWMPD", "Failed to initialize evlp");
        return rc;
    }

    void* foreign_loops[1] = { cwmp_evlp_get() };
    memset(&server_info, 0, sizeof server_info);
    memset(&client_info, 0, sizeof client_info);

    /* Daemonize if needed */
    if(cwmp_app.daemonize) {
        if(daemon(0, 0) < 0) {
            SAH_TRACEZ_ERROR("CWMPD", "unable to daemonize: %s", strerror(errno));
            goto error;
        }
    }

    /* Init http Server */
    if(cwmp_server_init(&server_info) != cwmp_status_ok) {
        SAH_TRACEZ_ERROR("CWMPD", "Failed to initialize HTTP server");
        goto error;
    }

    /* Start http Server */
    if(cwmp_server_start(&server_info, foreign_loops, server_ctx) != cwmp_status_ok) {
        SAH_TRACEZ_ERROR("CWMPD", "Failed to start HTTP server");
        goto error;
    }

    /* Init http Client */
    if(cwmp_client_init(&client_info) != cwmp_status_ok) {
        SAH_TRACEZ_ERROR("CWMPD", "HTTP client initialization failed");
        goto error;
    }

    /* Start http Server */
    if(cwmp_client_start_session(&client_info, foreign_loops, client_ctx) != cwmp_status_ok) {
        SAH_TRACEZ_ERROR("CWMPD", "failed to start HTTP client");
        goto error;
    }

    /* Start the main loop */
    SAH_TRACEZ_INFO("CWMPD", "starting cwmpd main loop");

    /* Start the main event loop and wait for events*/
    if(cwmp_evlp_start() != cwmp_status_ok) {
        SAH_TRACEZ_ERROR("CWMPD", "eventloop start failed going to exit");
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

