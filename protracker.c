/*
 *  SF3KtoProT - Converts Star Fighter 3000 music to Amiga ProTracker format
 *  ProTracker conversion routines
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
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

/* StreamLib headers */
#include "Reader.h"

/* CBUtilLib headers */
#include "StringBuff.h"

/* Local header files */
#include "misc.h"
#include "main.h"
#include "samp.h"
#include "protracker.h"

enum {
  INIT_SIZE              = 4, /* No. of ProTracker samples */
  SEMITONES_PER_OCTAVE   = 12,
  SECONDS_PER_MINUTE     = 60,

/* The following values are dictated by the SF3000 music file format */
  MAX_SF_PATTERNS        = 64,
  SF_CLOCK_FREQ          = 90, /* Hz (actually 100, but for latency) */
  SF_MAX_VOLUME          = 15,
  SF_MAX_REPEATS         = 15,
  SF_GLISSANDO_THRESHOLD = 2, /* Values below this mean 'play note' */
  SF_TUNING_OCTAVE       = 4096, /* Tuning units per octave */
  NUM_SF_CHANNELS        = 4,
  BYTES_PER_SF_SAMPLE    = 2,
  NUM_SF_VOICES          = 16,

/* The following values are dictated by the ProTracker file format */
  MAX_PT_SAMPLES         = 31,
  BYTES_PER_PT_SAMPLE    = 30,
  BYTES_PER_PT_COMMAND   = 4,
  MAX_PT_SONG_LEN        = 128,
  MAX_PT_POSITIONS       = 64,
  NUM_SF_DIVISIONS       = 64,
  NUM_PT_CHANNELS        = 4,
  PT_BPM_DIVISOR         = 24, /* ProTracker tempo is based upon 1/24th of the
                                  no. of ticks per minute of a 50Hz timer. */
  PT_SPEED_THRESHOLD     = 32,
  PT_MAX_VOLUME          = 64,
  PT_COM_NORMAL          = 0x0,
  PT_COM_TONE_PORTAMENTO = 0x3,
  PT_COM_SET_VOLUME      = 0xc,
  PT_COM_PATTERN_BREAK   = 0xd,
  PT_COM_SET_SPEED       = 0xf,
  PT_TUNING_SEMITONE     = 8, /* Tuning units per semitone */
  PT_OCTAVE_RANGE        = 5,
  PT_GLISSANDO_SPEED     = 2
};

typedef struct {
  unsigned char  num_repeats;
  unsigned char  sample_num;
  unsigned short half_len;
  unsigned short half_repeat_offset;
  unsigned short half_repeat_len;
  signed long    pt_tuning;
  signed int     octaves_cheat;
} PTSampleInfo;

typedef struct {
  int count;
  int alloc;
  PTSampleInfo *sample_info;
} PTSampleArray;

typedef struct {
  uint8_t note;
  uint8_t oct_vol;
  uint8_t voice_act;
  uint8_t num_repeats;
} SFChannelData;

typedef struct {
  SFChannelData channels[NUM_SF_CHANNELS];
} SFDivision;

typedef struct {
  SFDivision divisions[NUM_SF_DIVISIONS];
} SFPattern;

typedef struct {
  uint8_t speed;
  uint8_t voice_table[NUM_SF_VOICES];
  int32_t last_pattern_no;
  uint8_t play_order[MAX_SF_PATTERNS];
  SFPattern *patterns;
} SFTrack;

typedef enum {
  GlissandoState_None, /* No glissando on this channel since last note */
  GlissandoState_Start, /* First event during a glissando */
  GlissandoState_Continue /* Subsequent events */
} GlissandoState;

typedef struct {
  unsigned char  pt_sample_no;
  unsigned char  sample_num;
  unsigned short target_pitch;
  GlissandoState glissando_state;
} ChannelState;

static int get_pt_period(const int octave, int note)
{
  /* Period table for Tuning 0, normal. Octaves 0 and 4 are non-standard and
     may not be supported by a tracker player. */
  static const int period_table[PT_OCTAVE_RANGE][SEMITONES_PER_OCTAVE] =
  {
    {1712,1616,1525,1440,1357,1281,1209,1141,1077,1017,961,907},/* C-0 to B-0 */
    {856,808,762,720,678,640,604,570,538,508,480,453}, /* C-1 to B-1 */
    {428,404,381,360,339,320,302,285,269,254,240,226}, /* C-2 to B-2 */
    {214,202,190,180,170,160,151,143,135,127,120,113}, /* C-3 to B-3 */
    {107,101,95,90,85,80,76,71,67,64,60,57} /* C-4 to B-4 */
  };
  assert(octave >= 0);
  assert(octave < PT_OCTAVE_RANGE);
  assert(note >= 0);
  assert(note <= SEMITONES_PER_OCTAVE);

  return period_table[octave][note];
}

static bool fput_pt_command(const int pt_effect_com,
                            const int pt_effect_val,
                            const int pt_sample_no,
                            const int pt_period,
                            FILE * const f)
{
  assert(pt_effect_com >= PT_COM_NORMAL);
  assert(pt_effect_com <= PT_COM_SET_SPEED);
  assert(pt_effect_val >= 0);
  assert(pt_effect_val <= UCHAR_MAX);
  assert(pt_sample_no >= 0);
  assert(pt_sample_no <= MAX_PT_SAMPLES);
  assert(pt_period >= 0);
  assert(pt_period <= 0xfff);
  assert(f != NULL);
  assert(!ferror(f));

  uint8_t bytes[BYTES_PER_PT_COMMAND];

  /* Write higher 4 bits of note period and sample number */
  bytes[0] = (pt_period >> 8 & 0xf) | (pt_sample_no >> 4 & 0xf);

  /* Write lower 8 bits of note period */
  bytes[1] = pt_period & UCHAR_MAX;

  /* Write effect command number and lower 4 bits of sample number */
  bytes[2] = (pt_effect_com & 0xf) | (pt_sample_no & 0xf) << 4;

  /* Write effect value */
  bytes[3] = pt_effect_val;

  return fwrite(bytes, sizeof(bytes), 1, f) == 1;
}

static bool fput_halfword(const unsigned short halfword, FILE * const f)
{
  assert(f != NULL);
  assert(!ferror(f));

  /* All half-word values in a ProTracker file are big-endian */
  uint8_t bytes[2];
  bytes[0] = (halfword >> 8) & UCHAR_MAX; /* most-significant byte first */
  bytes[1] = halfword & UCHAR_MAX;
  return fwrite(bytes, sizeof(bytes), 1, f) == 1;
}

static bool command(const SFChannelData * const com)
{
  assert(com != NULL);
  return com->note != 0 || com->oct_vol != 0 || com->voice_act != 0 ||
         com->num_repeats != 0;
}

