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
// cl_parse.c  -- parse a message received from the server

#include "quakedef.h"

char *svc_strings[] =
{
	"svc_bad",
	"svc_nop",
	"svc_disconnect",
	"svc_updatestat",
	"svc_version",		// [long] server version
	"svc_setview",		// [short] entity number
	"svc_sound",			// <see code>
	"svc_time",			// [float] server time
	"svc_print",			// [string] null terminated string
	"svc_stufftext",		// [string] stuffed into client's console buffer
						// the string should be \n terminated
	"svc_setangle",		// [vec3] set the view angle to this absolute value

	"svc_serverdata",		// [long] version ...
	"svc_lightstyle",		// [byte] [string]
	"svc_updatename",		// [byte] [string]
	"svc_updatefrags",	// [byte] [short]
	"svc_clientdata",		// <shortbits + data>
	"svc_stopsound",		// <see code>
	"svc_updatecolors",	// [byte] [byte]
	"svc_particle",		// [vec3] <variable>
	"svc_damage",			// [byte] impact [byte] blood [vec3] from

	"svc_spawnstatic",
	"svc_fte_spawnstatic2", /* FS: FTE extensions */
	"svc_spawnbaseline",

	"svc_temp_entity",		// <variable>
	"svc_setpause",
	"svc_signonnum",
	"svc_centerprint",
	"svc_killedmonster",
	"svc_foundsecret",
	"svc_spawnstaticsound",
	"svc_intermission",
	"svc_finale",

	"svc_cdtrack",
	"svc_sellscreen",

	"svc_smallkick",
	"svc_bigkick",

	"svc_updateping",
	"svc_updateentertime",

	"svc_updatestatlong",
	"svc_muzzleflash",
	"svc_updateuserinfo",
	"svc_download",
	"svc_playerinfo",
	"svc_nails",
	"svc_chokecount",
	"svc_modellist",
	"svc_soundlist",
	"svc_packetentities",
 	"svc_deltapacketentities",
	"svc_maxspeed",
	"svc_entgravity",

	"svc_setinfo",
	"svc_serverinfo",
	"svc_updatepl", /* FS: 53 */
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"svc_fte_modellistshort", /* FS: 60 -- FTE extensions */
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"svc_fte_spawnbaseline2", /* FS: 66 -- FTE extensions */
	"NEW PROTOCOL"
};

int		oldparsecountmod;
int		parsecountmod;
double	parsecounttime;
int		cl_spikeindex, cl_playerindex, cl_flagindex;

/* FS: Prototypes */
void		CL_ShowChat (char *name, int val);
void		CL_PlayBackgroundTrack (int track);
qboolean	CL_MaliciousStuffText(char *stufftext); /* FS: Check for malicious stufftext */
void		Model_Precache (void);
void		Sound_Precache (void);

//=============================================================================

int packet_latency[NET_TIMINGS];

int CL_CalcNet (void)
{
	int		a, i;
	frame_t	*frame;
	int lost;

	for (i=cls.netchan.outgoing_sequence-UPDATE_BACKUP+1
		; i <= cls.netchan.outgoing_sequence
		; i++)
	{
		frame = &cl.frames[i&UPDATE_MASK];
		if (frame->receivedtime == -1)
			packet_latency[i&NET_TIMINGSMASK] = 9999;	// dropped
		else if (frame->receivedtime == -2)
			packet_latency[i&NET_TIMINGSMASK] = 10000;	// choked
		else if (frame->invalid)
			packet_latency[i&NET_TIMINGSMASK] = 9998;	// invalid delta
		else
			packet_latency[i&NET_TIMINGSMASK] = (frame->receivedtime - frame->senttime)*20;
	}

	lost = 0;
	for (a=0 ; a<NET_TIMINGS ; a++)
	{
		i = (cls.netchan.outgoing_sequence-a) & NET_TIMINGSMASK;
		if (packet_latency[i] == 9999)
			lost++;
	}
	return lost * 100 / NET_TIMINGS;
}

//=============================================================================

/*
===============
CL_CheckOrDownloadFile

Returns true if the file exists, otherwise it attempts
to start a download from the server.
===============
*/
qboolean	CL_CheckOrDownloadFile (char *filename, qboolean queue)
{
	if (strstr (filename, ".."))
	{
		Com_Printf ("Refusing to download a path with ..\n");
		return true;
	}

	if (COM_FileExists(filename))
	{
		// it exists, no need to download
		return true;
	}

	//ZOID - can't download when recording
	if (cls.demorecording) {
		Com_Printf("Unable to download %s in record mode.\n", cls.downloadname);
		return true;
	}
	//ZOID - can't download when playback
	if (cls.demoplayback)
		return true;

	if (queue) /* FS: Are we just queuing it for later or grab it now? */
		return false;
	else
		cls.download_queue++;

	Com_sprintf(cls.downloadname, sizeof(cls.downloadname), "%s", filename);
	if(cls.download_queue && cls.download_queue_total)
		Com_Printf("Downloading %s [%d remaining files]...\n", cls.downloadname, cls.download_queue_total-cls.download_queue);
	else
		Com_Printf ("Downloading %s...\n", cls.downloadname);

	// download to a temp name, and only rename
	// to the real name when done, so if interrupted
	// a runt file wont be left
	COM_StripExtension (cls.downloadname, cls.downloadtempname);
	Q_strlcat (cls.downloadtempname, ".tmp", sizeof(cls.downloadtempname));

	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
	MSG_WriteString (&cls.netchan.message, va("download %s", cls.downloadname));

	cls.downloadnumber++;

	return false;
}

/*
=================
Model_NextDownload
=================
*/
void Model_NextDownload (qboolean queue)
{
	char	*s;

	if (cls.downloadnumber == 0)
	{
		Com_Printf ("Checking models...\n");
		cls.downloadnumber = 1;
		cls.download_queue = cls.download_queue_total = 0;
	}

	cls.downloadtype = dl_model;
	for (
		; cl.model_name[cls.downloadnumber][0]
		; cls.downloadnumber++)
	{
		s = cl.model_name[cls.downloadnumber];
		if (s[0] == '*')
			continue;	// inline brush model
		if (queue)
		{
			if (!CL_CheckOrDownloadFile(s, true)) /* FS: Queue a download */
				cls.download_queue_total++;
		}
		else
		{
			if (!CL_CheckOrDownloadFile(s, false)) /* FS: Start a download */
				return;
		}
	}

	if (queue) /* FS: OK, we tallied how many assets we need... Start downloading */
	{
		cls.downloadnumber = 1;
		Model_NextDownload(false);
	}
	else
		Model_Precache();
}

void Model_Precache (void)
{
	int i;
	extern	char gamedirfile[];

	R_BeginRegistration();

	for (i=1 ; i<MAX_MODELS ; i++)
	{
		if (!cl.model_name[i][0])
			break;

		cl.model_precache[i] = Mod_ForName (cl.model_name[i], false);

		if (!cl.model_precache[i])
		{
			Com_Printf ("\nThe required model file '%s' could not be found or downloaded.\n\n"
				, cl.model_name[i]);
			Com_Printf ("You may need to download or purchase a %s client "
				"pack in order to play on this server.\n\n", gamedirfile);
			CL_Disconnect ();
			return;
		}
	}

	// all done
	cl.worldmodel = cl.model_precache[1];
	R_NewMap ();

	R_EndRegistration();

	Hunk_Check ();		// make sure nothing is hurt

	// done with modellist, request first of static signon messages
	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
	MSG_WriteString (&cls.netchan.message, va(prespawn_name, cl.servercount, cl.worldmodel->checksum2));
}

