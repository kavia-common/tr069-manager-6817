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

#include <amxc/amxc.h>
#include <amxp/amxp.h>

#include "dmmain/cwmpd.h"
#include <dmcom/dm_com_digest.h>
#include <dmengine/DM_ENG_RPCInterface.h>
#include <dmcommon/DM_GlobalDefs.h>
#include <dmcom/dm_com.h>
#include "httpparser/picohttpparser.h"

extern dm_com_struct g_DmComData;

#define MAXCONNECTIONSPERIOD 3600
#define MAXCONNECTIONSINPERIOD 50
#define RESTART_TIMER_TIMEOUT 60000 //ms = 1 min

amxc_string_t buffer;

static const int DEFAULT_SESSION_TIMEOUT = 45;
static const int MAX_CONTENT_LENGTH = 33554432;

#define CPE_REALM     "sahrealm"
char* g_randomCpeUrl = NULL;
static int httpCode = 0;

struct lws_context_creation_info* g_lws_ctx_info;

static bool connection_timestamp_list_initialized = false;
static amxc_llist_t connection_timestamp_list;

typedef struct _time_list_item {
    time_t t;
    amxc_llist_it_t it;
} time_list_item_t;

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
        SAH_TRACE_ERROR("cannot create socket");
        return 1;
    }

    ifc.ifc_len = sizeof(ifbuf);
    ifc.ifc_buf = ifbuf;
    if(ioctl(sock, SIOCGIFCONF, &ifc) < 0) {
        SAH_TRACE_ERROR("ioctl failed");
        close(sock);
        return 1;
    }

    ifr = ifc.ifc_req;
    total = ifc.ifc_len / sizeof(struct ifreq);
    for(i = 0; i < total; i++) {
        struct ifreq* item = &ifr[i];
        struct sockaddr_in* sa = (struct sockaddr_in*) &item->ifr_addr;
        switch(sa->sin_family) {
        case 0xFFFF:
        case 0:
            break;
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
                SAH_TRACE_ERROR("ioctl(SIOCGIFHWADDR) failed");
                close(sock);
                return 1;
            }
            int rc = snprintf(macbuf, buflen, "%02x:%02x:%02x:%02x:%02x:%02x\n",
                              (unsigned int) (unsigned char) item->ifr_hwaddr.sa_data[0],
                              (unsigned int) (unsigned char) item->ifr_hwaddr.sa_data[1],
                              (unsigned int) (unsigned char) item->ifr_hwaddr.sa_data[2],
                              (unsigned int) (unsigned char) item->ifr_hwaddr.sa_data[3],
                              (unsigned int) (unsigned char) item->ifr_hwaddr.sa_data[4],
                              (unsigned int) (unsigned char) item->ifr_hwaddr.sa_data[5]);
            if(rc < 0) {
                SAH_TRACE_ERROR("snprintf failed");
            }
        }
    }
    close(sock);
    return 0;
}

static char* server_generateMacBasedPath(const char* ip) {
    char token[] = "ABCDE67FGHqrtuvwSTU48VWabcdefIJKL39MNPQRghijkmnpxyz2";
    unsigned char nb_tokens = strlen(token);
    unsigned short rnd_idx = 59;
    char* path = (char*) calloc(17, sizeof(char));
    uint i;
    char macAddress[40];
    char* seed = macAddress;
    char tmp[40];

    // Use MACAddress as Connection Request Path
    memset(macAddress, 0, sizeof(macAddress));
    server_getHWAddressFromIp(ip, macAddress, sizeof(macAddress));
    sprintf(tmp, "%.2X:%.2X:%.2X:%.2X:%.2X:%.2X", macAddress[0], macAddress[1], macAddress[2], macAddress[3], macAddress[4], macAddress[5]);
    SAH_TRACE_INFO("MACAddress: %s", tmp);

    for(i = 0; i < (sizeof(path) - 1); i++) {
        if(!*seed) { // end of seed string reached, recommence from start
            seed = macAddress;
        }
        rnd_idx = (rnd_idx << 1) ^ *seed++;
        path[i] = token[rnd_idx % nb_tokens];
    }
    path[i] = '\0';
    return path;
}

