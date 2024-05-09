//
//  sha256.cpp
//  butt
//
//  Created by Daniel Nöthen on 17.03.21.
//  Copyright © 2021 Daniel Nöthen. All rights reserved.
//
//
#ifdef WIN32

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/sha.h>

#include "sha256.h"

void sha256_string(char *string, char outputBuffer[65], int salt)
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, string, strlen(string));
    SHA256_Final(hash, &sha256);

    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(outputBuffer + (i * 2), "%02x", hash[i]);
    }
    outputBuffer[64] = 0;
}

int sha256_file(char *path, char outputBuffer[65], int salt)
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    const int buf_size = 32768;
    unsigned char *buffer;
    size_t bytes_read = 0;

    FILE *file = fopen(path, "rb");
    if (!file) {
        return -1;
    }

    buffer = (unsigned char *)malloc(buf_size);
    if (!buffer) {
        return -1;
    }

    SHA256_Init(&sha256);

    if (salt > 0) {
        SHA256_Update(&sha256, &salt, sizeof(int));
    }

    while ((bytes_read = fread(buffer, 1, buf_size, file))) {
        SHA256_Update(&sha256, buffer, bytes_read);
    }
    SHA256_Final(hash, &sha256);

    // sha256_hash_string(hash, outputBuffer);

    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(outputBuffer + (i * 2), "%02x", hash[i]);
    }
    outputBuffer[64] = 0;

    fclose(file);
    free(buffer);
    return 0;
}
#endif