/*
=================
Sound_NextDownload
=================
*/
void Sound_NextDownload (qboolean queue)
{
	char	*s;

	if (cls.downloadnumber == 0)
	{
		Com_Printf ("Checking sounds...\n");
		cls.downloadnumber = 1;
		cls.download_queue = cls.download_queue_total = 0;
	}

	cls.downloadtype = dl_sound;
	for (
		; cl.sound_name[cls.downloadnumber][0]
		; cls.downloadnumber++)
	{
		s = cl.sound_name[cls.downloadnumber];

		if (!allow_download_sounds->intValue) /* FS: Added */
			continue;
		if (queue)
		{
			if (!CL_CheckOrDownloadFile(va("sound/%s",s), true)) /* FS: Queue a download */
				cls.download_queue_total++;
		}
		else
		{
			if (!CL_CheckOrDownloadFile(va("sound/%s",s), false)) /* FS: Start a download */
				return;
		}
	}

	if (queue) /* FS: OK, we tallied how many assets we need... Start downloading */
	{
		cls.downloadnumber = 1;
		Sound_NextDownload(false);
	}
	else
		Sound_Precache();
}

void Sound_Precache (void)
{
	int i;

	for (i=1 ; i<MAX_SOUNDS ; i++)
	{
		if (!cl.sound_name[i][0])
			break;
		cl.sound_precache[i] = S_PrecacheSound (cl.sound_name[i]);
	}

	// done with sounds, request models now
	memset (cl.model_precache, 0, sizeof(cl.model_precache));
	cl_playerindex = -1;
	cl_spikeindex = -1;
	cl_flagindex = -1;
	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
	MSG_WriteString (&cls.netchan.message, va(modellist_name, cl.servercount, 0));
}

qboolean CL_CreateDownload(int size, qboolean extended)
{
	char name[MAX_OSPATH];

	if (strncmp(cls.downloadtempname,"skins/",6))
		Com_sprintf(name, sizeof(name), "%s/%s", com_gamedir, cls.downloadtempname);
	else
		Com_sprintf(name, sizeof(name), "qw/%s", cls.downloadtempname);

	COM_CreatePath (name);

	cls.download = fopen (name, "wb");
	if (!cls.download)
	{
		if(!extended)
			msg_readcount += size;
		Com_Printf ("Failed to open %s\n", cls.downloadtempname);
		CL_FinishDownload(false);
		return false;
	}

	return true;
}
void CL_FinishDownload(qboolean rename_files)
{
	char	oldn[MAX_OSPATH];
	char	newn[MAX_OSPATH];
	char	oldrate[32];
	int	r;

	if (cls.download)
		fclose (cls.download);

	//
	// if we fail of some kind, do not rename files
	//

	if(rename_files)
	{
		// rename the temp file to it's final name
		if (strcmp(cls.downloadtempname, cls.downloadname))
		{
			if (strncmp(cls.downloadtempname,"skins/",6))
			{
				Com_sprintf(oldn, sizeof(oldn), "%s/%s", com_gamedir, cls.downloadtempname);
				Com_sprintf(newn, sizeof(newn), "%s/%s", com_gamedir, cls.downloadname);
			}
			else
			{
				Com_sprintf(oldn, sizeof(oldn), "qw/%s", cls.downloadtempname);
				Com_sprintf(newn, sizeof(newn), "qw/%s", cls.downloadname);
			}
			Com_DPrintf(DEVELOPER_MSG_IO, "oldn: %s\n", oldn);
			Com_DPrintf(DEVELOPER_MSG_IO, "newn: %s\n", newn);
			r = rename (oldn, newn);

			if (r)
				Com_Printf ("failed to rename. r: %i\n", r); /* FS: Added */
		}
	}

	cls.download = NULL;
	cls.downloadpercent = 0;
	cls.downloadmethod = DL_NONE;

	if (cl_downloadrate_hack->intValue && (rate->intValue > 0 && cls.downloadoldrate > 0)) /* FS: FIXME Shitty hack to accelerate the downloading a bit */
	{
		Com_sprintf(oldrate, sizeof(oldrate), "%d", cls.downloadoldrate);
		Com_DPrintf(DEVELOPER_MSG_NET, "Changing rate from %i to %i\n", cls.downloadmaxrate, cls.downloadoldrate);
		Cvar_Set("rate", oldrate);
	}

	// get another file if needed
	if (cls.state != ca_disconnected)
		CL_RequestNextDownload ();
}

#ifdef FTE_PEXT_CHUNKEDDOWNLOADS

//
// FTE's chunked download
//

#define MAXBLOCKS 1024	// Must be power of 2
#define DLBLOCKSIZE 1024

int chunked_download_number = 0; // Never reset, bumped up.

int downloadsize;
int receivedbytes;
int recievedblock[MAXBLOCKS];
int firstblock;
int blockcycle;

int CL_RequestADownloadChunk(void)
{
	int i;
	int b;

	if (cls.downloadmethod != DL_QWCHUNKS) // Paranoia!
		Host_Error("download not initiated\n");

	for (i = 0; i < MAXBLOCKS; i++)
	{
		blockcycle++;

		b = ((blockcycle) & (MAXBLOCKS-1)) + firstblock;

		if (!recievedblock[b&(MAXBLOCKS-1)]) // Don't ask for ones we've already got.
		{
			if (b >= (downloadsize+DLBLOCKSIZE-1)/DLBLOCKSIZE)	// Don't ask for blocks that are over the size of the file.
				continue;
			return b;
		}
	}

	return -1;
}

void CL_SendChunkDownloadReq(void)
{
	int i, j, chunks = bound(1, cl_chunksperframe->intValue, 5);

	for (j = 0; j < chunks; j++)
	{
		if (cls.downloadmethod != DL_QWCHUNKS)
			return;

		// Ugly workaround
		if(strstr(Info_ValueForKey(cl.serverinfo, "*version"), "MVDSV") == NULL)
		{
			j = chunks;
		}
		i = CL_RequestADownloadChunk();
		// i < 0 mean client complete download, let server know
		// qqshka: download percent optional, server does't really require it, that my extension, hope does't fuck up something
		CL_SendClientCommand(i < 0 ? true : false, "nextdl %d %d %d", i, cls.downloadpercent, chunked_download_number);

		if (i < 0)
			CL_FinishDownload(true); // this also request next dl
	}
}

