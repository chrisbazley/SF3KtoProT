/*
 *  SF3KtoProT - Converts Star Fighter 3000 music to Amiga ProTracker format
 *  Main program and command line arguments parser
 *  Copyright (C) 2009-2011 Christopher Bazley
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

/* StreamLib headers */
#include "Reader.h"
#include "ReaderGKey.h"
#include "ReaderRaw.h"

/* CBUtilLib headers */
#include "ArgUtils.h"
#include "StrExtra.h"
#include "StringBuff.h"

/* Local header files */
#include "misc.h"
#include "samp.h"
#include "protracker.h"
#include "main.h"
#include "filetype.h"
#include "version.h"

enum {
  HistoryLog2 = 9 /* Base 2 logarithm of the history size used by
                     the compression algorithm */
};

static bool process_file(_Optional const char * const input_file,
                         _Optional const char * const output_file,
                         _Optional const char *song_name,
                         const char * const samples_dir,
                         const SampleArray * const sf_samples,
                         const unsigned int flags, const bool raw)
{
  assert(samples_dir != NULL);
  assert(sf_samples != NULL);
  assert(sf_samples->count >= 0);
  assert(sf_samples->count <= sf_samples->alloc);
  assert((sf_samples->alloc == 0) == (sf_samples->sample_info == NULL));
  assert(!(flags & ~FLAGS_ALL));

  _Optional FILE *out = NULL, *in = NULL;
  bool success = true;

  if (input_file != NULL) {
    if (song_name == NULL) {
      /* Use the leaf part of the input file path as the song name */
      song_name = strtail(&*input_file, PATH_SEPARATOR, 1);
    }

    /* An explicit input file name was specified, so open it */
    if (flags & FLAGS_VERBOSE)
      printf("Opening input file '%s'\n", input_file);

    in = fopen(&*input_file, "rb");
    if (in == NULL) {
      fprintf(stderr,
              "Failed to open input file: %s\n",
              strerror(errno));
      success = false;
    }
  } else {
    if (song_name == NULL) {
      song_name = "Star Fighter 3000";
    }

    /* Default input is from standard input stream */
    fprintf(stderr, "Reading from stdin...\n");
    in = stdin;
  }

  if (success) {
    if (output_file != NULL) {
      if (flags & FLAGS_VERBOSE)
        printf("Opening output file '%s'\n", output_file);

      out = fopen(&*output_file, "wb");
      if (out == NULL) {
        fprintf(stderr,
                "Failed to open output file: %s\n",
                strerror(errno));
        success = false;
      }
    } else {
      /* Default output is to standard output stream */
      out = stdout;
    }
  }

  if (success && in) {
    Reader r;
    if (raw) {
      reader_raw_init(&r, &*in);
    } else {
      success = reader_gkey_init(&r, HistoryLog2, &*in);
    }

    if (success && song_name && out) {
      /* Create the ProTracker output file */
      success = create_protracker(flags,
                                  &*song_name,
                                  &r,
                                  samples_dir,
                                  sf_samples,
                                  &*out);
      reader_destroy(&r);
    }
  }

  if (in != NULL && in != stdin) {
    if (flags & FLAGS_VERBOSE)
      puts("Closing input file");
    fclose(&*in);
  }

  if (out != NULL && out != stdout) {
    if (flags & FLAGS_VERBOSE)
      puts("Closing output file");
    if (fclose(&*out)) {
      fprintf(stderr, "Failed to close output file: %s\n", strerror(errno));
      success = false;
    }
  }

  if (output_file != NULL) {
    /* Use OS-specific functionality to update the output file's metadata */
    if (success && !set_file_type(&*output_file)) {
      fprintf(stderr, "Failed to set type of output file '%s'\n", &*output_file);
      success = false;
    }

    /* Delete malformed output unless debugging is enabled */
    if (!success && !(flags & FLAGS_VERBOSE)) {
      remove(&*output_file);
    }
  }

  return success;
}

