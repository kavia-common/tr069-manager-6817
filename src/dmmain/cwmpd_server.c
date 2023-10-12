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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>
#include <ctype.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>

#include <debug/sahtrace.h>
#include "dmmain/cwmpd.h"
#include <dmcom/dm_com_digest.h>
#include <dmengine/DM_ENG_RPCInterface.h>
#include <dmcommon/DM_GlobalDefs.h>
#include <dmcom/dm_com.h>

// Size of the CPE URL
#define CPE_URL_SIZE (16)

#define NONCESIZE  (34)
static char randomNonceStr[NONCESIZE + 1];  // Max(NONCESIZE, OPAQUESIZE) + 1
static char randomOpaqueStr[NONCESIZE + 1]; // Max(NONCESIZE, OPAQUESIZE) + 1

extern dm_com_struct g_DmComData;

amxc_string_t buffer;

char* g_randomCpeUrl = NULL;

static struct lws_context_creation_info lws_server_ctx_info;
static struct lws_context* lws_server_ctx = NULL; /* server lws context */
static struct lws_vhost* lws_server_vhost = NULL; /* server vhost */

static int server_getHWAddressFromIp(const char* ip, char* macbuf, size_t buflen) {
    char ifbuf[1024];
    int total;
    struct ifconf ifc;
    struct ifreq* ifr;
    int sock;
    int i;
    char ipbuf[INET6_ADDRSTRLEN];

    ipbuf[0] = '\0';
    macbuf[0] = 0;
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock < 0) {
        SAH_TRACEZ_ERROR("CWMPD", "cannot create socket");
        return 1;
    }

    ifc.ifc_len = sizeof(ifbuf);
    ifc.ifc_buf = ifbuf;
    if(ioctl(sock, SIOCGIFCONF, &ifc) < 0) {
        SAH_TRACEZ_ERROR("CWMPD", "ioctl failed");
        close(sock);
        return 1;
    }

    ifr = ifc.ifc_req;
    total = ifc.ifc_len / sizeof(struct ifreq);
    for(i = 0; i < total; i++) {
        struct ifreq* item = &ifr[i];
        struct sockaddr_in* sa = (struct sockaddr_in*) &item->ifr_addr;
        switch(sa->sin_family) {
        case AF_INET:
        case AF_INET6:
            if(inet_ntop(sa->sin_family, &sa->sin_addr, ipbuf, sizeof(ipbuf)) == NULL) {
                close(sock);
                return -1;
            }
            break;
        default:
            break;
        }
        if(strcmp(ipbuf, ip) == 0) {
            if(ioctl(sock, SIOCGIFHWADDR, item) < 0) {
                SAH_TRACEZ_ERROR("CWMPD", "ioctl(SIOCGIFHWADDR) failed");
                close(sock);
                return 1;
            }
            snprintf(macbuf, buflen, "%02hhx:%02hhx:%02hhx:%02hhx:%02x:%02hhx\n",
                     item->ifr_hwaddr.sa_data[0], item->ifr_hwaddr.sa_data[1],
                     item->ifr_hwaddr.sa_data[2], item->ifr_hwaddr.sa_data[3],
                     item->ifr_hwaddr.sa_data[4], item->ifr_hwaddr.sa_data[5]);
        }
    }
    close(sock);
    return 0;
}

static char* server_generateMacBasedPath(const char* ip) {
    char token[] = "ABCDE67FGHqrtuvwSTU48VWabcdefIJKL39MNPQRghijkmnpxyz2";
    unsigned char nb_tokens = strlen(token);
    unsigned short rnd_idx = 59;
    char* path = (char*) calloc(1, sizeof(char) * (CPE_URL_SIZE + 1));
    char macAddress[40] = {0};
    char* seed = macAddress;
    char tmp[40] = {0};

    // Use MACAddress as Connection Request Path
    memset(macAddress, 0, sizeof(macAddress));
    server_getHWAddressFromIp(ip, macAddress, sizeof(macAddress));
    sprintf(tmp, "%.2X:%.2X:%.2X:%.2X:%.2X:%.2X", macAddress[0], macAddress[1], macAddress[2], macAddress[3], macAddress[4], macAddress[5]);
    SAH_TRACEZ_INFO("CWMPD", "MACAddress: %s", tmp);

    for(uint i = 0; i < CPE_URL_SIZE; i++) {
        if(!*seed) { // end of seed string reached, recommence from start
            seed = macAddress;
        }
        rnd_idx = (rnd_idx << 1) ^ *seed++;
        path[i] = token[rnd_idx % nb_tokens];
    }
    return path;
}

