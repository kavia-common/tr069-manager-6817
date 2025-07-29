/****************************************************************************
**
** SPDX-License-Identifier: BSD-2-Clause-Patent
** SPDX-FileCopyrightText: Copyright (c) 2025 Consult Red
** This code is subject to the terms of the BSD+Patent license.
** See LICENSE file for more details.
**
****************************************************************************/

#include <string.h>
#include <amxc/amxc.h>
#include <amxp/amxp.h>
#include <amxd/amxd_dm.h>
#include <amxb/amxb.h>

/**
   @brief Retrieves the certificate and private key URIs for a given path.

   This function calls the "GetCertificateURI" method on the backend associated with the provided `fullpath`.

   @param[in]  fullpath  A path name of a row in the Security.Certificate table, e.g. "Security.Certificate.1."
   @param[out] certuri   Pointer to a string that will be allocated and set to the certificate URI.
   @param[out] keyuri    Pointer to a string that will be allocated and set to the private key URI.

   @return 0 on success, -1 on failure.

   @note The caller is responsible for freeing the memory allocated for `*certuri` and `*keyuri`.
 */
int cwmp_get_certificate_uri(const char* fullpath, char** certuri, char** keyuri) {
    amxc_var_t inargs;
    amxc_var_t outargs;
    int ret = -1;

    amxc_var_init(&inargs);
    amxc_var_init(&outargs);

    ret = amxb_call(amxb_be_who_has(fullpath), fullpath, "GetCertificateURI", &inargs, &outargs, 3);

    if(ret == 0) {
        amxc_var_t* outputs = amxc_var_get_first(&outargs); // string error/status
        outputs = amxc_var_get_next(outputs);               // map
        const char* cert = amxc_var_constcast(cstring_t, amxc_var_get_key(outputs, "CertificateURI", AMXC_VAR_FLAG_DEFAULT));
        const char* key = amxc_var_constcast(cstring_t, amxc_var_get_key(outputs, "PrivateKeyURI", AMXC_VAR_FLAG_DEFAULT));

        if(cert) {
            *certuri = strdup(cert);
        }
        if(key) {
            *keyuri = strdup(key);
        }

        ret = 0;
    }

    amxc_var_clean(&inargs);
    amxc_var_clean(&outargs);
    return ret;
}

