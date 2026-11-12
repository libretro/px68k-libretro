/*
 * Minimal libretro-common rthreads interface used by PX68K.
 * Copyright (C) 2010-2020 The RetroArch team
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 */
#ifndef LIBRETRO_SDK_RTHREADS_H__
#define LIBRETRO_SDK_RTHREADS_H__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sthread sthread_t;
typedef struct slock slock_t;
typedef struct scond scond_t;

sthread_t *sthread_create(void (*thread_func)(void *), void *userdata);
void sthread_join(sthread_t *thread);

slock_t *slock_new(void);
void slock_free(slock_t *lock);
void slock_lock(slock_t *lock);
void slock_unlock(slock_t *lock);

scond_t *scond_new(void);
void scond_free(scond_t *cond);
void scond_wait(scond_t *cond, slock_t *lock);
void scond_signal(scond_t *cond);
void scond_broadcast(scond_t *cond);

#ifdef __cplusplus
}
#endif
#endif
