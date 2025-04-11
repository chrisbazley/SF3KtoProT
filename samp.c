/*
 *  SF3KtoProT - Converts Star Fighter 3000 music to Amiga ProTracker format
 *  Sound samples index file parser
 *  Copyright (C) 2009-2010 Christopher Bazley
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public Licence as published by
 *  the Free Software Foundation; either version 2 of the Licence, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public Licence for more details.
 *
 *  You should have received a copy of the GNU General Public Licence
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* ISO library header files */
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>
#include <errno.h>

/* CBUtilLib headers */
#include "StringBuff.h"

/* Local header files */
#include "misc.h"
#include "samp.h"
#include "protracker.h"

enum {
  INIT_SIZE = 4 /* No. of sound samples */
};

static void report_error(const char   *error,
                         int           line_no,
                         const char   *index_file)
{
  fprintf(stderr, "%s at line %d of samples index file '%s'\n",
          error, line_no + 1, index_file);
}

static long int get_sample_len(const bool verbose,
                               const char * const samples_dir,
                               const char * const file_name)
{
  long int len = -1;
  assert(samples_dir != NULL);
  assert(file_name != NULL);

  /* Construct full path name of sample data file */
  StringBuffer sample_path;
  stringbuffer_init(&sample_path);

  if (!stringbuffer_append(&sample_path, samples_dir, SIZE_MAX) ||
      !stringbuffer_append_separated(&sample_path, PATH_SEPARATOR,
                                     file_name)) {
    fprintf(stderr, "Failed to allocate memory for sample data file path\n");
  } else {
    /* Get the length of the sample data file */
    if (verbose)
      printf("Opening sample data file '%s'\n",
             stringbuffer_get_pointer(&sample_path));

    _Optional FILE * const sample_handle = fopen(
         stringbuffer_get_pointer(&sample_path), "rb");
    if (sample_handle == NULL) {
      fprintf(stderr,
              "Failed to open sample data file: %s\n",
              strerror(errno));
    } else {
      if (!fseek(&*sample_handle, 0, SEEK_END))
        len = ftell(&*sample_handle);

      if (len < 0) {
        fprintf(stderr,
                "Couldn't determine length of sample data file: %s\n",
                strerror(errno));
      }

      /* Note that even a sample that is apparently too long for ProTracker
         format might only be included in a form where it has been pre-tuned
         upward by one or more octaves (thus shortening it). */

      if (verbose)
        puts("Closing sample data file");

      fclose(&*sample_handle);
    }
  }

  stringbuffer_destroy(&sample_path);
  return len;
}

static SampleInfo_Type char_to_type(int c)
{
  SampleInfo_Type type;

  assert(c >= CHAR_MIN);
  assert(c <= CHAR_MAX);

  switch (c) {
    case 'e':
    case 'E':
      type = SampleInfo_Type_Effect;
      break;

    case 'm':
    case 'M':
      type = SampleInfo_Type_Music;
      break;

    default:
      type = SampleInfo_Type_Unused;
      break;
  }
  return type;
}

static bool add_sf_sample(const bool verbose, SampleArray * const sf_samples,
                          const int sample_id, const char * const file_name,
                          const int repeat_offset, long int len,
                          const SampleInfo_Type type,
                          const int tuning)
{
  assert(sf_samples != NULL);
  assert(sf_samples->count >= 0);
  assert(sf_samples->count <= sf_samples->alloc);
  assert((sf_samples->alloc == 0) == (sf_samples->sample_info == NULL));
  assert(sample_id >= 0);
  assert(sample_id <= UCHAR_MAX);
  assert(file_name != NULL);
  assert(repeat_offset >= 0);
  assert(len >= 0);
  assert((repeat_offset / 2) < (len / 4));
  assert((type == SampleInfo_Type_Music) || (type == SampleInfo_Type_Effect));

  if (sample_id >= sf_samples->alloc || !sf_samples->sample_info) {
    /* (Re-)allocate buffer for data */

    DEBUGF("Sample ID %d is beyond end of array\n", sample_id);
    int new_limit = sf_samples->alloc;
    do {
      if (new_limit == 0)
        new_limit = INIT_SIZE;
      else
        new_limit *= 2;
    } while (sample_id >= new_limit);

    DEBUGF("Extending samples array from %d to %d\n",
           sf_samples->alloc, new_limit);

    const size_t new_size = new_limit * sizeof(SampleInfo);
    _Optional SampleInfo * const new_buf = realloc(sf_samples->sample_info, new_size);

    if (new_buf == NULL) {
      fprintf(stderr,
              "Failed to allocate %zu bytes for SF3000 sample info\n",
              new_size);
      return false;
    }

    sf_samples->sample_info = new_buf;
    sf_samples->alloc = new_limit;
  }

  if (sample_id >= sf_samples->count) {
    sf_samples->count = sample_id+1;
  }
  assert(sf_samples->sample_info != NULL);
  SampleInfo * const write_ptr = &sf_samples->sample_info[sample_id];

  strncpy(write_ptr->file_name, file_name, sizeof(write_ptr->file_name) - 1);
  write_ptr->file_name[sizeof(write_ptr->file_name) - 1] = '\0';

  write_ptr->len = len;
  write_ptr->repeat_offset = repeat_offset;
  write_ptr->type = type;
  write_ptr->tuning = tuning;

  if (verbose) {
    printf("Sample %d ('%s') has length %lu, tuning %d and repeats from %d\n",
           sample_id,
           write_ptr->file_name,
           write_ptr->len,
           write_ptr->tuning,
           write_ptr->repeat_offset);
  }

  return true;
}

