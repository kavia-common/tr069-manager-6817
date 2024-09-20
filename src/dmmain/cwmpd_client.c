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

/**********************************************************
* Include files
**********************************************************/
#include <stdarg.h>
#include <stdio.h>
#include <debug/sahtrace.h>
#include <debug/sahtrace_macros.h>
#include <amxc/amxc_macros.h>
#include <dmcom/dm_com.h>
#include "dmmain/cwmpd.h"
#include <dmengine/DM_ENG_RPCInterface.h>
#include <dmengine/DM_ENG_Mapping.h>
#include <dmcom/dm_com_digest.h>
#include <stdlib.h>

#include <ares.h>
#include <arpa/inet.h>
#include <event2/event.h>
#include <string.h>

/**********************************************************
* Macro/Const definitions
**********************************************************/
#define DEFAULT_SESSION_TIMEOUT 45
#define CNONCE_SIZE 16

#define CWMP_HTTP_CALLBACK_CONTINUE (1)
#define CWMP_HTTP_CALLBACK_ERROR (-1)
#define CWMP_HTTP_CALLBACK_SKIP (0)

static const unsigned char USER_AGENT[] = "prpl_user_agent";
static const unsigned char CONTENT_TYPE[] = "text/xml; charset=ISO-8859-1";
static const unsigned char SOAP_HEADER[] = "SOAPAction:";
static const unsigned char EMPTY_USTR[] = "";

/**********************************************************
* Variable declarations
**********************************************************/

typedef enum auth_e {
    auth_basic = 0,
    auth_digest,
    auth_unsupported,
    auth_none
} auth_t;

auth_t auth_type = auth_none;                     /* Auth method */
static char* acs_server_host = NULL;              /* ACS server hostname */
static char* acs_server_ip = NULL;                /* ACS server ip address */
static int acs_server_port = -1;                  /* ACS server port */
static char* acs_server_path = NULL;              /* ACS server path */
static char* acs_server_scheme = NULL;            /* ACS server connection scheme */
int http_status = 0;                              /* ACS last http return code */
static char* rcv_buf = NULL;                      /* Buffer for SOAP message received from ACS */
static size_t rcv_buf_len = 0;                    /* SOAP buffer size */
static int session_timeout = 0;                   /* session timeout  */
static char* pending_msg = NULL;                  /* SOAP message waiting to be sent to ACS */
static char* session_cookie = NULL;               /* HTTP session Cookie if any */
static char* auth_hdr = NULL;                     /* Auth headers */
static bool connected = false;
static struct lws* lws_client_wsi = NULL;         /* client ws interface */
static struct lws_context* lws_client_ctx = NULL; /* client lws context */
static struct lws_context_creation_info lws_client_ctx_info;
static struct lws_client_connect_info lws_connect_info;

//lws cannot handle session cookie
//check LWS_WITH_CACHE_NSCOOKIEJAR for more info
static void cwmp_client_parse_cookie(char* cookies) {
    char* tok = strtok(cookies, ";");
    int total_len = 0;
    int buff_len = 256;
    CWMPD_FREE(session_cookie);
    if(cookies && !tok) {
        session_cookie = strdup(cookies);
    } else {
        session_cookie = calloc(1, buff_len * sizeof(char));
        if(!session_cookie) {
            SAH_TRACEZ_ERROR("CWMPD", "malloc failed for session cookies!!");
            return;
        }
    }

    while(tok) {
        char* p = strchr((char*) tok, '=');
        char* start = tok;
        char* end = p;
        int len = 0;
        if(p) { //skip white spaces
            while(start && (*start) == ' ') {
                start++;
            }
            while(end && (*end) == ' ') {
                end--;
            }
            len = end - start;
        }
        if((len > 0) && (strncasecmp(start, "Path", len) != 0) && (strncasecmp(start, "Max-Age", len) != 0)
           && (strncasecmp(start, "Expires", len) != 0) && (strncasecmp(start, "SameSite", len) != 0)
           && (strncasecmp(start, "Domain", len) != 0)) {
            total_len += (len + 1);
            if(total_len >= buff_len) {
                session_cookie = (char*) realloc(session_cookie, (buff_len + 256 + 1) * sizeof(char));
                buff_len += 256;
                if(!session_cookie) {
                    SAH_TRACEZ_ERROR("CWMPD", "Couldn't realloc");
                    return;
                }
                session_cookie[buff_len] = '\0';
            }
            strcat(session_cookie, start);
            strcat(session_cookie, ";");
        }
        tok = strtok(NULL, ";");
    }
}