static bool write_sample_table(const unsigned int flags,
                               const PTSampleArray * const pt_samples,
                               const SampleArray * const sf_samples,
                               FILE * const f)
{
  assert(!(flags & ~FLAGS_ALL));
  assert(pt_samples != NULL);
  assert(pt_samples->count >= 0);
  assert(pt_samples->count <= pt_samples->alloc);
  assert((pt_samples->alloc == 0) == (pt_samples->sample_info == NULL));
  assert(sf_samples != NULL);
  assert(sf_samples->count >= 0);
  assert(sf_samples->count <= sf_samples->alloc);
  assert((sf_samples->alloc == 0) == (sf_samples->sample_info == NULL));
  assert(f != NULL);
  assert(!ferror(f));

  for (int pt_sample_no = 0;
       pt_sample_no < pt_samples->count;
       pt_sample_no++) {
    Fortify_CheckAllMemory();

    const PTSampleInfo * const ptsi = pt_samples->sample_info + pt_sample_no;
    const SampleInfo * const sample = sf_samples->sample_info + ptsi->sample_num;

    /* Write sample name, padded with null bytes */
    char sample_name[22];
    memset(sample_name, '\0', sizeof(sample_name));
    sprintf(sample_name, "%s-R%d-O%d", sample->file_name, ptsi->num_repeats,
            ptsi->octaves_cheat);

    if ((flags & FLAGS_VERBOSE) != 0)
      printf("Writing ProTracker sample table entry %d ('%s')\n",
             pt_sample_no, sample_name);

    if (fwrite(sample_name, sizeof(sample_name), 1, f) != 1) {
      return false; /* failure */
    }

    /* Write len of sample data DIV 2. */
    if (!fput_halfword(ptsi->half_len, f))
      return false; /* failure */

    /* Convert the ProTracker tuning value to semitones (coarsen it). */
    signed long finetune = ptsi->pt_tuning / PT_TUNING_SEMITONE;

    /* Find the fractional remainder that was discarded by the integer
       division above, and use that as the 'finetune' value. */
    assert(finetune <= LONG_MAX / PT_TUNING_SEMITONE);
    assert(finetune >= LONG_MIN / PT_TUNING_SEMITONE);
    finetune = ptsi->pt_tuning - finetune * PT_TUNING_SEMITONE;

    /* Write signed 8 bit 'finetune' value for sample
       -8 means 1 semitone lower. 7 means 0.875 semitone higher. */
    assert(finetune >= SCHAR_MIN);
    assert(finetune <= SCHAR_MAX);
    if (fputc((int)finetune, f) == EOF)
      return false; /* failure */

    /* Write volume for sample */
    if (fputc(PT_MAX_VOLUME, f) == EOF)
      return false; /* failure */

    /* Write repeat offset DIV 2 */
    if (!fput_halfword(ptsi->half_repeat_offset, f))
      return false; /* failure */

    /* Write repeat len DIV 2 */
    if (!fput_halfword(ptsi->half_repeat_len, f))
      return false; /* failure */
  }

  /* The ProTracker file format allocates a fixed amount of space for the
     sample table, so we must pad it to the required size. */
  assert(pt_samples->count <= MAX_PT_SAMPLES);
  for (int n = MAX_PT_SAMPLES - pt_samples->count; n != 0; n--) {
    static const uint8_t blank[BYTES_PER_PT_SAMPLE];
    if (fwrite(blank, sizeof(blank), 1, f) != 1)
      return false; /* failure */
  }

  return true; /* success */
}

static bool write_sample(const unsigned int flags,
                         const PTSampleInfo * const ptsi,
                         const SampleInfo * const sample,
                         FILE * const f,
                         FILE * const sample_handle)
{
  assert(!(flags & ~FLAGS_ALL));
  assert(ptsi != NULL);
  assert(sample != NULL);
  assert(f != NULL);
  assert(!ferror(f));
  assert(sample_handle != NULL);
  assert(!ferror(sample_handle));

  /* Initialise the output counter to one less than the defined sample
     len (because termination check is inclusive of zero). */
  long int out_count = (long int)ptsi->half_len * 2 - 1;

  int num_repeats = ptsi->num_repeats;
  if (num_repeats == SF_MAX_REPEATS)
    num_repeats = 0; /* unlimited repeats will be handled automatically */

  for (int repeat = 0; repeat <= num_repeats; repeat++) {
    /* If we are looping the sample data then apply the repeat offset to
       prevent repeating the attack phase of the note. */
    if (repeat != 0) {
      if ((flags & FLAGS_VERBOSE) != 0)
        printf("Seeking repeat offset %ld in sample data file\n",
               (long int)sample->repeat_offset * 2);

      if (fseek(sample_handle,
                (long int)sample->repeat_offset * 2,
                SEEK_SET)) {
        fprintf(stderr,
                "Failed to loop sample data file at %d: %s\n",
                sample->repeat_offset * 2,
                strerror(errno));
        return false;
      }
    }

    /* Must copy exactly the defined number of bytes, regardless of whether
       or not we are manually looping the sample data. */
    if ((flags & FLAGS_VERBOSE) != 0)
      printf("About to copy %ld bytes from sample data file\n",
             out_count + 1);

    for (;out_count >= 0; out_count--) {
      /* Read a 16 bit little-endian value but discard the least
         significant 8 bits. */
      static uint8_t sample_bytes[BYTES_PER_SF_SAMPLE];
      if (fread(sample_bytes, sizeof(sample_bytes), 1, sample_handle) != 1)
      {
        if (!ferror(sample_handle))
          break; /* End of sample file (not an error) */

        fprintf(stderr,
                "Failed reading from sample data file: %s\n",
                strerror(errno));
        return false;
      }

      /* Create a lower pitched version of a sound by doubling or quadrupling
         each sample. */
      int dup = 1;
      for (int pow = ptsi->octaves_cheat; pow < 0; pow ++)
        dup *= 2;

      /* Copy most significant byte of the sample to the ProTracker file. */
      for (;dup > 0; dup --) {
        if (fputc(sample_bytes[1], f) == EOF) {
          fprintf(stderr,
                  "Failed writing to output file: %s\n",
                  strerror(errno));
          return false;
        }
      }

      if (ptsi->octaves_cheat > 0) {
        /* Resampling interval is 2 to the power of the number of octaves by
           which to transpose upwards. */
        long int skip = 1;
        for (int pow = ptsi->octaves_cheat; pow > 0; pow --)
          skip *= BYTES_PER_SF_SAMPLE;

        skip -= 1; /* the file pointer has already advanced by one sample */
        if (fseek(sample_handle, skip * BYTES_PER_SF_SAMPLE, SEEK_CUR)) {
          if (feof(sample_handle))
            break; /* End of sample (not an error) */

          fprintf(stderr,
                  "Failed seeking within sample data file: %s\n",
                  strerror(errno));
          return false;
        }
      }
    }
  }
  return true;
}

