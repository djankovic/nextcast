//
//  url.c
//  iziCast
//
//  Created by Daniel Nöthen on 29.05.19.
//  Copyright © 2019 Daniel Nöthen. All rights reserved.
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <curl/curl.h>

#include "url.h"
#include "fl_funcs.h" // write_log()
#include "config.h"   // VERSION

int write_logfile = 0;
int print_log = 0;
int curl_initialized = 0;
static char log_buf[100 * 1024];

struct MemoryStruct {
    char *memory;
    uint32_t size;
};

struct data {
    char trace_ascii; /* 1 or 0 */
};

static void dump(const char *text, FILE *stream, unsigned char *ptr, size_t size, char nohex)
{
    size_t i;
    size_t c;

    unsigned int width = 0x10;

    if (nohex) {
        /* without the hex output, we can fit more on screen */
        width = 0x40;
    }

    if (print_log == 1) {
        fprintf(stream, "%s, %10.10lu bytes (0x%8.8lx)\n", text, (unsigned long)size, (unsigned long)size);
    }

    if (write_logfile == 1) {
        snprintf(log_buf, sizeof(log_buf), "%s, %10.10lu bytes (0x%8.8lx)\n", text, (unsigned long)size, (unsigned long)size);
        write_log(log_buf);
    }

    for (i = 0; i < size; i += width) {
        if (print_log == 1) {
            fprintf(stream, "%4.4lx: ", (unsigned long)i);
        }
        if (write_logfile == 1) {
            snprintf(log_buf, sizeof(log_buf), "%4.4lx: ", (unsigned long)i);
            write_log(log_buf);
        }

        if (!nohex) {
            /* hex not disabled, show it */
            for (c = 0; c < width; c++)
                if (i + c < size) {
                    if (print_log == 1) {
                        fprintf(stream, "%02x ", ptr[i + c]);
                    }

                    if (write_logfile == 1) {
                        snprintf(log_buf, sizeof(log_buf), "%02x ", ptr[i + c]);
                        write_log(log_buf);
                    }
                }
                else {
                    if (print_log == 1) {
                        fputs("   ", stream);
                    }
                    if (write_logfile == 1) {
                        write_log("   ");
                    }
                }
        }

        for (c = 0; (c < width) && (i + c < size); c++) {
            /* check for 0D0A; if found, skip past and start a new line of output */
            if (nohex && (i + c + 1 < size) && ptr[i + c] == 0x0D && ptr[i + c + 1] == 0x0A) {
                i += (c + 2 - width);
                break;
            }
            // fprintf(stream, "%c", (ptr[i + c] >= 0x20) && (ptr[i + c]<0x80) ? ptr[i + c] : '.');
            if (print_log == 1) {
                fprintf(stream, "%c", ptr[i + c]);
            }

            if (write_logfile == 1) {
                snprintf(log_buf, sizeof(log_buf), "%c", ptr[i + c]);
                write_log(log_buf);
            }

            /* check again for 0D0A, to avoid an extra \n if it's at width */
            if (nohex && (i + c + 2 < size) && ptr[i + c + 1] == 0x0D && ptr[i + c + 2] == 0x0A) {
                i += (c + 3 - width);
                break;
            }
        }
        if (print_log == 1) {
            fputc('\n', stream); /* newline */
        }

        if (write_logfile == 1) {
            write_log("\n");
        }
    }
    if (print_log == 1) {
        fflush(stream);
    }
}

static int my_trace(CURL *handle, curl_infotype type, char *data, size_t size, void *userp)
{
    struct data *config = (struct data *)userp;
    const char *text;
    (void)handle; /* prevent compiler warning */

    switch (type) {
    case CURLINFO_TEXT:
        if (print_log == 1) {
            fprintf(stderr, "== Info: %s", data);
        }

        if (write_logfile == 1) {
            snprintf(log_buf, sizeof(log_buf), "== Info: %s", data);
            write_log(log_buf);
        }
        /* FALLTHROUGH */
    default: /* in case a new one is introduced to shock us */
        return 0;

    case CURLINFO_HEADER_OUT:
        text = "=> Send header";
        break;
    case CURLINFO_DATA_OUT:
        text = "=> Send data";
        break;
    case CURLINFO_SSL_DATA_OUT:
        text = "=> Send SSL data";
        break;
    case CURLINFO_HEADER_IN:
        text = "<= Recv header";
        break;
    case CURLINFO_DATA_IN:
        text = "<= Recv data";
        break;
    case CURLINFO_SSL_DATA_IN:
        text = "<= Recv SSL data";
        break;
    }

    dump(text, stderr, (unsigned char *)data, size, config->trace_ascii);
    return 0;
}