void CL_Parse_OOB_ChunkedDownload(void)
{
	int j;

	for ( j = 0; j < sizeof("\\chunk")-1; j++ )
		MSG_ReadByte ();

	//
	// qqshka: well, this is evil.
	// In case of when one file completed download and next started
	// here may be some packets which travel via network,
	// so we got packets from different file, that mean we may assemble wrong data,
	// need somehow discard such packets, i have no idea how, so adding at least this check.
	//

	if (chunked_download_number != MSG_ReadLong ())
	{
		Com_DPrintf(DEVELOPER_MSG_NET, "Dropping OOB chunked message, out of sequence\n");
		return;
	}

	if (MSG_ReadByte() != svc_download)
	{
		Com_DPrintf(DEVELOPER_MSG_NET, "Something wrong in OOB message and chunked download\n");
		return;
	}

	CL_ParseDownload ();
}

void CL_ParseChunkedDownload(void)
{
	char *svname;
	int totalsize;
	int chunknum;
	char data[DLBLOCKSIZE];
	double tm;
	char maxrate[32];

	chunknum = MSG_ReadLong();
	if (chunknum < 0)
	{
		totalsize = MSG_ReadLong();
		svname    = MSG_ReadString();

		if (cls.download)
		{
			// Ensure FILE is closed
			if (totalsize != -3) // -3 = dl stopped, so this known issue, do not warn
				Com_Printf ("cls.download shouldn't have been set\n");

			fclose (cls.download);
			cls.download = NULL;
		}

		if (cls.demoplayback)
			return;

		if (totalsize < 0)
		{
			switch (totalsize)
			{
				case -3: Com_DPrintf(DEVELOPER_MSG_NET, "Server cancel downloading file %s\n", svname);			break;
				case -2: Com_Printf("Server permissions deny downloading file %s\n", svname);	break;
				default: Com_Printf("Couldn't find file %s on the server\n", svname);			break;
			}

			CL_FinishDownload(false); // this also request next dl
			return;
		}

		if (cls.downloadmethod == DL_QWCHUNKS)
			Host_Error("Received second download - \"%s\"\n", svname);

// FIXME: damn, fixme!!!!!
//		if (strcasecmp(cls.downloadname, svname))
//			Host_Error("Server sent the wrong download - \"%s\" instead of \"%s\"\n", svname, cls.downloadname);

		// Start the new download
		if (!cls.download)
		{
			if(!CL_CreateDownload(0, true))
				return;
		}

		if (cl_downloadrate_hack->intValue)
		{
			cls.downloadoldrate = rate->intValue;
			cls.downloadmaxrate = 250000;
			Com_sprintf(maxrate, sizeof(maxrate), "%i", cls.downloadmaxrate);
			Com_DPrintf(DEVELOPER_MSG_NET, "Changing rate from %i to %i\n", cls.downloadoldrate, cls.downloadmaxrate);
			Cvar_Set("rate", maxrate);
		}

		cls.downloadmethod  = DL_QWCHUNKS;
		cls.downloadpercent = 0;

		chunked_download_number++;

		downloadsize        = totalsize;

		firstblock    = 0;
		receivedbytes = 0;
		blockcycle    = -1;	//so it requests 0 first. :)
		memset(recievedblock, 0, sizeof(recievedblock));
		return;
	}

	MSG_ReadData(data, DLBLOCKSIZE);

	if (!cls.download)
	{
		return;
	}

	if (cls.downloadmethod != DL_QWCHUNKS)
		Host_Error("cls.downloadmethod != DL_QWCHUNKS\n");

	if (cls.demoplayback)
	{
		// Err, yeah, when playing demos we don't actually pay any attention to this.
		return;
	}

	if (chunknum < firstblock)
	{
		return;
	}

	if (chunknum - firstblock >= MAXBLOCKS)
	{
		return;
	}

	if (recievedblock[chunknum&(MAXBLOCKS-1)])
	{
		return;
	}

	receivedbytes += DLBLOCKSIZE;
	recievedblock[chunknum&(MAXBLOCKS-1)] = true;

	while(recievedblock[firstblock&(MAXBLOCKS-1)])
	{
		recievedblock[firstblock&(MAXBLOCKS-1)] = false;
		firstblock++;
	}

	fseek(cls.download, chunknum * DLBLOCKSIZE, SEEK_SET);
	if (downloadsize - chunknum * DLBLOCKSIZE < DLBLOCKSIZE)	//final block is actually meant to be smaller than we recieve.
		fwrite(data, 1, downloadsize - chunknum * DLBLOCKSIZE, cls.download);
	else
		fwrite(data, 1, DLBLOCKSIZE, cls.download);

	cls.downloadpercent = receivedbytes/(float)downloadsize*100;

	tm = Sys_DoubleTime() - cls.downloadstarttime; // how long we dl-ing
	cls.downloadrate = (tm ? receivedbytes / 1024 / tm : 0); // some average dl speed in KB/s
}

#endif // FTE_PEXT_CHUNKEDDOWNLOADS


/*
======================
CL_RequestNextDownload
======================
*/
void CL_RequestNextDownload (void)
{
	switch (cls.downloadtype)
	{
	case dl_single:
		break;
	case dl_skin:
		Skin_NextDownload(false);
		break;
	case dl_model:
		Model_NextDownload(false);
		break;
	case dl_sound:
		Sound_NextDownload(false);
		break;
	case dl_none:
	default:
		Com_DPrintf(DEVELOPER_MSG_NET, "Unknown download type.\n");
	}
}

/*
=====================
CL_ParseDownload

A download message has been received from the server
=====================
*/
void CL_ParseDownload (void)
{
	int		size, percent;
	//byte    name[1024];
	//int              r;


#ifdef FTE_PEXT_CHUNKEDDOWNLOADS
	if ((cls.fteprotocolextensions & FTE_PEXT_CHUNKEDDOWNLOADS) && !cls.quakeforge_http_dl) /* FS: QuakeForge HTTP DL and FTE chunked downloads will clash */
	{
		CL_ParseChunkedDownload();
		return;
	}
#endif // PFTE_PEXT_CHUNKEDDOWNLOADS

	// read the data
	size = (short)MSG_ReadShort ();
	percent = MSG_ReadByte ();

	if (cls.demoplayback) {
		if (size > 0)
			msg_readcount += size;
		return; // not in demo playback
	}

	if (size == -1)
	{
		Com_Printf ("File not found.\n");
		if (cls.download)
		{
			Com_Printf ("cls.download shouldn't have been set\n");
			fclose (cls.download);
			cls.download = NULL;
		}
		CL_RequestNextDownload ();
		return;
	}
	if (size == DL_HTTP)
	{
#ifdef USE_CURL
		const char *url;
		const char *newname;

		url = MSG_ReadString ();
		Com_sprintf (cls.downloadurl, sizeof(cls.downloadurl), "%s", url);

		newname = MSG_ReadString ();
		if (*newname)
		{
			if (strncmp (newname, cls.downloadname,
						 strlen (cls.downloadname))
				|| strstr (newname + strlen (cls.downloadname), "/")) {
				Con_Warning("Server tried to give a strange new name: %s\n", newname);
				CL_RequestNextDownload ();
				return;
			}
			if (cls.download)
			{
				fclose(cls.download);

			}
			Com_sprintf(cls.downloadname, sizeof(cls.downloadname), "%s", newname);
		}
		CL_HTTP_StartDownload ();
#else
		MSG_ReadString ();
		MSG_ReadString ();
		Con_Warning ("Server sent HTTP redirect but QWDOS was compiled without CURL.\n");
#endif
		CL_CreateDownload(0, true);
		return;
	}
	// open the file if not opened yet
	if (!cls.download)
	{
		CL_Download_Reset_KBps_counter();

		if(!CL_CreateDownload(size, false)) /* FS: Added */
			return;
	}

	cls.downloadmethod  = DL_QW;
	fwrite (net_message.data + msg_readcount, 1, size, cls.download);
	msg_readcount += size;

	if (percent != 100)
	{
		// request next block
		CL_Download_Calculate_KBps (size, 0);	// Knightmare- for KB/s counter

		cls.downloadpercent = percent;

		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		SZ_Print (&cls.netchan.message, "nextdl");
	}
	else
	{
		CL_FinishDownload(true);
	}
}