static bool integrate_samples(const unsigned int flags,
                              const PTSampleArray * const pt_samples,
                              const SampleArray * const sf_samples,
                              const char * const samples_dir,
                              FILE * const f)
{
  bool success = true;

  assert(!(flags & ~FLAGS_ALL));
  assert(pt_samples != NULL);
  assert(pt_samples->count >= 0);
  assert(pt_samples->count <= pt_samples->alloc);
  assert((pt_samples->alloc == 0) == (pt_samples->sample_info == NULL));
  assert(sf_samples != NULL);
  assert(sf_samples->count >= 0);
  assert(sf_samples->count <= sf_samples->alloc);
  assert((sf_samples->alloc == 0) == (sf_samples->sample_info == NULL));
  assert(samples_dir != NULL);
  assert(f != NULL);
  assert(!ferror(f));

  for (int pt_sample_no = 0;
       pt_sample_no < pt_samples->count && success;
       pt_sample_no++) {

    Fortify_CheckAllMemory();

    if ((flags & FLAGS_VERBOSE) != 0)
      printf("About to write data for ProTracker sample %d\n", pt_sample_no);

    const PTSampleInfo * const ptsi = pt_samples->sample_info + pt_sample_no;
    const SampleInfo * const sample = sf_samples->sample_info + ptsi->sample_num;

    /* Construct full path name of sample data file */
    StringBuffer sample_path;
    stringbuffer_init(&sample_path);

    if (!stringbuffer_append(&sample_path, samples_dir, SIZE_MAX) ||
        !stringbuffer_append_separated(&sample_path, PATH_SEPARATOR,
                                       sample->file_name)) {
      fprintf(stderr,"Failed to allocate memory for sample data file path\n");
      success = false;
    } else {
      /* Open the sample data file */
      if ((flags & FLAGS_VERBOSE) != 0)
        printf("Opening sample data file '%s'\n", stringbuffer_get_pointer(&sample_path));

      FILE * const sample_handle = fopen(stringbuffer_get_pointer(&sample_path), "rb");
      if (sample_handle == NULL) {
        fprintf(stderr,
                "Failed to open sample data file: %s\n",
                strerror(errno));
        success = false;
      } else {
        success = write_sample(flags, ptsi, sample, f, sample_handle);

        /* Close the sample data file */
        if ((flags & FLAGS_VERBOSE) != 0)
          puts("Closing sample data file");

        fclose(sample_handle);
      }
    }
    stringbuffer_destroy(&sample_path);
  }

  return success;
}

static signed int note_to_pt(const SFChannelData * const com,
                             int * const note_out,
                             const signed long semitone_tuning)
{
  /* Convert the SF3000 octave and note numbers into ProTracker format.
     This function may return an out-of-range octave number because the
     ProTracker format is more restrictive. */
  signed long note;
  signed int octave;

  assert(com != NULL);
  octave = (com->oct_vol & 0xf) - 1;
  note = com->note & 0xfl;
  note += semitone_tuning;
  while (note < 0) {
    octave --;
    note += SEMITONES_PER_OCTAVE;
  }
  while (note >= SEMITONES_PER_OCTAVE) {
    octave ++;
    note -= SEMITONES_PER_OCTAVE;
  }
  assert(note >= 0);
  assert(note < SEMITONES_PER_OCTAVE);
  if (note_out != NULL)
    *note_out = (int)note;

  return octave;
}

static bool make_pt_sample(PTSampleInfo * const ptsi,
                           const SampleInfo * const sample,
                           const int num_repeats,
                           const signed int octaves_cheat)
{
  unsigned long repeat_len, sample_len, repeat_offset;

  assert(ptsi != NULL);
  assert(sample != NULL);

  ptsi->num_repeats = num_repeats;
  ptsi->octaves_cheat = octaves_cheat;

  /* The resolution of the sample data will be reduced from 8 to 16 bits. */
  sample_len = sample->len / 2;
  DEBUGF("Sample len: %lu\n", sample_len);

  repeat_offset = sample->repeat_offset; /* in sample frames not bytes */
  DEBUGF("Repeat offset: %lu\n", repeat_offset);

  /* If we pre-tune the sample to lower or raise the pitch then that will
     affect the length and repeat offset. */
  for (int pow = ptsi->octaves_cheat; pow < 0; pow++) {
    assert(repeat_offset <= ULONG_MAX / 2);
    repeat_offset *= 2;

    assert(sample_len <= ULONG_MAX / 2);
    sample_len *= 2;
  }
  for (int pow = ptsi->octaves_cheat; pow > 0; pow--) {
    repeat_offset /= 2;
    sample_len /= 2;
  }

  /* ProTracker isn't capable of representing odd sample lengths or offsets
     within a sample (only multiples of 2). */
  repeat_offset /= 2;
  sample_len /= 2;

  DEBUGF("Revised sample len: %lu\n", sample_len);
  DEBUGF("Revised offset: %lu\n", repeat_offset);

  if (num_repeats == SF_MAX_REPEATS) {
    /* Calculate repeat length */
    repeat_len = sample_len - repeat_offset;
  } else {
    if (num_repeats != 0) {
      /* There is no facility in ProTracker to loop a sample a specific
         number of times, so we must repeat the sample data! */
      DEBUGF("Loop size: %ld\n", sample_len - repeat_offset);
      sample_len += (sample_len - repeat_offset) * num_repeats;
    }
    /* No repeats for this variant of the sample */
    repeat_offset = 0;
    repeat_len = 0;
  }

  /* Validate sample length */
  if (sample_len > USHRT_MAX) {
    fprintf(stderr, "Sample data file '%s' is too long with %d repeats "
                    "from offset %u (when pre-tuned by %d octaves)\n",
                    sample->file_name, num_repeats, sample->repeat_offset,
                    ptsi->octaves_cheat);
    return false; /* failure */
  }

  assert(repeat_len <= sample_len);
  assert(repeat_len <= USHRT_MAX);
  ptsi->half_repeat_len = (unsigned short)repeat_len;

  assert(sample_len <= USHRT_MAX);
  ptsi->half_len = (unsigned short)sample_len;

  assert(repeat_offset < sample_len);
  assert(repeat_offset <= USHRT_MAX);
  ptsi->half_repeat_offset = (unsigned short)repeat_offset;

  return true; /* success */
}

