// OpenSSL functions for butt
//
// Copyright 2020 by Daniel Noethen.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or (at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
//

#ifndef TLS_H
#define TLS_H

#include <openssl/ssl.h>

/* Mozilla's 'Intermediate' list as of 2015-04-19 */
#define ALLOWED_CIPHERS                                                                                                                                        \
    "ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-AES256-GCM-SHA384:DHE-RSA-AES128-GCM-SHA256:DHE-DSS-AES128-GCM-SHA256:kEDH+AESGCM:ECDHE-RSA-AES128-SHA256:ECDHE-ECDSA-AES128-SHA256:ECDHE-RSA-AES128-SHA:ECDHE-ECDSA-AES128-SHA:ECDHE-RSA-AES256-SHA384:ECDHE-ECDSA-AES256-SHA384:ECDHE-RSA-AES256-SHA:ECDHE-ECDSA-AES256-SHA:DHE-RSA-AES128-SHA256:DHE-RSA-AES128-SHA:DHE-DSS-AES128-SHA256:DHE-RSA-AES256-SHA256:DHE-DSS-AES256-SHA:DHE-RSA-AES256-SHA:AES128-GCM-SHA256:AES256-GCM-SHA384:AES128-SHA256:AES256-SHA256:AES128-SHA:AES256-SHA:AES:CAMELLIA:DES-CBC3-SHA:!aNULL:!eNULL:!EXPORT:!DES:!RC4:!MD5:!PSK:!aECDH:!EDH-DSS-DES-CBC3-SHA:!EDH-RSA-DES-CBC3-SHA:!KRB5-DES-CBC3-SHA"

enum err {
    TLS_OK = 0,
    TLS_MALLOC = -1,
    TLS_NOCERT = -2,
    TLS_CERTERR = -3,
    TLS_HOSTERR = -4,
    TLS_UNSPECIFICERR = -5,
    TLS_CONNETERR = -6,
    TLS_SENDERR = -7,
    TLS_RECVERR = -8,
    TLS_TIMEOUT = -9,
    TLS_CHECK_HOST = -10,
    TLS_CHECK_CERT = -11,
};

enum state {
    TLS_STATE_IDLE = 0,
    TLS_STATE_SENDING = 1,
    TLS_STATE_RECEIVING = 2,
};

typedef struct tls {
    SSL_CTX *ssl_ctx;
    SSL *ssl;
    char *host;
    char *last_err;
    char *cert_file;
    char *cert_dir;
    char sha256[65];
    int socket;
    int skip_verification;
    int state;
} tls_t;

int tls_setup(tls_t *tls);
int tls_send(tls_t *tls, char *buf, int len, int timeout_ms);
int tls_recv(tls_t *tls, char *buf, int len, int timeout_ms);
void tls_close(tls_t *tls);

#endif
