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

/**********************************************************
* Include files
**********************************************************/
#include <stdio.h>
#include <fcntl.h>
#include <debug/sahtrace.h>
#include <dmcom/dm_com.h>
#include "dmmain/cwmpd.h"
#include "httpparser/picohttpparser.h"
#include <dmengine/DM_ENG_RPCInterface.h>
#include <stdlib.h>

#include <amxc/amxc.h>
#include <amxp/amxp.h>
#include <amxd/amxd_types.h>
#include <amxb/amxb.h>

#include <ares.h>
#include <arpa/inet.h>
#include <event2/event.h>

/**********************************************************
* Macro definitions
**********************************************************/
#define HTTP_HEADER_SIZE        4096
#define DEFAULT_SESSION_TIMEOUT 45
#define MAX_CONTENT_LENGTH      33554432
#define USER_AGENT              "prpl_user_agent"

/**********************************************************
* Type definitions
**********************************************************/
struct dnsresolve_context {
    struct event_base* base;
    struct event* fd_event_cb;
    ares_channel channel;
    struct ares_options ares_opts;
};

typedef struct AddrInfo {
    char* addrStr;
    unsigned int ttl;
    amxc_llist_it_t it;
} AddrInfo_t;

/**********************************************************
* Variable declarations
**********************************************************/
static char* acs_server_host;
static char* acs_server_ip;
static int acs_server_port;
static char* acs_server_path;
static char* acsip_affinity = NULL;
static bool new_session = true;

static bool connectedToServer = false;
static bool retransmitLastMessage = false;
static int session_timeout = 0;
static char* lastMessage = NULL;
static char* pending_message = NULL;
static bool unexpected_close = true;
static char* read_buffer = NULL;
static int read_buffer_len = 0;
char http_header[HTTP_HEADER_SIZE] = {0};
int last_used = 0;

static struct lws* client_wsi = NULL;
struct lws_client_connect_info cnx_info;
UNUSED static const char* ba_user, * ba_password; // TO DO move to client struct
struct lws_context* g_lws_ctx;

struct dnsresolve_context dns_ctx;

static amxp_timer_t* dns_ttl_timer = NULL;
static unsigned int dns_ttl_timer_value = 0;
static bool ttl_timer_started = false;

// list of ACS addresses with ttl.
amxc_llist_t addrInfo_list;

// current addrInfo it
static amxc_llist_it_t* curr_addrInfo_it = NULL;

/**********************************************************
* Function Prototypes
**********************************************************/
static int cwmp_client_connect_to_acs();
static void cwmp_client_try_connect_to_acsip(const AddrInfo_t* addr_info);
static void cwmp_client_sessionTimedOut(UNUSED char* name);
static void cwmp_client_send_header(int msgLength);
static int cwmp_client_getACSAddrFamily();
static bool cwmp_client_dns_resolve(const char* hostname);
static int cwmp_client_initialize_connection(const char* acs_url);
static int cwmp_client_handle_raw_reply(char* raw, int len);

/**********************************************************
* Functions
**********************************************************/
static int isIPAddress(const char* ip) {
    struct in6_addr result;
    int res = inet_pton(AF_INET, ip, &result);
    if(res) {
        return 1;
    }
    res = inet_pton(AF_INET6, ip, &result);
    if(res) {
        return 1;
    }
    return 0;
}

static int cwmp_client_getACSAddrFamily() {
    uint8_t addrFamily = 0;
    char* tmp = NULL;
    if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_ACSADDRFAMILY, &tmp) == 0) {
        if(tmp) {
            addrFamily = atoi(tmp);
            free(tmp);
        }
    }
    SAH_TRACEZ_INFO("CWMPD", "ACSADDFamily = %d", addrFamily);
    if(addrFamily == 4) {
        return AF_INET;
    } else if(addrFamily == 6) {
        return AF_INET6;
    }
    return AF_UNSPEC;
}

static void cwmp_client_fd_event_cb(int fd, short flags, UNUSED void* arg) {
    int write = ARES_SOCKET_BAD;
    int read = ARES_SOCKET_BAD;
    if(flags & EV_READ) {
        read = fd;
    }
    if(flags & EV_WRITE) {
        write = fd;
    }
    ares_process_fd(dns_ctx.channel, read, write);
}

