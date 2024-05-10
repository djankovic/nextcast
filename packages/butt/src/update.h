#ifndef UPDATE_H
#define UPDATE_H

int update_check_for_new_version(void);
char *update_get_version(void);

enum {
    UPDATE_NEW_VERSION = 0,
    UPDATE_SOCKET_ERROR = -1,
    UPDATE_ILLEGAL_ANSWER = -2,
    UPDATE_UP_TO_DATE = -3,
};

#endif
