// sha256 functions for butt
//
// Copyright 2007-2021 by Daniel Noethen.
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
#ifndef SHA256_H
#define SHA256_H

void sha256_string(char *string, char outputBuffer[65], int salt);
int sha256_file(char *path, char outputBuffer[65], int salt);

#endif // SHA256_H