static void cwmp_client_ares_sock_state_cb(UNUSED void* data, int fd, int read, int write) {
    if(((read + write) == 0) && (dns_ctx.fd_event_cb != NULL)) {
        // remove registred event if any
        event_del(dns_ctx.fd_event_cb);
        free(dns_ctx.fd_event_cb);
        dns_ctx.fd_event_cb = NULL;
        return;
    }
    short events = 0;
    SAH_TRACEZ_INFO("CWMPD", "Change state fd %d read:%d write:%d", fd, read, write);
    if(read) {
        events = EV_READ;
    }
    if(write) {
        events = EV_WRITE;
    }
    SAH_TRACEZ_INFO("CWMPD", "Assign event callback here");
    dns_ctx.fd_event_cb = event_new(dns_ctx.base, fd, events, cwmp_client_fd_event_cb, NULL);
    // Add event
    event_add(dns_ctx.fd_event_cb, NULL);
}

static cwmp_status_t cwmp_client_dnsresolve_cleanup() {

    // cleanup fd_event_cb
    if(dns_ctx.fd_event_cb) {
        event_del(dns_ctx.fd_event_cb);
        free(dns_ctx.fd_event_cb);
        dns_ctx.fd_event_cb = NULL;
    }

    if(dns_ctx.channel) {
        ares_destroy(dns_ctx.channel);
        ares_library_cleanup();
        dns_ctx.channel = NULL;
    }

    SAH_TRACEZ_INFO("CWMPD", "Cleaup dns resolve");

    return cwmp_status_ok;
}

/*
 * Callback that is invoked by c-ares when an asynchronous name resolution
 * request that we have previously initiated is complete.
 */
static void cwmp_client_dnsresolve_cb(UNUSED void* data, int status, UNUSED int timeouts, struct ares_addrinfo* ai) {
    SAH_TRACEZ_INFO("CWMPD", "cwmp_client_dnsresolve_cb called");

    if(!ai || (status != ARES_SUCCESS)) {
        SAH_TRACEZ_INFO("CWMPD", "Failed to lookup %s", ares_strerror(status));
        return;
    }

    if(acsip_affinity && (strcmp(acsip_affinity, "0") == 0) && dns_ttl_timer) {
        SAH_TRACEZ_INFO("CWMPD", "Stop dns TTL timer");
        amxp_timer_stop(dns_ttl_timer);
    }

    amxc_llist_clean(&addrInfo_list, NULL);
    amxc_llist_init(&addrInfo_list);

    if(ai->nodes != NULL) {
        // pointer to current ares_addrinfo_node
        const struct ares_addrinfo_node* ai_cur;
        char addrstr[100] = "";
        uint8_t* addr = NULL;
        for(ai_cur = ai->nodes; ai_cur != NULL; ai_cur = ai_cur->ai_next) {
            inet_ntop(ai_cur->ai_family, ai_cur->ai_addr->sa_data, addrstr, 100);
            switch(ai_cur->ai_family) {
            case AF_INET:
                addr = (uint8_t*) &((struct sockaddr_in*) ai_cur->ai_addr)->sin_addr;
                break;
            case AF_INET6:
                addr = (uint8_t*) &((struct sockaddr_in6*) ai_cur->ai_addr)->sin6_addr;
                break;
            }
            ares_inet_ntop(ai_cur->ai_family, addr, addrstr, 100);

            if((addrstr == NULL) || (strlen(addrstr) == 0)) {
                continue;
            }

            // create a new AddrInfo (addr + ttl).
            AddrInfo_t* new_addrinfo = (AddrInfo_t*) calloc(1, sizeof(AddrInfo_t));
            new_addrinfo->addrStr = strndup(addrstr, strlen(addrstr));
            new_addrinfo->ttl = ai_cur->ai_ttl;

            // add the new AddrInfo to addrInfo_list.
            amxc_llist_append(&addrInfo_list, &new_addrinfo->it);
            SAH_TRACEZ_INFO("CWMPD", "ACSIP : IPv%d address: %s, TTL = %d", ai_cur->ai_family == PF_INET6 ? 6 : 4,
                            addrstr, ai_cur->ai_ttl);
        }
    }

    // connect to acs.
    cwmp_client_connect_to_acs();
    ares_freeaddrinfo(ai);
    if(cwmp_client_dnsresolve_cleanup() != cwmp_status_ok) {
        SAH_TRACEZ_ERROR("CWMPD", "DNS resolve cleanup failed");
    }
}

