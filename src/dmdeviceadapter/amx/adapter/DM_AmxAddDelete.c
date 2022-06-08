
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


#include <dmengine/DM_ENG_ParameterStatus.h>
#include <dmengine/DM_ENG_Error.h>
#include <debug/sahtrace.h>
#include <string.h>

#include "DM_AmxCommon.h"

#include "DM_DeviceAdapter.h"


//---------------------------------------------------------------------------------------------
/**
 * @addtogroup sah_cwmp_amxdeviceadapter
 * @{
 */

//---------------------------------------------------------------------------------------------
/**
   @brief
   Add an object to the datamodel.(only instanciated objects)

   @details
   Add an object to the datamodel.

   @param amx A pointer to the ambiorix system bus environment variable we want to initialize.
   @param objectName TR69 path of the of the object we want to create
   @param pInstanceNumber The instance number of the newly created object
   @param pStatus The resulting parameter status

   @return
   - tr69 error code (e.g. 900xx) in case of an error
   - 0 if succesfull
 */
int DM_ENG_Device_AddDeleteObject_Add(dm_amx_env_t* amx, char* objectName, unsigned int* pInstanceNumber, DM_ENG_ParameterStatus* pStatus) {
    SAH_TRACEZ_IN("DM_DA");
    int error = 0;
    int rv = 0;
    int type_id = 0;
    const char* internalPath = NULL;
    amxc_var_t ret;
    amxc_var_t desc;
    amxc_var_init(&ret);
    amxc_var_init(&desc);
    amxd_path_t path;
    amxd_path_init(&path, "");

    *pStatus = DM_ENG_ParameterStatus_UNDEFINED;
    *pInstanceNumber = 0;
    // check for path validity
    if(objectName[strlen(objectName) - 1] != '.') {
        SetErrorGotoStop(DM_ENG_INVALID_PARAMETER_NAME, "NOT a valid object path");
    }

    internalPath = DM_ENG_Device_Common_ACSToAMXPath_noalloc(objectName);
    if(internalPath == NULL) {
        SetErrorGotoStop(DM_ENG_INVALID_PARAMETER_NAME, "Not a valid parameter Name");
    }
    amxd_path_setf(&path, false, "%s", internalPath);

    if(amxd_path_is_search_path(&path)) {
        SetErrorGotoStop(DM_ENG_INVALID_PARAMETER_NAME, "Not a valid parameter Name");
    }
    // get object data
    rv = amxb_describe(amx->bus_ctx, amxd_path_get(&path, AMXD_OBJECT_TERMINATE),
                       0, &desc, 2);

    if((rv != 0) || amxc_var_is_null(&desc)) {
        SetErrorGotoStop(DM_ENG_INVALID_PARAMETER_NAME, "Not a valid parameter Name");
    }
    type_id = GET_INT32(GETI_ARG(&desc, 0), "type_id");

    if(type_id != amxd_object_template) {
        SetErrorGotoStop(DM_ENG_INVALID_PARAMETER_NAME, "Object is Not a template");
    }

    // else its a valid object , add instance
    rv = amxb_add(amx->bus_ctx, amxd_path_get(&path, AMXD_OBJECT_TERMINATE),
                  0, NULL, NULL, &ret, 2);

    if((rv != 0) || amxc_var_is_null(&ret)) {
        //TODO!: check for error type?
        // SetErrorGotoStop(DM_ENG_RESOURCES_EXCEEDED, "Resources exceeded");
        SetErrorGotoStop(DM_ENG_INTERNAL_ERROR, "Add Instance failed");
    } else {
        *pInstanceNumber = GET_INT32(GETI_ARG(&ret, 0), "index");
        *pStatus = DM_ENG_ParameterStatus_APPLIED;
    }

stop:
    amxc_var_clean(&desc);
    amxc_var_clean(&ret);
    amxd_path_clean(&path);
    SAH_TRACEZ_OUT("DM_DA");
    return error;
}


//---------------------------------------------------------------------------------------------
/**
   @brief
   Remove an object from the datamodel.

   @details
   Remove an object from the datamodel.

   @param amx A pointer to the amx system bus environment variable.
   @param objectName TR69 path of the of the object we want to remove
   @param pStatus The resulting parameter status

   @return
   - tr69 error code (e.g. 900xx) in case of an error
   - 0 if succesfull
 */

int DM_ENG_Device_AddDeleteObject_Delete(dm_amx_env_t* amx, char* objectName, DM_ENG_ParameterStatus* pStatus) {
    int error = 0;
    const char* internalPath = NULL;
    int rv = 0;
    int type_id = 0;
    amxc_var_t ret;
    amxc_var_t desc;
    amxc_var_init(&ret);
    amxc_var_init(&desc);
    amxd_path_t path;
    amxd_path_init(&path, "");
    *pStatus = DM_ENG_ParameterStatus_UNDEFINED;

    // check for path validity
    if(!((objectName[strlen(objectName) - 1] == '.')
         || ( objectName[strlen(objectName) - 1] == '*'))) {
        SetErrorGotoStop(DM_ENG_INVALID_PARAMETER_NAME, "Invalid object path");
    }
    internalPath = DM_ENG_Device_Common_ACSToAMXPath_noalloc(objectName);
    if(internalPath == NULL) {
        SetErrorGotoStop(DM_ENG_INVALID_PARAMETER_NAME, "Invalid objet path")
    }

    amxd_path_setf(&path, false, "%s", internalPath);
    // check for path validity
    if(amxd_path_is_search_path(&path)) {
        SetErrorGotoStop(DM_ENG_INVALID_PARAMETER_NAME, "Invalid search path");
    }
    // get object data
    rv = amxb_describe(amx->bus_ctx, amxd_path_get(&path, AMXD_OBJECT_TERMINATE),
                       0, &desc, 2);

    if((rv != 0) || amxc_var_is_null(&desc)) {
        SetErrorGotoStop(DM_ENG_INVALID_PARAMETER_NAME, "Not a valid parameter Name");
    }

    type_id = GET_INT32(GETI_ARG(&desc, 0), "type_id");

    if(type_id != amxd_object_instance) {
        SetErrorGotoStop(DM_ENG_INVALID_PARAMETER_NAME, "Object is Not a instance");
    }

    // else its a valid object , add instance
    rv = amxb_del(amx->bus_ctx, amxd_path_get(&path, AMXD_OBJECT_TERMINATE),
                  0, NULL, &ret, 2);
    if((rv != 0) || amxc_var_is_null(&ret)) {
        SetErrorGotoStop(DM_ENG_INTERNAL_ERROR, "Delete Instance failed");
    } else {
        *pStatus = DM_ENG_ParameterStatus_APPLIED;
    }

stop:
    amxc_var_clean(&desc);
    amxc_var_clean(&ret);
    amxd_path_clean(&path);
    SAH_TRACEZ_IN("DM_DA");
    return error;
}

/** @} */