/*generate Basic auth data*/
static char* cwmp_client_auth_basic(const char* username, const char* passwd) {
    char* credentials = NULL;
    char* result = NULL;
    int len = strlen(username) + strlen(passwd) + 1;
    int hdr_size = 6 + ((4 * (len + 2)) / 3) + 2;

    credentials = calloc(1, (len + 1) * sizeof(char));
    if(!credentials) {
        goto stop;
    }

    result = calloc(1, hdr_size * sizeof(char));
    if(!result) {
        goto stop;
    }

    sprintf(result, "Basic ");
    sprintf(credentials, "%s:%s", username, passwd);

    //base64 encode
    if(lws_b64_encode_string(credentials, len, result + 6, hdr_size - 6) < 0) {
        SAH_TRACEZ_ERROR("CWMPD", "Failed to encode user/passwd to base64 %s", result);
        CWMPD_FREE(result);
        goto stop;
    }
    result[hdr_size - 1] = '\0';

stop:
    CWMPD_FREE(credentials);
    return result;
}

/* generate authentication headers */
static cwmp_status_t cwmp_client_gen_auth_hdr(auth_t type, char* auth_d) {
    cwmp_status_t ret = cwmp_status_ko;
    char* username = NULL;
    char* passwd = NULL;
    char* cnonce = NULL;

    if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_USERNAME, &username) != 0) {
        SAH_TRACEZ_ERROR("CWMPD", "failed to fetch acs username");
        goto stop;
    }
    if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_PASSWORD, &passwd) != 0) {
        SAH_TRACEZ_ERROR("CWMPD", "failed to fetch acs password");
        goto stop;
    }
    CWMPD_FREE(auth_hdr);

    if(type == auth_basic) {
        auth_hdr = cwmp_client_auth_basic(username, passwd);
    } else if(type == auth_digest) {
        cnonce = malloc(CNONCE_SIZE + 1);
        _generateRandomString(cnonce, CNONCE_SIZE);
        auth_hdr = DM_COM_GenerateDigestResponse_MD5(auth_d, cnonce, acs_server_path, "POST", username, passwd);
    } else {
        SAH_TRACEZ_ERROR("CWMPD", "ACS requested an unsupported auth method [%s]", auth_d);
        ret = cwmp_status_ko;
    }

    if(auth_hdr) {
        ret = cwmp_status_ok;
    }
stop:
    CWMPD_FREE(username);
    CWMPD_FREE(passwd);
    CWMPD_FREE(cnonce);
    return ret;
}

/* resend the message with authentication*/
static void cwmp_client_auth_retry() {
    DM_SendHttpMessage(pending_msg);
}

static void cwmp_client_sessionTimedOut(UNUSED char* name) {
    SAH_TRACEZ_WARNING("CWMPD", "session timed out, Force closing session");
    _closeACSSession(false);
}

/* connection error callbacks */
static int cwmp_client_connection_error_cb(void* error) {
    char* acsurl = NULL;

    if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_URL, &acsurl) != 0) {
        SAH_TRACEZ_ERROR("CWMPD", "Failed to get ACS URL");
    }
    SAH_TRACEZ_ERROR("CWMPD", "CONNECTION_ERROR: [%s], ACS URL: [%s], IP: [%s]",
                     error ? (char*) error : "(null)",
                     acsurl ? acsurl : "(null)",
                     acs_server_ip ? acs_server_ip : "(null)");
    _closeACSSession(false);
    free(acsurl);
    return CWMP_HTTP_CALLBACK_CONTINUE;
}

static int cwmp_client_new_wsi_cb() {
    CWMPD_FREE(rcv_buf);
    rcv_buf_len = 0;
    return CWMP_HTTP_CALLBACK_CONTINUE;
}

