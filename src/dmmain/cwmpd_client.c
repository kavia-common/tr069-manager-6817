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
#include <netinet/in.h>
#include <signal.h>
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
#include <string.h>

/**********************************************************
* Macro definitions
**********************************************************/
#define HTTP_HEADER_SIZE        4096
#define DEFAULT_SESSION_TIMEOUT 45
#define MAX_CONTENT_LENGTH      33554432
#define USER_AGENT              "prpl_user_agent"

/**********************************************************
* Variable declarations
**********************************************************/
static char* acs_server_host;
static char* acs_server_ip;
static int acs_server_ttl = 0;
static int acs_server_port;
static char* acs_server_path;
static char* acsip_affinity = NULL;

static bool connectedToServer = false;
static int session_timeout = 0;
static char* pending_message = NULL;
static bool unexpected_close = true;
static char* read_buffer = NULL;
static int read_buffer_len = 0;

static struct lws* lws_client_wsi = NULL;         /* client ws interface */
static struct lws_context* lws_client_ctx = NULL; /* client lws context */
static struct lws_vhost* lws_client_vhost = NULL; /* client vhost */
static struct lws_context_creation_info lws_client_ctx_info;

static struct ares_addrinfo* dns_cache = NULL;
static amxp_timer_t* dns_ttl_timer = NULL;
static bool resolve_DNS = true; /* do we need a dns resolution */

static void cwmp_client_sessionTimedOut(UNUSED char* name);
static void cwmp_client_send_header(int msgLength);
static int cwmp_client_handle_raw_reply(char* raw, int len);
static int cwmp_client_http_callback(struct lws* wsi, enum lws_callback_reasons reason,
                                     UNUSED void* user, void* in, size_t len);
static void cwmp_client_dnsTTLTimeout_handler(UNUSED amxp_timer_t* timer, UNUSED void* priv);
static int isIPAddress(const char* ip);
static int get_content_length(struct phr_header* values, unsigned int len);
static int append_read_buffer(char** msg, int* len);
static int create_read_buffer(char* raw, int len);
static void reset_read_buffer();
static int cwmp_client_process_body(char* body, int len);
static cwmp_status_t cwmp_client_connect_to(sa_family_t sa_family, const char* ip);
static cwmp_status_t cwmp_client_connect();


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

/* check if the given ip is in the DNS cache*/
static bool foundInDNSCache(const char* ip) {
    if(ip && dns_cache && dns_cache->nodes) {
        struct ares_addrinfo_node* ai_cur;
        char addrstr[46] = "";
        for(ai_cur = dns_cache->nodes; ai_cur != NULL; ai_cur = ai_cur->ai_next) {
            uint8_t* addr = NULL;

            switch(ai_cur->ai_family) {
            case AF_INET:
                addr = (uint8_t*) &((struct sockaddr_in*) ai_cur->ai_addr)->sin_addr;
                break;
            case AF_INET6:
                addr = (uint8_t*) &((struct sockaddr_in6*) ai_cur->ai_addr)->sin6_addr;
                break;
            }
            ares_inet_ntop(ai_cur->ai_family, addr, addrstr, sizeof(addrstr));
            SAH_TRACE_INFO("found ip %s with ttl = %d", addrstr, ai_cur->ai_ttl);
            if(strcmp(ip, addrstr) == 0) {
                return true;
            }
        }
    }
    return false;
}


static int get_content_length(struct phr_header* values, unsigned int len) {
    for(unsigned int i = 0; i < len; ++i) {
        if(strncasecmp("content-length:", values[i].name, 15) == 0) {
            return (atoi(values[i].value));
        }
    }
    return 0;
}

