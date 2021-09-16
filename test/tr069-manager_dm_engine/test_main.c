#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <setjmp.h>
#include <stdarg.h>
#include <string.h>
#include <cmocka.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>

#include "test_dmengine.h"

int main(void) {

    int ret = 0;

    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_dmengine_bus_connection)
    };
    ret = cmocka_run_group_tests(tests, NULL, NULL);

    return ret;
}