static bool parse_index(const bool verbose, FILE * const f,
                        const char * const samples_dir,
                        const char * const index_file,
                        SampleArray * const sf_samples)
{
  bool success = true;

  assert(f != NULL);
  assert(!ferror(f));
  assert(samples_dir != NULL);
  assert(index_file != NULL);
  assert(sf_samples != NULL);

  sf_samples->count = 0;
  sf_samples->alloc = 0;
  sf_samples->sample_info = NULL;

  /* Read one line of text at a time from the samples index file */
  int line_no;
  char str_buf[256];
  for (line_no = 0;
       (fgets(str_buf, sizeof(str_buf), f) != NULL) && success;
       line_no++) {

    Fortify_CheckAllMemory();

    /* Skip comments */
    if (*str_buf == '#') {
      DEBUGF("Skipping a comment\n");
      continue;
    }

    /* Eat up any leading whitespace characters */
    const char *nws;
    for (nws = str_buf; isspace(*nws); nws++)
    {}
    DEBUGF("Ate %td leading spaces\n", nws - str_buf);

    /* Have we reached the end of the line? */
    if (*nws == '\0') {
      DEBUGF("Skipping a blank line\n");
      continue;
    }

    /* Parse the text line and extract the sample's attributes */
    char file_name[12], type_buf[2];
    int tuning, repeat_offset, sample_id;
    const int nread = sscanf(nws, "%d %11s %d %1s %d\n", &sample_id,
                             file_name, &repeat_offset, type_buf, &tuning);
    if (nread != 5) {
      report_error("Syntax error", line_no, index_file);
      success = false;
      break;
    }

    if (sample_id < 0 || sample_id > UCHAR_MAX) {
      report_error("Bad ID", line_no, index_file);
      success = false;
    } else if (sample_id < sf_samples->count && sf_samples->sample_info) {
      if (sf_samples->sample_info[sample_id].type != SampleInfo_Type_Unused) {
        report_error("ID already used", line_no, index_file);
        success = false;
      }
    }

    const long int len = get_sample_len(verbose, samples_dir, file_name);
    if (len < 0) {
      success = false;
    } else {
      /* ProTracker isn't capable of representing odd sample lengths or offsets
         within a sample (only multiples of 2). Also, the length is in bytes
         (2 bytes per sample frame) */
      if (repeat_offset < 0 ||
          repeat_offset / 2l >= len / 4) {
        report_error("Bad repeat offset", line_no, index_file);
        success = false;
      }
    }

    const SampleInfo_Type type = char_to_type(*type_buf);
    if (type == SampleInfo_Type_Unused) {
      report_error("Bad sample type", line_no, index_file);
      success = false;
    }

    if (!check_tuning(tuning)) {
      report_error("Bad tuning value", line_no, index_file);
      success = false;
    }

    if (success) {
      success = add_sf_sample(verbose, sf_samples, sample_id, file_name,
                              repeat_offset, len, type, tuning);
    }
  }

  /* fgets failed - was it an error or just end of file? */
  if (success && ferror(f)) {
    fprintf(stderr,
            "Error reading line %d from samples index file: %s\n",
            line_no + 1,
            strerror(errno));
    success = false;
  }

  if (!success) {
    free(sf_samples->sample_info);
    sf_samples->sample_info = NULL;
  }

  return success;
}

bool load_sample_index(const bool verbose, const char * const index_file,
                       const char * const samples_dir,
                       SampleArray * const sf_samples)
{
  bool success = false;
  assert(index_file != NULL);
  assert(samples_dir != NULL);
  assert(sf_samples != NULL);

  /* Open samples index file */
  if (verbose)
    printf("Opening sound samples index file '%s'\n", index_file);

  _Optional FILE * const f = fopen(index_file, "r"); /* open text for reading */
  if (f == NULL) {
    fprintf(stderr,
            "Failed to open samples index file: %s\n",
            strerror(errno));
  } else {
    success = parse_index(verbose, &*f, samples_dir, index_file, sf_samples);

    if (verbose)
      puts("Closing sound samples index file");

    fclose(&*f);
  }
  return success;
}
