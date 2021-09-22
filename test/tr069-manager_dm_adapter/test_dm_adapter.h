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

#ifndef __TEST_DM_ADAPTER_H__
#define __TEST_DM_ADAPTER_H__

#include <dmengine/DM_ENG_Common.h>
#include <dmengine/DM_ENG_Device.h>

/* define a few callbacks functions used just as templates not really needed for test */
int inform(DM_ENG_DeviceIdStruct* id,
           DM_ENG_EventStruct* events[],
           DM_ENG_ParameterValueStruct* parameterList[],
           time_t currentTime,
           unsigned int retryCount);

int transferComplete(DM_ENG_TransferCompleteStruct* tcs);

int requestDownload(const char* fileType, DM_ENG_ArgStruct* args[]);

int getRPCMethods();

int timerStart(const char* name, int waitTime, int intervalTime, timerHandler handler);

int timerStop(const char* name);

unsigned int timerTimeRemaining(const char* name);

int engineEvent(const char* eventType);

int DM_SendHttpMessage(const char* msgToSendStr);

int client_startSession();

int DM_CloseHttpSession(int closeMode);

/*setup test*/
int test_dmadapter_setup(void** state);

int test_dmadapter_teardown(void** state);

/* test func */
void test_dmadapter_load(void** state);

void test_dmadapter_connection(void** state);

void test_dmadapter_ManagementServer_GetParameterValue(void** state);

void test_dmadapter_ManagementServer_SetParameterValue(void** state);

void test_dmadapter_GetParameterNames_Parameter(void** state);

void test_dmadapter_GetParameterNames_EmptyPath_NextLevel_True(void** state);

void test_dmadapter_GetParameterNames_Object_AllDataModel(void** state);

void test_dmadapter_GetParameterNames_Object_NextLevel_False(void** state);

void test_dmadapter_GetParameterNames_Object_NextLevel_True(void** state);

void test_dmadapter_GetParameterNames_Object_NextLevel_False_searchPath(void** state);

void test_dmadapter_GetParameterNames_Object_NextLevel_True_searchPath(void** state);

void test_dmadapter_GetParametersValues_Parameters(void** state);

void test_dmadapter_GetParametersValues_Object(void** state);

void test_dmadapter_GetParametersValues_searchPath_Parameter(void** state);

void test_dmadapter_GetParametersValues_searchPath_Object(void** state);

void test_dmadapter_SetParameterValues(void** state);

void test_dmadapter_SetParameterValues_Faults(void** state);

void test_dmadapter_AddDeleteObject(void** state);

#endif // __TEST_LIB_DMENGINE_H__
