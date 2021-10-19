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
#include <fcntl.h>
#include <debug/sahtrace.h>
#include <dmcom/dm_com.h>
#include "dmmain/cwmpd.h"
#include "httpparser/picohttpparser.h"
#include <dmengine/DM_ENG_RPCInterface.h>
#include <stdlib.h>

static int connect_to_acs();

static char* acs_server;
static char* acs_server_ip;
int acs_server_port;
static char* acs_server_path;
static bool connectedToServer = false;
static bool retransmitLastMessage = false;
static int session_timeout = 0;
static char* lastMessage = NULL;
static char* pending_message = NULL;
static bool unexpected_close = true;
static char* read_buffer = NULL;
static int read_buffer_len = 0;

static const int DEFAULT_SESSION_TIMEOUT = 45;
static const int MAX_CONTENT_LENGTH = 33554432;


#define HTTP_HEADER_SIZE 4096
char http_header[HTTP_HEADER_SIZE] = {0};
int last_used = 0;
#define USER_AGENT      "prpl_user_agent"

static struct lws* client_wsi = NULL;
UNUSED static const char* ba_user, * ba_password; // TO DO move to client struct


/* needed by link state notifier */
// UNUSED static const lws_retry_bo_t retry = {
//     .secs_since_valid_ping = 3,
//     .secs_since_valid_hangup = 10,
// };

struct lws_context* g_lws_ctx;

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
            SAH_TRACE_ERROR("Couldn't realloc");
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
    SAH_TRACE_INFO("Message body arrived (%zu)", strlen(body));
    if(strstr(body, DM_COM_ENV_TAG) == NULL) {
        SAH_TRACE_WARNING("This is not a SOAP body\n");
    } else {
        DM_SoapXml SoapMsg;
        DM_HttpCheckNamespace(body, len);
        DM_InitSoapMsgReceived(&SoapMsg);
        if(DM_OK == DM_AnalyseSoapMessage(&SoapMsg, body, TYPE_ACS, false)) {
            DM_ParseSoapEnveloppe(SoapMsg.pBody, SoapMsg.pSoapID, SoapMsg.nHoldRequest);
        } else {
            SAH_TRACE_ERROR("Invalid SOAP Message closing session");
            _closeACSSession(false); // really put this one as last one, as it might trigger a new session
        }
        xmlDocumentFree(SoapMsg.pParser);

        DM_InitSoapMsgReceived(&SoapMsg);
    }
    return 0;
}

int handle_raw_reply(char* raw, int len) {
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
        SAH_TRACE_WARNING("header is incomplete");
        create_read_buffer(msg, len);
        return 0;
    } else if(last_len == -1) {
        SAH_TRACE_ERROR("Message is erroneous");
        reset_read_buffer();
        return 0;
    }
    content_length = get_content_length(values, header_len);
    if(content_length > MAX_CONTENT_LENGTH) {
        SAH_TRACE_WARNING("Content-length superior to %d (%d)\n", MAX_CONTENT_LENGTH, content_length);
        _closeACSSession(false);
        return 0;
    }
    if(content_length > len - last_len) {
        create_read_buffer(msg, len);
        return 0;
    }
    body = msg + last_len;
    printf("<--------------------------\n%s\n-------------------------->\n", msg);
    switch(status) {
    case 200:
        process_body(body, content_length);
        break;
    }
    reset_read_buffer();
    return 0;
}