// Create the randomly chosen CPE URL (This URL is provided in the inform message
// and is used by the ACS to connect to the CPE HTTP Server). The Randomly Chosen
// URL must be used by the DM_ENGINE to build the inform message.
static int cwmp_server_create_url() {
    char* random_cpe_url = NULL;
    char* url_env = getenv("TR069_URL_PATH");
    if(url_env) {
        random_cpe_url = strdup(url_env);
    } else {
        char* pathType = NULL;
        if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_CONNECTIONREQUESTPATHTYPE, &pathType) != 0) {
            SAH_TRACEZ_ERROR("CWMPD", "Cannot fetch DM_ENG_CONNECTIONREQUESTPATHTYPE");
            return -1;
        }
        if(strcmp(pathType, "Fixed-Default") == 0) {
            if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_CONNECTIONREQUESTPATH, &random_cpe_url) != 0) {
                SAH_TRACEZ_ERROR("CWMPD", "Cannot fetch DM_ENG_CONNECTIONREQUESTPATH");
                return -1;
            }
        } else if(strncmp(pathType, "Random", strlen("Random")) == 0) {
            random_cpe_url = (char*) malloc(CPE_URL_SIZE + 1);
            _generateRandomString(random_cpe_url, CPE_URL_SIZE);
        } else if(strcmp(pathType, "Fixed-MacBased") == 0) {
            char* host = NULL;
            if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_LOCALIPADDRESS, &host) != 0) {
                SAH_TRACEZ_ERROR("CWMPD", "Cannot fetch the local ip address");
                return -1;
            }
            random_cpe_url = server_generateMacBasedPath(host);
            free(host);
        } else {
            SAH_TRACEZ_ERROR("CWMPD", "Not a valid path type, generating random url");
            random_cpe_url = (char*) malloc(CPE_URL_SIZE + 1);
            _generateRandomString(random_cpe_url, CPE_URL_SIZE);
        }

        free(pathType);
    }

    if(DM_ENG_SetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_CONNECTIONREQUESTPATH, random_cpe_url) != 0) {
        SAH_TRACEZ_ERROR("CWMPD", "failed to set the random path in the datamodel");
        free(random_cpe_url);
        return -1;
    }
    free(g_randomCpeUrl);
    g_randomCpeUrl = strdup(random_cpe_url);
    SAH_TRACEZ_INFO("CWMPD", "CPE URL: %s", g_randomCpeUrl);
    free(random_cpe_url);
    return 0;
}

// 3.2.2: The Connection Request MUST use an HTTP 1.1 GET to a specific URL designated by the CPE. The
//URL value is available as read-only Parameter on the CPE. The path of this URL value SHOULD be
//randomly generated by the CPE so that it is unique per CPE.
static cwmp_status_t cwmp_server_validate_uri(struct lws* wsi, const char* requested_uri) {

    if(!lws_hdr_total_length(wsi, WSI_TOKEN_GET_URI) || !lws_hdr_total_length(wsi, WSI_TOKEN_HTTP)) {
        return cwmp_status_ko;
    }
    //+1 because we need to skip the initial slash
    if(( requested_uri == NULL) || ( g_randomCpeUrl == NULL) || ( strcmp(requested_uri + 1, g_randomCpeUrl) != 0)) {
        SAH_TRACEZ_ERROR("CWMPD", "Not a valid path (%s != %s)", requested_uri + 1, g_randomCpeUrl);
        return cwmp_status_ko;
    }
    return cwmp_status_ok;
}