static byte *upload_data;
static int upload_pos;
static int upload_size;

void CL_NextUpload(void)
{
	byte	buffer[1024];
	int		r;
	int		percent;
	int		size;

	if (!upload_data)
		return;

	r = upload_size - upload_pos;
	if (r > 768)
		r = 768;
	memcpy(buffer, upload_data + upload_pos, r);
	MSG_WriteByte (&cls.netchan.message, clc_upload);
	MSG_WriteShort (&cls.netchan.message, r);

	upload_pos += r;
	size = upload_size;
	if (!size)
		size = 1;
	percent = upload_pos*100/size;
	MSG_WriteByte (&cls.netchan.message, percent);
	SZ_Write (&cls.netchan.message, buffer, r);

	Com_DPrintf (DEVELOPER_MSG_NET, "UPLOAD: %6d: %d written\n", upload_pos - r, r);

	if (upload_pos != upload_size)
		return;

	Com_Printf ("Upload completed\n");

	free(upload_data);
	upload_data = 0;
	upload_pos = upload_size = 0;
}

void CL_StartUpload (byte *data, int size)
{
	if (cls.state < ca_onserver)
		return; // gotta be connected

	// override
	if (upload_data)
		free(upload_data);

Com_DPrintf(DEVELOPER_MSG_NET, "Upload starting of %d...\n", size);

	upload_data = malloc(size);
	memcpy(upload_data, data, size);
	upload_size = size;
	upload_pos = 0;

	CL_NextUpload();
}

qboolean CL_IsUploading(void)
{
	if (upload_data)
		return true;
	return false;
}

void CL_StopUpload(void)
{
	if (upload_data)
		free(upload_data);
	upload_data = NULL;
}

/*
=====================================================================

  SERVER CONNECTING MESSAGES

=====================================================================
*/

/*
==================
CL_ParseServerData
==================
*/
void CL_ParseServerData (void)
{
	char	*str;
	FILE	*f;
	char	fn[MAX_OSPATH];
	qboolean	cflag = false;
	extern	char	gamedirfile[MAX_OSPATH];
	int protover;

	Com_DPrintf (DEVELOPER_MSG_NET, "Serverdata packet received.\n");
//
// wipe the client_state_t struct
//
	CL_ClearState ();
	Cvar_GetLatchedVars (); /* FS: Update latched CVARs now, like r_maxedges, r_maxsurfs, etc */

// parse protocol version number
// allow 2.2 and 2.29 demos to play
#ifdef PROTOCOL_VERSION_FTE
	cls.fteprotocolextensions = 0;

	for(;;)
	{
		protover = MSG_ReadLong ();
		if (protover == PROTOCOL_VERSION_FTE)
		{
			cls.fteprotocolextensions =  MSG_ReadLong();
			Com_DPrintf (DEVELOPER_MSG_NET, "Using FTE extensions 0x%x\n", cls.fteprotocolextensions);
			continue;
		}
		if (protover == PROTOCOL_VERSION) //this ends the version info
			break;
		if (cls.demoplayback && (protover == 26 || protover == 27 || protover == 28))	//older versions, maintain demo compatability.
			break;
		Host_Error ("Server returned version %i, not %i\nYou probably need to upgrade.\nCheck http://www.quakeworld.net/", protover, PROTOCOL_VERSION);
	}
#ifdef FTE_PEXT_FLOATCOORDS
	if (cls.fteprotocolextensions & FTE_PEXT_FLOATCOORDS)
	{
		msg_coordsize = 4;
		msg_anglesize = 2;
	}
	else
	{
		msg_coordsize = 2;
		msg_anglesize = 1;
	}
#endif // FTE_PEXT_FLOATCOORDS
#else
	protover = MSG_ReadLong ();
	if (protover != PROTOCOL_VERSION &&	!(cls.demoplayback && (protover == 26 || protover == 27 || protover == 28)))
		Host_Error ("Server returned version %i, not %i\nYou probably need to upgrade.\nCheck http://www.quakeworld.net,\nhttp://ezquake.sourceforge.net,\nhttp://mvdsv.sourceforge.net", protover, PROTOCOL_VERSION);
#endif // PROTOCOL_VERSION_FTE

	cl.servercount = MSG_ReadLong ();

	// game directory
	str = MSG_ReadString ();

	if (stricmp(gamedirfile, str)) {
		// save current config
		Host_WriteConfiguration ("qdos");
		cflag = true;
	}

	COM_Gamedir(str);

	//ZOID--run the autoexec.cfg in the gamedir
	//if it exists
	if (cflag) {
		Com_sprintf(fn, sizeof(fn), "%s/%s", com_gamedir, "qdos.cfg");
		if ((f = fopen(fn, "r")) != NULL) {
			fclose(f);
			Cbuf_AddText ("cl_warncmd 0\n");
			Cbuf_AddText("exec qdos.cfg\n");
			Cbuf_AddText("exec frontend.cfg\n");
			Cbuf_AddText ("cl_warncmd 1\n");
		}
	}

	// parse player slot, high bit means spectator
	cl.playernum = MSG_ReadByte ();
	if (cl.playernum & 128)
	{
		cl.spectator = true;
		cl.playernum &= ~128;
	}

	// get the full level name
	str = MSG_ReadString ();
	Q_strlcpy (cl.levelname, str, sizeof(cl.levelname));

	// get the movevars
	movevars.gravity			= MSG_ReadFloat();
	movevars.stopspeed          = MSG_ReadFloat();
	movevars.maxspeed           = MSG_ReadFloat();
	movevars.spectatormaxspeed  = MSG_ReadFloat();
	movevars.accelerate         = MSG_ReadFloat();
	movevars.airaccelerate      = MSG_ReadFloat();
	movevars.wateraccelerate    = MSG_ReadFloat();
	movevars.friction           = MSG_ReadFloat();
	movevars.waterfriction      = MSG_ReadFloat();
	movevars.entgravity         = MSG_ReadFloat();

	// seperate the printfs so the server message can have a color
	Com_Printf("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n");
	Com_Printf ("%c%s\n", 2, str);

	// ask for the sound list next
	memset(cl.sound_name, 0, sizeof(cl.sound_name));
	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
//	MSG_WriteString (&cls.netchan.message, va("soundlist %i 0", cl.servercount));
	MSG_WriteString (&cls.netchan.message, va(soundlist_name, cl.servercount, 0));

	// now waiting for downloads, etc
	cls.state = ca_onserver;
}