static int append_read_buffer(char** msg, int* len) {
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

static int create_read_buffer(char* raw, int len) {
    if(!read_buffer) {
        read_buffer = (char*) realloc(read_buffer, (len + 1) * sizeof(char));
        memcpy(read_buffer, raw, len);
        read_buffer[len] = '\0';
        read_buffer_len = len;
    }
    return 0;
}

static void reset_read_buffer() {
    if(read_buffer) {
        free(read_buffer);
        read_buffer = NULL;
        read_buffer_len = 0;
    }
}

static int cwmp_client_process_body(char* body, int len) {
    SAH_TRACEZ_INFO("CWMPD", "Message body arrived (%zu)", strlen(body));
    if(strstr(body, DM_COM_ENV_TAG) == NULL) {
        SAH_TRACEZ_WARNING("CWMPD", "This is not a SOAP body");
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
        SAH_TRACEZ_WARNING("CWMPD", "Content-length superior to %d (%d)", MAX_CONTENT_LENGTH, content_length);
        _closeACSSession(false);
        return 0;
    }
    if(content_length > len - last_len) {
        create_read_buffer(msg, len);
        return 0;
    }
    body = msg + last_len;
    SAH_TRACEZ_INFO("CWMPD", "Received message : \n<--------------------------\n%s\n<--------------------------\n", msg);
    switch(status) {
    case 200:
        cwmp_client_process_body(body, content_length);
        break;
    }
    reset_read_buffer();
    return 0;
}
/* websocket configuration struct , protocol : http */
static const struct lws_protocols protocols[] =
{
    {"http-only", cwmp_client_http_callback, 0, 0, 0, NULL, 0},
    { NULL, NULL, 0, 0, 0, NULL, 0} /* mark protocol end  needed by lws */
};


/* http callback */
static int cwmp_client_http_callback(struct lws* wsi, enum lws_callback_reasons reason,
                                     UNUSED void* user, void* in, size_t len) {
    char serverip[16];
    lws_get_peer_simple(wsi, serverip, 16);
    /* protocol logic goes here */
    switch(reason) {
    case LWS_CALLBACK_PROTOCOL_INIT:
        break;
    case LWS_CALLBACK_RAW_ADOPT:
    {
        lws_client_wsi = wsi;
        connectedToServer = true;
        /* Register for ONWRITABLE event */
        lws_callback_on_writable(wsi);
    }
    break;
    case LWS_CALLBACK_RAW_CLOSE:
        connectedToServer = false;
        if(unexpected_close) {
            // lets assume session is OK otherwise : periodic inform will not be sent
            // TODO!: properly manage start/end new session ?
            _closeACSSession(true);
        }
        break;
    case LWS_CALLBACK_WSI_DESTROY:
        if(lws_client_wsi && (lws_client_wsi == wsi)) {
            lws_client_wsi = NULL;
        }
        break;
    case LWS_CALLBACK_RAW_RX:
        cwmp_client_handle_raw_reply((char*) in, len);
        break;
    case LWS_CALLBACK_RAW_WRITEABLE:
    {
        if(pending_message) { /* a message is waiting for this connection send it */
            DM_SendHttpMessage(pending_message);
        }
    }
    break;
    default:
        break;
    }
    return 0;
}

static cwmp_status_t cwmp_client_connect_to(sa_family_t sa_family, const char* ip) {
    SAH_TRACEZ_INFO("CWMPD", "cwmp_client_connect_to %s", ip);
    lws_sock_file_fd_type sock;//connection socket
    sock.sockfd = -1;
    switch(sa_family) {
    case AF_INET://IPv4
    {
        struct sockaddr_in sk_in;
        memset(&sk_in, 0, sizeof(sk_in));

        inet_pton(sa_family, ip, &(sk_in.sin_addr));
        sk_in.sin_family = sa_family;
        sk_in.sin_port = htons(acs_server_port);
        sock.sockfd = socket(sa_family, SOCK_STREAM, IPPROTO_TCP);

        if(sock.sockfd < 0) {
            SAH_TRACEZ_ERROR("CWMPD", "Failed to create INET socket");
            return cwmp_status_ko;
        }
        SAH_TRACEZ_INFO("CWMPD", "Starting connect to %s...", ip);
        if(connect(sock.sockfd, (struct sockaddr*) &sk_in, sizeof(sk_in)) < 0) {
            SAH_TRACEZ_INFO("CWMPD", "unable to connect");
            return cwmp_status_ko;
        }
    }
    break;
    case AF_INET6://IPv6
    {
        struct sockaddr_in6 sk6_in;
        memset(&sk6_in, 0, sizeof(sk6_in));

        inet_pton(sa_family, ip, &(sk6_in.sin6_addr));
        sk6_in.sin6_family = sa_family;
        sk6_in.sin6_port = htons(acs_server_port);
        sock.sockfd = socket(sa_family, SOCK_STREAM, IPPROTO_TCP);

        if(sock.sockfd < 0) {
            SAH_TRACEZ_ERROR("CWMPD", "Failed to create INET socket");
            return cwmp_status_ko;
        }
        SAH_TRACEZ_INFO("CWMPD", "Starting connect to %s...", ip);
        if(connect(sock.sockfd, (struct sockaddr*) &sk6_in, sizeof(sk6_in)) < 0) {
            SAH_TRACEZ_INFO("CWMPD", "unable to connect");
            return cwmp_status_ko;
        }
    }
    break;
    }

    if(lws_client_vhost == NULL) {
        //Create a client lws vhost
        SAH_TRACEZ_INFO("CWMPD", "Client vhost has been destroyed, create a new one");
        lws_client_vhost = lws_create_vhost(lws_client_ctx, &lws_client_ctx_info);

        if(lws_client_vhost == NULL) {
            SAH_TRACEZ_ERROR("CWMPD", "lws failed to attribute a vhost for client");
            return cwmp_status_ko;
        }
    }
    SAH_TRACEZ_INFO("CWMPD", "Adopting foreign socket into lws");
    lws_client_wsi = lws_adopt_descriptor_vhost(lws_client_vhost, LWS_ADOPT_SOCKET, sock,
                                                protocols[0].name, NULL);
    if(lws_client_wsi == NULL) {
        SAH_TRACEZ_ERROR("CWMPD", "lws return bad context, failed to adopt open socket");
        close(sock.sockfd);
        return cwmp_status_ko;
    }

    return cwmp_status_ok;
}

static cwmp_status_t cwmp_client_connect() {
    SAH_TRACE_INFO("cwmp_client_connect");
    //Use the last used IP if it's set
    if(acs_server_ip) {
        SAH_TRACE_INFO("cwmp_client_connect connecting to %s", acs_server_ip);
        sa_family_t sa_family = AF_INET;
        // check the IP family
        struct in6_addr result;
        if(inet_pton(AF_INET6, acs_server_ip, &result) == 1) {
            sa_family = AF_INET6;
        }
        //IP still valid no need to search a new one
        if(cwmp_client_connect_to(sa_family, acs_server_ip) == cwmp_status_ok) {
            if((strcmp(acsip_affinity, "0") == 0) &&
               (amxp_timer_remaining_time(dns_ttl_timer) <= 0) &&
               (acs_server_ttl != 0)) {
                //Start new Timer
                resolve_DNS = false;    //stop resolving DNS until ttl timer expire
                amxp_timer_start(dns_ttl_timer, acs_server_ttl * 1000);
            }
            return cwmp_status_ok;
        }
    }

    //if last used ip is no longer valid, search for a new one
    //from the dns_cache
    SAH_TRACE_INFO("ACS IP is not set, search for one");
    if(dns_cache && dns_cache->nodes) {
        struct ares_addrinfo_node* ai_cur;
        char addrstr[46] = "";
        for(ai_cur = dns_cache->nodes; ai_cur != NULL; ai_cur = ai_cur->ai_next) {
            uint8_t* addr = NULL;
            switch(ai_cur->ai_family) {
            case AF_INET:
                addr = (uint8_t*) &((struct sockaddr_in*) ai_cur->ai_addr)->sin_addr;
                break;
            case AF_INET6:
                addr = (uint8_t*) &((struct sockaddr_in6*) ai_cur->ai_addr)->sin6_addr;
                break;
            }
            ares_inet_ntop(ai_cur->ai_family, addr, addrstr, sizeof(addrstr));
            SAH_TRACEZ_INFO("CWMPD", "trying address [%s]:[%d] , DNS_ttl = %d", addrstr, acs_server_port, ai_cur->ai_ttl);
            //try to connect
            if(cwmp_client_connect_to(ai_cur->ai_family, addrstr) == cwmp_status_ok) {
                //CONNECTED
                if(acs_server_ip) {
                    free(acs_server_ip);
                }
                acs_server_ip = strdup(addrstr);
                resolve_DNS = false;
                SAH_TRACEZ_INFO("CWMPD", "connected to ACS [%s]:[%d]", acs_server_ip, acs_server_port);

                if((strcmp(acsip_affinity, "0") == 0) &&
                   (amxp_timer_remaining_time(dns_ttl_timer) <= 0) &&
                   (ai_cur->ai_ttl != 0)) {
                    acs_server_ttl = ai_cur->ai_ttl;//cache ttl value
                    /* update ttl_timer */
                    amxp_timer_start(dns_ttl_timer, acs_server_ttl * 1000);
                }
                return cwmp_status_ok;
            }
        }
    } else {
        SAH_TRACEZ_ERROR("CWMPD", "no dns_cache found, not connecting to server");
    }
    return cwmp_status_ko;
}

static void cwmp_client_sessionTimedOut(UNUSED char* name) {
    unexpected_close = false;
    SAH_TRACEZ_WARNING("CWMPD", "session timed out, Force closing session");
    _closeACSSession(false);
}

static void cwmp_client_dnsTTLTimeout_handler(UNUSED amxp_timer_t* timer, UNUSED void* priv) {
    if(strcmp(acsip_affinity, "0") == 0) {
        resolve_DNS = true; // force DNS resolution if affinity is enabled
        // dont free acs_server_ip as it maybe reused if it's still
        // a valid IP
    }
}


static void cwmp_client_send_header(int msgLength) {
    char http_header[HTTP_HEADER_SIZE] = {0};
    int last_used = 0;
    unsigned char* p = (unsigned char*) http_header;
    int ret;

    memset(http_header, 0, HTTP_HEADER_SIZE);
    last_used = sprintf(http_header, "POST %s HTTP/1.1\r\n", acs_server_path);
    p += last_used;
    ret = lws_add_http_header_by_name(lws_client_wsi, (const unsigned char*) "Host:", (const unsigned char*) acs_server_host, strlen(acs_server_host), &p, (unsigned char*) (http_header + HTTP_HEADER_SIZE));
    ret += lws_add_http_header_by_name(lws_client_wsi, (const unsigned char*) "User-agent:", (const unsigned char*) USER_AGENT, strlen(USER_AGENT), &p, (unsigned char*) (http_header + HTTP_HEADER_SIZE));
    ret += lws_add_http_header_content_length(lws_client_wsi, msgLength, &p, (unsigned char*) (http_header + HTTP_HEADER_SIZE));
    ret += lws_add_http_header_by_name(lws_client_wsi, (const unsigned char*) "Content-Type:", (const unsigned char*) "text/xml; charset=ISO-8859-1", 28, &p, (unsigned char*) (http_header + HTTP_HEADER_SIZE));
    ret += lws_add_http_header_by_name(lws_client_wsi, (const unsigned char*) "SOAPAction:", (const unsigned char*) "", 0, &p, (unsigned char*) (http_header + HTTP_HEADER_SIZE));
    ret += lws_finalize_write_http_header(lws_client_wsi, (unsigned char*) http_header, &p, (unsigned char*) (http_header + HTTP_HEADER_SIZE));
    SAH_TRACEZ_INFO("CWMPD", "Send HTTP Header [code %d] : \n------------->\n%s", ret, http_header);
}

/* fetch all client info from data model and feed them to server info struct*/
cwmp_status_t cwmp_client_init() {
    SAH_TRACEZ_INFO("CWMPD", "Client initialize");
    memset(&lws_client_ctx_info, 0, sizeof lws_client_ctx_info);
    void* main_loop[1] = { cwmp_evlp_get() };
    lws_client_ctx_info.protocols = protocols;
    lws_client_ctx_info.ssl_cert_filepath = NULL;
    lws_client_ctx_info.ssl_private_key_filepath = NULL;
    lws_client_ctx_info.options = LWS_SERVER_OPTION_LIBEVENT;
    lws_client_ctx_info.foreign_loops = main_loop;
    lws_client_ctx_info.port = CONTEXT_PORT_NO_LISTEN;
    lws_client_ctx_info.user = NULL;
    lws_client_ctx_info.timeout_secs = 60;
    lws_client_ctx_info.fd_limit_per_thread = 1 + 1 + 1;

    //Create lws context
    lws_client_ctx = lws_create_context(&lws_client_ctx_info);
    if(lws_client_ctx == NULL) {
        SAH_TRACEZ_ERROR("CWMPD", "lws client context creation failed");
        return cwmp_status_ko;
    }

    //Create a client lws vhost
    lws_client_vhost = lws_create_vhost(lws_client_ctx, &lws_client_ctx_info);

    if(lws_client_vhost == NULL) {
        SAH_TRACEZ_ERROR("CWMPD", "lws failed to attribute a vhost for client");
        return cwmp_status_ko;
    }
    amxp_timer_new(&dns_ttl_timer, cwmp_client_dnsTTLTimeout_handler, NULL);
    resolve_DNS = true;

    if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_ACSIPAFFINITY, &acsip_affinity) != 0) {
        SAH_TRACEZ_ERROR("CWMPD", "DM_ENGINE: Cannot fetch ACSIPAffinity parameter");
    }
    return cwmp_status_ok;
}

