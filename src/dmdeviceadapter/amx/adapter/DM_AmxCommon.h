
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
#include <amxc/amxc.h>
#include <amxp/amxp.h>
#include <amxd/amxd_dm.h>
#include <amxd/amxd_path.h>
#include <amxd/amxd_object.h>
#include <amxb/amxb.h>
#include <sys/types.h>
#include <dmengine/DM_ENG_ParameterType.h>
#include "DM_DeviceAdapter.h"

#define SetErrorGotoStop(errorNumber, message) \
    { error = errorNumber; SAH_TRACEZ_ERROR("DM_DA", message); goto stop; }

#define GotoStop(message) \
    { SAH_TRACEZ_ERROR("DM_DA", message); goto stop; }

char* DM_ENG_Device_Common_ACSToAMXPath_noalloc(const char* acsPath);
char* DM_ENG_Device_Common_ACSToAMXPath(const char* acsPath);
bool DM_ENG_Device_Common_CheckSystem(dm_amx_env_t* amx);
bool DM_ENG_Device_Common_AmxConnect(dm_amx_env_t* amx, const char* envVariable, const char* defaultLocation, const char* envURI, const char* defaultURI);
bool DM_ENG_Device_Common_IsWildcardPath(const char* path);
bool DM_ENG_Device_Common_IsWildcardPathValid(const char* path);
bool DM_ENG_Device_Common_Resolve_Path(dm_amx_env_t* amx, char* path, amxc_var_t* resolved);
DM_ENG_ParameterType DM_ENG_Device_Common_ConvertParameterType(u_int32_t type);
char* DM_ENG_Device_Common_GetRootParameterInternalPath(char* path);
char* DM_ENG_Device_Common_GetRootParameterAcsPath(char* path);
bool DM_ENG_Device_Common_IsRootParameter(char* path);