static bool cwmp_client_dns_resolve(const char* hostname) {
    int status, optmask = 0;
    memset(&dns_ctx, 0, sizeof(dns_ctx));
    dns_ctx.base = cwmp_evlp_get();
    dns_ctx.channel = NULL;

    status = ares_library_init(ARES_LIB_INIT_ALL);
    if(status != ARES_SUCCESS) {
        SAH_TRACEZ_INFO("CWMPD", "ares_library_init: %s", ares_strerror(status));
        return false;
    }

    dns_ctx.ares_opts.sock_state_cb = cwmp_client_ares_sock_state_cb;
    optmask |= ARES_OPT_SOCK_STATE_CB;

    status = ares_init_options(&dns_ctx.channel, &dns_ctx.ares_opts, optmask);
    if(status != ARES_SUCCESS) {
        SAH_TRACEZ_INFO("CWMPD", "ares_init_options failed: %s", ares_strerror(status));
        return false;
    }

    struct ares_addrinfo_hints hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = cwmp_client_getACSAddrFamily();
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = ARES_AI_CANONNAME | ARES_AI_ENVHOSTS | ARES_AI_NOSORT;

    SAH_TRACEZ_INFO("CWMPD", "Send a new dns query to resolve uriHost : %s", hostname);
    ares_getaddrinfo(dns_ctx.channel, hostname, NULL, &hints, cwmp_client_dnsresolve_cb, NULL);

    return true;
}

int get_content_length(struct phr_header* values, unsigned int len) {
    for(unsigned int i = 0; i < len; ++i) {
        if(strncasecmp("content-length:", values[i].name, 15) == 0) {
            return (atoi(values[i].value));
        }
    }
    return 0;
}

int append_read_buffer(char** msg, int* len) {
    char* raw = *msg;
    if(read_buffer) {
        read_buffer = (char*) realloc(read_buffer, (read_buffer_len + *len + 1) * sizeof(char));
        if(!read_buffer) {
            SAH_TRACEZ_ERROR("CWMPD", "Couldn't realloc");
            return 0;
        }
        memcpy(read_buffer + read_buffer_len, raw, *len);
        *len += read_buffer_len;
        read_buffer[*len] = '\0';
        read_buffer_len = *len;
        *msg = read_buffer;
    }
    return 0;
}

int create_read_buffer(char* raw, int len) {
    if(!read_buffer) {
        read_buffer = (char*) realloc(read_buffer, (len + 1) * sizeof(char));
        memcpy(read_buffer, raw, len);
        read_buffer[len] = '\0';
        read_buffer_len = len;
    }
    return 0;
}

void reset_read_buffer() {
    if(read_buffer) {
        free(read_buffer);
        read_buffer = NULL;
        read_buffer_len = 0;
    }
}

int process_body(char* body, int len) {
    SAH_TRACEZ_INFO("CWMPD", "Message body arrived (%zu)", strlen(body));
    if(strstr(body, DM_COM_ENV_TAG) == NULL) {
        SAH_TRACEZ_WARNING("CWMPD", "This is not a SOAP body\n");
    } else {
        DM_SoapXml SoapMsg;
        DM_HttpCheckNamespace(body, len);
        DM_InitSoapMsgReceived(&SoapMsg);
        if(DM_OK == DM_AnalyseSoapMessage(&SoapMsg, body, TYPE_ACS, false)) {
            DM_ParseSoapEnveloppe(SoapMsg.pBody, SoapMsg.pSoapID, SoapMsg.nHoldRequest);
        } else {
            SAH_TRACEZ_ERROR("CWMPD", "Invalid SOAP Message closing session");
            _closeACSSession(false); // really put this one as last one, as it might trigger a new session
        }
        xmlDocumentFree(SoapMsg.pParser);

        DM_InitSoapMsgReceived(&SoapMsg);
    }
    return 0;
}

