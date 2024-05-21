/*
 *  SF3KtoProT - Converts Star Fighter 3000 music to Amiga ProTracker format
 *  Platform-specific code (RISC OS implementation)
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
#include <stdbool.h>

#ifdef ACORN_C
/* RISC OS header files */
#include "kernel.h"
#endif

/* Local header files */
#include "misc.h"
#include "filetype.h"

enum
{
  FTYPE_TEQMUSIC = 0xCC5 /* RISC OS file type equivalent to file
                            extension *.mod or *.nst */
};

/* Platform-specific function */
bool set_file_type(const char *file_path)
{
#ifdef ACORN_C
  _kernel_osfile_block kob;

  assert(file_path != NULL);

  /* Apply the RISC OS file type for Amiga ProTracker music
     to the specified file. */
  kob.load = FTYPE_TEQMUSIC;
  return (_kernel_osfile(18, file_path, &kob) != _kernel_ERROR);
#else
  (void)file_path;
  return true;
#endif
}
