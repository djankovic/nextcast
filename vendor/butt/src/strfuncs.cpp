// string manipulation functions for butt
//
// Copyright 2007-2018 by Daniel Noethen.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "strfuncs.h"

int strinsrt(char **dest, char *insert, char *pos)
{
    char *pre;
    char *post;
    char *temp;
    int pre_len;
    int post_len;
    int new_len;

    new_len = strlen(*dest) + strlen(insert);
    pre_len = strlen(*dest) - strlen(pos);
    post_len = strlen(pos);

    pre = (char *)malloc(pre_len * sizeof(char) + 1);
    post = (char *)malloc(post_len * sizeof(char) + 1);
    temp = (char *)malloc(new_len * sizeof(char) + 1);

    memcpy(pre, *dest, pre_len);
    pre[pre_len] = '\0';

    memcpy(post, *dest + pre_len, post_len);
    post[post_len] = '\0';

    sprintf(temp, "%s%s%s", pre, insert, post);

    *dest = (char *)realloc(*dest, new_len * sizeof(char) + 1);
    strcpy(*dest, temp);

    free(pre);
    free(post);
    free(temp);

    return 0;
}

// returns a pointer to the last occurance of string "needle" in string "haystack"
char *strrstr(char *haystack, char *needle)
{
    char *last;
    char *found = NULL;

    do {
        last = found;
        found = strstr(haystack, needle);

        if (found != NULL) {
            haystack = found + strlen(needle);
        }

    } while (found != NULL);

    return last;
}

// replaces all strings named by search with strings named by replace in dest
int strrpl(char **dest, char *search, char *replace, int mode)
{
    char *loc;
    char *temp;
    char *orig;
    char *result;
    int search_len, repl_len, diff;
    int size;
    int count;

    // do nothing if there is not at least one string of "search" in "*dest"
    if (strstr(*dest, search) == NULL) {
        return -1;
    }

    search_len = strlen(search);
    repl_len = strlen(replace);
    diff = repl_len - search_len;

    // how many strings do we need to replace?
    if (mode == MODE_ALL) {
        temp = *dest;
        for (count = 0; (temp = strstr(temp, search)); count++)
            temp += search_len;
    }
    else {
        count = 1;
    }

    // length of the new string
    size = strlen(*dest) + (diff * count);

    temp = strdup(*dest);
    orig = temp;

    result = (char *)malloc(size * sizeof(char) + 1);
    if (!result) {
        free(orig);
        return -1;
    }
    memset(result, 0, size);

    // build the new string
    switch (mode) {
    case MODE_ALL:
        while ((loc = strstr(temp, search))) {
            strncat(result, temp, loc - temp);
            strcat(result, replace);
            temp = loc + strlen(search);
        }
        // append remaininc characaters (if any)
        if (strlen(temp) > 0) {
            strcat(result, temp);
        }
        break;

    case MODE_FIRST:
        loc = strstr(temp, search);
        strncat(result, temp, loc - temp);
        strcat(result, replace);

        temp = loc + strlen(search);
        // append remaining characters
        if (strlen(temp) > 0) {
            strcat(result, temp);
        }
        break;
    case MODE_LAST:
        loc = strrstr(temp, search);
        strncat(result, temp, loc - temp);
        strcat(result, replace);

        temp = loc + strlen(search);
        // append remaininc characaters (if any)
        if (strlen(temp) > 0) {
            strcat(result, temp);
        }

        break;
    default:
        return -1;
    }

    if (strlen(result) > strlen(*dest)) {
        *dest = (char *)realloc(*dest, size * sizeof(char) + 1);
        if (!*dest) {
            free(result);
            free(orig);
            return -1;
        }
    }

    // save the new string back to orig position in memory
    strcpy(*dest, result);

    free(orig);
    free(result);

    return 0;
}

char *strtolower(char *str)
{
    if (str != NULL) {
        for (int i = 0; i < strlen(str); i++) {
            str[i] = tolower(str[i]);
        }
    }

    return str;
}

char *strtoupper(char *str)
{
    if (str != NULL) {
        for (int i = 0; i < strlen(str); i++) {
            str[i] = toupper(str[i]);
        }
    }

    return str;
}

// "Percent encode" reserved characters according to RFC3986 section 2.2 for use in URIs
void strencoderfc3986(char **buf)
{
    /* Reserved characters: % :/?#[]@!$&'()*+,;= */
    /* Results in: %3d%20%3a%2f%3f%23%5b%5d%40%21%24%26%27%28%29%2a%2b%2c%3b%25 */

    strrpl(buf, (char *)"%", (char *)"%25", MODE_ALL); // this must come first
    strrpl(buf, (char *)" ", (char *)"%20", MODE_ALL);
    strrpl(buf, (char *)":", (char *)"%3a", MODE_ALL);
    strrpl(buf, (char *)"/", (char *)"%2f", MODE_ALL);
    strrpl(buf, (char *)"?", (char *)"%3f", MODE_ALL);
    strrpl(buf, (char *)"#", (char *)"%23", MODE_ALL);
    strrpl(buf, (char *)"[", (char *)"%5b", MODE_ALL);
    strrpl(buf, (char *)"]", (char *)"%5d", MODE_ALL);
    strrpl(buf, (char *)"@", (char *)"%40", MODE_ALL);
    strrpl(buf, (char *)"!", (char *)"%21", MODE_ALL);
    strrpl(buf, (char *)"$", (char *)"%24", MODE_ALL);
    strrpl(buf, (char *)"&", (char *)"%26", MODE_ALL);
    strrpl(buf, (char *)"'", (char *)"%27", MODE_ALL);
    strrpl(buf, (char *)"(", (char *)"%28", MODE_ALL);
    strrpl(buf, (char *)")", (char *)"%29", MODE_ALL);
    strrpl(buf, (char *)"*", (char *)"%2a", MODE_ALL);
    strrpl(buf, (char *)"+", (char *)"%2b", MODE_ALL);
    strrpl(buf, (char *)",", (char *)"%2c", MODE_ALL);
    strrpl(buf, (char *)";", (char *)"%3b", MODE_ALL);
    strrpl(buf, (char *)"=", (char *)"%3d", MODE_ALL);
}
