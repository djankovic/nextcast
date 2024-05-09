//
//  url.h
//  iziCast
//
//  Created by Daniel Nöthen on 29.05.19.
//  Copyright © 2019 Daniel Nöthen. All rights reserved.
//

#ifndef URL_H
#define URL_H

#include <stdio.h>
#include <stdint.h>

void url_init_curl(void);
void url_cleanup_curl(void);
uint32_t url_get(const char *url, const char *custom_hdr, char *data, uint32_t data_size);
uint32_t url_get_listener_count(const char *url, char *data, uint32_t data_size);
uint32_t url_post(const char *url, char *post_data, char *answer, uint32_t max_answer_size);

#endif /* URL_H */