static void cwmp_client_get_authentication_hdr(struct lws* wsi) {
    char auth_d[512];
    memset(auth_d, 0x00, 512);
    auth_type = auth_unsupported;

    int auth_len = lws_hdr_total_length(wsi, WSI_TOKEN_HTTP_WWW_AUTHENTICATE);

    if(lws_hdr_copy(wsi, auth_d, auth_len + 1,
                    WSI_TOKEN_HTTP_WWW_AUTHENTICATE) < 0) {
        SAH_TRACEZ_ERROR("CWMPD", "error when copying auth data");
        //do nothing , going to close the session
    } else {
        if(strstr(auth_d, "Basic")) {
            auth_type = auth_basic;
        } else if(strstr(auth_d, "Digest")) {
            auth_type = auth_digest;
        }

        if(cwmp_client_gen_auth_hdr(auth_type, auth_d) != cwmp_status_ok) {
            SAH_TRACEZ_ERROR("CWMPD", "failed to generate auth headers");
        }
    }
}

static int cwmp_client_handle_cookies(struct lws* wsi) {
    SAH_TRACEZ_INFO("CWMPD", "Handle cookies");
    int cookie_len = lws_hdr_total_length(wsi, WSI_TOKEN_HTTP_SET_COOKIE);
    if(cookie_len > 0) {
        //try to get session cookies
        char* server_cookie = malloc(cookie_len + 1);
        if(!server_cookie) {
            SAH_TRACEZ_ERROR("CWMPD", "malloc fail !!!");
            return CWMP_HTTP_CALLBACK_ERROR;
        }
        memset(server_cookie, 0, cookie_len + 1);

        if(lws_hdr_copy(wsi, server_cookie, cookie_len + 1,
                        WSI_TOKEN_HTTP_SET_COOKIE) < 0) {
            SAH_TRACEZ_ERROR("CWMPD", "error when copying cookies ???");
        }
        //rebuild session cookie
        cwmp_client_parse_cookie(server_cookie);
        CWMPD_FREE(server_cookie);
    }
    return CWMP_HTTP_CALLBACK_CONTINUE;
}

static int cwmp_client_connection_established_cb(struct lws* wsi) {
    int ret = CWMP_HTTP_CALLBACK_CONTINUE;

    if(cwmp_socket_set_dscp(wsi) != 0) {
        SAH_TRACEZ_WARNING("CWMPD", "Failed to set DSCP value to client connection");
    }

    http_status = lws_http_client_http_response(wsi);
    SAH_TRACEZ_INFO("CWMPD", "Server return code [%d]", http_status);

    if(http_status == HTTP_NO_CONTENT) {
        if(DM_ENG_IsReadyToClose(DM_ENG_EntityType_ACS) && DM_COM_EmptyHTTPPostSent()) {
            SAH_TRACEZ_INFO("CWMPD", "Close Session");
            lws_wsi_close(wsi, LWS_TO_KILL_ASYNC);
        }

        //closing free any message
        CWMPD_FREE(pending_msg);
    } else if(http_status == HTTP_STATUS_UNAUTHORIZED) {
        //get the WWW-Authenticate headers
        cwmp_client_get_authentication_hdr(wsi);
        //jump to LWS_CALLBACK_CLOSED_CLIENT_HTTP
        lws_wsi_close(wsi, LWS_TO_KILL_ASYNC);

    } else if((http_status == HTTP_OK)) {
        if(!connected) {
            connected = true;
            //store ACSIP in persistent storage
            if(DM_ENG_SetManagementServerValue(DM_ENG_EntityType_SYSTEM,
                                               DM_ENG_ACSIP,
                                               acs_server_ip) != 0) {
                SAH_TRACEZ_ERROR("CWMPD", "ACSIP failed to update data model");
            }
        }
        ret = cwmp_client_handle_cookies(wsi);
    }
    return ret;
}

static int cwmp_client_recieve_raw_cb(struct lws* wsi) {
    //receive raw data
    char buf[1024 + LWS_PRE];
    char* pre_buf = buf + LWS_PRE;
    int len = sizeof(buf) - LWS_PRE;

    if(lws_http_client_read(wsi, &pre_buf, &len) < 0) {
        SAH_TRACEZ_ERROR("CWMPD", "failed to read data from socket ?");
        return CWMP_HTTP_CALLBACK_ERROR;
    }
    return CWMP_HTTP_CALLBACK_SKIP; /* SKIP lws http_callback */
}

