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
#if !defined(_CWMPD_H_)
#define _CWMPD_H_

#include <libwebsockets.h>
//#include<libwebsockets/lws-dll2.h>
#include "dmengine/DM_ENG_NotificationInterface.h"
#include "httpparser/picohttpparser.h"

#define COPY_BUFFER_SIZE 4 * 1024

typedef enum server_state {INIT = 0, RUN, EXIT, ERROR } server_state_t;
typedef enum cwmp_status {cwmp_status_ok=0, cwmp_status_ko} cwmp_status_t;

struct application {
    char* name;
    int daemonize;
    int traceLevel;
    int traceType;
    server_state_t state;
    char* trustedCA;
    char* pidFile;
    char* da_path;
};

typedef struct application application_t;

cwmp_status_t cwmp_server_init(struct lws_context_creation_info* lws_ctx_info);

cwmp_status_t cwmp_server_start(struct lws_context_creation_info* lws_ctx_info,
                                void** evlp, struct lws_context* lws_ctx);

cwmp_status_t cwmp_server_stop(struct lws_context* lws_ctx);


cwmp_status_t cwmp_client_init(struct lws_context_creation_info* lws_ctx_info);

cwmp_status_t cwmp_client_start_session(struct lws_context_creation_info* lws_ctx_info,
                                        void** evlp, struct lws_context* lws_ctx);

cwmp_status_t cwmp_client_stop(struct lws_context* lws_ctx);

int timer_stop(const char* name);

int timer_start(const char* name, int waitTime, int intervalTime, timerHandler handler);

void timer_cleanup();

unsigned int timer_remainingTime(const char* name);

int get_content_length(struct phr_header* values, unsigned int len);
int append_read_buffer(char** msg, int* len);
int create_read_buffer(char* raw, int len);
void reset_read_buffer();
int process_body(char* body, int len);



#endif // !_CWMPD_H_