/* Initialize and Start new client session */
cwmp_status_t cwmp_client_start_session() {

    cwmp_status_t ret = cwmp_status_ko;
    char* acs_url = NULL;
    //char* acs_affinity = NULL;
    const char* uriHost = NULL;
    const char* uriScheme = NULL;
    int uriPort = 0;
    const char* uriPath = NULL;
    char* crHost = NULL;

    if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_URL, &acs_url) != 0) {
        SAH_TRACEZ_ERROR("CWMPD", "Cannot fetch the ACS SERVER URL");
        goto error;
    }

    SAH_TRACEZ_INFO("CWMPD", "ACS URL = %s", acs_url);

    if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_CONNECTIONREQUESTHOST, &crHost) != 0) {
        SAH_TRACEZ_ERROR("CWMPD", "Cannot fetch the connection request host URL");
        goto error;
    }
    // Check if wan is up
    if(!(*crHost) || (strcmp(crHost, "0.0.0.0") == 0)) {
        SAH_TRACEZ_WARNING("CWMPD", "WAN is not connected, not connecting to server");
        free(crHost);
        goto error;
    }
    free(crHost);
    free(acs_server_host);
    acs_server_host = NULL;
    if(lws_parse_uri(acs_url, &uriScheme, &uriHost, &uriPort, &uriPath)) {
        SAH_TRACEZ_ERROR("CWMPD", "Couldn't parse URL (%s)", acs_url);
        goto error;
    }
    SAH_TRACEZ_INFO("CWMPD", "Host: %s | Scheme: %s | Port: %d | Path: %s", uriHost, uriScheme, uriPort, uriPath);
    acs_server_host = strdup(uriHost);

    if(uriPort == 0) {
        SAH_TRACEZ_WARNING("CWMPD", "URI port is not set, default to Port:7547");
        uriPort = 7547; // default tr-069 CWMP port 7547
    }

    acs_server_port = uriPort;
    free(acs_server_path);
    acs_server_path = strdup(uriPath);

    if(isIPAddress(uriHost)) {
        if(acs_server_ip) {
            free(acs_server_ip);
            acs_server_ip = NULL;
        }
        acs_server_ip = strdup(uriHost);
    } else if(resolve_DNS || (acs_server_ip == NULL)) {
        //free DNS cache
        if(dns_cache) {
            ares_freeaddrinfo(dns_cache);
            dns_cache = NULL;
        }
        // To prevent impacting the libdnengine internal
        // Behavior, DNS resolution should done in sync mode
        // when we starting a new session,So we should wait here
        // until it's done
        if(cwmp_dns_resolve(uriHost, &dns_cache) != cwmp_status_ok) {
            ret = cwmp_status_ko;
            goto error;
        }
        /* check if the last used IP still part of the DNS pool*/
        if(foundInDNSCache(acs_server_ip) == false) {
            //this IP is no longer valid free it
            if(acs_server_ip) {
                free(acs_server_ip);
                acs_server_ip = NULL;
            }
        }
    }
    ret = cwmp_client_connect();