static signed int calc_octaves_cheat(const unsigned int flags,
                                     const SFChannelData * const com,
                                     const signed long pt_tuning,
                                     int * const octave_out,
                                     int * const note_out)
{
  signed int octave, octaves_cheat, min_octave, max_octave;

  assert(!(flags & ~FLAGS_ALL));
  assert(com != NULL);

  /* Convert the SF3000 octave and note numbers into their ProTracker
     equivalents. */
  octave = note_to_pt(com, note_out, pt_tuning / PT_TUNING_SEMITONE);

  /* ProTracker octaves 0 and 4 are non-standard and may not be available. */
  if ((flags & FLAGS_EXTRA_OCTAVES) == 0) {
    min_octave = 1;
    max_octave = PT_OCTAVE_RANGE - 2;
  } else {
    min_octave = 0;
    max_octave = PT_OCTAVE_RANGE - 1;
  }

  if (octave < min_octave) {
    DEBUGF("Invalid octave %d; must pre-tune sample down\n", octave);
    octaves_cheat = octave - min_octave;
    octave = min_octave;
  } else if (octave > max_octave) {
    DEBUGF("Invalid octave %d; must pre-tune sample up\n", octave);
    octaves_cheat = octave - max_octave;
    octave = max_octave;
  } else {
    octaves_cheat = 0;
  }
  if (octave_out != NULL)
    *octave_out = octave;

  return octaves_cheat;
}

static int find_song_len(const SFTrack * const music_data)
{
  assert(music_data != NULL);

  /* There is no record of the song length in a SF3000 music file, so
     iterate through the play order in search of the terminator. */
  int song_len;
  for (song_len = 0; song_len < MAX_SF_PATTERNS; song_len++) {
    /* Is this the end of the play list? */
    if (music_data->play_order[song_len] == 255)
      break; /* found the terminator */
  }

  return song_len;
}

static bool write_play_order(const unsigned int flags,
                             const SFTrack * const music_data,
                             const int song_len,
                             const int pt_song_len,
                             FILE * const f)
{
  assert(!(flags & ~FLAGS_ALL));
  assert(music_data != NULL);
  assert(song_len >= 0);
  assert(song_len <= MAX_SF_PATTERNS);
  assert(pt_song_len >= 1);
  assert(pt_song_len > song_len);
  assert(pt_song_len <= MAX_PT_SONG_LEN);
  assert(f != NULL);
  assert(!ferror(f));

  /* Write the song length */
  if ((flags & FLAGS_VERBOSE) != 0)
    printf("Writing ProTracker song length %d\n", pt_song_len);

  if (fputc(pt_song_len, f) == EOF)
    return false;

  /* Apparently this byte must be 127 so that old trackers search through all
     patterns when loading. */
  if (fputc(127, f) == EOF)
    return false;

  if ((flags & FLAGS_VERBOSE) != 0)
    printf("Writing ProTracker song positions: 0 (tempo)");

  /* An extra song position (pattern 0) will be required to set the tempo. */
  if (fputc(0, f) == EOF)
    return false;

  /* Write the song positions that dictate the play order for patterns. */
  int pos;
  for (pos = 0; pos < song_len; pos++) {
    /* Pattern numbers are offset by 1 because pattern 0 will set the tempo. */
    int pattern = music_data->play_order[pos] + 1;

    if ((flags & FLAGS_VERBOSE) != 0)
      printf(",%d", pattern);

    if (fputc(pattern, f) == EOF)
      return false;
  }

  /* An extra song position may be required to allow late notes to finish. */
  if ((flags & FLAGS_BLANK_PATTERN) != 0) {
    long int extra_pattern = music_data->last_pattern_no + 2;

    if ((flags & FLAGS_VERBOSE) != 0)
      printf(",%ld (blank)", extra_pattern);

    assert(extra_pattern <= UCHAR_MAX);
    if (fputc((int)extra_pattern, f) == EOF)
      return false;
    pos++;
  }

  if ((flags & FLAGS_VERBOSE) != 0)
    puts("");

  /* The ProTracker file format allocates a fixed amount of space for the
     song positions, so we must pad it to the required size. We have already
     written one more position (pattern 0) than 'pos' reflects. */
  for (;pos < MAX_PT_SONG_LEN - 1; pos++) {
    if (fputc(0, f) == EOF)
      return false;
  }

  return true;
}

static bool write_tempo_pattern(const unsigned int flags, const int speed, FILE * const f)
{
  /* ProTracker's representation of tempo is based upon 1/24th of the no. of
     ticks per minute of a 50Hz timer. Star Fighter 3000's music player is
     instead based on a 100 Hz clock and therefore the default ProTracker
     tempo of 125 is too slow. */
  const int tempo = (SECONDS_PER_MINUTE * SF_CLOCK_FREQ) /
                     PT_BPM_DIVISOR;

  assert(!(flags & ~FLAGS_ALL));
  assert(speed < PT_SPEED_THRESHOLD); /* higher values set tempo */
  assert(f != NULL);

  if ((flags & FLAGS_VERBOSE) != 0)
    printf("Writing ProTracker pattern to set tempo %d and speed %d\n",
           tempo, speed);

  /* Write a command to set the tempo. */
  assert(tempo >= PT_SPEED_THRESHOLD); /* lower values set speed */
  if (!fput_pt_command(PT_COM_SET_SPEED, tempo, 0, 0, f))
    return false; /* failure */

  /* Write a command to set the speed (i.e. multiplier for the base tempo to
     get the period between playing each position in the patterns). */
  if (!fput_pt_command(PT_COM_SET_SPEED, speed, 0, 0, f))
    return false; /* failure */

  /* Write a command to skip the rest of this pattern. */
  if (!fput_pt_command(PT_COM_PATTERN_BREAK, 0, 0, 0, f))
    return false; /* failure */

  /* The ProTracker file format allocates a fixed amount of space for each
     pattern, so we must pad pattern 0 to the required size. */
  for (int n = (MAX_PT_POSITIONS - 1) * NUM_PT_CHANNELS; n >= 0; n--) {
    if (!fput_pt_command(PT_COM_NORMAL, 0, 0, 0, f))
      return false; /* failure */
  }

  return true; /* success */
}

static int find_pt_sample(const PTSampleArray * const pt_samples,
                          const int num_repeats,
                          const int sample_num,
                          const signed int octaves_cheat)
{
  assert(pt_samples != NULL);
  assert(pt_samples->count >= 0);
  assert(pt_samples->count <= pt_samples->alloc);
  assert((pt_samples->alloc == 0) == (pt_samples->sample_info == NULL));

  for (int pt_sample_no = 0;
       pt_sample_no < pt_samples->count;
       pt_sample_no++)
  {
    const PTSampleInfo * const ptsi = pt_samples->sample_info + pt_sample_no;
    if (ptsi->num_repeats == num_repeats &&
        ptsi->sample_num == sample_num &&
        ptsi->octaves_cheat == octaves_cheat)
      return pt_sample_no + 1; /* ProTracker sample numbers are based at 1 */
  }

  return 0; /* no matching ProTracker sample */
}

static signed long sf_to_pt_tuning(const signed int sf_tuning)
{
  const int pt_octave = PT_TUNING_SEMITONE * SEMITONES_PER_OCTAVE;
  signed int round;

  assert(check_tuning(sf_tuning));
  round = (sf_tuning >= 0 ? SF_TUNING_OCTAVE / 2 : -(SF_TUNING_OCTAVE / 2));
  return ((long)sf_tuning * pt_octave + round) / SF_TUNING_OCTAVE;
}

