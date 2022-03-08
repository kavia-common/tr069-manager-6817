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
#include <debug/sahtrace.h>
#include <dmcom/dm_com.h>
#include "dmmain/cwmpd.h"
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
* Macro/Const definitions
**********************************************************/
#define DEFAULT_SESSION_TIMEOUT 45

static const unsigned char USER_AGENT[] = "prpl_user_agent";
static const unsigned char CONTENT_TYPE[] = "text/xml; charset=ISO-8859-1";
static const unsigned char CONNECTION_KEEP_ALIVE[] = "keep-alive";
static const unsigned char SOAP_HEADER[] = "SOAPAction:";
static const unsigned char EMPTY_USTR[] = "";

/**********************************************************
* Variable declarations
**********************************************************/
static char* acs_server_host = NULL;              /* ACS server hostname */
static char* acs_server_ip = NULL;                /* ACS server ip address */
static int acs_server_port = -1;                  /* ACS server port */
static char* acs_server_path = NULL;              /* ACS server path */
static char* acs_server_scheme = NULL;            /* ACS server connection scheme */
int http_status = 0;                              /* ACS last http return code */
static char* rcv_buf = NULL;                      /* Buffer for SOAP message received from ACS */
static int rcv_buf_len = 0;                       /* SOAP buffer size */
static int retry_count = 0;                       /* retry counter */
static int session_timeout = 0;                   /* session timeout  */
static char* pending_msg = NULL;                  /* SOAP message waiting to be sent to ACS */
// TODO : proper cookie parser/handler
static char* session_cookie = NULL;               /* HTTP session Cookie if any */

static struct lws* lws_client_wsi = NULL;         /* client ws interface */
static struct lws_context* lws_client_ctx = NULL; /* client lws context */
static struct lws_context_creation_info lws_client_ctx_info;
static struct lws_client_connect_info lws_connect_info;

static bool is_ipaddr(const char* ip) {
    struct in6_addr result;
    int res = inet_pton(AF_INET, ip, &result);
    if(res) {
        return true;
    }
    res = inet_pton(AF_INET6, ip, &result);
    if(res) {
        return true;
    }
    return false;
}

static void cwmp_free(char** val) {
    if(*val) {
        free(*val);
        *val = NULL;
    }
}

/*retry if this is an inform message*/
static void cwmp_client_retry() {
    cwmp_free(&acs_server_ip);
    cwmp_dns_getRandomIP(&acs_server_ip);
    if(!acs_server_ip) {
        // just close the session
        _closeACSSession(false);
    }
    // retry
    DM_SendHttpMessage(pending_msg);
}

