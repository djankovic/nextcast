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

// This OpenSSL implementation is based on the implementation of libshout 2.4.3 https://icecast.org/download/

#include "gettext.h"
#include "config.h"

#ifdef HAVE_LIBSSL

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#ifndef WIN32
#include <sys/select.h>
#endif

#ifdef __APPLE__
#include <libgen.h>      // for dirname
#include <mach-o/dyld.h> // for _NSGetExecutablePath
#endif

#include <openssl/err.h>
#include <openssl/x509v3.h>

#include <ctype.h>

#include "tls.h"
#include "sockfuncs.h"

//#define DEBUG 1

// https://en.wikibooks.org/wiki/OpenSSL/Error_handling
//#undef HAVE_X509_CHECK_HOST

#ifdef DEBUG
FILE *fd;
static void key_log_cb(const SSL *ssl, const char *line)
{
    fprintf(fd, "%s\n", line);
}
#endif

static void set_error(tls_t *tls, const char *err_msg)
{
    int len = (int)strlen(err_msg) + 1;
    tls->last_err = (char *)realloc(tls->last_err, len);
    snprintf(tls->last_err, len, "%s", err_msg);
}

static void clr_error(tls_t *tls)
{
    set_error(tls, "No error");
}

/*
static void print_error_stack(void)
{
    BIO *bio = BIO_new(BIO_s_mem ());
    ERR_print_errors(bio);
    char *buf = NULL;
    BIO_get_mem_data(bio, &buf);
    printf("%s\n", buf);
    BIO_free (bio);
}


static int get_peer_certificate_chain(tls_t *tls, char **buf)
{
    int certs;
    unsigned char *data;
    unsigned int len;
    BIO *bio;
    STACK_OF(X509) *chain;

    chain = SSL_get_peer_cert_chain(tls->ssl);
    certs = sk_X509_num(chain);

    if (certs == 0) {
        return TLS_NOCERT;
    }

    bio = BIO_new(BIO_s_mem());
    if (bio == 0) {
        return TLS_MALLOC;
    }

    for (int i = 0; i < certs; i++)
    {
        X509 *cert = sk_X509_value(chain, i);
        PEM_write_bio_X509(bio, cert);
    }

    len = (unsigned int)BIO_get_mem_data(bio, &data);

    if (len) {
        *buf = (char*)malloc(len + 1);
        memcpy(*buf, data, len);
        (*buf)[len] = 0;
    }

    BIO_free(bio);
    return TLS_OK;
}
*/

#ifndef HAVE_X509_CHECK_HOST
static int check_pattern(const char *key, const char *pattern)
{
    for (; *key && *pattern; key++) {
        if (*pattern == '*') {
            for (; *pattern == '*'; pattern++)
                ;

            for (; *key && *key != '.'; key++)
                ;

            if (!*pattern && !*key) {
                return 1;
            }
            if (!*pattern || !*key) {
                return 0;
            }
        }

        if (tolower(*key) != tolower(*pattern)) {
            return 0;
        }
        pattern++;
    }
    return *key == 0 && *pattern == 0;
}
#endif

#ifndef HAVE_X509_CHECK_HOST
static int check_host(tls_t *tls, X509 *cert)
{
    char common_name[256] = "";
    X509_NAME *xname = X509_get_subject_name(cert);
    X509_NAME_ENTRY *xentry;
    ASN1_STRING *sdata;
    int i, j;
    int ret;

    ret = X509_NAME_get_text_by_NID(xname, NID_commonName, common_name, sizeof(common_name));
    if (ret < 1 || (size_t)ret >= (sizeof(common_name) - 1)) {
        set_error(tls, _("check_host: could not read host name from cert"));
        return TLS_HOSTERR;
    }

    if (!check_pattern(tls->host, common_name)) {
        return TLS_HOSTERR;
    }

    /* check for inlined \0,
     * see https://www.blackhat.com/html/bh-usa-09/bh-usa-09-archives.html#Marlinspike
     */
    for (i = -1;; i = j) {
        j = X509_NAME_get_index_by_NID(xname, NID_commonName, i);
        if (j == -1) {
            break;
        }
    }

    xentry = X509_NAME_get_entry(xname, i);
    sdata = X509_NAME_ENTRY_get_data(xentry);

    if ((size_t)ASN1_STRING_length(sdata) != strlen(common_name)) {
        return TLS_HOSTERR;
    }

    return TLS_OK;
}
#endif