static size_t callback_for_curl(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    char *ptr = (char *)realloc(mem->memory, mem->size + realsize + 1);
    if (ptr == NULL) {
        /* out of memory! */
        printf("%s\n", "not enough memory (realloc returned NULL)");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

void url_init_curl(void)
{
    if (curl_initialized == 1) {
        return;
    }
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl_initialized = 1;
}

uint32_t url_post_json(const char *url, char *post_data, char *answer, uint32_t max_answer_size)
{
    if (curl_initialized == 0) {
        url_init_curl();
    }

    struct MemoryStruct chunk;

    chunk.memory = (char *)malloc(1); /* will be grown as needed by the realloc */
    chunk.size = 0;                   /* no data at this point */

    CURL *curl;
    CURLcode res;

    struct data config;
    config.trace_ascii = 1; /* enable ascii tracing for debug */

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "charset: utf-8");

    curl = curl_easy_init();
    if (curl) {
        if (print_log || write_logfile) {
            curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, my_trace);
            curl_easy_setopt(curl, CURLOPT_DEBUGDATA, &config);
            curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        }

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "butt");
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 7L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, callback_for_curl);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

        /* Perform the request, res will get the return code */
        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            printf("URL request failed: %s\n", curl_easy_strerror(res));
            chunk.size = 0;
        }
        else {
            long response_code;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

            if (chunk.size + 1 > max_answer_size) {
                printf("%s\n", "buffer too small");
                chunk.size = 0;
            }
            /*     else if (response_code != 200) {
                     printf("html response = %ld\n", response_code);
                     chunk.size = 0;
                 }
                 else if (strstr(chunk.memory, "Invalid resource")) { // Shoutcast v1 response with http_code = 200 but "Invalid ressource"
                                                                      // if a wrong url is requested
                     printf("%s\n", "Invalid resource");
                     chunk.size = 0;
                 }*/
            else {
                memcpy(answer, chunk.memory, chunk.size);
                answer[chunk.size] = '\0';
            }
        }

        curl_easy_cleanup(curl);
    }

    free(chunk.memory);

    return chunk.size;
}

uint32_t url_post(const char *url, char *post_data, char *answer, uint32_t max_answer_size)
{
    if (curl_initialized == 0) {
        url_init_curl();
    }

    struct MemoryStruct chunk;

    chunk.memory = (char *)malloc(1); /* will be grown as needed by the realloc */
    chunk.size = 0;                   /* no data at this point */

    CURL *curl;
    CURLcode res;

    struct data config;
    config.trace_ascii = 1; /* enable ascii tracing for debug */

    curl = curl_easy_init();
    if (curl) {
        if (print_log || write_logfile) {
            curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, my_trace);
            curl_easy_setopt(curl, CURLOPT_DEBUGDATA, &config);
            curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        }

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "butt");
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 7L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, callback_for_curl);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

        /* Perform the request, res will get the return code */
        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            printf("URL post failed: %s\n", curl_easy_strerror(res));
            write_log("url_post: URL post failed: ");
            write_log(curl_easy_strerror(res));
            write_log("\n");

            chunk.size = 0;
        }
        else {
            long response_code;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

            if (chunk.size + 1 > max_answer_size) {
                printf("%s\n", "buffer too small");
                write_log("url_post: buffer too small");
                chunk.size = 0;
            }
            /*     else if (response_code != 200) {
                     printf("html response = %ld\n", response_code);
                     chunk.size = 0;
                 }
                 else if (strstr(chunk.memory, "Invalid resource")) { // Shoutcast v1 response with http_code = 200 but "Invalid ressource"
                                                                      // if a wrong url is requested
                     printf("%s\n", "Invalid resource");
                     chunk.size = 0;
                 }*/
            else {
                memcpy(answer, chunk.memory, chunk.size);
                answer[chunk.size] = '\0';
            }
        }

        curl_easy_cleanup(curl);
    }

    free(chunk.memory);

    return chunk.size;
}