/*
==================
CL_ParseSoundlist
==================
*/
void CL_ParseSoundlist (void)
{
	int	numsounds;
	char	*str;
	int n;

// precache sounds
//	memset (cl.sound_precache, 0, sizeof(cl.sound_precache));

	numsounds = MSG_ReadByte();

	for (;;) {
		str = MSG_ReadString ();
		if (!str[0])
			break;
		numsounds++;
		if (numsounds == MAX_SOUNDS)
		{
			Host_EndGame ("Server sent too many sound_precache");
			return;
		}
		Q_strlcpy (cl.sound_name[numsounds], str, sizeof(cl.sound_name[numsounds]));
	}

	n = MSG_ReadByte();

	if (n) {
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, va(soundlist_name, cl.servercount, n));
		return;
	}

	cls.downloadnumber = 0;
	cls.downloadtype = dl_sound;
	Sound_NextDownload (true);
}

/*
==================
CL_ParseModellist
==================
*/
void CL_ParseModellist (qboolean extended)
{
	int	nummodels;
	char	*str;
	int n;

// precache models and note certain default indexes
	nummodels = (extended) ? (unsigned) MSG_ReadShort () : MSG_ReadByte ();

	for (;;)
	{
		str = MSG_ReadString ();
		if (!str[0])
			break;
#if defined (PROTOCOL_VERSION_FTE) && defined (FTE_PEXT_MODELDBL)
		nummodels++;
		if (nummodels >= MAX_MODELS) //Spike: tweeked this, we still complain if the server exceeds the standard limit without using extensions.
		{
			Host_Error ("Server sent too many model_precache");
			return;
		}

		if (nummodels >= 256 && !(cls.fteprotocolextensions & FTE_PEXT_MODELDBL))
#else
		if (++nummodels == MAX_MODELS)
#endif // PROTOCOL_VERSION_FTE
		{
			Host_EndGame ("Server sent too many model_precache");
			return;
		}
		Q_strlcpy (cl.model_name[nummodels], str, sizeof(cl.model_name[nummodels]));

		if (!strcmp(cl.model_name[nummodels],"progs/spike.mdl"))
			cl_spikeindex = nummodels;
		if (!strcmp(cl.model_name[nummodels],"progs/player.mdl"))
			cl_playerindex = nummodels;
		if (!strcmp(cl.model_name[nummodels],"progs/flag.mdl"))
			cl_flagindex = nummodels;
	}

	n = MSG_ReadByte();

	if (n) {
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
#if defined (PROTOCOL_VERSION_FTE) && defined (FTE_PEXT_MODELDBL)
			MSG_WriteString (&cls.netchan.message, va("modellist %i %i", cl.servercount, (nummodels&0xff00)+n));
#else
			MSG_WriteString (&cls.netchan.message, va("modellist %i %i", cl.servercount, n));
#endif // PROTOCOL_VERSION_FTE
		return;
	}

	cls.downloadnumber = 0;
	cls.downloadtype = dl_model;
	Model_NextDownload (true);
}

/*
==================
CL_ParseBaseline
==================
*/
void CL_ParseBaseline (entity_state_t *es)
{
	int			i;

	es->modelindex = MSG_ReadByte ();
	es->frame = MSG_ReadByte ();
	es->colormap = MSG_ReadByte();
	es->skinnum = MSG_ReadByte();
	for (i=0 ; i<3 ; i++)
	{
		es->origin[i] = MSG_ReadCoord ();
		es->angles[i] = MSG_ReadAngle ();
	}
}

// An easy way to keep compatability with other entity extensions
#if defined (PROTOCOL_VERSION_FTE) && defined (FTE_PEXT_SPAWNSTATIC2)
extern void CL_ParseDelta (entity_state_t *from, entity_state_t *to, int bits);

static void CL_ParseSpawnBaseline2 (void)
{
	entity_state_t nullst, es;

	if (!(cls.fteprotocolextensions & FTE_PEXT_SPAWNSTATIC2))
	{
		Host_EndGame ("illegible server message\nsvc_fte_spawnbaseline2 (%i) without FTE_PEXT_SPAWNSTATIC2\n", svc_fte_spawnbaseline2);
	}

	memset(&nullst, 0, sizeof (entity_state_t));
	memset(&es, 0, sizeof (entity_state_t));

	CL_ParseDelta(&nullst, &es, MSG_ReadShort());
	memcpy(&cl_baselines[es.number], &es, sizeof(es));
}
#endif

/*
=====================
CL_ParseStatic

Static entities are non-interactive world objects
like torches
=====================
*/
void CL_ParseStatic (qboolean extended)
{
	entity_t *ent;
	int		i;
	entity_state_t	es;

	if (extended)
	{
		entity_state_t nullst;
		memset (&nullst, 0, sizeof(entity_state_t));

		CL_ParseDelta (&nullst, &es, MSG_ReadShort());
	}
	else
	{
		CL_ParseBaseline (&es);
	}

	i = cl.num_statics;
	if (i >= MAX_STATIC_ENTITIES)
		Host_EndGame ("Too many static entities");
	ent = &cl_static_entities[i];
	cl.num_statics++;

// copy it to the current state
	ent->model = cl.model_precache[es.modelindex];
	ent->frame = es.frame;
	ent->colormap = vid.colormap;
	ent->skinnum = es.skinnum;

	VectorCopy (es.origin, ent->origin);
	VectorCopy (es.angles, ent->angles);

	R_AddEfrags (ent);
}

/*
===================
CL_ParseStaticSound
===================
*/
void CL_ParseStaticSound (void)
{
	vec3_t		org;
	int			sound_num, vol, atten;
	int			i;

	for (i=0 ; i<3 ; i++)
		org[i] = MSG_ReadCoord ();
	sound_num = MSG_ReadByte ();
	vol = MSG_ReadByte ();
	atten = MSG_ReadByte ();

	S_StaticSound (cl.sound_precache[sound_num], org, vol, atten);
}



/*
=====================================================================

ACTION MESSAGES

=====================================================================
*/

/*
==================
CL_ParseStartSoundPacket
==================
*/
void CL_ParseStartSoundPacket(void)
{
    vec3_t  pos;
    int 	channel, ent;
    int 	sound_num;
    int 	volume;
    float 	attenuation;
 	int		i;

    channel = MSG_ReadShort();

    if (channel & SND_VOLUME)
		volume = MSG_ReadByte ();
	else
		volume = DEFAULT_SOUND_PACKET_VOLUME;

    if (channel & SND_ATTENUATION)
		attenuation = MSG_ReadByte () / 64.0;
	else
		attenuation = DEFAULT_SOUND_PACKET_ATTENUATION;

	sound_num = MSG_ReadByte ();

	for (i=0 ; i<3 ; i++)
		pos[i] = MSG_ReadCoord ();

	ent = (channel>>3)&1023;
	channel &= 7;

	if (ent > MAX_EDICTS)
		Host_EndGame ("CL_ParseStartSoundPacket: ent = %i", ent);

    S_StartSound (ent, channel, cl.sound_precache[sound_num], pos, volume/255.0, attenuation);
}


