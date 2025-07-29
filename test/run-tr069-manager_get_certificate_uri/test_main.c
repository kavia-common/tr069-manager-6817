/****************************************************************************
**
** SPDX-License-Identifier: BSD-2-Clause-Patent
** SPDX-FileCopyrightText: Copyright (c) 2025 Consult Red
** This code is subject to the terms of the BSD+Patent license.
** See LICENSE file for more details.
**
****************************************************************************/

#include <stdlib.h>
#include <setjmp.h>
#include <stdarg.h>
#include <cmocka.h>

#include "test_cwmp_get_certificate_uri.h"

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_cwmp_get_certificate_uri_success),
        cmocka_unit_test(test_cwmp_get_certificate_uri_invalid_input)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
