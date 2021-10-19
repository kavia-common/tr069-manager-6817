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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "cwmp_plugin.h"

static cwmp_plugin_app_t app;
static const char* AMXB_URI = "AMXB_URI";


static void cwmp_plugin_init(amxd_dm_t* dm, amxo_parser_t* parser) {
    /* SAH_TRACEZ_INFO(ME, "**************************************"); */
    /* SAH_TRACEZ_INFO(ME, "*        cwmp_plugin started          *"); */
    /* SAH_TRACEZ_INFO(ME, "**************************************"); */
    app.dm = dm;
    app.parser = parser;
    app.amxb_bus_ctx = NULL;
    app.dns_resolv_invoke = NULL;
    char* amxb_uri = getenv(AMXB_URI);

    if(amxb_uri && (amxb_connect(&app.amxb_bus_ctx, amxb_uri) == AMXB_STATUS_OK)) {
        amxb_new_invoke(&app.dns_resolv_invoke, app.amxb_bus_ctx, "DNS", NULL, "resolvURI");
    } else {
        fprintf(stderr, "Couldn't connect to amxb bus %s\n", amxb_uri);
    }
    // Load previous config
    amxo_parser_parse_file(parser, GET_CHAR(&parser->config, "save_file"), (amxd_object_t*) dm);
}

amxd_dm_t* cwmp_plugin_get_dm(void) {
    return app.dm;
}

amxo_parser_t* cwmp_plugin_get_parser(void) {
    return app.parser;
}

amxc_var_t* cwmp_plugin_get_config(void) {
    return &(app.parser->config);
}

amxb_invoke_t* cwmp_plugin_get_dns_resolv_invoke(void) {
    return app.dns_resolv_invoke;
}

amxb_bus_ctx_t* cwmp_plugin_get_bus(void) {
    return app.amxb_bus_ctx;
}

int _cwmp_plugin_main(int reason, amxd_dm_t* dm, amxo_parser_t* parser) {

    int retval = 0;

    //SAH_TRACEZ_INFO(ME, "cwmp_plugin_main, reason: %i", reason);
    switch(reason) {
    case 0: // START
        cwmp_plugin_init(dm, parser);
        start_cwmpd();
        break;
    case 1: // STOP
        app.dm = NULL;
        app.parser = NULL;
        app.dns_resolv_invoke = NULL;
        app.amxb_bus_ctx = NULL;
        stop_cwmpd();
        break;
    default:
        retval = -1;
        break;
    }

    return retval;
}