/*
==================
CL_ParseClientdata

Server information pertaining to this client only, sent every frame
==================
*/
void CL_ParseClientdata (void)
{
	int				i;
	float		latency;
	frame_t		*frame;

// calculate simulated time of message
	oldparsecountmod = parsecountmod;

	i = cls.netchan.incoming_acknowledged;
	cl.parsecount = i;
	i &= UPDATE_MASK;
	parsecountmod = i;
	frame = &cl.frames[i];
	parsecounttime = cl.frames[i].senttime;

	frame->receivedtime = realtime;

// calculate latency
	latency = frame->receivedtime - frame->senttime;

	if (latency < 0 || latency > 1.0)
	{
//		Com_Printf ("Odd latency: %5.2f\n", latency);
	}
	else
	{
	// drift the average latency towards the observed latency
		if (latency < cls.latency)
			cls.latency = latency;
		else
			cls.latency += 0.001;	// drift up, so correction are needed
	}
}

/*
=====================
CL_NewTranslation
=====================
*/
void CL_NewTranslation (int slot)
{
#ifdef GLQUAKE
	if (slot > MAX_CLIENTS)
		Sys_Error ("CL_NewTranslation: slot > MAX_CLIENTS");

	R_TranslatePlayerSkin(slot);
#else

	int		i, j;
	int		top, bottom;
	byte	*dest, *source;
	player_info_t	*player;
	char s[512];

	if (slot > MAX_CLIENTS)
		Sys_Error ("CL_NewTranslation: slot > MAX_CLIENTS");

	player = &cl.players[slot];

	Q_strlcpy(s, Info_ValueForKey(player->userinfo, "skin"), sizeof(s));
	COM_StripExtension(s, s);
	if (player->skin && !stricmp(s, player->skin->name))
		player->skin = NULL;

	if (player->_topcolor != player->topcolor ||
		player->_bottomcolor != player->bottomcolor || !player->skin) {
		player->_topcolor = player->topcolor;
		player->_bottomcolor = player->bottomcolor;

		dest = player->translations;
		source = vid.colormap;
		memcpy (dest, vid.colormap, sizeof(player->translations));
		top = player->topcolor;
		if (top > 13 || top < 0)
			top = 13;
		top *= 16;
		bottom = player->bottomcolor;
		if (bottom > 13 || bottom < 0)
			bottom = 13;
		bottom *= 16;

		for (i=0 ; i<VID_GRADES ; i++, dest += 256, source+=256)
		{
			if (top < 128)	// the artists made some backwards ranges.  sigh.
				memcpy (dest + TOP_RANGE, source + top, 16);
			else
				for (j=0 ; j<16 ; j++)
					dest[TOP_RANGE+j] = source[top+15-j];

			if (bottom < 128)
				memcpy (dest + BOTTOM_RANGE, source + bottom, 16);
			else
				for (j=0 ; j<16 ; j++)
					dest[BOTTOM_RANGE+j] = source[bottom+15-j];
		}
	}
#endif
}

/*
==============
CL_UpdateUserinfo
==============
*/
void CL_ProcessUserInfo (int slot, player_info_t *player)
{
	Q_strlcpy (player->name, Info_ValueForKey (player->userinfo, "name"), sizeof(player->name));
	player->topcolor = atoi(Info_ValueForKey (player->userinfo, "topcolor"));
	player->bottomcolor = atoi(Info_ValueForKey (player->userinfo, "bottomcolor"));
	if (Info_ValueForKey (player->userinfo, "*spectator")[0])
		player->spectator = true;
	else
		player->spectator = false;

	if (cls.state == ca_active)
		Skin_Find (player);

	Sbar_Changed ();
	CL_NewTranslation (slot);
}

/*
==============
CL_UpdateUserinfo
==============
*/
void CL_UpdateUserinfo (void)
{
	int		slot;
	player_info_t	*player;

	slot = MSG_ReadByte ();
	if (slot >= MAX_CLIENTS)
		Host_EndGame ("CL_ParseServerMessage: svc_updateuserinfo > MAX_SCOREBOARD");

	player = &cl.players[slot];
	player->userid = MSG_ReadLong ();
	Q_strlcpy (player->userinfo, MSG_ReadString(), sizeof(player->userinfo));

	CL_ProcessUserInfo (slot, player);
}

/*
==============
CL_SetInfo
==============
*/
void CL_SetInfo (void)
{
	int		slot;
	player_info_t	*player;
	char key[MAX_MSGLEN];
	char value[MAX_MSGLEN];

	slot = MSG_ReadByte ();
	if (slot >= MAX_CLIENTS)
		Host_EndGame ("CL_ParseServerMessage: svc_setinfo > MAX_SCOREBOARD");

	player = &cl.players[slot];

	Q_strlcpy (key, MSG_ReadString(), sizeof(key));
	Q_strlcpy (value, MSG_ReadString(), sizeof(value));

	if(!stricmp(key, "chat")) /* FS: EZQ Chat */
	{
		switch (atoi(value))
		{
		case 1:
		case 2:
		case 3:
			CL_ShowChat(player->name, atoi(value));
			cl.players[slot].chatglow = atoi(value);
			break;
		default:
			cl.players[slot].chatglow = 0;
			break;
		}
	}

	Com_DPrintf(DEVELOPER_MSG_NET, "SETINFO %s: %s=%s\n", player->name, key, value);

	Info_SetValueForKey (player->userinfo, key, value, MAX_INFO_STRING);

	CL_ProcessUserInfo (slot, player);
}

/* FS: EZQ Chat */
void	CL_ShowChat (char *name, int val)
{
	if (net_showchat->intValue)
	{
		switch(val)
		{
			case 1:
				Com_Printf("%s->chat=Typing...\n", name);
				break;
			case 2:
				Com_Printf("%s->chat=AFK...\n", name);
				break;
			case 3:
				Com_Printf("%s->chat=Typing...\n", name);
				break;
			default:
				break;
		}
	}
}

/*
==============
CL_ServerInfo
==============
*/
void CL_ServerInfo (void)
{
	char key[MAX_MSGLEN];
	char value[MAX_MSGLEN];

	Q_strlcpy (key, MSG_ReadString(), sizeof(key));
	Q_strlcpy (value, MSG_ReadString(), sizeof(value));

	Com_DPrintf(DEVELOPER_MSG_NET, "SERVERINFO: %s=%s\n", key, value);

	Info_SetValueForKey (cl.serverinfo, key, value, MAX_SERVERINFO_STRING);
}

/*
=====================
CL_SetStat
=====================
*/
void CL_SetStat (int stat, int value)
{
	int	j;
	if (stat < 0 || stat >= MAX_CL_STATS)
		Sys_Error ("CL_SetStat: %i is invalid", stat);

	Sbar_Changed ();

	if (stat == STAT_ITEMS)
	{	// set flash times
		Sbar_Changed ();
		for (j=0 ; j<32 ; j++)
			if ( (value & (1<<j)) && !(cl.stats[stat] & (1<<j)))
				cl.item_gettime[j] = cl.time;
	}

	cl.stats[stat] = value;
}