// Size of the CPE URL
#define CPE_URL_SIZE (16)
static int server_createURL() {
    // Create the randomly chosen CPE URL (This URL is provided in the inform message
    // and is used by the ACS to connect to the CPE HTTP Server). The Randomly Chosen
    // URL must be used by the DM_ENGINE to build the inform message.
    char* random_cpe_url = NULL;
    char* url_env = getenv("TR069_URL_PATH");
    if(url_env) {
        random_cpe_url = strdup(url_env);
    } else {
        char* pathType = NULL;
        if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_CONNECTIONREQUESTPATHTYPE, &pathType) != 0) {
            SAH_TRACE_ERROR("Cannot fetch DM_ENG_CONNECTIONREQUESTPATHTYPE");
            return -1;
        }
        if(strcmp(pathType, "Fixed-Default") == 0) {
            if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_CONNECTIONREQUESTPATH, &random_cpe_url) != 0) {
                SAH_TRACE_ERROR("Cannot fetch DM_ENG_CONNECTIONREQUESTPATH");
                return -1;
            }
        } else if(strncmp(pathType, "Random", strlen("Random")) == 0) {
            random_cpe_url = (char*) malloc(CPE_URL_SIZE + 1);
            _generateRandomString(random_cpe_url, CPE_URL_SIZE);
        } else if(strcmp(pathType, "Fixed-MacBased") == 0) {
            char* host = NULL;
            if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_LOCALIPADDRESS, &host) != 0) {
                SAH_TRACE_ERROR("Cannot fetch the local ip address");
                return -1;
            }
            random_cpe_url = server_generateMacBasedPath(host);
            free(host);
        } else {
            SAH_TRACE_ERROR("Not a valid path type, generating random url");
            random_cpe_url = (char*) malloc(CPE_URL_SIZE + 1);
            _generateRandomString(random_cpe_url, CPE_URL_SIZE);
        }

        free(pathType);
    }

    if(DM_ENG_SetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_CONNECTIONREQUESTPATH, random_cpe_url) != 0) {
        SAH_TRACE_ERROR("failed to set the random path in the datamodel");
        free(random_cpe_url);
        return -1;
    }

    g_randomCpeUrl = (char*) malloc(CPE_URL_SIZE + 1);
    strcpy(g_randomCpeUrl, random_cpe_url);
    SAH_TRACE_INFO("CPE URL: %s\n", g_randomCpeUrl);
    free(random_cpe_url);
    return 0;
}

static bool server_maxConnectionsReached(void) {
    time_t now;
    time(&now);

    // get the DM_ENG_MAXCONNECTIONREQUEST
    char* maxconnectionrequest = NULL;
    unsigned int mc = MAXCONNECTIONSINPERIOD;
    if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_MAXCONNECTIONREQUEST, &maxconnectionrequest) != 0) {
        SAH_TRACE_ERROR("Cannot fetch the MaxConnectionRequest");
    }
    if(maxconnectionrequest != NULL) {
        mc = atoi(maxconnectionrequest);
    }

    free(maxconnectionrequest);

    // get the DM_ENG_FREQCONNECTIONREQUEST
    char* freqconnectionrequest = NULL;
    unsigned int fc = MAXCONNECTIONSPERIOD;
    if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_FREQCONNECTIONREQUEST, &freqconnectionrequest) != 0) {
        SAH_TRACE_ERROR("Cannot fetch the FreqConnectionRequest");
    }
    if(freqconnectionrequest != NULL) {
        fc = atoi(freqconnectionrequest);
    }

    free(freqconnectionrequest);

    // remove all outdated and invalid timestamps (in case the system clock has been changed)
    amxc_llist_it_t* cur = amxc_llist_get_first(&connection_timestamp_list);
    amxc_llist_it_t* next = NULL;
    while(cur != NULL) {
        next = amxc_llist_it_get_next(cur);
        time_list_item_t* value = amxc_llist_it_get_data(cur, time_list_item_t, it);
        if(((now > value->t) && (now - value->t > (int) fc)) || (value->t > now)) {
            amxc_llist_it_take(cur);
            free(value);
        }
        cur = next;
    }
    // 2*mc: due to the basic/digest authentication, 2 "physical"  connection requests are send per "logical" connection request
    if(amxc_llist_size(&connection_timestamp_list) > 2 * mc) {
        /* update RejectedConnectionRequests */
        // update_rejectedConnectionRequests();
        return true;
    }

    return false;
}