static int cwmp_client_handle_raw_reply(char* raw, int len) {
    char* msg = raw;
    char* body = NULL;
    size_t header_len = 100, msg_len = 0;
    int last_len = 0;
    int minor, status;
    struct phr_header values[100];
    int content_length;
    const char* header_body = NULL;

    raw[len] = '\0';
    append_read_buffer(&msg, &len);
    last_len = phr_parse_response(msg, len, &minor, &status, &header_body, &msg_len, values, &header_len, last_len);
    if(last_len == -2) {
        SAH_TRACEZ_WARNING("CWMPD", "header is incomplete");
        create_read_buffer(msg, len);
        return 0;
    } else if(last_len == -1) {
        SAH_TRACEZ_ERROR("CWMPD", "Message is erroneous");
        reset_read_buffer();
        return 0;
    }
    content_length = get_content_length(values, header_len);
    if(content_length > MAX_CONTENT_LENGTH) {
        SAH_TRACEZ_WARNING("CWMPD", "Content-length superior to %d (%d)\n", MAX_CONTENT_LENGTH, content_length);
        _closeACSSession(false);
        return 0;
    }
    if(content_length > len - last_len) {
        create_read_buffer(msg, len);
        return 0;
    }
    body = msg + last_len;
    SAH_TRACEZ_INFO("CWMPD", "<--------------------------\n%s\n-------------------------->\n", msg);
    switch(status) {
    case 200:
        process_body(body, content_length);
        break;
    }
    reset_read_buffer();
    return 0;
}

static void cwmp_client_try_connect_to_acsip(const AddrInfo_t* addr_info) {
    if(addr_info) {
        cnx_info.address = addr_info->addrStr;
        cnx_info.origin = addr_info->addrStr;
        cnx_info.host = addr_info->addrStr;
        /** Connecting to curr ACS IP and wait the connection callback :
         * If connected --> update acs_server_ip and start ttl timer for this address
         */
        if(lws_client_connect_via_info(&cnx_info)) {
            SAH_TRACEZ_INFO("CWMPD", "Start Connecting to ACS server IP:%s and wait the connection callback", addr_info->addrStr);
        }
    }
}

/* http callback */
static int cwmp_client_http_callback(struct lws* wsi, enum lws_callback_reasons reason,
                                     UNUSED void* user, void* in, size_t len) {
    int status = 0;
    char serverip[16];
    lws_get_peer_simple(wsi, serverip, 16);

    /* protocol logic goes here */
    switch(reason) {
    case LWS_CALLBACK_PROTOCOL_INIT:
        break;
    case LWS_CALLBACK_RAW_CONNECTED:
    {
        client_wsi = wsi;
        connectedToServer = true;
        SAH_TRACEZ_INFO("CWMPD", "Connected to ACS server IP : %s", serverip);
        free(acs_server_ip);
        acs_server_ip = strdup(serverip);
        AddrInfo_t* connected_addr = amxc_container_of(curr_addrInfo_it, AddrInfo_t, it);
        // Start TTl timer when acsip_affinity = false and the ttl > 0
        if(connected_addr && (connected_addr->ttl > 0) && (strcmp(acsip_affinity, "0") == 0)) {
            dns_ttl_timer_value = connected_addr->ttl;
            SAH_TRACEZ_INFO("CWMPD", "Start dns TTL timer [%d]", dns_ttl_timer_value);
            ttl_timer_started = true;
            amxp_timer_start(dns_ttl_timer, dns_ttl_timer_value * 1000);
        }

        free(read_buffer);
        read_buffer = NULL;
        read_buffer_len = 0;
        status = (int) lws_http_client_http_response(wsi);
        SAH_TRACEZ_INFO("CWMPD", "Client: Connected to ACS server with status (%d)", status);
        if(pending_message) {
            DM_SendHttpMessage(pending_message);
        }
    }
    break;
    case LWS_CALLBACK_RAW_CLOSE:
        connectedToServer = false;
        if(unexpected_close) {
            // lets assume session is OK otherwise : periodic inform will not be sent
            // TODO!: properly manage start/end new session ?
            _closeACSSession(true);
        }
        SAH_TRACEZ_INFO("CWMPD", "Client: Disconnected from ACS server");
        break;
    case LWS_CALLBACK_WSI_DESTROY:
        if(client_wsi) {
            client_wsi = NULL;
        }
        break;
    case LWS_CALLBACK_RAW_RX:
        cwmp_client_handle_raw_reply((char*) in, len);
        break;
    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
    {
        AddrInfo_t* curr_addrInfo = amxc_llist_it_get_data(curr_addrInfo_it, AddrInfo_t, it);
        SAH_TRACEZ_ERROR("CWMPD", "Client : connection error with %s : %s", curr_addrInfo->addrStr, in ? (char*) in : "(null)");
        if(client_wsi) {
            client_wsi = NULL;
        }
        // try with next ACS Server IP.
        amxc_llist_it_t* next_it = amxc_llist_it_get_next(curr_addrInfo_it);
        if(next_it) {
            SAH_TRACEZ_INFO("CWMPD", "Try connect to next ACS server IP");
            AddrInfo_t* next_addrInfo = amxc_llist_it_get_data(next_it, AddrInfo_t, it);
            if(next_addrInfo) {
                cwmp_client_try_connect_to_acsip(next_addrInfo);
            }
        }
        curr_addrInfo_it = next_it;
    }
    break;
    default:
        break;
    }
    return 0;
}

