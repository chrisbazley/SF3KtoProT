/*
 *  SF3KtoProT - Converts Star Fighter 3000 music to Amiga ProTracker format
 *  Sound samples index file parser
 *  Copyright (C) 2009 Christopher Bazley
 */

#ifndef SAMP_H
#define SAMP_H

/* ISO library header files */
#include <stdbool.h>

typedef enum {
  SampleInfo_Type_Effect,
  SampleInfo_Type_Music,
  SampleInfo_Type_Unused
} SampleInfo_Type;

typedef struct {
  char            file_name[12];
  unsigned int    repeat_offset;
  signed   int    tuning;
  unsigned long   len;
  SampleInfo_Type type;
} SampleInfo;

typedef struct {
  int count;
  int alloc;
  SampleInfo *sample_info;
} SampleArray;

extern bool load_sample_index(bool          verbose,
                              const char   *index_file,
                              const char   *samples_dir,
                              SampleArray  *sf_samples);

#endif /* SAMP_H */