/* http callback */
static int cwmp_client_http_callback(struct lws* wsi, enum lws_callback_reasons reason,
                                     UNUSED void* user, void* in, size_t len) {
    int status = 0;

    /* protocol logic goes here */
    switch(reason) {
    case LWS_CALLBACK_PROTOCOL_INIT:
        break;
    case LWS_CALLBACK_RAW_CONNECTED:
        connectedToServer = true;
        free(read_buffer);
        read_buffer = NULL;
        read_buffer_len = 0;
        status = (int) lws_http_client_http_response(wsi);
        SAH_TRACE_INFO(" Client: Connected to ACS server with status (%d)", status);
        if(pending_message) {
            DM_SendHttpMessage(pending_message);
        }
        break;
    case LWS_CALLBACK_RAW_CLOSE:
        connectedToServer = false;
        if(unexpected_close) {
            // lets assume session is OK otherwise : periodic inform will not be sent
            // TODO!: properly manage start/end new session ?
            _closeACSSession(true);
        }
        SAH_TRACE_INFO(" Client: Disconnected from ACS server");
        break;
    case LWS_CALLBACK_WSI_DESTROY:
        client_wsi = NULL;
        break;
    case LWS_CALLBACK_RAW_RX:
        handle_raw_reply((char*) in, len);
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

static int connect_to_acs() {
    struct lws_client_connect_info cnx_info;
    char* acs_url_local = NULL;
    const char* uriHost = NULL;
    const char* uriScheme = NULL;
    int uriPort = 0;
    const char* uriPath = NULL;
    char* crHost = NULL;
    char* acsip_affinity = NULL;
    char* acsip_ttl = NULL;
    char* acsip = NULL;

    memset(&cnx_info, 0, sizeof(cnx_info));
    cnx_info.context = g_lws_ctx;
    cnx_info.method = "RAW";
    cnx_info.protocol = protocols[0].name;
    cnx_info.ssl_connection = 0;
    cnx_info.pwsi = &client_wsi;
    cnx_info.fi_wsi_name = "user";
    unexpected_close = true;
    if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_URL, &acs_url_local) != 0) {
        SAH_TRACE_ERROR("Cannot fetch the ACS SERVER URL");
        goto exit_error;
    }
    if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_CONNECTIONREQUESTHOST, &crHost) != 0) {
        SAH_TRACE_ERROR("Cannot fetch the connection request host URL");
        goto exit_error;
    }
    // Check if wan is up
    if(!(*crHost) || (strcmp(crHost, "0.0.0.0") == 0)) {
        SAH_TRACE_WARNING("WAN is not connected, not connecting to server");
        free(crHost);
        goto exit_error;
    }
    free(crHost);
    free(acs_server);
    acs_server = NULL;
    if(lws_parse_uri(acs_url_local, &uriScheme, &uriHost, &uriPort, &uriPath)) {
        SAH_TRACE_ERROR("Couldn't parse URL (%s)", acs_url_local);
        goto exit_error;
    }
    SAH_TRACE_APP_INFO("Host: %s | Scheme: %s | Port: %d | Path: %s\n", uriHost, uriScheme, uriPort, uriPath);
    acs_server = strdup(uriHost);
    acs_server_port = uriPort;
    free(acs_server_path);
    acs_server_path = strdup(uriPath);
    if(strcmp(uriScheme, "https") == 0) {
        // Do stuff for https
    }
    cnx_info.port = acs_server_port;
    cnx_info.host = acs_server;
    cnx_info.path = acs_server_path;
    if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_ACSIPAFFINITY, &acsip_affinity) != 0) {
        SAH_TRACE_ERROR("DM_ENGINE: Cannot fetch ACSIPAffinity parameter\n");
        goto exit_error;
    }
    if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_ACSIPTTL, &acsip_ttl) != 0) {
        SAH_TRACE_ERROR("Cannot fetch the ACSIPTTL parameter\n");
        goto exit_error;
    }
    if((acsip_affinity && (strcmp(acsip_affinity, "0") == 0)) &&
       (acsip_ttl && (atoi(acsip_ttl) <= 0))) {
        if(DM_ENG_ExecuteManagementServerFunction(DM_ENG_EntityType_SYSTEM, DM_ENG_UPDATEACSIP) != 0) {
            SAH_TRACE_ERROR("Cannot execute the updateACSIP function");
            goto exit_error;
        }
    } else {
        if(acs_server_ip) {
            cnx_info.address = acs_server_ip;
            if(!lws_client_connect_via_info(&cnx_info)) {
                SAH_TRACE_ERROR("Couldn't connect to %s", acs_server_ip);
                goto exit_error;
            }
        }
        if(!client_wsi) {
            if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_ACSIP, &acsip) != 0) {
                SAH_TRACE_ERROR("Cannot fetch the ACSIP parameter");
                goto exit_error;
            }
            free(acs_server_ip);
            acs_server_ip = strdup(acsip);
            cnx_info.address = acs_server_ip;
            cnx_info.origin = acs_server_ip;
            cnx_info.host = acs_server_ip;
            if(!lws_client_connect_via_info(&cnx_info)) {
                SAH_TRACE_ERROR("Couldn't connect to %s", acs_server_ip);
                goto exit_error;
            }
        }
    }
    return 0;
exit_error:
    free(acs_url_local);
    free(acsip_affinity);
    free(acsip_ttl);
    free(acsip);
    return -1;
}

/* fetch all server info from data model and feed them to server info struct*/
cwmp_status_t cwmp_client_init(struct lws_context_creation_info* lws_ctx_info) {
    // cwmp_status_t ret = cwmp_status_ko;

    lws_ctx_info->protocols = protocols;
    lws_ctx_info->ssl_cert_filepath = NULL;
    lws_ctx_info->ssl_private_key_filepath = NULL;
    return cwmp_status_ok;
}