// 3.2.2: If the CPE is already in a session with the ACS when it receives one or more Connection Requests, it
//        MUST NOT terminate that session prematurely as a result. The CPE MUST instead take one of the
//        following alternative actions:
//        (a)  Reject each Connection Request by responding with an HTTP 503 status code (Service
//             Unavailable). In this case, the CPE SHOULD NOT include the HTTP Retry-After header in the response.
//        (b)  Following the completion of the session, initiate exactly one new session (regardless of how many
//             Connection Requests had been received during the previous session) in which it includes the
//             “6 CONNECTION REQUEST” EventCode in the Inform. In this case, the CPE MUST initiate
//             the session immediately after the existing session is complete and all changes from that session
//             have been applied.
//       This requirement holds for Connection Requests received any time during the interval that the CPE
//       considers itself in a session, including the period in which the CPE is in the process of establishing the session.
// 3.2.2: The CPE SHOULD restrict the number of Connection Requests it accepts during a given period of
//        time in order to further reduce the possibility of a denial of service attack. If the CPE chooses to reject
//        a Connection Request for this reason, the CPE MUST respond to that Connection Request with an
//        HTTP 503 status code (Service Unavailable). In this case, the CPE SHOULD NOT include the HTTP
//        Retry-After header in the response.
static cwmp_status_t cwmp_server_check_availability() {
    // we currently support (b), remove the comments to support option (a)
#if 0
    if(g_DmComData.bSession == true) { // we implement action (a)
        SAH_TRACEZ_INFO("CWMPD", "Session is active, return HTTP 503");
        return cwmp_status_ko;
    }
#endif

    if(cwmp_server_maxConnectionsReached()) {
        SAH_TRACEZ_WARNING("CWMPD", "Maximum number of connections reached return HTTP 503");
        return cwmp_status_ko;
    }
    cwmp_server_maxConnectionsAdd(); // add a new entry in the list
    return cwmp_status_ok;
}

static cwmp_status_t cwmp_server_reply_http_unauthorized(struct lws* wsi) {
    unsigned char buf[LWS_PRE + 1024];
    unsigned char* start = &buf[LWS_PRE];
    unsigned char* p = start;
    unsigned char* end = &buf[sizeof(buf) - LWS_PRE - 1];

    char* requestDigestMsg = DM_COM_GenerateRequestDigestMsg(randomNonceStr, randomOpaqueStr);
    if(lws_add_http_header_status(wsi, HTTP_STATUS_UNAUTHORIZED, &p, end)) {
        return cwmp_status_ko;
    }

    if(lws_add_http_header_by_token(wsi, WSI_TOKEN_HTTP_WWW_AUTHENTICATE,
                                    (const unsigned char*) requestDigestMsg,
                                    strlen(requestDigestMsg),
                                    &p, end)) {
        return cwmp_status_ko;
    }

    if(lws_add_http_header_by_token(wsi, WSI_TOKEN_HTTP_CONTENT_TYPE,
                                    (const unsigned char*) "text/html", 9,
                                    &p, end)) {
        return cwmp_status_ko;
    }

    if(lws_add_http_header_content_length(wsi, 0, &p, end)) {
        return cwmp_status_ko;
    }

    if(lws_finalize_write_http_header(wsi, start, &p, end)) {
        return cwmp_status_ko;
    }
    DM_ENG_FREE(requestDigestMsg);
    return cwmp_status_ok;
}

// 3.2.2: The CPE MUST accept Connection Requests from any source that has the correct authentication parameters for the target CPE.
// OK: we do not check on the source address

// 3.2.2: The CPE MUST use digest-authentication to authenticate the ACS before proceeding—the CPE
//        MUST NOT initiate a connection to the ACS due to an unsuccessfully authenticated request.
// If this HTTP Message do not contain DIGEST Authentication data, send
// an authentication request.
static cwmp_status_t cwmp_server_validate_authentication(struct lws* wsi, const char* requested_uri) {
    cwmp_status_t ret = cwmp_status_ko;
    char* conn_req_username = NULL;
    char* conn_req_passwd = NULL;

    if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_CONNECTIONREQUESTUSERNAME, &conn_req_username) != 0) {
        SAH_TRACEZ_ERROR("CWMPD", "failed to fetch acs username");
        goto stop;
    }
    if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_CONNECTIONREQUESTPASSWORD, &conn_req_passwd) != 0) {
        SAH_TRACEZ_ERROR("CWMPD", "failed to fetch acs password");
        goto stop;
    }

    if(!lws_hdr_total_length(wsi, WSI_TOKEN_HTTP_AUTHORIZATION)) {
        SAH_TRACEZ_ERROR("CWMPD", "Received a request without authentication data");
    } else {
        char auth_data[1024];
        char* httpMethod = (char*) "";
        char* digest_message = NULL;
        if(lws_hdr_total_length(wsi, WSI_TOKEN_GET_URI)) {
            httpMethod = (char*) "GET";
        }

        lws_hdr_copy(wsi, auth_data, sizeof auth_data, WSI_TOKEN_HTTP_AUTHORIZATION);
        amxc_string_setf(&buffer, "%s %s\n\r%s %s", httpMethod, requested_uri, AuthorizationToken, auth_data);

        digest_message = amxc_string_dup(&buffer, 0, amxc_string_text_length(&buffer));
        if(DM_COM_CheckDigestAuthMessageContent(digest_message)) {
            if(false == DM_COM_PerformClientDigestAuthentication(digest_message,
                                                                 randomNonceStr,
                                                                 randomOpaqueStr,
                                                                 conn_req_username,
                                                                 conn_req_passwd)) {
                SAH_TRACEZ_ERROR("CWMPD", "Digest authentication failed");
                ret = cwmp_status_ko;
            } else {
                SAH_TRACEZ_INFO("CWMPD", "Digest authentication OK");
                ret = cwmp_status_ok;
            }
        } else {
            SAH_TRACEZ_ERROR("CWMPD", "cannot reterive all tokens");
        }
        DM_ENG_FREE(digest_message);
    }

