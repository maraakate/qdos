/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include <sys/types.h>
#include <limits.h>
#include <sys/stat.h> /* mkdir() */
#include <errno.h>
#include <stdio.h>
#include <dos.h>
#include <dir.h>
#include <io.h>
#include <sys/time.h>

#include "quakedef.h"


//============================================

static	struct ffblk	finddata;
static	int	findhandle = -1;
static	char	findbase[MAX_OSPATH];
static	char	findpath[MAX_OSPATH];

static qboolean CompareAttributes(const struct ffblk *ff,
				  unsigned musthave, unsigned canthave)
{
	/* . and .. never match */
	if (strcmp(ff->ff_name, ".") == 0 || strcmp(ff->ff_name, "..") == 0)
		return false;

	if (ff->ff_attrib & _A_VOLID) /* shouldn't happen */
		return false;

	if (ff->ff_attrib & _A_SUBDIR) {
		if (canthave & SFF_SUBDIR)
			return false;
	}
	else {
		if (musthave & SFF_SUBDIR)
			return false;
	}

	if (ff->ff_attrib & _A_RDONLY) {
		if (canthave & SFF_RDONLY)
			return false;
	}
	else {
		if (musthave & SFF_RDONLY)
			return false;
	}

	if (ff->ff_attrib & _A_HIDDEN) {
		if (canthave & SFF_HIDDEN)
			return false;
	}
	else {
		if (musthave & SFF_HIDDEN)
			return false;
	}

	if (ff->ff_attrib & _A_SYSTEM) {
		if (canthave & SFF_SYSTEM)
			return false;
	}
	else {
		if (musthave & SFF_SYSTEM)
			return false;
	}

	return true;
}

char *Sys_FindFirst (char *path, unsigned musthave, unsigned canthave)
{
	int attribs;

	if (findhandle == 0)
		Sys_Error ("Sys_BeginFind without close");

	COM_FilePath (path, findbase);
	memset (&finddata, 0, sizeof(finddata));

	attribs = FA_ARCH|FA_RDONLY;
	if (!(canthave & SFF_SUBDIR))
		attribs |= FA_DIREC;
	if (musthave & SFF_HIDDEN)
		attribs |= FA_HIDDEN;
	if (musthave & SFF_SYSTEM)
		attribs |= FA_SYSTEM;

	findhandle = findfirst(path, &finddata, attribs);
	if (findhandle != 0)
		return NULL;
	if (CompareAttributes(&finddata, musthave, canthave)) {
		Com_sprintf (findpath, sizeof(findpath), "%s/%s", findbase, finddata.ff_name);
		return findpath;
	}
	return Sys_FindNext(musthave, canthave);
}

char *Sys_FindNext (unsigned musthave, unsigned canthave)
{
	if (findhandle != 0)
		return NULL;

	while (findnext(&finddata) == 0) {
		if (CompareAttributes(&finddata, musthave, canthave)) {
			Com_sprintf (findpath, sizeof(findpath), "%s/%s", findbase, finddata.ff_name);
			return findpath;
		}
	}

	return NULL;
}

void Sys_FindClose (void)
{
	findhandle = -1;
}


//============================================