static int syntax_msg(FILE * const f, const char * const path)
{
  assert(f != NULL);
  assert(path != NULL);

  const char * const leaf = strtail(path, PATH_SEPARATOR, 1);
  fprintf(f,
          "usage: %s [switches] <samples-dir> [<input-file> [<output-file>]]\n"
          "or     %s -batch [switches] <samples-dir> <file1> [<file2> .. <fileN>]\n"
          "If no input file is specified, it reads from stdin.\n"
          "If no output file is specified, it writes to stdout.\n"
          "In batch processing mode, output file names are generated by appending\n"
          "extension 'mod' to the input file names.\n",
          leaf, leaf);

  fputs("Switches (names may be abbreviated):\n"
        "  -allowsfx           Allow notes to be played using sound effect samples\n"
        "  -batch              Process a batch of files (see above)\n"
        "  -blankend           Append a blank pattern to the end of the song\n"
        "  -channelglissando   Restrict glissando effects to the same channel\n"
        "  -extraoctaves       Utilise non-standard ProTracker octaves 0 and 4\n"
        "  -help               Display this text\n"
        "  -indexfile <file>   Index file to use instead of looking in <samples-dir>\n"
        "  -name <song-name>   Name to give the song (default is the input file name)\n"
        "  -outfile <file>     Specify a name for the output file\n"
        "  -raw                Input is uncompressed raw data\n"
        "  -verbose or -debug  Emit debug output (and keep bad output)\n", f);

  return EXIT_FAILURE;
}

#ifdef FORTIFY
int real_main(int argc, const char *argv[]);

int main(int argc, const char *argv[])
{
  unsigned long limit;
  int rtn = EXIT_FAILURE;
  for (limit = 0; rtn != EXIT_SUCCESS; ++limit)
  {
    rewind(stdin);
    clearerr(stdout);
    printf("------ Allocation limit %ld ------\n", limit);
    Fortify_SetNumAllocationsLimit(limit);
    Fortify_EnterScope();
    rtn = real_main(argc, argv);
    Fortify_LeaveScope();
  }
  return rtn;
}