static int cwmp_client_receive_http_cb(void* in, size_t len) {
    size_t rlen = len;
    const char* msg = (const char*) in;
    if(!rcv_buf) { // create the buffer
        rcv_buf = malloc((rlen + 1) * sizeof(char));
        if(rcv_buf == NULL) {
            SAH_TRACEZ_ERROR("CWMPD", "malloc failed");
            return CWMP_HTTP_CALLBACK_ERROR;
        }
    } else { // grow
        rcv_buf = (char*) realloc(rcv_buf, (rcv_buf_len + rlen + 1) * sizeof(char));
        if(!rcv_buf) {
            SAH_TRACEZ_ERROR("CWMPD", "Couldn't realloc");
            return CWMP_HTTP_CALLBACK_ERROR;
        }
    }
    //copy data
    lws_strnncpy(rcv_buf + rcv_buf_len, msg, rlen, rlen + 1);
    rcv_buf_len += rlen;
    rcv_buf[rcv_buf_len] = '\0';
    return CWMP_HTTP_CALLBACK_SKIP;
}

static int cwmp_client_http_complete_cb() {
    // check if we have server demand
    if(http_status == HTTP_OK) {
        SAH_TRACEZ_INFO("CWMPD", "Received Message: \n<-------------\n %s \n <-------------\n", rcv_buf);
        // check if we have a soap message
        if(strstr(rcv_buf, DM_COM_ENV_TAG)) {
            DM_SoapXml SoapMsg;
            DM_ENG_Mapping_smm_translate(&rcv_buf, &rcv_buf_len, true);
            DM_HttpCheckNamespace(rcv_buf, rcv_buf_len);
            DM_InitSoapMsgReceived(&SoapMsg);
            if(DM_OK == DM_AnalyseSoapMessage(&SoapMsg, rcv_buf, TYPE_ACS, false)) {
                DM_ParseSoapEnveloppe(SoapMsg.pBody, SoapMsg.pSoapID, SoapMsg.nHoldRequest);
            } else {
                SAH_TRACEZ_ERROR("CWMPD", "SOAP Message is not valid closing the session");
                _closeACSSession(false);
            }
            xmlDocumentFree(SoapMsg.pParser);
        } else {
            SAH_TRACEZ_WARNING("CWMPD", "this is not a SOAP message \n %s \n", rcv_buf);
        }
    }
    return CWMP_HTTP_CALLBACK_CONTINUE;
}

static int cwmp_client_handshake_cb(struct lws* wsi, void* in, size_t len) {
    SAH_TRACEZ_INFO("CWMPD", "Write HTTP Headers to wsi");
    int msg_len = 0;
    unsigned char** p = (unsigned char**) in;
    unsigned char* end = (*p) + len;

    when_null(pending_msg, error);
    msg_len = strlen(pending_msg);
    when_false(lws_add_http_header_by_token(wsi, WSI_TOKEN_HTTP_USER_AGENT, USER_AGENT, 15, p, end) == 0, error);
    when_false(lws_add_http_header_content_length(wsi, msg_len, p, end) == 0, error);

    if(session_cookie) { //Handle session cookie if any
        when_false(lws_add_http_header_by_token(wsi, WSI_TOKEN_HTTP_COOKIE,
                                                (const unsigned char*) session_cookie,
                                                strlen(session_cookie), p, end) == 0, error);
    }
    if(auth_hdr) { //Handle Authentication
        when_false(lws_add_http_header_by_token(wsi, WSI_TOKEN_HTTP_AUTHORIZATION,
                                                (unsigned char*) auth_hdr,
                                                strlen(auth_hdr), p, end) == 0, error);
    }

    if(msg_len > 0) {
        when_false(lws_add_http_header_by_token(wsi, WSI_TOKEN_HTTP_CONTENT_TYPE, CONTENT_TYPE, 28, p, end) == 0, error);
        when_false(lws_add_http_header_by_name(wsi, SOAP_HEADER, EMPTY_USTR, 0, p, end) == 0, error);
    }


    //headers finished, tell lws the message body is awaiting
    lws_client_http_body_pending(wsi, 1);
    return CWMP_HTTP_CALLBACK_CONTINUE;
error:
    SAH_TRACEZ_ERROR("CWMPD", "Can not write Headers to WSI, closing connection");
    return CWMP_HTTP_CALLBACK_ERROR; //We couldn't wrie Headers something went wrong
}

