/*
Copyright (C) 1997-2001 Id Software, Inc.

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

// snd_stream.c -- Ogg Vorbis stuff


#include "quakedef.h"

#ifdef OGG_SUPPORT

#define OV_EXCLUDE_STATIC_CALLBACKS
#if defined(VORBIS_USE_TREMOR)
/* for Tremor / Vorbisfile api differences,
 * see doc/diff.html in the Tremor package. */
#include <tremor/ivorbisfile.h>
#else
#include <vorbis/vorbisfile.h>
#endif

/* Vorbis codec can return the samples in a number of different
 * formats, we use the standard signed short format. */
#define VORBIS_SAMPLEBITS 16
#define VORBIS_SAMPLEWIDTH 2
#define VORBIS_SIGNED_DATA 1

static bgTrack_t	s_bgTrack;

static qboolean	ogg_first_init = true;	// First initialization flag
static qboolean	ogg_started = false;	// Initialization flag
static bgm_status_t	trk_status;		// Status indicator

static void S_OGG_ParseCmd (void);

static void S_OGGRawSamples (int samples, int rate, int width, int channels, byte *data, qboolean music);


/*
=======================================================================

OGG VORBIS STREAMING

=======================================================================
*/

static size_t ovc_read (void *ptr, size_t size, size_t nmemb, void *datasource)
{
	bgTrack_t	*track = (bgTrack_t *)datasource;

	if (!size || !nmemb)
		return 0;
	return fread(ptr, 1, size * nmemb, track->file) / size;
}

static int ovc_seek (void *datasource, ogg_int64_t offset, int whence)
{
	bgTrack_t	*track = (bgTrack_t *)datasource;

	switch (whence)
	{
	case SEEK_SET:
	case SEEK_CUR:
	case SEEK_END:
		return fseek(track->file, (long)offset, whence);
	}

	return -1;
}

static int ovc_close (void *datasource)
{
	return 0;
}

static long ovc_tell (void *datasource)
{
	bgTrack_t	*track = (bgTrack_t *)datasource;
	return ftell(track->file);
}


/*
=================
S_OpenBackgroundTrack
=================
*/
static qboolean S_OpenBackgroundTrack (char *name, bgTrack_t *track)
{
	OggVorbis_File	*vorbisFile;
	vorbis_info		*vorbisInfo;
	ov_callbacks	vorbisCallbacks = {ovc_read, ovc_seek, ovc_close, ovc_tell};
	char	filename[1024];
	char	*path = NULL;

//	Com_Printf("Opening background track: %s\n", name);
	do {
		path = COM_NextPath( path );
		Com_sprintf( filename, sizeof(filename), "%s/%s", path, name );
		if ( (track->file = fopen(filename, "rb")) != 0)
			break;
	} while ( path );

	if (!track->file)
	{
		Com_Printf("S_OpenBackgroundTrack: couldn't find %s\n", name);
		return false;
	}

	track->vorbisFile = vorbisFile = malloc(sizeof(OggVorbis_File));

//	Com_Printf("Opening callbacks for background track\n");
	if (ov_open_callbacks(track, vorbisFile, NULL, 0, vorbisCallbacks) < 0)
	{
		Com_Printf("S_OpenBackgroundTrack: couldn't open OGG stream (%s)\n", name);
		return false;
	}

//	Com_Printf("Getting info for background track\n");
	vorbisInfo = ov_info(vorbisFile, -1);
	if (vorbisInfo->channels != 1 && vorbisInfo->channels != 2)
	{
		Com_Printf("S_OpenBackgroundTrack: only mono and stereo OGG files supported (%s)\n", name);
		return false;
	}

	track->start = ov_raw_tell(vorbisFile);
	track->rate = vorbisInfo->rate;
	track->width = 2;
	track->channels = vorbisInfo->channels; // Knightmare added

//	Com_Printf("Vorbis info: frequency: %i channels: %i bitrate: %i\n",
//		vorbisInfo->rate, vorbisInfo->channels, vorbisInfo->bitrate_nominal);

	return true;
}


/*
=================
S_CloseBackgroundTrack
=================
*/
static void S_CloseBackgroundTrack (bgTrack_t *track)
{
	if (track->vorbisFile)
	{
		ov_clear(track->vorbisFile);
		free(track->vorbisFile);
		track->vorbisFile = NULL;
	}

	if (track->file)
	{
		fclose(track->file);
		track->file = NULL;
	}
}