/* websocket configuration struct , protocol : http */
static const struct lws_protocols protocols[] =
{
    {"http-only", cwmp_client_http_callback, 0, 0, 0, NULL, 0},
    { NULL, NULL, 0, 0, 0, NULL, 0} /* mark protocol end  needed by lws */
};

static int cwmp_client_initialize_connection(const char* acs_url) {
    char* acs_url_local = NULL;
    const char* uriHost = NULL;
    const char* uriScheme = NULL;
    int uriPort = 0;
    const char* uriPath = NULL;
    char* crHost = NULL;

    if(acs_url == NULL) {
        if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_URL, &acs_url_local) != 0) {
            SAH_TRACEZ_ERROR("CWMPD", "Cannot fetch the ACS SERVER URL");
            goto exit_error;
        }
    } else {
        acs_url_local = strdup(acs_url);
    }
    SAH_TRACEZ_INFO("CWMPD", "ACS URL = %s", acs_url_local);
    if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_CONNECTIONREQUESTHOST, &crHost) != 0) {
        SAH_TRACEZ_ERROR("CWMPD", "Cannot fetch the connection request host URL");
        goto exit_error;
    }
    // Check if wan is up
    if(!(*crHost) || (strcmp(crHost, "0.0.0.0") == 0)) {
        SAH_TRACEZ_WARNING("CWMPD", "WAN is not connected, not connecting to server");
        free(crHost);
        goto exit_error;
    }
    free(crHost);
    free(acs_server_host);
    acs_server_host = NULL;
    if(lws_parse_uri(acs_url_local, &uriScheme, &uriHost, &uriPort, &uriPath)) {
        SAH_TRACEZ_ERROR("CWMPD", "Couldn't parse URL (%s)", acs_url_local);
        goto exit_error;
    }
    SAH_TRACEZ_APP_INFO("CWMPD", "Host: %s | Scheme: %s | Port: %d | Path: %s\n", uriHost, uriScheme, uriPort, uriPath);
    acs_server_host = strdup(uriHost);
    acs_server_port = uriPort;
    free(acs_server_path);
    acs_server_path = strdup(uriPath);
    if(strcmp(uriScheme, "https") == 0) {
        // Do stuff for https
    }

    /**
     * Check if host is IP address so we don't need to resolve it.
     * Check if TTL has expired or initialize new session so :
     * 1 - Send new dns query to resolve acs host
     * 2 - If resolving OK : Try to connect to acs
     *  cwmp_client_connect_to_acs called in dnsresolve_cb
     */
    if(isIPAddress(uriHost) == 1) {
        acs_server_ip = strdup(uriHost);
        SAH_TRACEZ_INFO("CWMPD", "URI host : %s is IP address so we don't need to resolve it", uriHost);
        cwmp_client_connect_to_acs();
    } else {
        if(new_session || (acsip_affinity && (strcmp(acsip_affinity, "0") == 0) &&
                           dns_ttl_timer_value && (amxp_timer_remaining_time(dns_ttl_timer) <= 0))) {
            SAH_TRACEZ_INFO("CWMPD", "TTL expired, we need a new dns query to resolve acs host");
            cwmp_client_dns_resolve(uriHost);
        } else {
            SAH_TRACEZ_INFO("CWMPD", "ACS IP already resolved : %s", acs_server_ip);
            cwmp_client_connect_to_acs();
        }
    }
    return 0;

exit_error:
    free(acs_url_local);
    return -1;
}