static void server_maxConnectionsAdd(void) {
    // add a new timestamp to the linked list
    time_list_item_t* item = (time_list_item_t*) calloc(1, sizeof(time_list_item_t));
    if(!item) {
        SAH_TRACE_ERROR("Cannot allocate memory");
        return;
    }
    amxc_llist_it_init(&item->it);
    time(&item->t);
    amxc_llist_append(&connection_timestamp_list, &item->it);
}

static void server_maxConnectionsCleanup(void) {
    amxc_llist_it_t* cur = amxc_llist_get_first(&connection_timestamp_list);
    amxc_llist_it_t* next = NULL;
    while(cur != NULL) {
        next = amxc_llist_it_get_next(cur);
        time_list_item_t* value = amxc_llist_it_get_data(cur, time_list_item_t, it);
        amxc_llist_it_take(cur);
        free(value);
        cur = next;
    }

    amxc_llist_clean(&connection_timestamp_list, NULL);
}

int server_handleRequestBody(char* body, int len) {
    printf("<--------------------------\n%s\n-------------------------->\n", body);
    process_body(body, len);
    reset_read_buffer();
    return 0;
}

int server_handleRequest(struct lws* wsi, char* in, int len) {
    unsigned char buf[LWS_PRE + 2048],
        * start = &buf[LWS_PRE],
        * p = start,
        * end = &buf[sizeof(buf) - LWS_PRE - 1];
    const char* requested_uri = (char*) in;
    SAH_TRACE_INFO("Handling request %s\n", requested_uri);

    if(len < 1) {
        httpCode = HTTP_STATUS_BAD_REQUEST;
    }

    // 3.2.2: The CPE SHOULD restrict the number of Connection Requests it accepts during a given period of
    //        time in order to further reduce the possibility of a denial of service attack. If the CPE chooses to reject
    //        a Connection Request for this reason, the CPE MUST respond to that Connection Request with an
    //        HTTP 503 status code (Service Unavailable). In this case, the CPE SHOULD NOT include the HTTP
    //        Retry-After header in the response.
    if(server_maxConnectionsReached()) {
        SAH_TRACE_WARNING("Maximum number of connections , return HTTP 503");
        lws_return_http_status(wsi, HTTP_STATUS_SERVICE_UNAVAILABLE, NULL);
    }
    server_maxConnectionsAdd(); // add a new entry in the list

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

    if(g_DmComData.bSession == true) { // we implement action (a)
        SAH_TRACE_INFO("Session is active, return HTTP 503");
        httpCode = HTTP_SERVICE_UNAVAILABLE;
    }

    // 3.2.2: The Connection Request MUST use an HTTP 1.1 GET to a specific URL designated by the CPE. The
    //        URL value is available as read-only Parameter on the CPE. The path of this URL value SHOULD be
    //        randomly generated by the CPE so that it is unique per CPE.
    if(!lws_hdr_total_length(wsi, WSI_TOKEN_GET_URI)
       || !lws_hdr_total_length(wsi, WSI_TOKEN_HTTP)) {
        httpCode = HTTP_STATUS_NOT_FOUND;
    }

    char* random_cpe_url;
    if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_CONNECTIONREQUESTPATH, &random_cpe_url) != 0) {
        SAH_TRACE_ERROR("Cannot fetch the ACS connection request path\n");
        httpCode = HTTP_STATUS_NOT_FOUND;
    }

    if(( requested_uri == NULL) || ( random_cpe_url == NULL) || ( strcmp(requested_uri + 1, random_cpe_url) != 0)) { //+1 because we need to skip the initial slash
        SAH_TRACE_ERROR("Not a valid path (%s != %s)\n", requested_uri + 1, random_cpe_url);
        httpCode = HTTP_STATUS_NOT_FOUND;
    }
    free(random_cpe_url);

    char ip[16];
    lws_get_peer_simple(wsi, ip, 16);
    SAH_TRACE_INFO("peer ip = %s\n", ip);

    // 3.2.2: The CPE MUST accept Connection Requests from any source that has the correct authentication parameters for the target CPE.
    // OK: we do not check on the source address

    // 3.2.2: The CPE MUST use digest-authentication to authenticate the ACS before proceeding—the CPE
    //        MUST NOT initiate a connection to the ACS due to an unsuccessfully authenticated request.
    // If this HTTP Message do not contain DIGEST Authentication data, send
    // an authentication request.
    if(!lws_hdr_total_length(wsi, WSI_TOKEN_HTTP_AUTHORIZATION)) {
        SAH_TRACE_ERROR("Received a request without authentication data\n");
        httpCode = HTTP_STATUS_UNAUTHORIZED;
    } else {
        char auth_data[1024];
        char* httpMethod = (char*) "";
        char* tmp = NULL;
        if(lws_hdr_total_length(wsi, WSI_TOKEN_GET_URI)) {
            httpMethod = (char*) "GET";
        }

        lws_hdr_copy(wsi, auth_data, sizeof auth_data, WSI_TOKEN_HTTP_AUTHORIZATION);
        amxc_string_setf(&buffer, "%s %s\n\r%s %s", httpMethod, requested_uri, AuthorizationToken, auth_data);

        tmp = amxc_string_dup(&buffer, 0, amxc_string_text_length(&buffer));
        // Check if all the digest authentication data are in the message.
        if(_checkDigestAuthMessageContent(tmp)) {
            if(false == performClientDigestAuthentication(tmp)) {
                SAH_TRACE_ERROR("DIGEST AUTHENTICATION: FAILED.\n");
                httpCode = HTTP_STATUS_UNAUTHORIZED;
            } else {
                SAH_TRACE_INFO("DIGEST AUTHENTICATION: SUCCESS\n.");
                httpCode = 0;
            }
        } else {
            SAH_TRACE_ERROR("can not reterive all tokens");
        }
        DM_ENG_FREE(tmp);
    }

    // 3.2.2: The CPE’s response to a successfully authenticated Connection Request MUST use either a “200
    //        (OK)” or a “204 (No Content)” HTTP status code. The CPE MUST send this response immediately
    //        upon successful authentication, prior to it initiating the resulting session. The length of the message-
    //        body in the HTTP response MUST be zero.

    // 3.2.2: If the CPE successfully authenticates and responds to a Connection Request as described above, and if
    //        it is not already in a session, then it MUST, within 30 seconds of sending the response, attempt to
    //        establish a session with the pre-determined ACS address (see section 3.1) in which it includes the
    //        “6 CONNECTION REQUEST” EventCode in the Inform.
    // 3.2.2: If the ACS receives a successful response to a Connection Request but after at least 30 seconds the
    //        CPE has not successfully established a session that includes the “6 CONNECTION REQUEST”
    //        EventCode in the Inform, the ACS MAY retry the Connection Request to that CPE.
    // 3.2.2: If, once the CPE successfully authenticates and responds to a Connection Request, but before it
    //        establishes a session to the ACS, it receives one or more successfully authenticated Connection
    //        Requests, the CPE MUST return a successful response for each of those Connection Requests, but
    //        MUST NOT initiate any additional sessions as a result of these additional Connection Requests,
    //        regardless of how many it receives during this time.
    if(httpCode == 0) {
        // As to analyse and valid the content received
        // TODO : check the content received.
        if(DM_ENG_RequestConnection(DM_ENG_EntityType_ACS) == 0) {
            // TODO : Update the Request Connection Array
            httpCode = HTTP_OK;
        } else {
            httpCode = HTTP_SERVICE_UNAVAILABLE;
        }
    }

    if(httpCode == HTTP_STATUS_UNAUTHORIZED) {
        SAH_TRACE_INFO("Request the ACS to provide Authentication Data.\n");

        char* requestDigestMsg = _getRandomString();
        if(lws_add_http_header_status(wsi, HTTP_STATUS_UNAUTHORIZED, &p, end)) {
            return -1;
        }

        if(lws_add_http_header_by_token(wsi, WSI_TOKEN_HTTP_WWW_AUTHENTICATE,
                                        (const unsigned char*) requestDigestMsg,
                                        strlen(requestDigestMsg),
                                        &p, end)) {
            return -1;
        }

        if(lws_add_http_header_by_token(wsi, WSI_TOKEN_HTTP_CONTENT_TYPE,
                                        (const unsigned char*) "text/html", 9,
                                        &p, end)) {
            return -1;
        }

        if(lws_finalize_write_http_header(wsi, start, &p, end)) {
            return -1;
        }

        DM_ENG_FREE(requestDigestMsg);
    } else if(httpCode == HTTP_SERVICE_UNAVAILABLE) {
        lws_return_http_status(wsi, HTTP_STATUS_SERVICE_UNAVAILABLE, HTTP_STRING_SVR_BUSY);
    } else if(httpCode == HTTP_STATUS_NOT_FOUND) {
        lws_return_http_status(wsi, HTTP_STATUS_NOT_FOUND, "Service Not Found");
    } else if(httpCode == HTTP_STATUS_OK) {
        // Send a HTTP response with either the code 200 or 204
        lws_return_http_status(wsi, HTTP_STATUS_OK, HTTP_STRING_OK);
        return 0;
    }
    // 3.2.2: The CPE MUST NOT reject a properly authenticated Connection Request for any reason other than
    //        those described above. If the CPE rejects a Connection Request for any of the reasons described
    //        above, it MUST NOT initiate a session with the ACS as a result of that Connection Request.
    // OK, as we only implemented the specified reasons
    return 0;
}