static int check_cert(tls_t *tls)
{
    long ret;
    X509 *cert = SSL_get_peer_certificate(tls->ssl);

    if (cert == NULL) {
        set_error(tls, _("check_cert: No peer certificate available"));
        return TLS_NOCERT;
    }

    if ((ret = SSL_get_verify_result(tls->ssl)) != X509_V_OK) {
        X509_free(cert);
        set_error(tls, X509_verify_cert_error_string(ret));
        return TLS_CHECK_CERT;
    }

#ifdef HAVE_X509_CHECK_HOST
    if (X509_check_host(cert, tls->host, 0, 0, NULL) != 1) {
        X509_free(cert);
        set_error(tls, _("check_cert: X509_check_host failed"));
        return TLS_CHECK_HOST;
    }
#else
    if (check_host(tls, cert) != TLS_OK) {
        X509_free(cert);
        set_error(tls, _("check_cert: check_host failed"));
        return TLS_CHECK_HOST;
    }
#endif

    X509_free(cert);
    return TLS_OK;
}

static int calc_cert_hash(tls_t *tls)
{
    const uint32_t len = 32;
    char byte[3];
    uint8_t sha256_val[len];
    X509 *cert = SSL_get_peer_certificate(tls->ssl);
    if (cert == NULL) {
        set_error(tls, _("calc_cert_hash: No peer certificate available"));
        return TLS_NOCERT;
    }

    // Calc hash
    if (X509_digest(cert, EVP_get_digestbyname("sha256"), sha256_val, NULL) != 1) {
        X509_free(cert);
        set_error(tls, _("calc_cert_hash: Hash calculation failed"));
        return TLS_CERTERR;
    }

    // Convert hash to a char array
    memset(tls->sha256, 0, sizeof(tls->sha256));
    for (uint32_t i = 0; i < len; ++i) {
        snprintf(byte, sizeof(byte), "%02X", sha256_val[i]);
        memcpy(tls->sha256 + 2 * i, byte, 2);
    }

    X509_free(cert);
    return TLS_OK;
}

int tls_setup(tls_t *tls)
{
    int ret = 0;
    long ssl_opts = 0;

    tls->ssl = NULL;
    tls->ssl_ctx = NULL;
    tls->last_err = NULL;

// Checks for OpenSSL version < 1.1.x
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    SSL_library_init();
    SSL_load_error_strings();
    // SSLeay_add_all_algorithms();
    // SSLeay_add_ssl_algorithms();

    tls->ssl_ctx = SSL_CTX_new(TLSv1_client_method());
    ssl_opts |= SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3;
#else
    tls->ssl_ctx = SSL_CTX_new(TLS_client_method());
    SSL_CTX_set_min_proto_version(tls->ssl_ctx, TLS1_VERSION);
#endif

    // Compression is dangerous -> https://en.wikipedia.org/wiki/CRIME
    ssl_opts |= SSL_OP_NO_COMPRESSION;

    SSL_CTX_set_options(tls->ssl_ctx, ssl_opts);
    SSL_CTX_set_default_verify_paths(tls->ssl_ctx);

    // Add cert file and directory separately.
    // So in case one contains a invalid path, the other will still work
    SSL_CTX_load_verify_locations(tls->ssl_ctx, tls->cert_file, NULL);
    SSL_CTX_load_verify_locations(tls->ssl_ctx, NULL, tls->cert_dir);

#ifdef __APPLE__
    char path_to_executeable[PATH_MAX];
    char path_to_ca_file[PATH_MAX];

    uint32_t path_len = (uint32_t)sizeof(path_to_executeable);
    if (_NSGetExecutablePath(path_to_executeable, &path_len) == 0) {
        char *folder_of_executable = strdup(dirname(path_to_executeable));
        snprintf(path_to_ca_file, PATH_MAX, "%s%s", folder_of_executable, "/../Resources/cacert.pem");
        free(folder_of_executable);
        SSL_CTX_load_verify_locations(tls->ssl_ctx, path_to_ca_file, NULL);
    }
#endif
#ifdef WIN32
    SSL_CTX_load_verify_locations(tls->ssl_ctx, "cacert.pem", NULL);
#endif

    // We verify by ourself later
    SSL_CTX_set_verify(tls->ssl_ctx, SSL_VERIFY_NONE, NULL);

    if (SSL_CTX_set_cipher_list(tls->ssl_ctx, ALLOWED_CIPHERS) != 1) {
        set_error(tls, _("tls_setup: Could not set cipher list"));
        return TLS_UNSPECIFICERR;
    }

    SSL_CTX_set_mode(tls->ssl_ctx, SSL_MODE_AUTO_RETRY);

#ifdef DEBUG
    fd = fopen("/Users/bip/tls_key.txt", "ab");
    SSL_CTX_set_keylog_callback(tls->ssl_ctx, key_log_cb);
#endif

    tls->ssl = SSL_new(tls->ssl_ctx);
    if (tls->ssl == NULL) {
        set_error(tls, _("tls_setup: SSL_new failed"));
        return TLS_UNSPECIFICERR;
    }

    // Connect ssl to our socket (creates an internal BIO and inherits blocking/non-blocking from the socket)
    if (SSL_set_fd(tls->ssl, tls->socket) == 0) {
        set_error(tls, _("tls_setup: Could not bind socket to SSL"));
        return TLS_UNSPECIFICERR;
    }

    SSL_set_tlsext_host_name(tls->ssl, tls->host);

    // Set SSL to client mode
    SSL_set_connect_state(tls->ssl);

    while ((ret = SSL_connect(tls->ssl)) != 1) {
        int err;
        switch (err = SSL_get_error(tls->ssl, ret)) {
        case SSL_ERROR_WANT_READ:
            if (sock_select(tls->socket, 5000, READ) <= 0) {
                set_error(tls, _("tls_setup: SSL_connect read timeout"));
                return TLS_TIMEOUT;
            }
            break;
        case SSL_ERROR_WANT_WRITE:
            if (sock_select(tls->socket, 5000, WRITE) <= 0) {
                set_error(tls, _("tls_setup: SSL_connect write timeout"));
                return TLS_TIMEOUT;
            }
            break;
        case SSL_ERROR_SYSCALL: // Can occour if the server goes down and a reconnection attempt fails
            return TLS_TIMEOUT; // Try again
            break;
        default:
            printf("SSL_connect error: %d\n", err);
            fflush(stdout);
            // print_error_stack();
            set_error(tls, ERR_error_string(err, NULL));
            return TLS_CONNETERR;
        }
    }

    if (calc_cert_hash(tls) != TLS_OK) {
        set_error(tls, _("tls_setup: Cert hash could not be calculated"));
        return TLS_CERTERR;
    }

    if ((tls->skip_verification == 0) && ((ret = check_cert(tls)) != TLS_OK)) {
        return ret;
    }

#ifdef DEBUG
    fclose(fd);
#endif
    clr_error(tls);
    return TLS_OK;
}