stop:
    CWMPD_FREE(conn_req_username);
    CWMPD_FREE(conn_req_passwd);
    return ret;
}

static int cwmp_server_handle_request(struct lws* wsi, char* in, int len) {
    const char* requested_uri = (char*) in;
    int rc = 0;
    SAH_TRACEZ_INFO("CWMPD", "Handling request %s", requested_uri);

    if(len < 1) {
        lws_return_http_status(wsi, HTTP_STATUS_BAD_REQUEST, "Bad REQUEST");
        goto exit;
    }

    if(cwmp_server_check_availability() != 0) {
        lws_return_http_status(wsi, HTTP_STATUS_SERVICE_UNAVAILABLE, HTTP_STRING_SVR_BUSY);
        goto exit;
    }

    if(cwmp_server_validate_uri(wsi, requested_uri) != 0) {
        lws_return_http_status(wsi, HTTP_STATUS_NOT_FOUND, "Service Not Found");
        goto exit;
    }

    if(cwmp_server_validate_authentication(wsi, requested_uri) == cwmp_status_ko) {
        SAH_TRACEZ_INFO("CWMPD", "Ask ACS to provide authentication headers");
        cwmp_server_reply_http_unauthorized(wsi);
    } else {
        //authentication OK, schedule a new session
        if(DM_ENG_RequestConnection(DM_ENG_EntityType_ACS) == 0) {
            // Send a HTTP response with either the code 200 or 204
            lws_return_http_status(wsi, HTTP_STATUS_NO_CONTENT, NULL);
            rc = -1;//close TCP connection immediately
        } else {
            lws_return_http_status(wsi, HTTP_STATUS_SERVICE_UNAVAILABLE, HTTP_STRING_SVR_BUSY);
        }
    }
exit:
    return rc;
}

/* http server callback */
static int cwmp_server_http_callback(struct lws* wsi, enum lws_callback_reasons reason,
                                     void* user, void* in, size_t len) {

    switch(reason) {
    case LWS_CALLBACK_ESTABLISHED:
        lws_set_timer_usecs(wsi, 20 * LWS_USEC_PER_SEC);
        lws_set_timeout(wsi, PENDING_TIMEOUT_AWAITING_PROXY_RESPONSE, 60);
        break;
    case LWS_CALLBACK_HTTP:
        return cwmp_server_handle_request(wsi, (char*) in, len);
    case LWS_CALLBACK_HTTP_BODY_COMPLETION:
        /* the expected amount of http request body has been delivered */
        lws_return_http_status(wsi, HTTP_STATUS_OK, NULL);
        break;
    default:
        break;
    }
    return lws_callback_http_dummy(wsi, reason, user, in, len);
}

/* websocket configuration struct , protocol : http */
static const struct lws_protocols protocols[] = {
    {"http-only", cwmp_server_http_callback, 0, 0, 0, NULL, 0},
    { NULL, NULL, 0, 0, 0, NULL, 0} /* needed by lws */
};

