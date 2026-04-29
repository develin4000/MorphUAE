 /*
  * UAE - The Un*x Amiga Emulator
  *
  * OS-specific memory support functions
  *
  * Copyright 2004 Richard Drummond
  */

#include "sysconfig.h"
#include "sysdeps.h"
#include "include/memory.h"

//We need the memory handling functions for MorphOS
#include <proto/exec.h>
#include <exec/system.h>

/*
 * Allocate executable memory for JIT cache
 */
void * cache_alloc (int size)
{
	//For MorphOS we will try to figure out the cache line size for the aligned memory allocation
	ULONG cachelinesize;
	if (!NewGetSystemAttrsA(&cachelinesize, sizeof(cachelinesize), SYSTEMINFOTYPE_PPC_ICACHEL1LINESIZE, NULL))
	{
		//Failed: let's send a warning and use 32 byte alignment
		write_log("Warning: failed to read cache alignment requirement, 32 bytes alignment is used");
		cachelinesize = 32;
	}

	//Allocate memory
	return AllocVecAligned(size, MEMF_ANY, cachelinesize, 0);
}

/*
 * PowerPC instruction cache flush function
 */
void ppc_cacheflush(void* start, int length)
{
	CacheFlushDataInstArea(start, length);
}

/**
 * Free JIT cache
 */
void cache_free (void *cache)
{
	FreeVec(cache);
}