static int cwmp_client_http_writable_cb(struct lws* wsi) {
    if(pending_msg) {
        lws_write(wsi, (unsigned char*) pending_msg, strlen(pending_msg), LWS_WRITE_HTTP);
        lws_client_http_body_pending(wsi, 0);// stop calling on_writable_cb
    }
    return CWMP_HTTP_CALLBACK_CONTINUE;
}

static int cwmp_client_connection_closed_cb() {
    SAH_TRACEZ_INFO("CWMPD", "Close connection");
    /* this client is closing , check its state and update dmengine */
    if(http_status == HTTP_NO_CONTENT) {
        _closeACSSession(true);//ACS session finished OK
    } else if(http_status == HTTP_STATUS_UNAUTHORIZED) {
        if((auth_type != auth_unsupported) && auth_hdr) {
            SAH_TRACEZ_INFO("CWMPD", "Reconnection ...");
            DM_ENG_NotificationInterface_timerStart("cwmp_client_auth_retry",
                                                    0,
                                                    0,
                                                    cwmp_client_auth_retry);
        } else {
            _closeACSSession(false);
        }
    } else if(http_status != HTTP_OK) {
        SAH_TRACEZ_ERROR("CWMPD", "ACS Session failed, http error[%d]", http_status);
        _closeACSSession(false);
    }//else HTTP_OK session still in progress more messages are comming
    return CWMP_HTTP_CALLBACK_CONTINUE;
}

static int cwmp_client_http_callback(struct lws* wsi, enum lws_callback_reasons reason,
                                     void* user, void* in, size_t len) {
    int ret = CWMP_HTTP_CALLBACK_CONTINUE;
    switch(reason) {
    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        ret = cwmp_client_connection_error_cb(in);
        break;
    case LWS_CALLBACK_SERVER_NEW_CLIENT_INSTANTIATED:
        ret = cwmp_client_new_wsi_cb();
        break;
    case LWS_CALLBACK_ESTABLISHED_CLIENT_HTTP:
        ret = cwmp_client_connection_established_cb(wsi);
        break;
    case LWS_CALLBACK_RECEIVE_CLIENT_HTTP:
        ret = cwmp_client_recieve_raw_cb(wsi);
        break;
    case LWS_CALLBACK_RECEIVE_CLIENT_HTTP_READ:
        ret = cwmp_client_receive_http_cb(in, len);
        break;
    case LWS_CALLBACK_COMPLETED_CLIENT_HTTP:
        ret = cwmp_client_http_complete_cb();
        break;
    case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER:
        ret = cwmp_client_handshake_cb(wsi, in, len);
        break;
    case LWS_CALLBACK_CLIENT_HTTP_WRITEABLE:
        ret = cwmp_client_http_writable_cb(wsi);
        break;
    case LWS_CALLBACK_CLOSED_CLIENT_HTTP:
        ret = cwmp_client_connection_closed_cb();
        break;
    default:
        break;
    }

    if(ret != CWMP_HTTP_CALLBACK_CONTINUE) {
        return ret;
    } else {
        return lws_callback_http_dummy(wsi, reason, user, in, len);
    }
}

/* websocket configuration struct , protocol : http */
static const struct lws_protocols protocols[] =
{
    {"http", cwmp_client_http_callback, 0, 0, 0, NULL, 0},
    { NULL, NULL, 0, 0, 0, NULL, 0} /* mark protocol end */
};

