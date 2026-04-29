/* fsusage.c -- return space usage of mounted filesystems
   Copyright (C) 1991, 1992, 1996 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "sysconfig.h"
#include "target.h"

#include <devices/timer.h>

#include <stdlib.h>
#include <sys/types.h>

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#include "fsusage.h"

/* Return the number of TOSIZE-byte blocks used by
   BLOCKS FROMSIZE-byte blocks, rounding away from zero.
   TOSIZE must be positive.  Return -1 if FROMSIZE is not positive.  */

static long
adjust_blocks (long blocks, int fromsize, int tosize)
{
  if (tosize <= 0)
    abort ();
  if (fromsize <= 0)
    return -1;

  if (fromsize == tosize)	/* e.g., from 512 to 512 */
    return blocks;
  else if (fromsize > tosize)	/* e.g., from 2048 to 512 */
    return blocks * (fromsize / tosize);
  else				/* e.g., from 256 to 512 */
    return (blocks + (blocks < 0 ? -1 : 1)) / (tosize / fromsize);
}

#include <dos/dos.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <proto/dos.h>

int get_fs_usage (const char *path, const char *disk, struct fs_usage *fsp)
{
    struct InfoData *info = (struct InfoData *)AllocVec(sizeof *info, MEMF_ANY);
    int result = -1;

    if (info) {
	BPTR lock = Lock (path, SHARED_LOCK);
	if (lock) {
	    if (Info (lock, info)) {
		fsp->fsu_blocks = adjust_blocks (info->id_NumBlocks,
						 info->id_BytesPerBlock,
						 512);
		fsp->fsu_bfree = fsp->fsu_bavail =
				  adjust_blocks (info->id_NumBlocks - info->id_NumBlocksUsed,
						 info->id_BytesPerBlock,
						 512);
		fsp->fsu_files = fsp->fsu_ffree = -1;

		result = 0;
	    }
	    UnLock (lock);
	}
	FreeVec (info);
    }
    return result;
}


