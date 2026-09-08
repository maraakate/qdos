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

#include "quakedef.h"

char		allskins[128];
#define	MAX_CACHED_SKINS		128
skin_t		skins[MAX_CACHED_SKINS];
int			numskins;

/* FS: For Download skin queue checking */
typedef struct skinqueue_s
{
	char name[MAX_QPATH];
	qboolean queued;
} skinqueue_t;
skinqueue_t *queued_skins;

/*
================
Skin_Find

  Determines the best skin for the given scoreboard
  slot, and sets scoreboard->skin

================
*/
void Skin_Find (player_info_t *sc)
{
	skin_t *skin;
	int i;
	char name[128], *s;


	if (allskins[0])
		Q_strlcpy (name, allskins, sizeof(name));
	else
	{
		s = Info_ValueForKey (sc->userinfo, "skin");
		if (s && s[0])
			Q_strlcpy (name, s, sizeof(name));
		else
			Q_strlcpy (name, baseskin->string, sizeof(name));
	}

	if (strstr (name, "..") || *name == '.')
		Q_strlcpy (name, "base", sizeof(name));
	COM_StripExtension (name, name);

	for (i=0 ; i<numskins ; i++)
	{
		if (!strcmp (name, skins[i].name))
		{
			sc->skin = &skins[i];
			Skin_Cache (sc->skin);
			return;
		}
	}

	if (numskins == MAX_CACHED_SKINS)
	{	// ran out of spots, so flush everything
		Skin_Skins_f ();
		return;
	}

	skin = &skins[numskins];
	sc->skin = skin;
	numskins++;

	memset (skin, 0, sizeof(*skin));
	Q_strlcpy(skin->name, name, sizeof(skin->name));
}


/*
==========
Skin_Cache

Returns a pointer to the skin bitmap, or NULL to use the default
==========
*/
byte	*Skin_Cache (skin_t *skin)
{
	char	name[MAX_QPATH];
	byte	*raw;
	byte	*out, *pix;
	pcx_t	*pcx;
	int		x = 0; /* FS: Compiler warning */
	int		y;
	int		dataByte;
	int		runLength;

	if (cls.downloadtype == dl_skin)
	{
		return NULL;		// use base until downloaded
	}

	if (!allow_download_skins->intValue) /* FS: Was noskins */
	// JACK: So NOSKINS > 1 will show skins, but
	{
		return NULL;	  // not download new ones.
	}

	if (skin->failedload)
	{
		return NULL;
	}

	out = Cache_Check (&skin->cache);
	if (out)
	{
		return out;
	}
//
// load the pic from disk
//
	Com_sprintf (name, sizeof(name), "skins/%s.pcx", skin->name);

	Com_DPrintf (DEVELOPER_MSG_IO, "Loading skin: %s\n", name); /* FS */

	raw = COM_LoadTempFile(name);
	if (!raw)
	{
		Com_Printf ("Couldn't load skin %s\n", name);
		Com_sprintf (name, sizeof(name), "skins/%s.pcx", baseskin->string);
		raw = COM_LoadTempFile(name);
		if (!raw)
		{
			skin->failedload = true;
			return NULL;
		}
	}

//
// parse the PCX file
//
	pcx = (pcx_t *)raw;
	raw = &pcx->data;

	if (pcx->manufacturer != 0x0a
		|| pcx->version != 5
		|| pcx->encoding != 1
		|| pcx->bits_per_pixel != 8
		|| pcx->xmax >= 320
		|| pcx->ymax >= MAX_LBM_HEIGHT) /* FS: Was >= 200 */
	{
		skin->failedload = true;
		Com_Printf ("Bad skin %s\n", name);
		return NULL;
	}
	
	out = Cache_Alloc (&skin->cache, 320*MAX_LBM_HEIGHT, skin->name); /* FS: Was 320*200 */
	if (!out)
	{
		Sys_Error ("Skin_Cache: couldn't allocate");
		return NULL;
	}

	pix = out;
	memset (out, 0, 320*MAX_LBM_HEIGHT); /* FS: Was 320*200 */

	for (y=0 ; y<pcx->ymax ; y++, pix += 320)
	{
		for (x=0 ; x<=pcx->xmax ; )
		{
			if (raw - (byte*)pcx > com_filesize) 
			{
				Cache_Free (&skin->cache);
				skin->failedload = true;
				Com_Printf ("Skin %s was malformed.  You should delete it.\n", name);
				return NULL;
			}
			dataByte = *raw++;

			if((dataByte & 0xC0) == 0xC0)
			{
				runLength = dataByte & 0x3F;
				if (raw - (byte*)pcx > com_filesize) 
				{
					Cache_Free (&skin->cache);
					skin->failedload = true;
					Com_Printf ("Skin %s was malformed.  You should delete it.\n", name);
					return NULL;
				}
				dataByte = *raw++;
			}
			else
				runLength = 1;

			// skin sanity check
			if (runLength + x > pcx->xmax + 2) {
				Cache_Free (&skin->cache);
				skin->failedload = true;
				Com_Printf ("Skin %s was malformed.  You should delete it.\n", name);
				return NULL;
			}
			while(runLength-- > 0)
				pix[x++] = dataByte;
		}

	}

	Com_DPrintf(DEVELOPER_MSG_IO, "Skin: %s, Size: %d, Width: %d\n", name, com_filesize, x); /* FS */
	
	if ( raw - (byte *)pcx > com_filesize)
	{
		Cache_Free (&skin->cache);
		skin->failedload = true;
		Com_Printf ("Skin %s was malformed.  You should delete it.\n", name);
		return NULL;
	}

	skin->failedload = false;
	return out;
}