static int cwmp_client_connect_to_acs() {
    memset(&cnx_info, 0, sizeof(cnx_info));
    cnx_info.context = g_lws_ctx;
    cnx_info.method = "RAW";
    cnx_info.protocol = protocols[0].name;
    cnx_info.ssl_connection = 0;
    cnx_info.pwsi = &client_wsi;
    cnx_info.fi_wsi_name = "user";
    unexpected_close = true;

    cnx_info.port = acs_server_port;
    cnx_info.host = acs_server_host;
    cnx_info.path = acs_server_path;

    if(acs_server_ip) {
        cnx_info.address = acs_server_ip;
        if(!lws_client_connect_via_info(&cnx_info)) {
            SAH_TRACEZ_ERROR("CWMPD", "Couldn't connect to %s", acs_server_ip);
            return -1;
        }
    }

    if(!client_wsi) {
        SAH_TRACEZ_INFO("CWMPD", "Try to connect to acs ip list");
        amxc_llist_it_t* first_it = amxc_llist_get_first(&addrInfo_list);
        if(first_it) {
            curr_addrInfo_it = first_it;
            AddrInfo_t* first_addrInfo = amxc_llist_it_get_data(first_it, AddrInfo_t, it);
            if(first_addrInfo) {
                // try to connect to first acsip.
                cwmp_client_try_connect_to_acsip(first_addrInfo);
            }
        } else {
            SAH_TRACEZ_INFO("CWMPD", "ACS ip address list is empty");
        }
    }

    return 0;
}

/* fetch all server info from data model and feed them to server info struct*/
cwmp_status_t cwmp_client_init(struct lws_context_creation_info* lws_ctx_info) {
    lws_ctx_info->protocols = protocols;
    lws_ctx_info->ssl_cert_filepath = NULL;
    lws_ctx_info->ssl_private_key_filepath = NULL;
    return cwmp_status_ok;
}

/* Initialize and Start new client session */
cwmp_status_t cwmp_client_start_session(struct lws_context_creation_info* lws_ctx_info,
                                        void** main_loop, struct lws_context* lws_ctx) {
    SAH_TRACEZ_INFO("CWMPD", "Client stating session");

    lws_ctx_info->options = LWS_SERVER_OPTION_LIBEVENT;
    lws_ctx_info->foreign_loops = main_loop;
    lws_ctx_info->port = CONTEXT_PORT_NO_LISTEN;
    lws_ctx_info->user = NULL;
    lws_ctx_info->connect_timeout_secs = 60;
    lws_ctx_info->fd_limit_per_thread = 1 + 1 + 1;
    lws_ctx = lws_create_context(lws_ctx_info);
    if(lws_ctx == NULL) {
        SAH_TRACEZ_ERROR("CWMPD", "lws client context creation failed");
        return cwmp_status_ko;
    }
    g_lws_ctx = lws_ctx;

    if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_ACSIPAFFINITY, &acsip_affinity) != 0) {
        SAH_TRACEZ_ERROR("CWMPD", "DM_ENGINE: Cannot fetch ACSIPAffinity parameter");
        goto exit_error;
    }

    amxp_timer_new(&dns_ttl_timer, NULL, NULL);
    if(cwmp_client_initialize_connection(NULL) == -1) {
        SAH_TRACEZ_NOTICE("CWMPD", "Cannot initialize client connection");
        goto exit_error;
    }
    new_session = false;

    return cwmp_status_ok;

exit_error:
    free(acsip_affinity);
    return cwmp_status_ko;
}

cwmp_status_t cwmp_client_stop(struct lws_context* lws_ctx) {
    lws_context_destroy(lws_ctx);
    if(dns_ctx.channel) {
        ares_destroy(dns_ctx.channel);
        ares_library_cleanup();
    }

    if(acs_server_host) {
        free(acs_server_host);
        acs_server_host = NULL;
    }

    if(acs_server_path) {
        free(acs_server_path);
        acs_server_path = NULL;
    }

    return cwmp_status_ok;
}

int DM_CloseHttpSession(bool closeMode) {
    if(connectedToServer) {
        unexpected_close = false;
        if((closeMode != NORMAL_CLOSE) && client_wsi) {
            lws_set_timeout(client_wsi, PENDING_TIMEOUT_KILLED_BY_PROXY_CLIENT_CLOSE, LWS_TO_KILL_SYNC);
        }
        if(lastMessage) {
            free(lastMessage);
            lastMessage = NULL;
        }
        connectedToServer = false;
        free(acs_server_host);
        acs_server_host = NULL;
        free(acs_server_path);
        acs_server_path = NULL;
    }
    return 0;
}

static void cwmp_client_sessionTimedOut(UNUSED char* name) {
    unexpected_close = false;
    SAH_TRACEZ_WARNING("CWMPD", "session timed out\n");
    _closeACSSession(false);
}