/*
============
S_StreamBackgroundTrack
============
*/
void S_StreamBackgroundTrack (void)
{
	int		samples, maxSamples;
	int		read, maxRead, total, dummy;
	float	scale;

	if (!s_bgTrack.file || !s_musicvolume->value || !s_mastervolume->value)
		return;

	if (s_rawend < paintedtime)
		s_rawend = paintedtime;

	scale = (float)s_bgTrack.rate / dma.speed;
	maxSamples = (sizeof(byte) * s_rawsamples_size) / s_bgTrack.channels / s_bgTrack.width;

	while (1)
	{
		samples = (paintedtime + s_rawsamples_size - s_rawend) * scale;
		if (samples <= 0)
			return;
		if (samples > maxSamples)
			samples = maxSamples;
		maxRead = samples * s_bgTrack.channels * s_bgTrack.width;

		total = 0;
		while (total < maxRead)
		{
			/* # ov_read() from libvorbisfile returns the decoded PCM audio
			 *   in requested endianness, signedness and word size.
			 * # ov_read() from Tremor (libvorbisidec) returns decoded audio
			 *   always in host-endian, signed 16 bit PCM format.
			 * # For both of the libraries, if the audio is multichannel,
			 *   the channels are interleaved in the output buffer.
			 */
			read = ov_read(s_bgTrack.vorbisFile, (char *)(s_streamDataPtr + total), maxRead - total,
#if !defined(VORBIS_USE_TREMOR)
											bigendien,
											VORBIS_SAMPLEWIDTH,
											VORBIS_SIGNED_DATA,
#endif /* ! VORBIS_USE_TREMOR */
											&dummy);
			if (!read)
			{	// End of file
				if (!s_bgTrack.looping)
				{
					S_CloseBackgroundTrack(&s_bgTrack);
					return;
				}

				// Restart the track, skipping over the header
				ov_raw_seek(s_bgTrack.vorbisFile, (ogg_int64_t)s_bgTrack.start);
			}

			total += read;
		}
		S_OGGRawSamples (samples, s_bgTrack.rate, s_bgTrack.width, s_bgTrack.channels, s_streamDataPtr, true);
	}
}

/*
============
S_UpdateBackgroundTrack

Streams background track
============
*/
void S_OGGUpdateBackgroundTrack (void)
{
	// stop music if paused
	if (trk_status == BGM_PLAY)// && !cl_paused->intValue)
		S_StreamBackgroundTrack ();
}

// =====================================================================

/*
=================
S_StartBackgroundTrack
=================
*/
void S_StartOGGBackgroundTrack (const char *name)
{
	if (!ogg_started) // was sound_started
		return;

	if (!name)
		return;

	// Stop any playing tracks
	S_StopBackgroundTrack();

	// Start it up
	Q_strlcpy(s_bgTrack.name, name, sizeof(s_bgTrack.name));

	// Open the track
	if (!S_OpenBackgroundTrack(s_bgTrack.name, &s_bgTrack))
	{
		S_StopBackgroundTrack();
		return;
	}

	trk_status = BGM_PLAY;
	s_bgTrack.looping = true;

	S_StreamBackgroundTrack();
}

/*
=================
S_StopOGGBackgroundTrack
=================
*/
/* FS: Called from S_StopBackgroundTrack in snd_dma.c */
void S_StopOGGBackgroundTrack (void)
{
	if (!ogg_started)
		return;

	S_CloseBackgroundTrack(&s_bgTrack);

	trk_status = BGM_STOP;

	memset(&s_bgTrack, 0, sizeof(bgTrack_t));
}

// =====================================================================

/*
==========
S_OGG_Init

Initialize the Ogg Vorbis subsystem
Based on code by QuDos
==========
*/
void S_OGG_Init (void)
{
	if (ogg_started)
		return;

	Com_Printf("OGG Vorbis Engine Initialized\n");

	// Console commands
	Cmd_AddCommand("ogg", S_OGG_ParseCmd);

	// Initialize variables
	if (ogg_first_init) {
		trk_status = BGM_STOP;
		ogg_first_init = false;
	}

	ogg_started = true;
}

/*
==========
S_OGG_Shutdown

Shutdown the Ogg Vorbis subsystem
Based on code by QuDos
==========
*/
void S_OGG_Shutdown (void)
{
	if (!ogg_started)
		return;

	Cmd_RemoveCommand("ogg");

	S_StopBackgroundTrack ();

	ogg_started = false;
}

/*
==========
S_OGG_Restart

Reinitialize the Ogg Vorbis subsystem
Based on code by QuDos
==========
*/
void S_OGG_Restart (void)
{
	S_OGG_Shutdown ();
	S_OGG_Init ();
}

// =====================================================================

/*
=================
S_OGG_PlayCmd
Based on code by QuDos
=================
*/
static void S_OGG_PlayCmd (void)
{
	char	name[MAX_QPATH];

	if (Cmd_Argc() < 3) {
		Com_Printf("Usage: ogg play {track}\n");
		return;
	}
	Com_sprintf(name, sizeof(name), "music/%s.ogg", Cmd_Argv(2) );
	S_StartOGGBackgroundTrack (name);
}