void Skin_CheckQueue (char *name) /* FS: Check if we already queued this for download so huge custom TF servers don't show 20+ skins to grab though we actually need like 8 */
{
	int i;

	if(!name)
		return;

	Q_strlcpy(queued_skins[cls.downloadnumber].name, name, sizeof(queued_skins[cls.downloadnumber].name));

	for(i = 0; i <= cls.downloadnumber; i++)
	{
		if(!queued_skins[i].name[0])
			continue;

		if(!strcmp(name, queued_skins[i].name))
		{
			if(!queued_skins[i].queued) /* FS: Okay, this bad boy isn't queued up yet, so let's increase the counter */
			{
				queued_skins[i].queued = true;
				cls.download_queue_total++;
			}
			else /* FS: We already got this queued for later, don't go any further */
				break;
		}
	}
}

/*
=================
Skin_NextDownload
=================
*/
void Skin_NextDownload (qboolean queue) /* FS: FIXME: This is broken if the file doesn't exist on the HTTP server! */
{
	player_info_t	*sc;

	if (queue && cls.downloadnumber == 0)
	{
		Com_Printf ("Checking skins...\n");
		cls.download_queue = cls.download_queue_total = 0;

		queued_skins = (skinqueue_t *)calloc(MAX_CACHED_SKINS, sizeof(skinqueue_t));
		if(!queued_skins)
			Sys_Error("Failed to crated skin queue buffer!");
	}
	cls.downloadtype = dl_skin;

	for ( 
		; cls.downloadnumber != MAX_CLIENTS
		; cls.downloadnumber++)
	{
		sc = &cl.players[cls.downloadnumber];
		if (!sc->name[0])
			continue;
		if(!queue)
			Skin_Find (sc);
		if (!allow_download_skins->intValue)
			continue;
		if (queue)
		{
			if (!CL_CheckOrDownloadFile(va("skins/%s.pcx", sc->skin->name), true)) /* FS: Queue a download */
				Skin_CheckQueue(sc->skin->name);
		}
		else
		{
			if (!CL_CheckOrDownloadFile(va("skins/%s.pcx", sc->skin->name), false)) /* FS: Start a download */
				return;
		}
	}

	if (queue) /* FS: OK, we tallied how many assets we need... Start downloading */
	{
		cls.downloadnumber = 0;
		if(queued_skins)
			free(queued_skins);
		Skin_NextDownload(false);
	}
	else
		Skin_Precache();
}

void Skin_Precache (void)
{
	player_info_t	*sc;
	int i;

	cls.downloadtype = dl_none;

	// now load them in for real
	for (i=0 ; i<MAX_CLIENTS ; i++)
	{
		sc = &cl.players[i];
		if (!sc->name[0])
			continue;
		Skin_Cache (sc->skin);
#ifdef GLQUAKE
		sc->skin = NULL;
#endif
	}

	if (cls.state != ca_active)
	{	// get next signon phase
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message,
			va("begin %i", cl.servercount));
		Cache_Report ();		// print remaining memory
	}
}

void	Skin_FreeAll (void)
{
	int		i;

	for (i=0 ; i<numskins ; i++)
	{
		if (skins[i].cache.data)
			Cache_Free (&skins[i].cache);
	}

	numskins = 0;
}

/*
==========
Skin_Skins_f

Refind all skins, downloading if needed.
==========
*/
void	Skin_Skins_f (void)
{
	Skin_FreeAll();

	if (cls.state == ca_disconnected) /* FS: QuakeForge fix */
		return;

	cls.downloadnumber = 0;
	cls.downloadtype = dl_skin;
	Skin_NextDownload (true);
}


/*
==========
Skin_AllSkins_f

Sets all skins to one specific one
==========
*/
void	Skin_AllSkins_f (void)
{
	Q_strlcpy (allskins, Cmd_Argv(1), sizeof(allskins));
	Skin_Skins_f ();
}