/* http callback */
static int cwmp_client_http_callback(struct lws* wsi, enum lws_callback_reasons reason,
                                     UNUSED void* user, void* in, size_t len) {

    switch(reason) {
    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        SAH_TRACEZ_INFO("CWMPD", "CONNECTION_ERROR: %s, retry_count %d",
                        in ? (char*) in : "(null)", retry_count);
        lws_client_wsi = NULL;
        retry_count++;
        //(retry_count * connection_timeout) must stay below session timeout default 45sec
        // retry only we are sending an inform message
        if(!is_ipaddr(acs_server_host)
           && pending_msg
           && strstr(pending_msg, "Inform")
           && ((session_timeout - (retry_count * 5) - 10) > 5)) {
            //one more shot, retry in 1 sec
            DM_ENG_NotificationInterface_timerStart("cwmp_client_retry",
                                                    1,
                                                    0,
                                                    cwmp_client_retry);
        } else {
            // give up
            _closeACSSession(false);
        }
        break;
    case LWS_CALLBACK_SERVER_NEW_CLIENT_INSTANTIATED:
        //prepare the receive buffer
        cwmp_free(&rcv_buf);
        rcv_buf_len = 0;
        lws_client_wsi = wsi;

        break;
    case LWS_CALLBACK_OPENSSL_LOAD_EXTRA_CLIENT_VERIFY_CERTS:
        //extra validation for client cert
        break;

    case LWS_CALLBACK_OPENSSL_LOAD_EXTRA_SERVER_VERIFY_CERTS:
        //extra validation for server cert
        break;
    /* uninterpreted http content */
    case LWS_CALLBACK_ESTABLISHED_CLIENT_HTTP:
    {
        /* only on this callback we can get the http status code
           returned by the server save it for later use*/
        http_status = lws_http_client_http_response(wsi);
        SAH_TRACEZ_INFO("CWMPD", "Server return code is %d", http_status);

        if(http_status == HTTP_NO_CONTENT) {
            //closing free any message
            cwmp_free(&pending_msg);
        } else if(http_status == HTTP_OK) {
            //store ACSIP in persistent storage,need to optimize?
            if(DM_ENG_SetManagementServerValue(DM_ENG_EntityType_SYSTEM, DM_ENG_ACSIP, acs_server_ip) != 0) {
                SAH_TRACEZ_ERROR("CWMPD", "ACSIP : failed to update data model");
            }
            // first message contains set-cookie
            int cookie_len = lws_hdr_custom_length(wsi, "Set-Cookie:", 11);
            if(cookie_len > 0) {
                if(session_cookie) {
                    free(session_cookie);
                }
                session_cookie = malloc(cookie_len + 1);
                if(!session_cookie) {
                    SAH_TRACEZ_ERROR("CWMPD", "malloc fail !!!");
                    return -1;
                }
                memset(session_cookie, 0, cookie_len + 1);

                if(lws_hdr_custom_copy(wsi, session_cookie, cookie_len,
                                       "Set-Cookie:", 11) < 0) {
                    SAH_TRACEZ_ERROR("CWMPD", "error when copying cookie ???");
                }
            }
        }


    }
    break;

    case LWS_CALLBACK_RECEIVE_CLIENT_HTTP:
    {
        //receive raw data
        char buffer[512 + LWS_PRE];
        char* px = buffer + LWS_PRE;
        int lenx = sizeof(buffer) - LWS_PRE;

        if(lws_http_client_read(wsi, &px, &lenx) < 0) {
            SAH_TRACEZ_ERROR("CWMPD", "failed to read data from socket ?");
            return -1;
        }
    }
        return 0; /* don't passthru otherwise lws_callback_http_dummy will call us again*/
    /* chunks of chunked content, without headers */
    case LWS_CALLBACK_RECEIVE_CLIENT_HTTP_READ:
    {
        int rlen = (int) len;
        const char* msg = (const char*) in;
        if(!rcv_buf) { // create the buffer
            rcv_buf = malloc((rlen + 1) * sizeof(char));
            if(rcv_buf == NULL) {
                SAH_TRACEZ_ERROR("CWMPD", "malloc failed");
                return -1;
            }
        } else { // grow
            rcv_buf = (char*) realloc(rcv_buf, (rcv_buf_len + rlen + 1) * sizeof(char));
            if(!rcv_buf) {
                SAH_TRACEZ_ERROR("CWMPD", "Couldn't realloc");
                return -1;
            }
        }
        //copy data
        lws_strnncpy(rcv_buf + rcv_buf_len, msg, rlen, rlen + 1);
        rcv_buf_len += rlen;
        rcv_buf[rcv_buf_len] = '\0';
    }
        return 0; /* don't passthru */
    /* transaction completed , handle the ACS message */
    case LWS_CALLBACK_COMPLETED_CLIENT_HTTP:
        // check if we have server demand
        if(http_status == HTTP_OK) {
            SAH_TRACEZ_INFO("CWMPD", "Received Message: \n<-------------\n %s \n <-------------\n", rcv_buf);
            // check if we have a soap message
            if(strstr(rcv_buf, DM_COM_ENV_TAG)) {
                DM_SoapXml SoapMsg;
                DM_HttpCheckNamespace(rcv_buf, len);
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
        break;
    /* ADD custom headers */
    case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER:
    {
        if(!pending_msg) {
            SAH_TRACEZ_INFO("CWMPD", "there is nothing to be sent, return");
            return -1; //Dont proceed otherwise lws_callback_http_dummy
            //will send an empty post any way
        }
        unsigned char** p = (unsigned char**) in;
        unsigned char* end = (*p) + len;
        int ret = 0;

        ret += lws_add_http_header_by_token(wsi,
                                            WSI_TOKEN_HTTP_USER_AGENT,
                                            USER_AGENT,
                                            15, p, end);

        ret += lws_add_http_header_content_length(wsi,
                                                  strlen(pending_msg),
                                                  p, end);

        ret += lws_add_http_header_by_token(wsi,
                                            WSI_TOKEN_HTTP_CONTENT_TYPE,
                                            CONTENT_TYPE,
                                            28, p, end);
        //ADD cookie if any
        if(session_cookie) {
            ret += lws_add_http_header_by_token(wsi, WSI_TOKEN_HTTP_COOKIE,
                                                (const unsigned char*) session_cookie,
                                                strlen(session_cookie),
                                                p, end);
        }
        ret += lws_add_http_header_by_token(wsi, WSI_TOKEN_CONNECTION,
                                            CONNECTION_KEEP_ALIVE,
                                            10, p, end);

        ret += lws_add_http_header_by_name(wsi, SOAP_HEADER,
                                           EMPTY_USTR,
                                           0, p, end);

        if(ret != 0) {
            SAH_TRACEZ_ERROR("CWMPD", "Cant write Header to LWS client Instance, Not sending the message");
            return -1; //We couldn't wrie Headers something went wrong
        }
        lws_client_http_body_pending(wsi, 1);
    }
    break;
    case LWS_CALLBACK_WSI_DESTROY:
        // called for each websocket instance
        if(lws_client_wsi && (lws_client_wsi == wsi)) {
            lws_client_wsi = NULL;
        }
        break;
    case LWS_CALLBACK_CLIENT_HTTP_WRITEABLE:
        /* a message is waiting for this connection send it */
        if(pending_msg) {
            lws_write(wsi, (unsigned char*) pending_msg, strlen(pending_msg), LWS_WRITE_HTTP);
            lws_client_http_body_pending(wsi, 0);// stop calling on_writable_cb
        }
        break;
    /* this client is closing , check its state and inform dmengine */
    case LWS_CALLBACK_CLOSED_CLIENT_HTTP:
        if(http_status == HTTP_NO_CONTENT) {
            _closeACSSession(true);
        } else {
            //error code received from server
            SAH_TRACEZ_ERROR("CWMPD", "ACS Session failed error [%d]", http_status);
            _closeACSSession(false);
        }
        break;
    default:
        break;
    }

    return lws_callback_http_dummy(wsi, reason, user, in, len);
}

/* websocket configuration struct , protocol : http */
static const struct lws_protocols protocols[] =
{
    {"http", cwmp_client_http_callback, 0, 0, 0, NULL, 0},
    { NULL, NULL, 0, 0, 0, NULL, 0} /* mark protocol end  needed by lws */
};

static cwmp_status_t cwmp_client_prepare_session() {
    // connect using info
    memset(&lws_connect_info, 0, sizeof lws_connect_info);

    lws_connect_info.context = lws_client_ctx;
    lws_connect_info.method = "POST";
    lws_connect_info.protocol = protocols[0].name;
    lws_connect_info.ssl_connection = LCCSCF_PIPELINE;

    /* https stuff */
    if(strcmp(acs_server_scheme, "https") == 0) {
        char* ssl_validate_hosname = NULL;
        char* ssl_accept_self_signed = NULL;
        char* ssl_accept_expied = NULL;
        //secure connection
        lws_connect_info.ssl_connection |= LCCSCF_USE_SSL;

        //should we validate hosnames ?
        if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM,
                                           DM_ENG_SSLVERIFYHOSTNAME,
                                           &ssl_validate_hosname) == 0) {
            if(atol(ssl_validate_hosname) == 0) {
                //Do not do hostname validation
                lws_connect_info.ssl_connection |= LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK;
            }
            free(ssl_validate_hosname);
        }

        //should we accept expired ?
        if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM,
                                           DM_ENG_SSLACCEPTSELFSIGNED,
                                           &ssl_accept_self_signed) == 0) {
            if(atol(ssl_accept_self_signed)) {
                lws_connect_info.ssl_connection |= LCCSCF_ALLOW_SELFSIGNED;
            }
            free(ssl_accept_self_signed);
        }

        //allow expired certs (LCCSCF_ALLOW_INSECURE)
        if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM,
                                           DM_ENG_SSLACCEPTEXPIRED,
                                           &ssl_accept_expied) != 0) {
            SAH_TRACEZ_ERROR("CWMPD", "failed to fetch the accept expired flag");
        } else if(strcmp(ssl_accept_expied, "Always") == 0) {
            /* Always accept expired cert*/
            lws_connect_info.ssl_connection |= LCCSCF_ALLOW_EXPIRED;
        } else if(strcmp(ssl_accept_expied, "NTP") == 0) {
            /* accept only if NTP is not synched */
            char* ntpStatus = NULL;
            if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM,
                                               DM_ENG_NTPSTATUS,
                                               &ntpStatus) != 0) {
                SAH_TRACEZ_ERROR("CWMPD", "failed to fetch the NTP status");
            } else if(ntpStatus && (strcmp(ntpStatus, "Synchronized") != 0)) {
                // ntp is not synchronized, we should NOT check the expiration date
                // we should accept the certeficate even if it is already expired
                lws_connect_info.ssl_connection |= LCCSCF_ALLOW_EXPIRED;
                free(ntpStatus);
            }
        }
        free(ssl_accept_expied);
    }

    lws_connect_info.pwsi = &lws_client_wsi;
    lws_connect_info.port = acs_server_port;
    lws_connect_info.path = acs_server_path;
    lws_connect_info.alpn = "http/1.1";
    lws_connect_info.protocol = protocols[0].name;
    return cwmp_status_ok;
}

