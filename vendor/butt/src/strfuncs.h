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

#ifndef STRFUNCS_H
#define STRFUNCS_H

enum {
    MODE_ALL = 0,
    MODE_FIRST = 1,
    MODE_LAST = 2,
};

// replaces all strings "replace" with "search" in "dest"
int strrpl(char **dest, char *search, char *replace, int mode);

// inserts string "insert" at position "pos"
int strinsrt(char **dest, char *insert, char *pos);

// finds last occurrence of substring "needle" in string "haystack"
char *strrstr(char *haystack, char *needle);

// "Percent encode" reserved characters according to RFC3986 section 2.2 for use in URIs
void strencoderfc3986(char **buf);

// Converts string "str" to lower case variant
char *strtolower(char *str);

// Converts string "str" to upper case variant
char *strtoupper(char *str);

#endif