static bool add_pt_sample(const unsigned int flags,
                          PTSampleArray * const pt_samples,
                          const SampleInfo * const sample,
                          const int num_repeats,
                          const int sample_num,
                          const signed int octaves_cheat,
                          const signed long pt_tuning)
{
  assert(pt_samples != NULL);
  assert(pt_samples->count >= 0);
  assert(pt_samples->count <= pt_samples->alloc);
  assert((pt_samples->alloc == 0) == (pt_samples->sample_info == NULL));
  assert(sample != NULL);
  assert(num_repeats >= 0);
  assert(sample_num >= 0);

  if (pt_samples->count >= MAX_PT_SAMPLES) {
    fprintf(stderr, "Song requires too many ProTracker samples "
                    "(limit is %d)\n", MAX_PT_SAMPLES);
    return false;
  }

  if (pt_samples->count >= pt_samples->alloc) {
    /* (Re-)allocate buffer for ProTracker samples array */
    if (pt_samples->alloc == 0)
      pt_samples->alloc = INIT_SIZE;
    else
      pt_samples->alloc *= 2;

    const size_t new_size = pt_samples->alloc * sizeof(PTSampleInfo);
    PTSampleInfo * const new_buf = realloc(pt_samples->sample_info, new_size);
    if (new_buf == NULL) {
      fprintf(stderr, "Failed to allocate %zu bytes for ProTracker "
                      "sample info\n", new_size);
      return false;
    }
    pt_samples->sample_info = new_buf;
  }

  /* Populate the next element of the ProTracker samples array */
  assert(pt_samples->sample_info != NULL);
  PTSampleInfo * const ptsi = pt_samples->sample_info + pt_samples->count;
  ptsi->sample_num = sample_num;
  ptsi->pt_tuning = pt_tuning;
  if (!make_pt_sample(ptsi,
                      sample,
                      num_repeats,
                      octaves_cheat))
    return false;

  if ((flags & FLAGS_VERBOSE) != 0) {
    printf("ProTracker sample %d will be:\n", pt_samples->count);

    printf("  %d repeats of sample %d ('%s'), "
           "pre-tuned up by %d octaves\n", ptsi->num_repeats,
           ptsi->sample_num, sample->file_name, ptsi->octaves_cheat);

    printf("  Tuning:%ld Length: %d Repeat offset:%d "
           "Repeat length:%d\n", ptsi->pt_tuning, ptsi->half_len * 2,
           ptsi->half_repeat_offset * 2, ptsi->half_repeat_len * 2);
  }

  pt_samples->count++;
  return true;
}

static bool make_pt_sample_list(const unsigned int flags,
                                const SFTrack * const music_data,
                                const SampleArray * const sf_samples,
                                PTSampleArray * const pt_samples)
{
  bool success = true;

  assert(!(flags & ~FLAGS_ALL));
  assert(music_data != NULL);
  assert(sf_samples != NULL);
  assert(sf_samples->count >= 0);
  assert(sf_samples->count <= sf_samples->alloc);
  assert((sf_samples->alloc == 0) == (sf_samples->sample_info == NULL));
  assert(pt_samples != NULL);

  pt_samples->count = 0;
  pt_samples->alloc = 0;
  pt_samples->sample_info = NULL;

  if ((flags & FLAGS_VERBOSE) != 0) {
    puts("SF3000 voice table:");
    for (int v = 0; v < NUM_SF_VOICES; v++)
      printf("  %d maps to sample %d\n", v, music_data->voice_table[v]);

    printf("SF3000 music comprises %" PRId32 " patterns\n",
           music_data->last_pattern_no + 1);
  }

  for (long int pattern_no = 0;
       (pattern_no <= music_data->last_pattern_no) && success;
       pattern_no++)
  {
    const SFPattern * const pattern = music_data->patterns + pattern_no;

    Fortify_CheckAllMemory();

    if ((flags & FLAGS_VERBOSE) != 0)
      printf("About to pre-scan pattern %ld\n", pattern_no);

    for (int division_no = 0;
         (division_no < NUM_SF_DIVISIONS) && success;
         division_no++)
    {
      const SFDivision * const division = &pattern->divisions[division_no];

      assert(NUM_PT_CHANNELS <= NUM_SF_CHANNELS);
      for (int c = 0; (c < NUM_SF_CHANNELS) && success; c++) {
        const SFChannelData * const com = &division->channels[c];

        if (!command(com))
          continue; /* No command here. */

        /* Only interested in note-playing actions, for now. */
        if (com->voice_act >> 4 >= SF_GLISSANDO_THRESHOLD)
          continue;

        /* Decode the voice number into a sample number */
        const int sample_num = music_data->voice_table[com->voice_act & 0xf];
        const SampleInfo * const sample = sf_samples->sample_info + sample_num;
        if ((sample_num >= sf_samples->count) ||
            (sample->type == SampleInfo_Type_Unused)) {
          printf("%d %d %d\n", sf_samples->count, sample_num, sample->type);
          fprintf(stderr, "Warning: Sample number %d is not defined!\n",
                          sample_num);
          continue;
        }

        if (sample->type == SampleInfo_Type_Effect) {
          if ((flags & FLAGS_VERBOSE) != 0) {
            printf("Sound effect on channel %d is %s (division %d of pattern %ld)\n",
                   c + 1,
                   (flags & FLAGS_ALLOW_SFX) != 0 ? "allowed" : "forbidden",
                   division_no,
                   pattern_no);
          }
          if ((flags & FLAGS_ALLOW_SFX) == 0)
            continue; /* Sound effects not allowed during music */
        } else {
          assert(sample->type == SampleInfo_Type_Music);
        }

        /* Decode the number of repeats */
        const int num_repeats = com->num_repeats >> 4;

        /* Calculate the equivalent tuning value in ProTracker units
           (-8 means 1 semitone lower. 7 means 0.875 semitone higher) */
        const signed long pt_tuning = sf_to_pt_tuning(sample->tuning);
        const signed int octaves_cheat = calc_octaves_cheat(flags, com, pt_tuning, NULL, NULL);

        /* If no usable variation of the sample required for this note
           already exists then invent one. */
        if (find_pt_sample(pt_samples,
                           num_repeats,
                           sample_num,
                           octaves_cheat) == 0) {
          success = add_pt_sample(flags, pt_samples, sample,
                                  num_repeats, sample_num, octaves_cheat,
                                  pt_tuning);
        }
      }
    }
  }

  if (success && (pt_samples->count == 0)) {
    fprintf(stderr, "Cannot create output file containing no samples!\n");
    success = false;
  }

  if (!success) {
    free(pt_samples->sample_info);
    pt_samples->sample_info = NULL;
  }

  return success;
}