static cwmp_status_t cwmp_server_init_lws(const char* server_host, int port) {
    memset(&lws_server_ctx_info, 0, sizeof lws_server_ctx_info);
    void* main_loop[1] = { cwmp_evlp_get() };
    /* this will attach our server to our main evlp */
    lws_server_ctx_info.foreign_loops = main_loop;
    lws_server_ctx_info.options = LWS_SERVER_OPTION_VALIDATE_UTF8
        | LWS_SERVER_OPTION_LIBEVENT
        | LWS_SERVER_OPTION_EXPLICIT_VHOSTS
        | LWS_SERVER_OPTION_ALLOW_LISTEN_SHARE;

    lws_server_ctx_info.protocols = protocols;

    /* there is No ssl on the server */
    lws_server_ctx_info.ssl_cert_filepath = NULL;
    lws_server_ctx_info.ssl_private_key_filepath = NULL;
    // set server info struct.
    lws_server_ctx_info.vhost_name = server_host;
    lws_server_ctx_info.port = port;
    if(!lws_server_ctx_info.vhost_name || !(*lws_server_ctx_info.vhost_name)) {
        SAH_TRACEZ_ERROR("CWMPD", "No Connection request host is set, stop initializing server");
        return cwmp_status_ko;
    }
    // Create the lws context
    lws_server_ctx = lws_create_context(&lws_server_ctx_info);

    if(!lws_server_ctx) {
        SAH_TRACEZ_ERROR("CWMPD", "lws init failed");
        return cwmp_status_ko;
    }
    return cwmp_status_ok;
}
/* fetch all server info from data model and feed them to server info struct*/
cwmp_status_t cwmp_server_init() {
    cwmp_status_t ret = cwmp_status_ko;
    char* server_host = NULL;
    char* server_port = NULL;

    SAH_TRACEZ_INFO("CWMPD", "lws init server");
    if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_LOCALIPADDRESS, &server_host) != 0) {
        SAH_TRACEZ_ERROR("CWMPD", "Cannot fetch the local ip address");
        goto error;
    }

    // Check if wan is up
    if(server_host && (!(*server_host) || (strcmp(server_host, "0.0.0.0") == 0))) {
        SAH_TRACEZ_ERROR("CWMPD", "WAN is not connected, not starting server");
        goto error;
    }

    if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_CONNECTIONREQUESTPORT, &server_port) != 0) {
        SAH_TRACEZ_ERROR("CWMPD", "Cannot fetch the local connection request port #");
        goto error;
    }
    SAH_TRACEZ_INFO("CWMPD", "Connection request host %s, port = %s", server_host, server_port);
    // Create the randomly chosen CPE URL
    if(cwmp_server_create_url() != 0) {
        goto error;
    }

    cwmp_server_initConnectionTimestampList();
    ret = cwmp_server_init_lws(server_host, atoi(server_port));
error:
    if(server_host) {
        free(server_host);
    }
    if(server_port) {
        free(server_port);
    }
    return ret;
}

/* Start the main server */
cwmp_status_t cwmp_server_start() {
    SAH_TRACEZ_IN("CWMPD");
    char* cpe_enabled = NULL;
    bool cpe_enabled_b = false;

    if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_ENABLECWMP, &cpe_enabled) != 0) {
        SAH_TRACEZ_ERROR("CWMPD", "Cannot fetch the ACS enable flag");
        return cwmp_status_ko;
    }

    if(cpe_enabled && ( strcmp(cpe_enabled, "1") == 0)) {
        cpe_enabled_b = true;
    }
    free(cpe_enabled);

    if(cpe_enabled_b) {
        /* create the vhost */
        lws_server_vhost = lws_create_vhost(lws_server_ctx, &lws_server_ctx_info);

        if(lws_server_vhost == NULL) {
            SAH_TRACEZ_ERROR("CWMPD", "lws vhost creation failed");
            return cwmp_status_ko;
        }

        // All good, server is now listening
        return cwmp_status_ok;
    } else {
        SAH_TRACEZ_ERROR("CWMPD", "CWMP Server is not Started : CPE not enabled !");
        return cwmp_status_ko;
    }
    SAH_TRACEZ_OUT("CWMPD");
    return cwmp_status_ok;
}

cwmp_status_t cwmp_server_stop() {
    SAH_TRACEZ_INFO("CWMPD", "Server is going to stop Cleaning ressources");
    if(lws_server_vhost) {
        lws_vhost_destroy(lws_server_vhost);
    }
    lws_server_vhost = NULL;
    lws_context_destroy(lws_server_ctx);
    lws_server_ctx = NULL;
    // clean up maxconnections.
    cwmp_server_maxConnectionsCleanup();
    return cwmp_status_ok;
}