/*
==============
CL_MuzzleFlash
==============
*/
void CL_MuzzleFlash (void)
{
	vec3_t		fv, rv, uv;
	dlight_t	*dl;
	int			i;
	player_state_t	*pl;

	i = MSG_ReadShort ();

	if ((unsigned)(i-1) >= MAX_CLIENTS)
		return;

#ifdef GLQUAKE
	// don't draw our own muzzle flash in gl if flashblending
	if (i-1 == cl.playernum && gl_flashblend->value)
		return;
#endif

	pl = &cl.frames[parsecountmod].playerstate[i-1];

	dl = CL_AllocDlight (i);
	VectorCopy (pl->origin,  dl->origin);
	AngleVectors (pl->viewangles, fv, rv, uv);

	VectorMA (dl->origin, 18, fv, dl->origin);
	dl->radius = 200 + (rand()&31);
	dl->minlight = 32;
	dl->die = cl.time + 0.1;
	dl->color[0] = 0.2;
	dl->color[1] = 0.1;
	dl->color[2] = 0.05;
	dl->color[3] = 0.7;
}


#define SHOWNET(x) if(cl_shownet->value==2)Com_Printf ("%3i:%s\n", msg_readcount-1, x);
/*
=====================
CL_ParseServerMessage
=====================
*/
int	received_framecount;
void CL_ParseServerMessage (void)
{
	int		cmd;
	char	*s;
	int		i, j;

	received_framecount = host_framecount;
	cl.last_servermessage = realtime;
	CL_ClearProjectiles ();

//
// if recording demos, copy the message out
//
	if (cl_shownet->value == 1)
		Com_Printf ("%i ",net_message.cursize);
	else if (cl_shownet->value == 2)
		Com_Printf ("------------------\n");


	CL_ParseClientdata ();

//
// parse the message
//
	while (1)
	{
		if (msg_badread)
		{
			Host_EndGame ("CL_ParseServerMessage: Bad server message");
			break;
		}

		cmd = MSG_ReadByte ();

		if (cmd == -1)
		{
			msg_readcount++;	// so the EOM showner has the right value
			SHOWNET("END OF MESSAGE");
			break;
		}

		SHOWNET(svc_strings[cmd]);

	// other commands
		switch (cmd)
		{
		default:
			Host_EndGame ("CL_ParseServerMessage: Illegible server message %d", cmd);
			break;

		case svc_nop:
//			Com_Printf ("svc_nop\n");
			break;

		case svc_disconnect:
			if (cls.state == ca_connected)
				Host_EndGame ("Server disconnected\n"
					"Server version may not be compatible");
			else
				Host_EndGame ("Server disconnected");
			break;

		case svc_print:
			i = MSG_ReadByte ();
			if (i == PRINT_CHAT)
			{
				S_LocalSound ("misc/talk.wav");
				con_ormask = 128;
			}
			s = MSG_ReadString();
			if (!strncmp(s, "Downloading: ", 13)) /* FS: MVDSV XE hack */
			{
				Com_DPrintf(DEVELOPER_MSG_NET, "%s", s);
				con_ormask = 0;
				break;
			}

			//r1: change !p_version to !version since p is for proxies
			if ((strstr (s, "f_version") || strstr (s, "q_version")) &&
				(cls.lastSpamTime == 0.0f || realtime > cls.lastSpamTime + 300.0f)) /* FS: 5 minutes */
			{
				cls.spamTime = realtime + 1.5f; /* FS: 1.5 second delay */
			}

			Com_Printf ("%s", s); /* FS: f_version and q_version reply */
			con_ormask = 0;
			break;

		case svc_centerprint:
			//johnfitz -- log centerprints to console
			s = MSG_ReadString ();
			SCR_CenterPrint (s);
			Con_LogCenterPrint (s);
			//johnfitz
			break;

		case svc_stufftext:
			s = MSG_ReadString ();
			Com_DPrintf (DEVELOPER_MSG_NET, "stufftext: %s\n", s);

			if(CL_MaliciousStuffText(s)) /* FS: Ignore malicious stufftext */
				break;

			Cbuf_AddText (s);
			break;

		case svc_damage:
			V_ParseDamage ();
			break;

		case svc_serverdata:
			Cbuf_Execute ();		// make sure any stuffed commands are done
			CL_ParseServerData ();
			vid.recalc_refdef = true;	// leave full screen intermission
			break;

		case svc_setangle:
			for (i=0 ; i<3 ; i++)
				cl.viewangles[i] = MSG_ReadAngle ();
//			cl.viewangles[PITCH] = cl.viewangles[ROLL] = 0;
			break;

		case svc_lightstyle:
			i = MSG_ReadByte ();
			if (i >= MAX_LIGHTSTYLES)
			{
				Sys_Error ("svc_lightstyle > MAX_LIGHTSTYLES");
				return;
			}
			Q_strlcpy (cl_lightstyle[i].map,  MSG_ReadString(), sizeof(cl_lightstyle[i].map));
			cl_lightstyle[i].length = Q_strlen(cl_lightstyle[i].map);
			break;

		case svc_sound:
			CL_ParseStartSoundPacket();
			break;

		case svc_stopsound:
			i = MSG_ReadShort();
			S_StopSound(i>>3, i&7);
			break;

		case svc_updatefrags:
			Sbar_Changed ();
			i = MSG_ReadByte ();
			if (i >= MAX_CLIENTS)
			{
				Host_EndGame ("CL_ParseServerMessage: svc_updatefrags > MAX_SCOREBOARD");
				return;
			}
			cl.players[i].frags = MSG_ReadShort ();
			break;

		case svc_updateping:
			i = MSG_ReadByte ();
			if (i >= MAX_CLIENTS)
			{
				Host_EndGame ("CL_ParseServerMessage: svc_updateping > MAX_SCOREBOARD");
				return;
			}
			cl.players[i].ping = MSG_ReadShort ();
			break;

		case svc_updatepl:
			i = MSG_ReadByte ();
			if (i >= MAX_CLIENTS)
			{
				Host_EndGame ("CL_ParseServerMessage: svc_updatepl > MAX_SCOREBOARD");
				return;
			}
			cl.players[i].pl = MSG_ReadByte ();
			break;

		case svc_updateentertime:
		// time is sent over as seconds ago
			i = MSG_ReadByte ();
			if (i >= MAX_CLIENTS)
			{
				Host_EndGame ("CL_ParseServerMessage: svc_updateentertime > MAX_SCOREBOARD");
				return;
			}
			cl.players[i].entertime = realtime - MSG_ReadFloat ();
			break;

		case svc_spawnbaseline:
			i = MSG_ReadShort ();
			CL_ParseBaseline (&cl_baselines[i]);
			break;
#if defined (PROTOCOL_VERSION_FTE) && defined (FTE_PEXT_SPAWNSTATIC2)
		case svc_fte_spawnbaseline2:
			CL_ParseSpawnBaseline2();
			break;
#endif // PROTOCOL_VERSION_FTE
		case svc_spawnstatic:
			CL_ParseStatic (false);
			break;
#if defined (PROTOCOL_VERSION_FTE) && defined (FTE_PEXT_SPAWNSTATIC2)
		case svc_fte_spawnstatic2:
		{
			if (cls.fteprotocolextensions & FTE_PEXT_SPAWNSTATIC2)
				CL_ParseStatic(true);
			else
				Host_Error("CL_ParseServerMessage: svc_fte_modellistshort without FTE_PEXT_SPAWNSTATIC2");
			break;
		}
#endif // PROTOCOL_VERSION_FTE
		case svc_temp_entity:
			CL_ParseTEnt ();
			break;

		case svc_killedmonster:
			cl.stats[STAT_MONSTERS]++;
			break;

		case svc_foundsecret:
			cl.stats[STAT_SECRETS]++;
			break;

		case svc_updatestat:
			i = MSG_ReadByte ();
			j = MSG_ReadByte ();
			CL_SetStat (i, j);
			break;
		case svc_updatestatlong:
			i = MSG_ReadByte ();
			j = MSG_ReadLong ();
			CL_SetStat (i, j);
			break;

		case svc_spawnstaticsound:
			CL_ParseStaticSound ();
			break;

		case svc_cdtrack:
			/* FS: Stream from WAV or OGG if we can */
			cl.cdtrack = MSG_ReadByte ();
			CL_PlayBackgroundTrack(cl.cdtrack);
			break;

		case svc_intermission:
			cl.intermission = 1;
			cl.completed_time = realtime;
			vid.recalc_refdef = true;	// go to full screen
			for (i=0 ; i<3 ; i++)
				cl.simorg[i] = MSG_ReadCoord ();
			for (i=0 ; i<3 ; i++)
				cl.simangles[i] = MSG_ReadAngle ();
			VectorCopy (vec3_origin, cl.simvel);
			break;

		case svc_finale:
			cl.intermission = 2;
			cl.completed_time = realtime;
			vid.recalc_refdef = true;	// go to full screen
			//johnfitz -- log centerprints to console
			s = MSG_ReadString ();
			SCR_CenterPrint (s);
			Con_LogCenterPrint (s);
			//johnfitz

			break;

		case svc_sellscreen:
			Cmd_ExecuteString ("help", src_client);
			break;

		case svc_smallkick:
			cl.punchangle = -2;
			break;
		case svc_bigkick:
			cl.punchangle = -4;
			break;

		case svc_muzzleflash:
			CL_MuzzleFlash ();
			break;

		case svc_updateuserinfo:
			CL_UpdateUserinfo ();
			break;

		case svc_setinfo:
			CL_SetInfo ();
			break;

		case svc_serverinfo:
			CL_ServerInfo ();
			break;

		case svc_download:
			CL_ParseDownload ();
			break;

		case svc_playerinfo:
			CL_ParsePlayerinfo ();
			break;

		case svc_nails:
			CL_ParseProjectiles ();
			break;

		case svc_chokecount:		// some preceding packets were choked
			i = MSG_ReadByte ();
			for (j=0 ; j<i ; j++)
				cl.frames[ (cls.netchan.incoming_acknowledged-1-j)&UPDATE_MASK ].receivedtime = -2;
			break;

		case svc_modellist:
			CL_ParseModellist (false);
			break;

#if defined (PROTOCOL_VERSION_FTE) && defined (FTE_PEXT_MODELDBL)
		case svc_fte_modellistshort:
		{
			if (cls.fteprotocolextensions & FTE_PEXT_MODELDBL)
				CL_ParseModellist(true);
			else
				Host_Error("CL_ParseServerMessage: svc_fte_modellistshort without FTE_PEXT_MODELDBL");
			break;
		}
#endif  // PROTOCOL_VERSION_FTE

		case svc_soundlist:
			CL_ParseSoundlist ();
			break;

		case svc_packetentities:
			CL_ParsePacketEntities (false);
			break;

		case svc_deltapacketentities:
			CL_ParsePacketEntities (true);
			break;

		case svc_maxspeed :
			movevars.maxspeed = MSG_ReadFloat();
			break;

		case svc_entgravity :
			movevars.entgravity = MSG_ReadFloat();
			break;

		case svc_setpause:
			cl.paused = MSG_ReadByte ();
			if (cl.paused)
				CDAudio_Pause ();
			else
				CDAudio_Resume ();
			break;

		}
	}

	CL_SetSolidEntities ();
}