static bool glissando_machine(ChannelState channels[NUM_PT_CHANNELS],
                              const int c,
                              FILE * const f)
{
  int effect_com, effect_val, sample_no, period;

  assert(channels != NULL);
  assert(c < NUM_SF_CHANNELS);
  assert(f != NULL);
  assert(!ferror(f));

  switch (channels[c].glissando_state) {
    case GlissandoState_None:
      /* No glissando on this channel yet */
      effect_com = PT_COM_NORMAL,
      effect_val = 0;
      sample_no = 0;
      period = 0;
      break;

    case GlissandoState_Start:
      /* Tell the player the target pitch and sample number only at
         the start of the glissando. */
      DEBUGF("Starting glissando of sample %d to pitch %d on "
                "channel %d\n", channels[c].pt_sample_no,
                channels[c].target_pitch, c);
      channels[c].glissando_state = GlissandoState_Continue;
      effect_com = PT_COM_TONE_PORTAMENTO,
      effect_val = PT_GLISSANDO_SPEED;
      sample_no = channels[c].pt_sample_no;
      period = channels[c].target_pitch;
      break;

    case GlissandoState_Continue:
      DEBUGF("Continuing glissando on channel %d\n", c);
      effect_com = PT_COM_TONE_PORTAMENTO,
      effect_val = PT_GLISSANDO_SPEED;
      sample_no = 0; /* Don't like but Martin says it's correct */
      period = 0; /* Don't like but Martin says it's correct */
      break;

    default:
      assert("Bad glissando state" == NULL);
      return false; /* failure */
  }

  return fput_pt_command(effect_com, effect_val, sample_no, period, f);
}

static void warn_octave(const signed int    octave,
                        const int           channel,
                        const int           division_no,
                        const long int      pattern_no)
{
  if (octave < 1 || octave > PT_OCTAVE_RANGE - 2) {
    printf("Utilising non-standard octave %d on channel %d (division %d of "
           "pattern %ld)\n",
           octave,
           channel,
           division_no,
           pattern_no);
  }
}