int real_main(int argc, const char *argv[])
#else
int main(int argc, const char *argv[])
#endif
{
  unsigned int flags = 0;
  _Optional const char *output_file = NULL, *input_file = NULL, *index_file = NULL;
  _Optional const char *song_name = NULL;
  bool batch = false, raw = false;

  assert(argc > 0);
  assert(argv != NULL);

  DEBUG_SET_OUTPUT(DebugOutput_Reporter, "");

  /* Parse any options specified on the command line */
  int n;
  for (n = 1; n < argc && argv[n][0] == '-'; n++) {
    const char *opt = argv[n] + 1;

    if (is_switch(opt, "batch", 2)) {
      /* Enable batch processing mode */
      batch = true;
    } else if (is_switch(opt, "allowsfx", 1)) {
      /* Allow sound effects during music */
      flags |= FLAGS_ALLOW_SFX;
    } else if (is_switch(opt, "blankend", 2)) {
      /* Generate an extra blank pattern to prevent late notes being cut off */
      flags |= FLAGS_BLANK_PATTERN;
    } else if (is_switch(opt, "extraoctaves", 1)) {
      /* Utilise non-standard ProTracker octaves 0 and 4 in preference to
         pre-tuning samples. */
      flags |= FLAGS_EXTRA_OCTAVES;
    } else if (is_switch(opt, "help", 1)) {
      /* Output version number and usage information */
      (void)syntax_msg(stdout, argv[0]);
      return EXIT_SUCCESS;
    } else if (is_switch(opt, "name", 1)) {
      /* ProTracker song name was specified */
      if (++n >= argc || argv[n][0] == '-') {
        fprintf(stderr, "Missing song name\n");
        return syntax_msg(stderr, argv[0]);
      }
      song_name = argv[n];
    } else if (is_switch(opt, "outfile", 1)) {
      /* Samples index file path was specified */
      if (++n >= argc || argv[n][0] == '-') {
        fprintf(stderr, "Missing output file name\n");
        return syntax_msg(stderr, argv[0]);
      }
      output_file = argv[n];
    } else if (is_switch(opt, "indexfile", 1)) {
      /* Samples index file path was specified */
      if (++n >= argc || argv[n][0] == '-') {
        fprintf(stderr, "Missing samples index file name\n");
        return syntax_msg(stderr, argv[0]);
      }
      index_file = argv[n];
    } else if (is_switch(opt, "raw", 1)) {
      /* Enable raw input */
      raw = true;
    } else if (is_switch(opt, "verbose", 1) || is_switch(opt, "debug", 1)) {
      /* Enable debugging output */
      flags |= FLAGS_VERBOSE;
    } else if (is_switch(opt, "channelglissando", 1)) {
      /* Restrict effect of glissando command to a single channel */
      flags |= FLAGS_GLISSANDO_SINGLE;
    } else {
      fprintf(stderr, "Unrecognised option '%s'\n", opt);
      return syntax_msg(stderr, argv[0]);
    }
  }

  /* The samples directory path must follow any switches */
  if (argc < n + 1) {
    fprintf(stderr, "Must specify a directory containing sound sample files\n");
    return syntax_msg(stderr, argv[0]);
  }
  const char *const samples_dir = argv[n++];

  if (batch) {
    if (output_file != NULL) {
      fputs("Cannot specify an output file in batch processing mode\n", stderr);
      return syntax_msg(stderr, argv[0]);
    }
    if (n >= argc) {
      fputs("Must specify file(s) in batch processing mode\n", stderr);
      return syntax_msg(stderr, argv[0]);
    }
  } else {
    /* If an input file was specified, it should follow the samples directory */
    if (n < argc) {
      input_file = argv[n++];
    }

    /* An output file name may follow the input file name, but only if not
       already specified */
    if (n < argc) {
      if (output_file != NULL) {
        fputs("Cannot specify more than one output file\n", stderr);
        return syntax_msg(stderr, argv[0]);
      }
      output_file = argv[n++];
    }

    /* Ensure that MOD output isn't mixed up with other text on stdout */
    if ((output_file == NULL) && (flags & FLAGS_VERBOSE)) {
      fputs("Must specify an output file in verbose mode\n", stderr);
      return syntax_msg(stderr, argv[0]);
    }

    if (n < argc) {
      fputs("Too many arguments (did you intend -batch?)\n", stderr);
      return syntax_msg(stderr, argv[0]);
    }
  }

  if (flags & FLAGS_VERBOSE) {
    printf("Star Fighter 3000 to ProTracker convertor, "VERSION_STRING"\n"
           "Copyright (C) 2009, Christopher Bazley\n");
  }

  int rtn = EXIT_SUCCESS;

  /* If no samples index filename was specified then invent one */
  StringBuffer default_index;
  stringbuffer_init(&default_index);

  if (index_file == NULL) {
    /* Append hard-wired leaf name to generate full path of index file */
    if (!stringbuffer_append(&default_index, samples_dir, SIZE_MAX) ||
        !stringbuffer_append_separated(&default_index, PATH_SEPARATOR, "index")) {
      fprintf(stderr,"Failed to allocate memory for samples index file path\n");
      rtn = EXIT_FAILURE;
    }
    index_file = stringbuffer_get_pointer(&default_index);
  }

  SampleArray sf_samples = {0, 0, NULL};

  if (rtn == EXIT_SUCCESS) {
    /* Load the sound samples index file */
    if (!load_sample_index((flags & FLAGS_VERBOSE) != 0, &*index_file, samples_dir,
                           &sf_samples)) {
      rtn = EXIT_FAILURE;
    }
  }

  stringbuffer_destroy(&default_index);

  if (batch) {
    /* In batch processing mode, the remaining arguments are treated as a
       list of file names (output to default file names) */
    for (; n < argc && rtn == EXIT_SUCCESS; n++) {
      /* Invent an output file name */
      assert(argv[n] != NULL);

      StringBuffer default_output;
      stringbuffer_init(&default_output);

      if (!stringbuffer_append(&default_output, argv[n], SIZE_MAX) ||
          !stringbuffer_append_separated(&default_output, EXT_SEPARATOR, "mod")) {
        fprintf(stderr, "Failed to allocate memory for output file path\n");
        rtn = EXIT_FAILURE;
      } else {
        if (!process_file(argv[n], stringbuffer_get_pointer(&default_output),
                          song_name, samples_dir, &sf_samples, flags, raw)) {
          rtn = EXIT_FAILURE;
        }
      }

      stringbuffer_destroy(&default_output);
    }
  } else if (rtn == EXIT_SUCCESS) {
    if (!process_file(input_file, output_file, song_name, samples_dir,
                      &sf_samples, flags, raw)) {
      rtn = EXIT_FAILURE;
    }
  }

  free(sf_samples.sample_info);

  if (flags & FLAGS_VERBOSE) {
    puts(rtn == EXIT_SUCCESS ?
         "Conversion completed successfully" : "Conversion failed");
  }
  return rtn;
}