static void cwmp_client_https_session_init() {
    char* ssl_validate_hosname = NULL;
    char* ssl_accept_self_signed = NULL;
    char* ssl_accept_expied = NULL;
    char* ntp_status = NULL;
    lws_connect_info.ssl_connection |= LCCSCF_USE_SSL;    // enable secure connection

    //check LWS_CALLBACK_OPENSSL_LOAD_EXTRA_CLIENT_VERIFY_CERTS
    //for the problem of no hostname validation
    if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_SSLVERIFYHOSTNAME, &ssl_validate_hosname) == 0) {
        if(atol(ssl_validate_hosname) == 0) {
            lws_connect_info.ssl_connection |= LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK;
        }
        free(ssl_validate_hosname);
    }
    if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_SSLACCEPTSELFSIGNED, &ssl_accept_self_signed) == 0) {
        if(atol(ssl_accept_self_signed)) {
            lws_connect_info.ssl_connection |= LCCSCF_ALLOW_SELFSIGNED;
        }
        free(ssl_accept_self_signed);
    }
    if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_SSLACCEPTEXPIRED, &ssl_accept_expied) != 0) {
        SAH_TRACEZ_ERROR("CWMPD", "failed to fetch the accept expired flag");
    } else if(strcmp(ssl_accept_expied, "Always") == 0) {
        lws_connect_info.ssl_connection |= LCCSCF_ALLOW_EXPIRED;    /* Always accept expired cert*/
    } else if(strcmp(ssl_accept_expied, "NTP") == 0) {
        if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_NTPSTATUS, &ntp_status) != 0) {
            SAH_TRACEZ_ERROR("CWMPD", "failed to fetch the NTP status");
        } else if(ntp_status && (strcmp(ntp_status, "Synchronized") != 0)) {
            // ntp is not synchronized, no expiration date check
            lws_connect_info.ssl_connection |= LCCSCF_ALLOW_EXPIRED;
            free(ntp_status);
        }
    }
    free(ssl_accept_expied);
}

static void cwmp_client_prepare_session() {
    // connect using info
    memset(&lws_connect_info, 0, sizeof lws_connect_info);

    lws_connect_info.context = lws_client_ctx;
    lws_connect_info.method = "POST";
    lws_connect_info.protocol = protocols[0].name;
    lws_connect_info.pwsi = &lws_client_wsi;
    lws_connect_info.port = acs_server_port;
    lws_connect_info.path = acs_server_path;
    lws_connect_info.alpn = "http/1.1";
    lws_connect_info.protocol = protocols[0].name;
    lws_connect_info.ssl_connection = LCCSCF_PIPELINE;

    /* https stuff */
    if(strcmp(acs_server_scheme, "https") == 0) {
        cwmp_client_https_session_init();
    }
}

/*get the next ACS IP address*/
static cwmp_status_t cwmp_client_get_acsip() {
    cwmp_status_t ret = cwmp_status_ko;

    if(is_ipaddr(acs_server_host)) {
        CWMPD_FREE(acs_server_ip);
        acs_server_ip = strdup(acs_server_host);
    } else {
        //try to reuse the same ip from the last session
        SAH_TRACEZ_INFO("CWMPD", "Try to reuse the same ip from the last session");
        if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM,
                                           DM_ENG_ACSIP,
                                           &acs_server_ip) != 0) {
            SAH_TRACEZ_ERROR("CWMPD", "Cannot fetch the ACS SERVER URL");
        }
        /* check if the last used IP still part of the DNS pool*/
        if(acs_server_ip && (strlen(acs_server_ip) != 0)) {
            char* ip_list = NULL;
            bool valid_ip = false;
            if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM,
                                               DM_ENG_ACSIPLIST,
                                               &ip_list) != 0) {
                SAH_TRACEZ_ERROR("CWMPD", "Cannot fetch the ACS SERVER URL");
                return ret;
            }
            SAH_TRACEZ_INFO("CWMPD", "ACSIP LIST is %s", ip_list);
            valid_ip = (ip_list != NULL) && strstr(ip_list, acs_server_ip);

            if(!valid_ip) {
                //the IP is no longer part of the dns_pool
                CWMPD_FREE(acs_server_ip);
                cwmp_dns_get_random_ip(&acs_server_ip);
            }
            CWMPD_FREE(ip_list);
        } else {
            CWMPD_FREE(acs_server_ip);
            cwmp_dns_get_random_ip(&acs_server_ip);
        }
    }

    if(acs_server_ip) {
        ret = cwmp_status_ok;
    }
    return ret;
}