/*get the next ACS IP address*/
static cwmp_status_t cwmp_client_get_acsip() {
    cwmp_status_t ret = cwmp_status_ko;

    if(is_ipaddr(acs_server_host)) {
        cwmp_free(&acs_server_ip);
        acs_server_ip = strdup(acs_server_host);
    } else {
        //try to reuse the same ip from the last session
        if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM,
                                           DM_ENG_ACSIP,
                                           &acs_server_ip) != 0) {
            SAH_TRACEZ_ERROR("CWMPD", "Cannot fetch the ACS SERVER URL");
        }

        /* check if the last used IP still part of the DNS pool*/
        if(acs_server_ip && (strlen(acs_server_ip) != 0)) {
            char* ip_list = NULL;
            if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM,
                                               DM_ENG_ACSIPLIST,
                                               &ip_list) != 0) {
                SAH_TRACEZ_ERROR("CWMPD", "Cannot fetch the ACS SERVER URL");
            }
            SAH_TRACEZ_ERROR("CWMPD", "ACSIP LIST is %s", ip_list);
            if(!(ip_list && strstr(ip_list, acs_server_ip))) {
                //if we have old IP but no longer part of the dns_pool
                //try to get a new one
                cwmp_free(&acs_server_ip);
                cwmp_dns_getRandomIP(&acs_server_ip);
            }
            cwmp_free(&ip_list);
        } else {//No last ip find a new one
            cwmp_free(&acs_server_ip);
            cwmp_dns_getRandomIP(&acs_server_ip);
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

    SAH_TRACEZ_INFO("CWMPD", "ACS URL = %s", acsurl);

    if(lws_parse_uri(acsurl, &scheme, &host, &port, &path)) {
        SAH_TRACEZ_ERROR("CWMPD", "Couldn't parse URL (%s)", acsurl);
        goto error;
    }

    acs_server_host = strdup(host);
    acs_server_path = strdup(path);
    acs_server_port = port;
    acs_server_scheme = strdup(scheme);

    if(acs_server_port <= 0) {
        SAH_TRACEZ_WARNING("CWMPD", "URI port is not set, default to Port:7547");
        acs_server_port = 7547; // default tr-069 CWMP port 7547
    }

    SAH_TRACEZ_INFO("CWMPD", "Host: %s | Scheme: %s | Port: %d | Path: %s",
                    acs_server_host,
                    acs_server_scheme,
                    acs_server_port,
                    acs_server_path);

    ret = cwmp_status_ok;
error:
    free(acsurl);
    return ret;
}

