/*
Author:
    Daniel Nöthen
Date:
    August 2022
Description:
    Helper functions to create and handle atomic variables with pthreads
*/

#ifndef ATOM_H
#define ATOM_H
#include <stdint.h>
#include <pthread.h>
#include <errno.h>

#define ATOM_NEW_COND(cond) atom_cond_t cond = {0, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER}
#define ATOM_NEW_INT(atom)  atom_int_t atom = {0, PTHREAD_MUTEX_INITIALIZER}

typedef struct atom_cond {
    int cond;
    pthread_mutex_t m;
    pthread_cond_t c;
} atom_cond_t;

typedef struct atom_int {
    int val;
    pthread_mutex_t m;
} atom_int_t;

inline void atom_cond_init(atom_cond_t *cond)
{
    cond->cond = 0;
    pthread_mutex_init(&cond->m, NULL);
    pthread_cond_init(&cond->c, NULL);
}

inline void atom_cond_reset(atom_cond_t *cond)
{
    pthread_mutex_lock(&cond->m);
    cond->cond = 0;
    pthread_mutex_unlock(&cond->m);
}

inline void atom_cond_destroy(atom_cond_t *cond)
{
    pthread_cond_destroy(&cond->c);
    pthread_mutex_destroy(&cond->m);
}

inline void atom_cond_wait(atom_cond_t *cond)
{
    pthread_mutex_lock(&cond->m);
    while (cond->cond == 0) {
        pthread_cond_wait(&cond->c, &cond->m);
    }
    cond->cond = 0;
    pthread_mutex_unlock(&cond->m);
}

inline int atom_cond_timedwait(atom_cond_t *cond, uint32_t timeout_us)
{
    int ret;
    struct timespec t = {0};
    t.tv_sec = 0;
    t.tv_nsec = timeout_us * 1000;

    pthread_mutex_lock(&cond->m);
    while (cond->cond == 0) {
        ret = pthread_cond_timedwait(&cond->c, &cond->m, &t);
        if (ret == ETIMEDOUT) {
            pthread_mutex_unlock(&cond->m);
            return 0;
        }
    }
    cond->cond = 0;
    pthread_mutex_unlock(&cond->m);
    return 1;
}

inline void atom_cond_signal(atom_cond_t *cond)
{
    pthread_mutex_lock(&cond->m);
    cond->cond = 1;
    pthread_cond_signal(&cond->c);
    pthread_mutex_unlock(&cond->m);
}

inline void atom_set_int(atom_int_t *atom, int val)
{
    pthread_mutex_lock(&atom->m);
    atom->val = val;
    pthread_mutex_unlock(&atom->m);
}

inline int atom_get_int(atom_int_t *atom)
{
    int val;
    pthread_mutex_lock(&atom->m);
    val = atom->val;
    pthread_mutex_unlock(&atom->m);
    return val;
}

#endif // ATOM_H