static bool transcode_patterns(const unsigned int  flags,
                               const SFTrack * const music_data,
                               const PTSampleArray *pt_samples,
                               const SampleArray *sf_samples,
                               const int last_play,
                               FILE * const f)
{
  long int last_pattern_no;
  ChannelState channels[NUM_PT_CHANNELS], final_channels[NUM_PT_CHANNELS];

  assert(music_data != NULL);
  assert(pt_samples != NULL);
  assert(pt_samples->count >= 0);
  assert(pt_samples->count <= pt_samples->alloc);
  assert((pt_samples->alloc == 0) == (pt_samples->sample_info == NULL));
  assert(sf_samples != NULL);
  assert(sf_samples->count >= 0);
  assert(sf_samples->count <= sf_samples->alloc);
  assert((sf_samples->alloc == 0) == (sf_samples->sample_info == NULL));
  assert(f != NULL);
  assert(!ferror(f));

  last_pattern_no = music_data->last_pattern_no;

  /* An extra pattern may be required to allow late notes to finish. */
  if ((flags & FLAGS_BLANK_PATTERN) != 0)
    last_pattern_no ++;

  for (long int pattern_no = 0; pattern_no <= last_pattern_no; pattern_no++)
  {
    const SFPattern *pattern;

    Fortify_CheckAllMemory();

    if ((flags & FLAGS_BLANK_PATTERN) != 0 && pattern_no == last_pattern_no) {
      if ((flags & FLAGS_VERBOSE) != 0)
        puts("About to write a blank ProTracker pattern");

      /* We are appending a blank pattern so restore the state of the channels
         at the end of the pattern played immediately beforehand, to allow
         continuation of any glissando effects. */
      pattern = NULL;
      memcpy(&channels, &final_channels, sizeof(channels));
    } else {
      /* Clear the state of every channel at the start of each new pattern. This
         isn't strictly accurate, but it's the best that we can practically do
         given that patterns may be played in any order. */
      if ((flags & FLAGS_VERBOSE) != 0)
        printf("About to transcode pattern %ld\n", pattern_no);

      for (int c = 0; c < NUM_PT_CHANNELS; c++) {
        channels[c].glissando_state = GlissandoState_None;
        channels[c].sample_num = UCHAR_MAX;
        /* Fixed implicit truncation of UINT_MAX to unsigned char, 11/04/2010 */
      }
      pattern = music_data->patterns + pattern_no;
    }

    for (int division_no = 0; division_no < NUM_SF_DIVISIONS; division_no++)
    {
      const SFDivision *division;
      static const SFDivision blank_division = {{{0,0,0,0},
                                                 {0,0,0,0},
                                                 {0,0,0,0},
                                                 {0,0,0,0}}};

      if (pattern == NULL)
        division = &blank_division;
      else
        division = &pattern->divisions[division_no];

      /* First examine the command for each channel to discover any glissando
         effects that should be applied to all channels. */
      assert(NUM_PT_CHANNELS <= NUM_SF_CHANNELS);
      for (int c = 0; c < NUM_PT_CHANNELS; c++) {
        const SFChannelData * const com = &division->channels[c];
        int sample_num, note;
        signed long pt_tuning;
        signed int octave;

        /* Is this a glissando effect? */
        if (com->voice_act >> 4 < SF_GLISSANDO_THRESHOLD)
          continue; /* no */

        sample_num = music_data->voice_table[com->voice_act & 0xf];
        if ((sample_num >= sf_samples->count) ||
            ((sf_samples->sample_info + sample_num)->type == SampleInfo_Type_Unused))
          continue;

        /* Convert the SF3000 octave and note numbers into ProTracker
           equivalents. */
        pt_tuning = sf_to_pt_tuning((sf_samples->sample_info + sample_num)->tuning);
        octave = note_to_pt(com, &note, pt_tuning / PT_TUNING_SEMITONE);

        /* A quirk is that a glissando affects all instances of the specified
           sample - regardless of which channel it is playing on. */
        for (int c2 = 0; c2 < NUM_SF_CHANNELS; c2++) {
          const PTSampleInfo *ptsi;
          signed int min_octave, max_octave, chan_octave;

          if (channels[c2].sample_num != sample_num)
            continue;

          if (c2 != c) {
            if ((flags & FLAGS_VERBOSE) != 0)
              printf("Glissando on channel %d->%d is %s (division %d of "
                     "pattern %ld)\n", c, c2,
                     (flags & FLAGS_GLISSANDO_SINGLE) == 0 ?
                     "allowed" : "forbidden", division_no, pattern_no);

            if ((flags & FLAGS_GLISSANDO_SINGLE) != 0)
              continue;
          }

          /* Get a pointer to the ProTracker sample information */
          assert(channels[c2].pt_sample_no > 0);
          assert(channels[c2].pt_sample_no <= pt_samples->count);
          ptsi = pt_samples->sample_info + channels[c2].pt_sample_no - 1;

          /* Make the target pitch specific to the variation of the sample
             playing on this channel (which may have been pre-tuned to a
             different octave). */
          if (ptsi->octaves_cheat != 0) {
            DEBUGF("Glissando of pre-tuned sample (by %d octaves)\n",
                      ptsi->octaves_cheat);
          }
          chan_octave = octave - ptsi->octaves_cheat;
          /* e.g. Use octave 1 to obtain octave 0 with a sample pre-tuned 'up'
                  by -1 octave. */

          /* ProTracker octaves 0 and 4 are non-standard and may not be
             available. */
          if ((flags & FLAGS_EXTRA_OCTAVES) == 0) {
            min_octave = 1;
            max_octave = PT_OCTAVE_RANGE - 2;
          } else {
            min_octave = 0;
            max_octave = PT_OCTAVE_RANGE - 1;
          }
          if (chan_octave < min_octave || chan_octave > max_octave) {
            if (chan_octave < min_octave)
              chan_octave = min_octave;
            else if (chan_octave > max_octave)
              chan_octave = max_octave;

            fprintf(stderr, "Warning: target for glissando out of range "
                    "on channel %d (division %d of pattern %ld)\n",
                    c2, division_no, pattern_no);
          }

          if ((flags & FLAGS_VERBOSE) != 0)
            warn_octave(chan_octave, c2, division_no, pattern_no);

          if (channels[c2].glissando_state != GlissandoState_None) {
            DEBUGF("New glissando cancels existing glissando of "
                      "sample %d to pitch %d on channel %d\n",
                      channels[c2].pt_sample_no, channels[c2].target_pitch,
                      c2);
          }
          /* Schedule an immediate Tone Portamento command */
          channels[c2].target_pitch = get_pt_period(chan_octave, note);
          channels[c2].glissando_state = GlissandoState_Start;

          DEBUGF("New glissando of sample %d to pitch %d on "
                 "channel %d (division %d of pattern %ld)\n",
                 channels[c2].pt_sample_no, channels[c2].target_pitch,
                 c2, division_no, pattern_no);
        }
      }

      assert(NUM_PT_CHANNELS <= NUM_SF_CHANNELS);
      for (int c = 0; c < NUM_PT_CHANNELS; c++) {
        const SFChannelData * const com = &division->channels[c];
        bool blank = false;
        const int sample_num = music_data->voice_table[com->voice_act & 0xf];
        const SampleInfo * const sample = sf_samples->sample_info + sample_num;

        if (!command(com)) {
          blank = true;
        } else if (com->voice_act >> 4 >= SF_GLISSANDO_THRESHOLD) {
          /* dealt with glissando starts on first pass */
          blank = true;
        } else if (sample_num >= sf_samples->count) {
          blank = true; /* undefined sample */
        } else {
          switch (sample->type) {
            case SampleInfo_Type_Unused:
              blank = true; /* undefined sample */

            case SampleInfo_Type_Effect:
              if ((flags & FLAGS_ALLOW_SFX) == 0)
                blank = true; /* Sound effects not allowed during music */
              break;

            default:
              assert(sample->type == SampleInfo_Type_Music);
              break;
          }
        }
        if (blank) {
          /* We may need to output a Tone Portamento command to continue a
             glissando. */
          if (!glissando_machine(channels, c, f))
            return false;
        } else {
          /* Convert the SF3000 octave and note numbers into ProTracker
             equivalents. */
          int octave, note;
          const signed int octaves_cheat = calc_octaves_cheat(flags,
                                             com,
                                             sf_to_pt_tuning(sample->tuning),
                                             &octave,
                                             &note);

          if ((flags & FLAGS_VERBOSE) != 0)
            warn_octave(octave, c, division_no, pattern_no);

          /* Search for the variation of the sample with the appropriate number
             of repeats. */
          const int pt_sample_no = find_pt_sample(pt_samples, com->num_repeats >> 4,
                                                  sample_num, octaves_cheat);
          assert(pt_sample_no != 0);

          if (!fput_pt_command(PT_COM_SET_VOLUME,
                               (com->oct_vol >> 4) *
                                 PT_MAX_VOLUME / SF_MAX_VOLUME,
                               pt_sample_no,
                               get_pt_period(octave, note),
                               f))
            return false; /* failure */

          channels[c].sample_num = sample_num;
          channels[c].pt_sample_no = pt_sample_no;

          if (channels[c].glissando_state != GlissandoState_None) {
            DEBUGF("New note cancels glissando of sample %d to pitch %d "
                      "on channel %d\n", channels[c].pt_sample_no,
                      channels[c].target_pitch, c);
          }

          channels[c].glissando_state = GlissandoState_None;
        }
      }
    }

    /* If we just transcoded the pattern to be played last then copy the state
       of the channels to allow continuation of any glissando effects on the
       additional 'blank' pattern (if one is to be appended). */
    if ((flags & FLAGS_BLANK_PATTERN) != 0 && pattern_no == last_play) {
      DEBUGF("Retaining channels state at end of pattern %ld\n",
                pattern_no);

      memcpy(&final_channels, &channels, sizeof(final_channels));
    }
  }
  return true; /* success */
}

bool check_tuning(signed int sf_tuning)
{
  const int pt_octave = PT_TUNING_SEMITONE * SEMITONES_PER_OCTAVE;

  /* Check that a long integer will accommodate the conversion from SF3000
     tuning units to ProTracker tuning units. */
  return ((int)abs(sf_tuning) <= (LONG_MAX - SF_TUNING_OCTAVE / 2) / pt_octave);
}