/* Initialize and Start new client session */
cwmp_status_t cwmp_client_start_session(struct lws_context_creation_info* lws_ctx_info,
                                        void** main_loop, struct lws_context* lws_ctx) {
    SAH_TRACE_NOTICE("Client stating session");

    lws_ctx_info->options = LWS_SERVER_OPTION_LIBEVENT;
    lws_ctx_info->foreign_loops = main_loop;
    lws_ctx_info->port = CONTEXT_PORT_NO_LISTEN;
    lws_ctx_info->user = NULL;
    lws_ctx_info->connect_timeout_secs = 60;
    lws_ctx_info->fd_limit_per_thread = 1 + 1 + 1;
    lws_ctx = lws_create_context(lws_ctx_info);
    if(lws_ctx == NULL) {
        SAH_TRACE_ERROR("lws client context creation failed");
        return cwmp_status_ko;
    }
    g_lws_ctx = lws_ctx;
    connect_to_acs();
    return cwmp_status_ok;
}

cwmp_status_t cwmp_client_stop(struct lws_context* lws_ctx) {
    lws_context_destroy(lws_ctx);
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
        free(acs_server);
        acs_server = NULL;
        free(acs_server_path);
        acs_server_path = NULL;
    }
    return 0;
}

static void client_sessionTimedOut(UNUSED char* name) {
    unexpected_close = false;
    SAH_TRACE_WARNING("session timed out\n");
    _closeACSSession(false);
}

static void send_header(int msgLength) {
    // char msgLengthStr[10] = "";
    unsigned char* p = (unsigned char*) http_header;
    int ret;

    memset(http_header, 0, HTTP_HEADER_SIZE);
    last_used = sprintf(http_header, "POST %s HTTP/1.1\r\n", acs_server_path);
    p += last_used;
    ret = lws_add_http_header_by_name(client_wsi, (const unsigned char*) "Host:", (const unsigned char*) acs_server, strlen(acs_server), &p, (unsigned char*) (http_header + HTTP_HEADER_SIZE));
    ret += lws_add_http_header_by_name(client_wsi, (const unsigned char*) "User-agent:", (const unsigned char*) USER_AGENT, strlen(USER_AGENT), &p, (unsigned char*) (http_header + HTTP_HEADER_SIZE));
    ret += lws_add_http_header_content_length(client_wsi, msgLength, &p, (unsigned char*) (http_header + HTTP_HEADER_SIZE));
    ret += lws_add_http_header_by_name(client_wsi, (const unsigned char*) "Content-Type:", (const unsigned char*) "text/xml; charset=ISO-8859-1", 28, &p, (unsigned char*) (http_header + HTTP_HEADER_SIZE));
    ret += lws_add_http_header_by_name(client_wsi, (const unsigned char*) "SOAPAction:", (const unsigned char*) "", 0, &p, (unsigned char*) (http_header + HTTP_HEADER_SIZE));
    ret += lws_finalize_write_http_header(client_wsi, (unsigned char*) http_header, &p, (unsigned char*) (http_header + HTTP_HEADER_SIZE));
    printf("ret %d <--------------------------\n%s", ret, http_header);
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

    send_header(msgLength);
    printf("%s\n-------------------------->\n", msgToSendStr);
    DM_UpdateRetryBuffer(msgToSendStr, msgLength);
    lws_write(client_wsi, (unsigned char*) msgToSendStr, strlen(msgToSendStr), LWS_WRITE_HTTP);
    SAH_TRACE_INFO("Message sent (%d).", msgLength);
    DM_ENG_NotificationInterface_timerStart("Session-timer", session_timeout, 0, client_sessionTimedOut);
    if(msgToSendStr == pending_message) {
        free(pending_message);
        pending_message = NULL;
    }
    return 0;
}

int client_startSession() {
    SAH_TRACE_INFO("HTTP start session");
    if(connectedToServer) {
        return 0;
    }
    char* acs_url = NULL;
    char* s_timeout = NULL;
    retransmitLastMessage = false;
    lastMessage = NULL;

    connect_to_acs();
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
    SAH_TRACE_INFO("Session timeout = %d", session_timeout);
    DM_ENG_NotificationInterface_timerStart("Session-timer", session_timeout, 0, client_sessionTimedOut);

    free(acs_url);
    return 0;

// error:
//     free(acs_url);
//     return -1;
}

void cwmp_client_clear_ACSIP() {
    if(acs_server_ip) {
        free(acs_server_ip);
        acs_server_ip = NULL;
    }
}