/*
=================
S_OGG_StatusCmd
Based on code by QuDos
=================
*/
static void S_OGG_StatusCmd (void)
{
	const char	*trackName;

	trackName = s_bgTrack.name;

	switch (trk_status) {
	case BGM_PLAY:
#if !defined(VORBIS_USE_TREMOR)
		Com_Printf("Playing file %s at %0.2f seconds.\n",
		    trackName, ov_time_tell(s_bgTrack.vorbisFile));
#else
		Com_Printf("Playing file %s at %0.2f seconds.\n",
		    trackName, ov_time_tell(s_bgTrack.vorbisFile)/1000.0);
#endif
		break;
	case BGM_PAUSE:
#if !defined(VORBIS_USE_TREMOR)
		Com_Printf("Paused file %s at %0.2f seconds.\n",
		    trackName, ov_time_tell(s_bgTrack.vorbisFile));
#else
		Com_Printf("Paused file %s at %0.2f seconds.\n",
		    trackName, ov_time_tell(s_bgTrack.vorbisFile)/1000.0);
#endif
		break;
	case BGM_STOP:
		Com_Printf("Stopped.\n");
		break;
	}
}

/*
=================
S_OGG_ParseCmd

Parses OGG commands
Based on code by QuDos
=================
*/
static void S_OGG_ParseCmd (void)
{
	char	*command;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: ogg {play | pause | resume | stop | status | list}\n");
		return;
	}

	command = Cmd_Argv (1);

	if (Q_strcasecmp(command, "play") == 0) {
		S_OGG_PlayCmd ();
		return;
	}

	if (Q_strcasecmp(command, "pause") == 0) {
		if (trk_status == BGM_PLAY)
			trk_status = BGM_PAUSE;
		return;
	}

	if (Q_strcasecmp(command, "resume") == 0) {
		if (trk_status == BGM_PAUSE)
			trk_status = BGM_PLAY;
		return;
	}

	if (Q_strcasecmp(command, "stop") == 0) {
		S_StopBackgroundTrack ();
		return;
	}

	if (Q_strcasecmp(command, "status") == 0) {
		S_OGG_StatusCmd ();
		return;
	}

	Com_Printf("Usage: ogg {play | pause | resume | stop | status}\n");
}

void S_PauseOGGBackgroundTrack (void)
{
	if (trk_status == BGM_PLAY)
		trk_status = BGM_PAUSE;
}

void S_ResumeOGGBackgroundTrack (void)
{
	if (trk_status == BGM_PAUSE)
		trk_status = BGM_PLAY;
}

/*
============
S_OGGRawSamples

Cinematic streaming and voice over network
Streaming music support. Byte swapping
of data must be handled by the codec.
Expects data in signed 16 bit, or unsigned
8 bit format.
============
*/
static void S_OGGRawSamples (int samples, int rate, int width, int channels, byte *data, qboolean music)
{
	int i;
	int src, dst;
	float scale;
	int intVolume;

	if (!sound_started)
		return;

	if (s_rawend < paintedtime)
		s_rawend = paintedtime;

	scale = (float) rate / dma.speed;
	if (music)
		intVolume = (int) ((s_musicvolume->value * s_mastervolume->value) * 256);
	else
		intVolume = (int) ((s_volume->value * s_mastervolume->value) * 256);

	if (channels == 2 && width == 2)
	{
		for (i = 0; ; i++)
		{
			src = i * scale;
			if (src >= samples)
				break;
			dst = s_rawend & (s_rawsamples_size - 1);
			s_rawend++;
			s_rawsamples [dst].left = ((short *) data)[src * 2] * intVolume;
			s_rawsamples [dst].right = ((short *) data)[src * 2 + 1] * intVolume;
		}
	}
	else if (channels == 1 && width == 2)
	{
		for (i = 0; ; i++)
		{
			src = i * scale;
			if (src >= samples)
				break;
			dst = s_rawend & (s_rawsamples_size - 1);
			s_rawend++;
			s_rawsamples [dst].left = ((short *) data)[src] * intVolume;
			s_rawsamples [dst].right = ((short *) data)[src] * intVolume;
		}
	}
	else if (channels == 2 && width == 1)
	{
		intVolume *= 256;

		for (i = 0; ; i++)
		{
			src = i * scale;
			if (src >= samples)
				break;
			dst = s_rawend & (s_rawsamples_size - 1);
			s_rawend++;
		//	s_rawsamples [dst].left = ((signed char *) data)[src * 2] * intVolume;
		//	s_rawsamples [dst].right = ((signed char *) data)[src * 2 + 1] * intVolume;
			s_rawsamples [dst].left = (((byte *) data)[src * 2] - 128) * intVolume;
			s_rawsamples [dst].right = (((byte *) data)[src * 2 + 1] - 128) * intVolume;
		}
	}
	else if (channels == 1 && width == 1)
	{
		intVolume *= 256;

		for (i = 0; ; i++)
		{
			src = i * scale;
			if (src >= samples)
				break;
			dst = s_rawend & (s_rawsamples_size - 1);
			s_rawend++;
		//	s_rawsamples [dst].left = ((signed char *) data)[src] * intVolume;
		//	s_rawsamples [dst].right = ((signed char *) data)[src] * intVolume;
			s_rawsamples [dst].left = (((byte *) data)[src] - 128) * intVolume;
			s_rawsamples [dst].right = (((byte *) data)[src] - 128) * intVolume;
		}
	}
}

#endif /* OGG_SUPPORT */