/* http server callback */
static int cwmp_server_http_callback(struct lws* wsi, enum lws_callback_reasons reason,
                                     void* user, void* in, size_t len) {

    struct per_session_data__http* pss = (struct per_session_data__http*) user;
    char buf[256];

    /* protocol logic goes here */
    switch(reason) {
    case LWS_CALLBACK_ESTABLISHED:
        lws_set_timer_usecs(wsi, 20 * LWS_USEC_PER_SEC);
        lws_set_timeout(wsi, PENDING_TIMEOUT_AWAITING_PROXY_RESPONSE, 60);
        break;
    case LWS_CALLBACK_HTTP:
        server_handleRequest(wsi, (char*) in, len);
        break;
    case LWS_CALLBACK_HTTP_BODY:
        server_handleRequestBody((char*) in, len);
        break;
    case LWS_CALLBACK_HTTP_BODY_COMPLETION:
        /* the expected amount of http request body has been delivered */
        lws_return_http_status(wsi, HTTP_STATUS_OK, NULL);
        break;
    default:
        break;
    }
    return lws_callback_http_dummy(wsi, reason, user, in, len);
    // return 0;
}

/* websocket configuration struct , protocol : http */
static const struct lws_protocols protocols[] = {
    {"http-only", cwmp_server_http_callback, 0, 0, },
    { NULL, NULL, 0, 0 } /* needed by lws */
};

