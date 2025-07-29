/****************************************************************************
**
** SPDX-License-Identifier: BSD-2-Clause-Patent
** SPDX-FileCopyrightText: Copyright (c) 2025 Consut Red
** This code is subject to the terms of the BSD+Patent license.
** See LICENSE file for more details.
**
****************************************************************************/

#if !defined(_TEST_CWMP_GET_CERTIFICATE_URI_)
#define _TEST_CWMP_GET_CERTIFICATE_URI_

#include <amxc/amxc.h>
#include <amxp/amxp.h>
#include <amxd/amxd_types.h>
#include <amxb/amxb_types.h>
#include <amxd/amxd_dm.h>
#include <amxd/amxd_object.h>

int __wrap_amxb_call(amxb_bus_ctx_t* const bus_ctx,
                     const char* object,
                     const char* method,
                     amxc_var_t* args,
                     amxc_var_t* ret,
                     int timeout);

amxb_bus_ctx_t* __wrap_amxb_be_who_has(const char* object_path);

void test_cwmp_get_certificate_uri_success(void** state);
void test_cwmp_get_certificate_uri_invalid_input(void** state);

#endif // _TEST_CWMP_GET_CERTIFICATE_URI_

