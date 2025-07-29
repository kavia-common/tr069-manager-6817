/****************************************************************************
**
** SPDX-License-Identifier: BSD-2-Clause-Patent
** SPDX-FileCopyrightText: Copyright (c) 2025 Consult Red
** This code is subject to the terms of the BSD+Patent license.
** See LICENSE file for more details.
**
****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <setjmp.h>
#include <cmocka.h>

#include <amxc/amxc_macros.h>

#include "cwmpd_get_certificate_uri.h"
#include "test_cwmp_get_certificate_uri.h"

int __wrap_amxb_call(amxb_bus_ctx_t* const bus_ctx,
                     const char* object,
                     const char* method,
                     UNUSED amxc_var_t* args,
                     amxc_var_t* ret,
                     int timeout) {
    amxc_var_t* out_args;

    check_expected_ptr(bus_ctx);
    check_expected_ptr(object);
    check_expected_ptr(method);
    check_expected_ptr(args);
    check_expected_ptr(ret);
    check_expected(timeout);

    amxc_var_new(&out_args);
    amxc_var_set_type(out_args, AMXC_VAR_ID_LIST);

    amxc_var_add(cstring_t, out_args, "");

    amxc_var_t* uris = amxc_var_add_new(out_args);
    amxc_var_set_type(uris, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, uris, "CertificateURI", "TestCertURI");
    amxc_var_add_key(cstring_t, uris, "PrivateKeyURI", "TestKeyURI");

    amxc_var_move(ret, out_args);
    amxc_var_delete(&out_args);

    return mock_type(int);
}

amxb_bus_ctx_t* __wrap_amxb_be_who_has(const char* object_path) {
    check_expected(object_path);
    return (amxb_bus_ctx_t*) mock_ptr_type(amxb_bus_ctx_t*);
}

void test_cwmp_get_certificate_uri_success(UNUSED void** state) {
    const char* fullpath = "/some/path";
    char* certuri = NULL;
    char* keyuri = NULL;
    amxb_bus_ctx_t dummy_ctx;

    expect_string(__wrap_amxb_be_who_has, object_path, fullpath);
    will_return(__wrap_amxb_be_who_has, &dummy_ctx);
    expect_any(__wrap_amxb_call, bus_ctx);
    expect_any(__wrap_amxb_call, object);
    expect_any(__wrap_amxb_call, method);
    expect_any(__wrap_amxb_call, args);
    expect_any(__wrap_amxb_call, ret);
    expect_any(__wrap_amxb_call, timeout);
    will_return(__wrap_amxb_call, 0);

    int result = cwmp_get_certificate_uri(fullpath, &certuri, &keyuri);

    assert_int_equal(result, 0);
    assert_string_equal(certuri, "TestCertURI");
    assert_string_equal(keyuri, "TestKeyURI");

    free(certuri);
    free(keyuri);
}

void test_cwmp_get_certificate_uri_invalid_input(UNUSED void** state) {
    char* certuri = NULL;
    char* keyuri = NULL;
    amxb_bus_ctx_t dummy_ctx;

    expect_value(__wrap_amxb_be_who_has, object_path, NULL);
    will_return(__wrap_amxb_be_who_has, &dummy_ctx);
    expect_any(__wrap_amxb_call, bus_ctx);
    expect_any(__wrap_amxb_call, object);
    expect_any(__wrap_amxb_call, method);
    expect_any(__wrap_amxb_call, args);
    expect_any(__wrap_amxb_call, ret);
    expect_any(__wrap_amxb_call, timeout);
    will_return(__wrap_amxb_call, 1);

    int result = cwmp_get_certificate_uri(NULL, &certuri, &keyuri);
    assert_int_not_equal(result, 0);
}