/* fetch all server info from data model and feed them to server info struct*/
cwmp_status_t cwmp_server_init(struct lws_context_creation_info* lws_ctx_info) {
    char* server_host = NULL;
    char* server_port = NULL;
    char* acsip = NULL;
    char* sourceprefix = NULL;

    SAH_TRACE_INFO("lws init server\n");
    lws_ctx_info->options = LWS_SERVER_OPTION_VALIDATE_UTF8
        | LWS_SERVER_OPTION_LIBEVENT
        | LWS_SERVER_OPTION_EXPLICIT_VHOSTS;

    lws_ctx_info->protocols = protocols;

    /* check app conf */
    lws_ctx_info->ssl_cert_filepath = NULL;
    lws_ctx_info->ssl_private_key_filepath = NULL;
    /* ..... other stuuf can be configured here*/
    // lws_ctx_info->ka_time = 0;
    // lws_ctx_info->ka_probes = 0;
    // lws_ctx_info->ka_interval = 0;

    if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_LOCALIPADDRESS, &server_host) != 0) {
        SAH_TRACE_ERROR("Cannot fetch the local ip address");
        return cwmp_status_ko;
    }
    SAH_TRACE_INFO("Connection request host = %s\n", server_host);

    if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_CONNECTIONREQUESTPORT, &server_port) != 0) {
        SAH_TRACE_ERROR("Cannot fetch the local connection request port #");
        return cwmp_status_ko;
    }
    SAH_TRACE_INFO("Connection request port # = %s", server_port);

    // set server info struct.
    lws_ctx_info->vhost_name = server_host;
    lws_ctx_info->port = atoi(server_port);

    if(connection_timestamp_list_initialized == false) {
        amxc_llist_init(&connection_timestamp_list);
        connection_timestamp_list_initialized = true;
    }

    // Create the randomly chosen CPE URL
    if(server_createURL() != 0) {
        goto error;
    }

    if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_ACSIP, &acsip) != 0) {
        SAH_TRACE_ERROR("Cannot fetch the ACSIP parameter");
        goto error;
    }

    sourceprefix = strdup(acsip);

    if(!lws_ctx_info->vhost_name || !(*lws_ctx_info->vhost_name)) {
        SAH_TRACE_NOTICE("No Connection request host is set, stop initializing server");
        goto error;
    }

    free(server_host);
    free(server_port);
    free(acsip);
    free(sourceprefix);

    return cwmp_status_ok;

