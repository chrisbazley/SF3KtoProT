/*
 *  SF3KtoProT - Converts Star Fighter 3000 music to Amiga ProTracker format
 *  ProTracker conversion routines
 *  Copyright (C) 2009-2010 Christopher Bazley
 */

#ifndef PROTRACKER_H
#define PROTRACKER_H

/* ISO library header files */
#include <stdio.h>
#include <stdbool.h>

/* StreamLib headers */
#include "Reader.h"

/* Local headers */
#include "samp.h"

/* Flags controlling generation of ProTracker music */
enum {
  FLAGS_GLISSANDO_SINGLE = 1<<0, /* default is all channels */
  FLAGS_BLANK_PATTERN    = 1<<1, /* default is abrupt finish to song */
  FLAGS_VERBOSE          = 1<<2, /* emit information about processing */
  FLAGS_ALLOW_SFX        = 1<<3, /* allow sound effects during music */
  FLAGS_EXTRA_OCTAVES    = 1<<4, /* use non-standard octaves 0 and 4 */
  FLAGS_ALL              = (1<<5)-1
};

extern bool create_protracker(unsigned int       flags,
                              const char        *song_name,
                              Reader            *in,
                              const char        *samples_dir,
                              const SampleArray *sf_samples,
                              FILE              *out);

extern bool check_tuning(signed int sf_tuning);

#endif /* PROTRACKER_H */