error:
    free(acs_url);
    return ret;
}

cwmp_status_t cwmp_client_stop() {

    lws_vhost_destroy(lws_client_vhost);
    lws_context_destroy(lws_client_ctx);

    if(acs_server_host) {
        free(acs_server_host);
        acs_server_host = NULL;
    }

    if(acs_server_path) {
        free(acs_server_path);
        acs_server_path = NULL;
    }

    if(acs_server_ip) {
        free(acs_server_ip);
        acs_server_ip = NULL;
    }

    if(dns_cache) {
        ares_freeaddrinfo(dns_cache);
        dns_cache = NULL;
    }

    return cwmp_status_ok;
}

void cwmp_client_clear_ACSIP() {
    if(acs_server_ip) {
        free(acs_server_ip);
        acs_server_ip = NULL;
    }
    // force new DNS resolution when next session kicks in
    resolve_DNS = true;
}

/** libtr069-engine callbacks **/

int DM_CloseHttpSession(bool closeMode) {
    if(connectedToServer) {
        unexpected_close = false;
        if((closeMode != NORMAL_CLOSE) && lws_client_wsi) {
            lws_set_timeout(lws_client_wsi,
                            PENDING_TIMEOUT_KILLED_BY_PROXY_CLIENT_CLOSE,
                            LWS_TO_KILL_SYNC);
        }
        connectedToServer = false;
        free(acs_server_host);
        acs_server_host = NULL;
        free(acs_server_path);
        acs_server_path = NULL;
    }
    return 0;
}

int DM_SendHttpMessage(const char* msgToSendStr) {
    int msgLength = 0;

    if(connectedToServer == false) {
        free(pending_message);
        pending_message = strdup(msgToSendStr);
        return 0;
    }

    if(msgToSendStr != NULL) {
        msgLength = strlen(msgToSendStr);
    }

    cwmp_client_send_header(msgLength);
    SAH_TRACEZ_INFO("CWMPD", "Sending message :\n%s\n--------------->", msgToSendStr);
    DM_UpdateRetryBuffer(msgToSendStr, msgLength);
    lws_write(lws_client_wsi, (unsigned char*) msgToSendStr, strlen(msgToSendStr), LWS_WRITE_HTTP);
    //Update session Timer if Any, else start a new one
    DM_ENG_NotificationInterface_timerStart("Session-timer",
                                            session_timeout,
                                            0,
                                            cwmp_client_sessionTimedOut);

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

    SAH_TRACEZ_INFO("CWMPD", "The reamining time of DNS TTL timer is : %d", amxp_timer_remaining_time(dns_ttl_timer));

    if(cwmp_client_start_session() == cwmp_status_ko) {
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