uint32_t url_get(const char *url, const char *custom_hdr, char *answer, uint32_t data_size)
{
    if (curl_initialized == 0) {
        url_init_curl();
    }

    struct MemoryStruct chunk;

    chunk.memory = (char *)malloc(1); /* will be grown as needed by the realloc */
    chunk.size = 0;                   /* no data at this point */

    CURL *curl;
    CURLcode res;

    struct data config;
    config.trace_ascii = 1; /* enable ascii tracing for debug */

    struct curl_slist *hdr = NULL;
    if (custom_hdr != NULL) {
        hdr = curl_slist_append(hdr, custom_hdr);
    }

    curl = curl_easy_init();

    if (curl) {
        char user_agent[32];
        snprintf(user_agent, sizeof(user_agent), "butt %s", VERSION);
        if (print_log || write_logfile) {
            curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, my_trace);
            curl_easy_setopt(curl, CURLOPT_DEBUGDATA, &config);
            curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        }

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, callback_for_curl);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

        if (hdr != NULL) {
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdr);
        }

        /* Perform the request, res will get the return code */
        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            printf("URL request failed: %s\n", curl_easy_strerror(res));
            write_log("url_get: URL get failed: ");
            write_log(curl_easy_strerror(res));
            write_log("\n");
            chunk.size = 0;
        }
        else {
            long response_code;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

            if (chunk.size + 1 > data_size) {
                printf("%s\n", "buffer too small");
                write_log("url_get: buffer too small");
                chunk.size = 0;
            }
            else {
                memcpy(answer, chunk.memory, chunk.size);
                answer[chunk.size] = '\0';
            }
        }

        curl_easy_cleanup(curl);
    }

    free(chunk.memory);

    return chunk.size;
}

// Same as url_get() with a few icecast/shoutcast specific changes
uint32_t url_get_listener_count(const char *url, char *data, uint32_t data_size)
{
    if (curl_initialized == 0) {
        url_init_curl();
        printf("curl initialized\n");
    }

    struct MemoryStruct chunk;

    chunk.memory = (char *)malloc(1); /* will be grown as needed by the realloc */
    chunk.size = 0;                   /* no data at this point */

    CURL *curl;
    CURLcode res;

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        // User Agent has to be set to "Mozilla". Otherwise Shoutcast v1 responses with 404
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla");
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, callback_for_curl);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

        /* Perform the request, res will get the return code */
        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            // printf("url_get_listener: URL request failed: %s\n", curl_easy_strerror(res));
            write_log("url_get_listener: failed with error: ");
            write_log(curl_easy_strerror(res));
            write_log("\n");
            chunk.size = 0;
        }
        else if (chunk.size > 0) {
            long response_code;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

            if (chunk.size > data_size) {
                write_log("url_get_listener: buffer too small");
                chunk.size = 0;
            }
            else if (response_code != 200) {
                // printf("url_get_listener: html response = %ld\n", response_code);
                write_log("url_get_listener: html response != 200\n");
                chunk.size = 0;
            }
            else if (strstr(chunk.memory, "Invalid resource")) { // Shoutcast v1 response with http_code = 200 but "Invalid ressource"
                // if a wrong url is requested
                write_log("url_get_listener: Invalid resource\n");
                chunk.size = 0;
            }
            else {
                memcpy(data, chunk.memory, chunk.size);
            }
        }

        curl_easy_cleanup(curl);
    }

    free(chunk.memory);

    return chunk.size;
}

void url_cleanup_curl(void)
{
    curl_global_cleanup();
}