static void cwmp_client_sessionTimedOut(UNUSED char* name) {
    SAH_TRACEZ_WARNING("CWMPD", "session timed out, Force closing session");
    _closeACSSession(false);
}

/* fetch all client info from data model and feed them to server info struct*/
cwmp_status_t cwmp_client_init() {
    SAH_TRACEZ_INFO("CWMPD", "Client initialize");
    //static struct lws_context_creation_info lws_client_ctx_info;
    memset(&lws_client_ctx_info, 0, sizeof(lws_client_ctx_info));
    // memset(&lws_connection_info, 0, sizeof(lws_connection_info));
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
    cwmp_free(&acs_server_ip);
}

/** libtr069-engine callbacks **/

int DM_CloseHttpSession(bool closeMode) {
    if(closeMode == true) {
        SAH_TRACEZ_INFO("CWMPD", "ACS Session finished with success");
    } else {
        SAH_TRACEZ_ERROR("CWMPD", "ERROR ACS session corrupted");
    }
    http_status = 0;
    cwmp_free(&acs_server_host);
    cwmp_free(&acs_server_path);
    cwmp_free(&acs_server_scheme);
    cwmp_free(&acs_server_ip);
    cwmp_free(&pending_msg);
    cwmp_free(&session_cookie);
    cwmp_free(&rcv_buf);
    rcv_buf_len = 0;
    return 0;
}