static cwmp_status_t cwmp_client_parse_url() {
    cwmp_status_t ret = cwmp_status_ko;
    char* acsurl = NULL;
    const char* host = NULL;
    const char* scheme = NULL;
    int port = 0;
    const char* path = NULL;

    if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_URL, &acsurl) != 0) {
        SAH_TRACEZ_ERROR("CWMPD", "Cannot fetch the ACS SERVER URL");
        goto error;
    }

    SAH_TRACEZ_INFO("CWMPD", "ACS URL [%s]", acsurl);

    if(lws_parse_uri(acsurl, &scheme, &host, &port, &path)) {
        SAH_TRACEZ_ERROR("CWMPD", "Couldn't parse URL (%s)", acsurl);
        goto error;
    }

    acs_server_host = strdup(host);
    if(path) {
        if(*path != '/') {
            //insert "/" in the front of path lws remove it by default
            acs_server_path = calloc(1, sizeof(char) * (strlen(path) + 2));
            sprintf(acs_server_path, "/%s", path);
        } else {
            acs_server_path = strdup(path);
        }
    }
    acs_server_port = port;
    acs_server_scheme = strdup(scheme);

    if(acs_server_port <= 0) {
        SAH_TRACEZ_WARNING("CWMPD", "URI port is not set, default to Port:7547");
        acs_server_port = 7547; // default tr-069 CWMP port 7547
    }

    SAH_TRACEZ_INFO("CWMPD", "Host: %s | Scheme: %s | Port: %d | Path: %s",
                    acs_server_host,
                    acs_server_scheme,
                    acs_server_port,
                    acs_server_path);

    ret = cwmp_status_ok;
error:
    free(acsurl);
    return ret;
}

/* fetch all client info from data model and feed them to server info struct*/
cwmp_status_t cwmp_client_init() {
    SAH_TRACEZ_INFO("CWMPD", "Client initialize");
    //static struct lws_context_creation_info lws_client_ctx_info;
    memset(&lws_client_ctx_info, 0, sizeof(lws_client_ctx_info));
    void* main_loop[1] = { cwmp_evlp_get() };
    application_t app_conf = cwmp_app_getconf();
    lws_client_ctx_info.protocols = protocols;
    /* client cert ,will be sent to the server if he asked for */
    lws_client_ctx_info.client_ssl_cert_filepath = app_conf.ssl_client_cert;
    /* client private key if he has a certeficate */
    lws_client_ctx_info.client_ssl_private_key_filepath = app_conf.ssl_client_priv_key;
    /* A CA cert and CRL can be used to validate the cert send by the server */
    lws_client_ctx_info.client_ssl_ca_filepath = app_conf.trustedCA;
    lws_client_ctx_info.options = LWS_SERVER_OPTION_LIBEVENT | LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    lws_client_ctx_info.foreign_loops = main_loop;
    lws_client_ctx_info.port = CONTEXT_PORT_NO_LISTEN;
    lws_client_ctx_info.user = NULL;
    lws_client_ctx_info.timeout_secs = 60;
    /* 3 client/fd/sockets max*/
    lws_client_ctx_info.fd_limit_per_thread = 3;

    //Create lws context
    lws_client_ctx = lws_create_context(&lws_client_ctx_info);
    if(lws_client_ctx == NULL) {
        SAH_TRACEZ_ERROR("CWMPD", "lws_client context creation failed");
        return cwmp_status_ko;
    }

    return cwmp_status_ok;
}

cwmp_status_t cwmp_client_stop() {
    lws_context_destroy(lws_client_ctx);
    DM_CloseHttpSession(true);
    return cwmp_status_ok;
}

void cwmp_client_clear_ACSIP() {
    SAH_TRACEZ_INFO("CWMPD", "CLEAN ACSIP From data model");
    if(DM_ENG_SetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_ACSIP, "") != 0) {
        SAH_TRACEZ_ERROR("CWMPD", "ACSIP failed to update data model");
    }
    if(DM_ENG_SetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_ACSIPLIST, "") != 0) {
        SAH_TRACEZ_ERROR("CWMPD", "failed to update data model ACSIPLIST");
    }
    CWMPD_FREE(acs_server_ip);
}

