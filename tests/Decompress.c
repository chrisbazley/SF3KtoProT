/*
 * SF3KtoProT test helper: decompress a Gordon Key encoded music file
 * Copyright (C) 2026 Christopher Bazley
 */

#include <stdbool.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "Reader.h"
#include "ReaderGKey.h"

#ifdef FORTIFY
#include "fortify.h"
#endif

#ifdef USE_OPTIONAL
#include "Optional.h"
#else
#define _Optional
#endif

enum {
  BufferSize = 256,
  HistoryLog2 = 9
};

#ifdef FORTIFY
static int real_main(int argc, char *argv[]);

int main(int argc, char *argv[])
{
  Fortify_SetNumAllocationsLimit(ULONG_MAX);
  Fortify_EnterScope();
  const int result = real_main(argc, argv);
  Fortify_LeaveScope();
  return result;
}

static int real_main(int argc, char *argv[])
#else
int main(int argc, char *argv[])
#endif
{
  if (argc != 3)
    return EXIT_FAILURE;

  _Optional FILE *const input = fopen(argv[1], "rb");
  if (input == NULL)
    return EXIT_FAILURE;

  _Optional FILE *const output = fopen(argv[2], "wb");
  if (output == NULL) {
    fclose(&*input);
    return EXIT_FAILURE;
  }

  Reader reader;
  bool success = reader_gkey_init(&reader, HistoryLog2, &*input);

  if (success) {
    unsigned char buffer[BufferSize];
    size_t count;

    while ((count = reader_fread(buffer, 1, sizeof(buffer), &reader)) > 0) {
      if (fwrite(buffer, 1, count, &*output) != count) {
        success = false;
        break;
      }
    }

    if (reader_ferror(&reader))
      success = false;

    reader_destroy(&reader);
  }

  if (fclose(&*output) != 0)
    success = false;
  if (fclose(&*input) != 0)
    success = false;

  return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