static bool read_track(const unsigned int flags, Reader * const r, SFTrack * const music_data)
{
  assert(!(flags & ~FLAGS_ALL));
  assert(r != NULL);
  assert(!reader_ferror(r));
  assert(music_data != NULL);

  /* First byte of Star Fighter 3000 music data gives the tempo as an interval
     between divisions (in centiseconds). */

  const int s = reader_fgetc(r);
  if (s == EOF) {
    fprintf(stderr, "Failed to read tempo\n");
    return false;
  }

  if (s >= PT_SPEED_THRESHOLD) {
    fprintf(stderr, "Tempo %d is too slow in input file (limit is %d)\n",
                    s, PT_SPEED_THRESHOLD - 1);
    return false;
  }

  if ((flags & FLAGS_VERBOSE) != 0)
    printf("SF3000 music tempo is %d cs\n", s);

  music_data->speed = s;

  if (reader_fseek(r, 16, SEEK_SET)) {
    fprintf(stderr, "Failed to seek voice table\n");
    return false;
  }

  if (reader_fread(music_data->voice_table, sizeof(music_data->voice_table), 1, r) != 1) {
    fprintf(stderr, "Failed to read voice table\n");
    return false;
  }

  if (!reader_fread_int32(&music_data->last_pattern_no, r)) {
    fprintf(stderr, "Failed to read no. of patterns\n");
    return false;
  }

  if (reader_fseek(r, 4, SEEK_CUR)) {
    fprintf(stderr, "Failed to seek play order\n");
    return false;
  }

  if (reader_fread(music_data->play_order, sizeof(music_data->play_order), 1, r) != 1) {
    fprintf(stderr, "Failed to read play order\n");
    return false;
  }

  assert(music_data->last_pattern_no >= 0);
  size_t const bytes = ((size_t)music_data->last_pattern_no + 1) * sizeof(SFPattern);
  music_data->patterns = malloc(bytes);
  if (music_data->patterns == NULL) {
    fprintf(stderr, "Failed to allocate %zu bytes for SF3000 patterns data\n", bytes);
    return false;
  }

  bool success = true;
  for (long int pattern_no = 0;
       pattern_no <= music_data->last_pattern_no && success;
       pattern_no++)
  {
    SFPattern * const pattern = music_data->patterns + pattern_no;

    Fortify_CheckAllMemory();

    if ((flags & FLAGS_VERBOSE) != 0)
      printf("Reading pattern %ld\n", pattern_no);

    for (int division_no = 0;
         division_no < NUM_SF_DIVISIONS && success;
         division_no++) {
      SFDivision * const division = &pattern->divisions[division_no];
      assert(NUM_PT_CHANNELS <= NUM_SF_CHANNELS);
      for (int c = 0; c < NUM_SF_CHANNELS && success; c++) {
        SFChannelData * const com = &division->channels[c];
        uint8_t raw[4];
        if (reader_fread(raw, sizeof(raw), 1, r) != 1) {
          fprintf(stderr,
                  "Failed to read channel %d (division %d of pattern %ld)\n",
                  c, division_no, pattern_no);
          success = false;
        } else {
          com->note = raw[0];
          com->oct_vol = raw[1];
          com->voice_act = raw[2];
          com->num_repeats = raw[3];
        }
      }
    }
  }

  if (!success) {
    free(music_data->patterns);
    music_data->patterns = NULL;
  }

  return success;
}

static bool write_track(const unsigned int flags,
                        const char * const song_name,
                        const SFTrack * const music_data,
                        const int song_len,
                        const int pt_song_len,
                        const SampleArray * const sf_samples,
                        const PTSampleArray * const pt_samples,
                        FILE * const f)
{
  assert(!(flags & ~FLAGS_ALL));
  assert(song_name != NULL);
  assert(music_data != NULL);
  assert(song_len >= 0);
  assert(song_len <= MAX_SF_PATTERNS);
  assert(pt_song_len >= 1);
  assert(pt_song_len > song_len);
  assert(pt_song_len <= MAX_PT_SONG_LEN);
  assert(sf_samples != NULL);
  assert(sf_samples->count >= 0);
  assert(sf_samples->count <= sf_samples->alloc);
  assert((sf_samples->alloc == 0) == (sf_samples->sample_info == NULL));
  assert(pt_samples != NULL);
  assert(pt_samples->count >= 0);
  assert(pt_samples->count <= pt_samples->alloc);
  assert((pt_samples->alloc == 0) == (pt_samples->sample_info == NULL));
  assert(f != NULL);
  assert(!ferror(f));

  char name[20];
  memset(name, '\0', sizeof(name));
  strncpy(name, song_name, sizeof(name)-1);
  if ((flags & FLAGS_VERBOSE) != 0)
    printf("Writing ProTracker song name '%s'\n", name);

  if (fwrite(name, sizeof(name), 1, f) != 1) {
    return false;
  }

  /* Write sample info */
  if (!write_sample_table(flags, pt_samples, sf_samples, f)) {
    return false;
  }

  /* Write the order in which to play patterns and get the number of the
     pattern to be played last. */
  if (!write_play_order(flags, music_data, song_len, pt_song_len, f)) {
    return false;
  }

  /* Write Mahoney & Kaktus identifier to indicate that the ProTracker file
     may include 31 rather than 15 samples. */
  if (fputs("M.K.", f) == EOF) {
    return false;
  }

  /* Write data for pattern 0, which will set the tempo for the song... */
  if (!write_tempo_pattern(flags, music_data->speed, f)) {
    return false;
  }

  /* Second pass is to transcode the command data from SF3000 to ProTracker
     format. */
  if (!transcode_patterns(flags,
                          music_data,
                          pt_samples,
                          sf_samples,
                          music_data->play_order[song_len - 1],
                          f)) {
    return false;
  }

  return true; /* success */
}

bool create_protracker(unsigned int flags,
                       const char * const song_name,
                       Reader * const in,
                       const char * const samples_dir,
                       const SampleArray * const sf_samples,
                       FILE * const out)
{
  assert(!(flags & ~FLAGS_ALL));
  assert(in != NULL);
  assert(!reader_ferror(in));

  SFTrack music_data;
  bool success = read_track(flags, in, &music_data);

  if (success) {
    /* Find the number of song positions in the SF3000 play order. */
    int pt_song_len;
    const int song_len = find_song_len(&music_data);
    if (song_len >= MAX_SF_PATTERNS) {
      fprintf(stderr, "Unterminated pattern play order in input file\n");
      success = false;
    } else {
      if ((flags & FLAGS_VERBOSE) != 0)
        printf("SF3000 pattern play order has length %d\n", song_len);

      /* One extra song position will be required to set the tempo and optionally
         another to allow late notes to decay. */
      pt_song_len = song_len + 1;
      if ((flags & FLAGS_BLANK_PATTERN) != 0)
        pt_song_len ++;

      /* Compare the hardwired limits first to avoid checking the actual song
         len unless unavoidable. Here, song_len may equal MAX_SF_PATTERNS. */
      if (MAX_SF_PATTERNS > MAX_PT_SONG_LEN) {
        if (pt_song_len > MAX_PT_SONG_LEN) {
          fprintf(stderr, "Too many patterns to be played in input file\n");
          success = false;
        }
      }
    }

    if (success) {
      /* First pass is to determine which samples (and variants thereof) to
         include in the ProTracker file. */
      PTSampleArray pt_samples = {0, 0, NULL};
      success = make_pt_sample_list(flags, &music_data, sf_samples, &pt_samples);
      if (success) {
        success = write_track(flags, song_name, &music_data, song_len,
                              pt_song_len, sf_samples, &pt_samples, out);
        if (!success) {
          fprintf(stderr, "Failed writing to output file: %s\n", strerror(errno));
        } else {
          /* Store the sound samples right after the pattern data. */
          success = integrate_samples(flags, &pt_samples, sf_samples, samples_dir, out);
        }
        free(pt_samples.sample_info);
      }
    }
    free(music_data.patterns);
  }

  return success;
}