static void cwmp_client_send_header(int msgLength) {
    unsigned char* p = (unsigned char*) http_header;
    int ret;

    memset(http_header, 0, HTTP_HEADER_SIZE);
    last_used = sprintf(http_header, "POST %s HTTP/1.1\r\n", acs_server_path);
    p += last_used;
    ret = lws_add_http_header_by_name(client_wsi, (const unsigned char*) "Host:", (const unsigned char*) acs_server_host, strlen(acs_server_host), &p, (unsigned char*) (http_header + HTTP_HEADER_SIZE));
    ret += lws_add_http_header_by_name(client_wsi, (const unsigned char*) "User-agent:", (const unsigned char*) USER_AGENT, strlen(USER_AGENT), &p, (unsigned char*) (http_header + HTTP_HEADER_SIZE));
    ret += lws_add_http_header_content_length(client_wsi, msgLength, &p, (unsigned char*) (http_header + HTTP_HEADER_SIZE));
    ret += lws_add_http_header_by_name(client_wsi, (const unsigned char*) "Content-Type:", (const unsigned char*) "text/xml; charset=ISO-8859-1", 28, &p, (unsigned char*) (http_header + HTTP_HEADER_SIZE));
    ret += lws_add_http_header_by_name(client_wsi, (const unsigned char*) "SOAPAction:", (const unsigned char*) "", 0, &p, (unsigned char*) (http_header + HTTP_HEADER_SIZE));
    ret += lws_finalize_write_http_header(client_wsi, (unsigned char*) http_header, &p, (unsigned char*) (http_header + HTTP_HEADER_SIZE));
    SAH_TRACEZ_INFO("CWMPD", "Send HTTP Header  %d -------------------------->\n%s", ret, http_header);
}

int DM_SendHttpMessage(const char* msgToSendStr) {
    int msgLength = 0;

    if(connectedToServer == false) {
        free(pending_message);
        pending_message = strdup(msgToSendStr);
        return 0;
    }
    if(msgToSendStr != lastMessage) {
        free(lastMessage);
        lastMessage = strdup(msgToSendStr);
    }
    if(msgToSendStr != NULL) {
        msgLength = strlen(msgToSendStr);
    }

    cwmp_client_send_header(msgLength);
    SAH_TRACEZ_INFO("CWMPD", "Send HTTP msg \n%s\n-------------------------->\n", msgToSendStr);
    DM_UpdateRetryBuffer(msgToSendStr, msgLength);
    lws_write(client_wsi, (unsigned char*) msgToSendStr, strlen(msgToSendStr), LWS_WRITE_HTTP);
    SAH_TRACEZ_INFO("CWMPD", "Message sent (%d).", msgLength);
    DM_ENG_NotificationInterface_timerStart("Session-timer", session_timeout, 0, cwmp_client_sessionTimedOut);
    if(msgToSendStr == pending_message) {
        free(pending_message);
        pending_message = NULL;
    }
    return 0;
}

int client_startSession() {
    SAH_TRACEZ_INFO("CWMPD", "HTTP start session");
    if(connectedToServer) {
        return 0;
    }
    char* s_timeout = NULL;
    retransmitLastMessage = false;
    lastMessage = NULL;

    SAH_TRACEZ_INFO("CWMPD", "The reamining time of DNS TTL timer is : %d", amxp_timer_remaining_time(dns_ttl_timer));

    if(cwmp_client_initialize_connection(NULL) == -1) {
        SAH_TRACEZ_NOTICE("CWMPD", "Cannot initialize client connection");
        return -1;
    }

    if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_SESSIONTIMEOUT, &s_timeout) == 0) {
        if(s_timeout) {
            session_timeout = atoi(s_timeout);
        } else {
            session_timeout = DEFAULT_SESSION_TIMEOUT;
        }
    }
    free(s_timeout);

    if(session_timeout <= 0) {
        session_timeout = DEFAULT_SESSION_TIMEOUT;
    }
    SAH_TRACEZ_INFO("CWMPD", "Session timeout = %d", session_timeout);
    DM_ENG_NotificationInterface_timerStart("Session-timer", session_timeout, 0, cwmp_client_sessionTimedOut);

    return 0;
}

void cwmp_client_clear_ACSIP() {
    if(acs_server_ip) {
        free(acs_server_ip);
        acs_server_ip = NULL;
    }
}