int tls_send(tls_t *tls, char *buf, int len, int timeout_ms)
{
    int ret;
    tls->state = TLS_STATE_SENDING;
    while ((tls->ssl != NULL) && ((ret = SSL_write(tls->ssl, buf, len)) <= 0)) {
        int err;
        switch (err = SSL_get_error(tls->ssl, ret)) {
        case SSL_ERROR_WANT_READ:
            if (sock_select(tls->socket, timeout_ms, READ) <= 0) {
                set_error(tls, _("tls_send: read timeout"));
                tls->state = TLS_STATE_IDLE;
                return TLS_TIMEOUT;
            }
            break;
        case SSL_ERROR_WANT_WRITE:
            if (sock_select(tls->socket, timeout_ms, WRITE) <= 0) {
                set_error(tls, _("tls_send: write timeout"));
                tls->state = TLS_STATE_IDLE;
                return TLS_TIMEOUT;
            }
            break;
        default:
            // Occurs if server goes down. Reconnection will be initiated
            set_error(tls, ERR_error_string(err, NULL));
            tls->state = TLS_STATE_IDLE;
            return TLS_SENDERR;
        }
    }

    tls->state = TLS_STATE_IDLE;
    return TLS_OK;
}

int tls_recv(tls_t *tls, char *buf, int len, int timeout_ms)
{
    int ret;
    tls->state = TLS_STATE_RECEIVING;
    while ((ret = SSL_read(tls->ssl, buf, len)) <= 0) {
        int err;
        switch (err = SSL_get_error(tls->ssl, ret)) {
        case SSL_ERROR_WANT_READ:
            if (sock_select(tls->socket, timeout_ms, READ) <= 0) {
                set_error(tls, _("tls_recv: read timeout"));
                tls->state = TLS_STATE_IDLE;
                return TLS_TIMEOUT;
            }
            break;
        case SSL_ERROR_WANT_WRITE:
            if (sock_select(tls->socket, timeout_ms, WRITE) <= 0) {
                set_error(tls, _("tls_recv: write timeout"));
                tls->state = TLS_STATE_IDLE;
                return TLS_TIMEOUT;
            }
            break;
        default:
            set_error(tls, ERR_error_string(err, NULL));
            tls->state = TLS_STATE_IDLE;
            return TLS_RECVERR;
        }
    }

    tls->state = TLS_STATE_IDLE;
    return ret;
}

void tls_close(tls_t *tls)
{
    // Make sure we don't free anything while the tls system is working
    while (tls->state != TLS_STATE_IDLE)
        ;

    if (tls->ssl) {
        SSL_shutdown(tls->ssl);
        SSL_free(tls->ssl);
        tls->ssl = NULL;
    }
    if (tls->ssl_ctx) {
        SSL_CTX_free(tls->ssl_ctx);
        tls->ssl_ctx = NULL;
    }
    if (tls->last_err != NULL) {
        free(tls->last_err);
        tls->last_err = NULL;
    }
}
#endif