error:
    free(server_host);
    free(server_port);
    free(acsip);
    free(sourceprefix);
    // TODO : fix restart timer with amxp.
    amxp_timer_t* restart_timer = NULL;
    amxp_timer_start(restart_timer, RESTART_TIMER_TIMEOUT);
    SAH_TRACE_INFO("Http server failed to start, we will retry in %d ms", RESTART_TIMER_TIMEOUT);
    return cwmp_status_ko;
}

/* Initialize and Start the main server */
cwmp_status_t cwmp_server_start(struct lws_context_creation_info* lws_ctx_info,
                                void** main_loop, struct lws_context* lws_ctx) {
    httpCode = 0;
    char* cpe_enabled = NULL;
    bool cpe_enabled_b = false;

    /* this will attach our server to our main evlp */
    lws_ctx_info->foreign_loops = main_loop;

    lws_ctx = lws_create_context(lws_ctx_info);

    if(!lws_ctx) {
        lwsl_err("lws init failed\n");
        return cwmp_status_ko;
    }

    if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_ENABLECWMP, &cpe_enabled) != 0) {
        SAH_TRACE_ERROR("Cannot fetch the ACS enable flag ");
    }

    if(cpe_enabled && ( strcmp(cpe_enabled, "1") == 0)) {
        cpe_enabled_b = true;
    }
    free(cpe_enabled);

    if(cpe_enabled_b) {
        /* create the vhost */
        struct lws_vhost* server_vhost = lws_create_vhost(lws_ctx, lws_ctx_info);

        if(server_vhost == NULL) {
            SAH_TRACE_ERROR("lws vhost creation failed failed\n");
            return cwmp_status_ko;
        }

        // All good
        return cwmp_status_ok;
    } else {
        SAH_TRACE_ERROR("Start server failed : CPE not enabled !");
    }
    return cwmp_status_ok;
}

cwmp_status_t cwmp_server_stop(struct lws_context* lws_ctx) {
    //TODO! lws Docs says we should destroy vhost only
    // for now I will destroy the whole server context
    lws_context_destroy(lws_ctx);
    return cwmp_status_ok;
}