int DM_SendHttpMessage(const char* soap_msg) {
    int msg_len = strlen(soap_msg);

    SAH_TRACEZ_INFO("CWMPD",
                    "sending soap message :\n ----------> \n%s\n ----------> ",
                    (msg_len == 0) ? "EMPTY_HTTP_MESSAGE" : soap_msg);

    if(pending_msg != soap_msg) {
        cwmp_free(&pending_msg);
        pending_msg = strdup(soap_msg);
        DM_UpdateRetryBuffer(soap_msg, msg_len);
    }

    if(acs_server_ip == NULL) {
        return -1;
    } else {
        lws_connect_info.address = acs_server_ip;
        // link : https://www.rfc-editor.org/rfc/rfc7230#section-5.4
        // According to RFC Host must contain the domain name and the port , but It seems that setting the host
        // to a value different than the ip will cause the conn pipeline to stop working and that will create a new
        // TCP for each http message which is not accepted by the ACS server, here is just a workaround
        // A proper fix on the libwebsockets side is needed this workaround will render the functionality
        // obsolete as a the server who serves multiple services on the same IP will not be able to distinguish
        // between them
        lws_connect_info.host = acs_server_ip;
        //try to send the message
        if(!lws_client_connect_via_info(&lws_connect_info)) {
            SAH_TRACEZ_ERROR("CWMPD", "Couldn't connect to %s", acs_server_ip);
            cwmp_free(&pending_msg);
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
        SAH_TRACEZ_ERROR("CWMPD", "Cannot fetch the connection request host URL");
        return -1;
    }
    // Check if wan is up
    if(!(*crhost) || (strcmp(crhost, "0.0.0.0") == 0)) {
        SAH_TRACEZ_WARNING("CWMPD", "WAN is not connected, not connecting to server");
        free(crhost);
        return -1;
    }
    free(crhost);

    if(cwmp_client_parse_url() == cwmp_status_ko) {
        SAH_TRACEZ_ERROR("CWMPD", "Cannot parse ACS url");
        return -1;
    }

    if(cwmp_client_get_acsip() == cwmp_status_ko) {
        SAH_TRACEZ_ERROR("CWMPD", "Cannot get a valid ACS ip");
        return -1;
    }

    if(cwmp_client_prepare_session() == cwmp_status_ko) {
        SAH_TRACEZ_ERROR("CWMPD", "failed to prepare connection info");
        return -1;
    }

    if(DM_ENG_GetManagementServerValue(DM_ENG_EntityType_SYSTEM,
                                       DM_ENG_SESSIONTIMEOUT,
                                       &s_timeout) == 0) {
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
    retry_count = 0; //reset the retry counter

    return 0;
}

