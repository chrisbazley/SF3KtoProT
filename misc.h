/*
 *  SF3KtoProT - Converts Star Fighter 3000 music to Amiga ProTracker format
 *  Miscellaneous macro definitions
 *  Copyright (C) 2018 Christopher Bazley
 */

#ifndef MISC_H
#define MISC_H

/* Modify these definitions for Unix or Windows file paths. */
#ifndef PATH_SEPARATOR
#define PATH_SEPARATOR '.'
#endif

#ifndef EXT_SEPARATOR
#define EXT_SEPARATOR '/' /* e.g. ADFS::4.$.Star3000.Graphics.Earth1/obj */
#endif

#ifdef FORTIFY
#include "Fortify.h"
#else
#define Fortify_CheckAllMemory()
#endif

#ifdef USE_CBDEBUG

#include "Debug.h"
#include "PseudoIO.h"
#include "PseudoKern.h"

#else /* USE_CBDEBUG */

#include <stdio.h>
#include <assert.h>

#define DEBUG_SET_OUTPUT(output_mode, log_name)

#ifdef DEBUG_OUTPUT
#define DEBUGF if (1) printf
#else
#define DEBUGF if (0) printf
#endif /* DEBUG_OUTPUT */

#endif /* USE_CBDEBUG */

#ifdef USE_OPTIONAL
#include <stdlib.h>

#undef NULL
#define NULL ((_Optional void *)0)

static inline void optional_free(_Optional void *x)
{
    free((void *)x);
}
#undef free
#define free(x) optional_free(x)

static inline _Optional void *optional_malloc(size_t n)
{
    return malloc(n);
}
#undef malloc
#define malloc(n) optional_malloc(n)

static inline _Optional void *optional_realloc(_Optional void *p, size_t n)
{
    return realloc((void *)p, n);
}
#undef realloc
#define realloc(p, n) optional_realloc(p, n)

#else
#define _Optional
#endif

#endif /* MISC_H */