/** libtr069-engine callbacks **/
int DM_CloseHttpSession(bool closeMode) {
    if(closeMode == true) {
        SAH_TRACEZ_INFO("CWMPD", "ACS Session finished with success");
    } else {
        SAH_TRACEZ_ERROR("CWMPD", "ACS session failed");
    }
    http_status = 0;
    auth_type = auth_none;
    connected = false;
    CWMPD_FREE(acs_server_host);
    CWMPD_FREE(acs_server_path);
    CWMPD_FREE(acs_server_scheme);
    CWMPD_FREE(acs_server_ip);
    CWMPD_FREE(pending_msg);
    CWMPD_FREE(session_cookie);
    CWMPD_FREE(rcv_buf);
    CWMPD_FREE(auth_hdr);
    rcv_buf_len = 0;
    return 0;
}

int DM_SendHttpMessage(const char* soap_msg) {
    int msg_len = 0;
    if(soap_msg == NULL) {
        SAH_TRACEZ_WARNING("CWMPD", "Invalid SOAP message");
        return -1;
    }
    msg_len = strlen(soap_msg);

    SAH_TRACEZ_INFO("CWMPD", "sending a new soap message \n >>>>>>>>>>> \n %s \n >>>>>>>>>>>> \n",
                    (msg_len != 0) ? soap_msg : "EMPTY MESSAGE");

    if(pending_msg != soap_msg) {
        CWMPD_FREE(pending_msg);
        pending_msg = strdup(soap_msg);
        size_t pending_msg_len = strlen(pending_msg);
        DM_ENG_Mapping_smm_translate(&pending_msg, &pending_msg_len, false);
        DM_UpdateRetryBuffer(soap_msg, msg_len);
    }

    if(acs_server_host == NULL) {
        return -1;
    } else {
        lws_connect_info.address = acs_server_host;
        // link : https://www.rfc-editor.org/rfc/rfc7230#section-5.4
        //work arround the problem of the broken pipelining
        //on TLS this work around doesnt work?
        //again the only solution is to use lws internal DNS handler
        //check https://github.com/warmcat/libwebsockets/issues/2575
        lws_connect_info.host = acs_server_host;
        lws_connect_info.origin = acs_server_host;
        //try to send the message
        if(!lws_client_connect_via_info(&lws_connect_info)) {
            SAH_TRACEZ_ERROR("CWMPD", "Couldn't connect to %s", acs_server_host);
            CWMPD_FREE(pending_msg);
            return -1;
        }
    }

    DM_ENG_NotificationInterface_timerStart("Session-timer",
                                            session_timeout,
                                            0,
                                            cwmp_client_sessionTimedOut);
    return 0;
}

int client_startSession() {
    char* crhost = NULL;
    char* s_timeout = NULL;

    if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM,
                                       DM_ENG_CONNECTIONREQUESTHOST,
                                       &crhost) != 0) {
        SAH_TRACEZ_ERROR("CWMPD", "Cannot fetch the connection request URL");
        return -1;
    }
    // Check if wan is up
    if(!(*crhost) || (strcmp(crhost, "0.0.0.0") == 0)) {
        SAH_TRACEZ_ERROR("CWMPD", "WAN is not connected, not connecting to server");
        CWMPD_FREE(crhost);
        return -1;
    }
    CWMPD_FREE(crhost);

    if(cwmp_client_parse_url() == cwmp_status_ko) {
        SAH_TRACEZ_ERROR("CWMPD", "Cannot parse ACS url");
        return -1;
    }

    if(cwmp_client_get_acsip() == cwmp_status_ko) {
        SAH_TRACEZ_ERROR("CWMPD", "Cannot get a valid ACS ip");
        return -1;
    }

    cwmp_client_prepare_session();

    if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_SESSIONTIMEOUT, &s_timeout) == 0) {
        if(session_timeout) {
            session_timeout = atoi(s_timeout);
            free(s_timeout);
        }
    }
    if(session_timeout <= 0) {
        session_timeout = DEFAULT_SESSION_TIMEOUT;
    }

    DM_ENG_NotificationInterface_timerStart("Session-timer", session_timeout, 0, cwmp_client_sessionTimedOut);

    return 0;
}