qboolean CL_MaliciousStuffText(char *stufftext) /* FS: Check for malicious stufftext */
{
	if (Q_StrIsNullOrEmpty(stufftext))
		return false;

	if ((Q_strncasecmp(stufftext, "fov ", 4) == 0) ||
		(Q_strncasecmp(stufftext, "_snd_mixahead ", 14) == 0) ||
		(Q_strncasecmp(stufftext, "rate ", 5) == 0) ||
		(Q_strncasecmp(stufftext, "r_restart", 9) == 0)
		) /* FS: Ignore malicious stufftexts */
	{
		Com_DPrintf (DEVELOPER_MSG_NET, "Ignoring malicious stufftext: %s\n", stufftext);
		return true;
	}

	return false;
}

/*
=================
CL_PlayBackgroundTrack
=================
*/
void CL_PlayBackgroundTrack (int track)
{
	char	name[MAX_QPATH], *p;
	int	have_extmusic;

	Com_DPrintf(DEVELOPER_MSG_CD, "CL_PlayBackgroundTrack\n");

	S_StopBackgroundTrack();

	if (track == 0)
	{	// Stop any playing track
		Com_DPrintf(DEVELOPER_MSG_CD, "CL_PlayBackgroundTrack: stopping\n");
		return;
	}

	/* If an external music file exists play it, otherwise fall back to CD audio */
	have_extmusic = 0;
	Com_sprintf (name, sizeof(name), "music/track%02i.", track);

	p = name + strlen(name);

	Q_strlcpy (p, "wav", sizeof(name));
	if (COM_FileExists(name))
	{
		have_extmusic |= BGMUSIC_WAV;
	}

	Q_strlcpy (p, "flac", sizeof(name));
	if (COM_FileExists(name))
	{
		have_extmusic |= BGMUSIC_FLAC;
	}

	Q_strlcpy (p, "mp3", sizeof(name));
	if (COM_FileExists(name))
	{
		have_extmusic |= BGMUSIC_MP3;
	}

#ifdef OGG_SUPPORT
	Q_strlcpy (p, "ogg", sizeof(name));
	if (COM_FileExists(name))
	{
		have_extmusic |= BGMUSIC_OGG;
	}
#endif

	/* play whatever is found */
	if (have_extmusic & BGMUSIC_WAV) {
		CDAudio_Stop();
		Q_strlcpy (p, "wav", sizeof(name));
		S_StartStreamBackgroundTrack(name);
	}
	else if (have_extmusic & BGMUSIC_FLAC) {
		CDAudio_Stop();
		Q_strlcpy (p, "flac", sizeof(name));
		S_StartStreamBackgroundTrack(name);
	}
	else if (have_extmusic & BGMUSIC_MP3) {
		CDAudio_Stop();
		Q_strlcpy (p, "mp3", sizeof(name));
		S_StartStreamBackgroundTrack(name);
	}
#ifdef OGG_SUPPORT
	else if (have_extmusic&BGMUSIC_OGG) {
		CDAudio_Stop();
		Q_strlcpy (p, "ogg", sizeof(name));
		S_StartOGGBackgroundTrack(name);
	}
#endif
	else
	{
		CDAudio_Play((byte)track, true);
	}
}
// end Knightmare
