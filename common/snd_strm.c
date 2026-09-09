/*
Copyright 2026 Frank Sapone

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
S_StreamRawSamples function is GPL code re-adapated from Q2E OGG streaming S_RawSamples function.
----------------------------------------------------------------------
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

// snd_strm.c

#include "quakedef.h"

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"

#define DR_FLAC_IMPLEMENTATION
#define DR_FLAC_NO_CRC
#include "dr_flac.h"

stream_t stream_channels[MAX_STREAMS];
static qboolean bMixed = false;

void S_Stream_f (void);
static void S_StreamRawSamples (const stream_t *stream, int samples, unsigned int rate, int width, unsigned int channels, const strm_int16_t *data, qboolean music);

void S_StreamInit (void)
{
	if (!snd_initialized)
		return;

	Com_Printf("Stream engine initialized.  Available formats:\n");
	Com_Printf(" * dr_flac v%s\n", drflac_version_string());
	Com_Printf(" * dr_mp3 v%s\n", drmp3_version_string());
	Com_Printf(" * dr_wav v%s\n", drwav_version_string());

	Cmd_AddCommand("stream", S_Stream_f);
}

void S_StreamShutdown (void)
{
	if (!snd_initialized)
		return;

	S_StopBackgroundTrack ();

	Cmd_RemoveCommand("stream");
}

void S_StreamRestart (void)
{
	S_StreamShutdown();
	S_StreamInit();
}

void S_StreamUpdate (void)
{
	int		i;
	int		numsamples, maxSamples;
	int		read, maxRead, total;
	float	scale;
	strm_int16_t	data[SND_BUFFER_SIZE];
	stream_t	*stream;

	/* FS: Save this stuff so we can mix in additional channels. */
	int prev_rawend = s_rawend;
	int prev_paintedtime = paintedtime;
	int after_rawend = s_rawend;
	int after_paintedtime = paintedtime;

	bMixed = false;

	for (i = 0; i < MAX_STREAMS; i++)
	{
		if (s_rawend < paintedtime)
			s_rawend = paintedtime;

		stream = &stream_channels[i];
		if (!stream->active || !stream->handle)
			continue;

		scale = (float)stream->handle->sampleRate / dma.speed;
		maxSamples = sizeof(data) / stream->handle->channels / 2; /* FS: DRMP3 uses signed 16-bit so width is 2. */

		while (1)
		{
			read = 0;
			numsamples = (paintedtime + s_rawsamples_size - s_rawend) * scale;
			if (numsamples <= 0)
				break;
			if (numsamples > maxSamples)
				numsamples = maxSamples;

			maxRead = numsamples;

			total = 0;
			while (total < maxRead)
			{
				switch (stream->handle->type)
				{
					case STREAM_WAV:
						read = drwav_read_pcm_frames_s16(stream->handle->drwav, maxRead - total, data);
						break;
					case STREAM_FLAC:
						read = drflac_read_pcm_frames_s16(stream->handle->drflac, maxRead - total, data);
						break;
					case STREAM_MP3:
						read = drmp3_read_pcm_frames_s16(stream->handle->drmp3, maxRead - total, data);
						break;
					default:
						Sys_Error("Unknown stream type!");
						break;
				}

				if (!read)
				{	// End of file
					if (!stream->looping)
					{	
						S_Destroy_Stream(stream);
						break;
					}
					else
					{
						switch (stream->handle->type)
						{
							case STREAM_WAV:
								drwav_seek_to_pcm_frame(stream->handle->drwav, 0);
								break;
							case STREAM_FLAC:
								drflac_seek_to_pcm_frame(stream->handle->drflac, 0);
								break;
							case STREAM_MP3:
								drmp3_seek_to_pcm_frame(stream->handle->drmp3, 0);
								break;
							default:
								Sys_Error("Unknown stream type!");
								break;
						}
					}
				}

				total += read;
			}

			if (read)
				S_StreamRawSamples (stream, numsamples, stream->handle->sampleRate, s_loadas8bit->intValue ? 1 : 2, stream->handle->channels, data,  true);
			else
				break;
		}

		after_paintedtime = paintedtime > after_paintedtime ? paintedtime : after_paintedtime;
		after_rawend = s_rawend > after_rawend ? s_rawend : after_rawend;
		s_rawend = prev_rawend;
		paintedtime = prev_paintedtime;
	}

	s_rawend = after_rawend;
	paintedtime = after_paintedtime;
}

/* FS: re-adapated from Q2E OGG streaming S_RawSamples function. */
static void S_StreamRawSamples (const stream_t *stream, int samples, unsigned int rate, int width, unsigned int channels, const strm_int16_t *data, qboolean music)
{
	int i;
	int src, dst;
	float scale;
	int intVolumeL, intVolumeR;

	if (!sound_started)
		return;

	if (s_rawend < paintedtime)
		s_rawend = paintedtime;

	if (music)
	{
		intVolumeL = (int)((stream->volume * (s_musicvolume->value * s_mastervolume->value)) * 256);
		intVolumeR = (int)((stream->volume * (s_musicvolume->value * s_mastervolume->value)) * 256);
	}
	else
	{
		if (stream->is3D)
		{
			intVolumeL = stream->leftvol;
			intVolumeR = stream->rightvol;
		}
		else
		{
			intVolumeL = (int)((stream->volume * (s_musicvolume->value * s_mastervolume->value)) * 256);
			intVolumeR = (int)((stream->volume * (s_musicvolume->value * s_mastervolume->value)) * 256);
		}
	}

	scale = (float)rate / dma.speed;

	if (channels == 2 && width == 2)
	{
		if (bMixed) /* FS: Moved this out of for loop.  Don't need to check this every iteration.  Comes at a cost of code dupe. */
		{
			for (i = 0; ; i++)
			{
				src = i * scale;
				if (src >= samples)
					break;
				dst = s_rawend & (s_rawsamples_size - 1);
				s_rawend++;

				s_rawsamples[dst].left += (data[src * 2] * intVolumeL);
				s_rawsamples[dst].right += (data[src * 2 + 1] * intVolumeR);
			}
		}
		else
		{
			for (i = 0; ; i++)
			{
				src = i * scale;
				if (src >= samples)
					break;
				dst = s_rawend & (s_rawsamples_size - 1);
				s_rawend++;

				s_rawsamples[dst].left = (data[src * 2] * intVolumeL);
				s_rawsamples[dst].right = (data[src * 2 + 1] * intVolumeR);
			}
		}

		bMixed = true;
	}
	else if (channels == 1 && width == 2)
	{
		if (bMixed)
		{
			for (i = 0; ; i++)
			{
				src = i * scale;
				if (src >= samples)
					break;
				dst = s_rawend & (s_rawsamples_size - 1);
				s_rawend++;

				s_rawsamples[dst].left += (data[src] * intVolumeL);
				s_rawsamples[dst].right += (data[src] * intVolumeR);
			}
		}
		else
		{
			for (i = 0; ; i++)
			{
				src = i * scale;
				if (src >= samples)
					break;
				dst = s_rawend & (s_rawsamples_size - 1);
				s_rawend++;

				s_rawsamples[dst].left = (data[src] * intVolumeL);
				s_rawsamples[dst].right = (data[src] * intVolumeR);
			}
		}

		bMixed = true;
	}
	else if (channels == 2 && width == 1)
	{
		intVolumeL *= 256;
		intVolumeR *= 256;

		if (bMixed)
		{
			for (i = 0; ; i++)
			{
				src = i * scale;
				if (src >= samples)
					break;
				dst = s_rawend & (s_rawsamples_size - 1);
				s_rawend++;

				s_rawsamples[dst].left += ((data[src * 2] - 128) >> 8) * intVolumeL;
				s_rawsamples[dst].right += ((data[src * 2 + 1] - 128) >> 8) * intVolumeR;

			}
		}
		else
		{
			for (i = 0; ; i++)
			{
				src = i * scale;
				if (src >= samples)
					break;
				dst = s_rawend & (s_rawsamples_size - 1);
				s_rawend++;

				s_rawsamples[dst].left = ((data[src * 2] - 128) >> 8) * intVolumeL;
				s_rawsamples[dst].right = ((data[src * 2 + 1] - 128) >> 8) * intVolumeR;
			}
		}

		bMixed = true;
	}
	else if (channels == 1 && width == 1)
	{
		intVolumeL *= 256;
		intVolumeR *= 256;

		if (bMixed)
		{
			for (i = 0; ; i++)
			{
				src = i * scale;
				if (src >= samples)
					break;
				dst = s_rawend & (s_rawsamples_size - 1);
				s_rawend++;

				s_rawsamples[dst].left += ((data[src] - 128) >> 8) * intVolumeL;
				s_rawsamples[dst].right += ((data[src] - 128) >> 8) * intVolumeR;

			}
		}
		else
		{
			for (i = 0; ; i++)
			{
				src = i * scale;
				if (src >= samples)
					break;
				dst = s_rawend & (s_rawsamples_size - 1);
				s_rawend++;

				s_rawsamples[dst].left = ((data[src] - 128) >> 8) * intVolumeL;
				s_rawsamples[dst].right = ((data[src] - 128) >> 8) * intVolumeR;
			}
		}

		bMixed = true;
	}
	else
	{
		Com_Printf("WARNING: Unknown raw sample type.  Channels: %d.  Width: %d\n", channels, width);
	}
}

hSTREAM *S_Open_Stream(const char *path)
{
	hSTREAM *ptr;

	if (!snd_initialized || !sound_started)
		return NULL;

	if (Q_StrIsNullOrEmpty(path))
		return NULL;

	if (!stricmp(COM_FileExtension(path), "wav"))
	{
		drwav *wav;
		drwav_uint64 pcmFrameCount = 0;

		wav = calloc(1, sizeof(drwav));
		if (!wav)
		{
			Sys_Error("S_Open_Stream: out of memory");
			return NULL;
		}

		if (!drwav_init_file(wav, path, NULL))
		{
			drwav_uninit(wav);
			return NULL;
		}

		ptr = calloc(1, sizeof(hSTREAM));
		if (!ptr)
		{
			Sys_Error("S_Open_Stream: out of memory");
			return NULL;
		}
		drwav_get_length_in_pcm_frames(wav, &pcmFrameCount);
		ptr->totallen = pcmFrameCount;
		ptr->datarate = wav->sampleRate;
		ptr->drwav = wav;
		ptr->channels = wav->channels;
		ptr->sampleRate = wav->sampleRate;
		ptr->type = STREAM_WAV;
	}
	else if (!stricmp(COM_FileExtension(path), "flac"))
	{
		drflac *flac;

		flac = drflac_open_file(path, NULL);
		if (!flac)
		{
			drflac_close(flac);
			return NULL;
		}

		ptr = calloc(1, sizeof(hSTREAM));
		if (!ptr)
		{
			Sys_Error("S_Open_Stream: out of memory");
			return NULL;
		}
		ptr->datarate = flac->sampleRate;
		ptr->drflac = flac;
		ptr->channels = flac->channels;
		ptr->sampleRate = flac->sampleRate;
		ptr->type = STREAM_FLAC;
	}
	else if (!stricmp(COM_FileExtension(path), "mp3"))
	{
		drmp3 *mp3;
		drmp3_uint64 mp3FrameCount = 0;
		drmp3_uint64 pcmFrameCount = 0;

		mp3 = calloc(1, sizeof(drmp3));
		if (!mp3)
		{
			Sys_Error("S_Open_Stream: out of memory");
			return NULL;
		}
		if (!drmp3_init_file(mp3, path, NULL))
		{
			drmp3_uninit(mp3);
			return NULL;
		}

		ptr = calloc(1, sizeof(hSTREAM));
		if (!ptr)
		{
			Sys_Error("S_Open_Stream: out of memory");
			return NULL;
		}
		drmp3_get_mp3_and_pcm_frame_count(mp3, &mp3FrameCount, &pcmFrameCount);
		ptr->totallen = pcmFrameCount;
		ptr->datarate = mp3->sampleRate;
		ptr->drmp3 = mp3;
		ptr->channels = mp3->channels;
		ptr->sampleRate = mp3->sampleRate;
		ptr->type = STREAM_MP3;
	}
#ifdef OGG_SUPPORT
	else if (!stricmp(COM_FileExtension(path), "ogg"))
	{
		Com_Printf("Use the 'ogg play' command for OGG files\n");
		return NULL;
	}
#endif
	else
	{
		Com_Printf("S_Open_Stream: unsupported format '%s'\n", COM_FileExtension(path));
		return NULL;
	}

	return ptr;
}

void S_Destroy_Stream (stream_t *stream)
{
	if (!snd_initialized || !sound_started)
		return;

	if (!stream)
		return;

	if (stream->handle)
	{
		if (stream->handle->drmp3)
		{
			drmp3_uninit(stream->handle->drmp3);
			free(stream->handle->drmp3);
		}
		stream->handle->drmp3 = NULL;

		if (stream->handle->drwav)
		{
			drwav_uninit(stream->handle->drwav);
			free(stream->handle->drwav);
		}
		stream->handle->drwav = NULL;

		if (stream->handle->drflac)
		{
			drflac_close(stream->handle->drflac);
		}
		stream->handle->drflac = NULL;

		free(stream->handle);
	}

	stream->handle = NULL;
	stream->name[0] = '\0';
	stream->is3D = false;
	stream->volume = 0.0f;
	stream->leftvol = 0;
	stream->rightvol = 0;
	stream->active = false;
	stream->looping = false;
}

void S_StartStreamBackgroundTrack (const char *name)
{
	char	filename[MAX_OSPATH];
	char	*path = NULL;
	FILE	*f = NULL;
	stream_t *stream;

	if (!snd_initialized || !sound_started || !name)
		return;

	// Stop any playing tracks
	S_StopBackgroundTrack();

	do
	{
		path = COM_NextPath( path );
		Com_sprintf( filename, sizeof(filename), "%s/%s", path, name );
		if ((f = fopen(filename, "rb")) != NULL)
			break;
	} while ( path );

	if (!f)
	{
		Com_Printf("%s not found\n", name);
		return;
	}

	fclose(f);

	stream = &stream_channels[0];
	stream->handle = S_Open_Stream(filename);
	if (stream->handle)
	{
		Q_strlcpy(stream->name, name, sizeof(stream->name));
		stream->volume = 1.0f;
		stream->active = true;
		stream->looping = true;
	}
	else
	{
		Com_Printf("Failed to get a handle for %s\n", name);
	}
}

void S_StopStreamBackgroundTrack (void)
{
	stream_t *stream;

	stream = &stream_channels[0];
	S_Destroy_Stream(stream);
}

void S_PauseStreamBackgroundTrack (void)
{
	stream_t *stream;

	stream = &stream_channels[0];
	stream->active = false;
}

void S_ResumeStreamBackgroundTrack (void)
{
	stream_t *stream;

	stream = &stream_channels[0];
	if (stream->handle)
		stream->active = true;
}

void S_StreamStatus (void)
{
	int i;
	stream_t	*stream;

	Com_Printf("Total stream channels available: %d\n", MAX_STREAMS);

	for (i = 0; i < MAX_STREAMS; i++)
	{
		stream = &stream_channels[i];
		if (stream->handle)
		{
			if (stream->active)
			{
				Com_Printf(" %d: Playing %s.  Looped: %d\n", i, stream->name, stream->looping);
			}
			else
			{
				Com_Printf(" %d: Paused %s.  Looped: %d\n", i, stream->name, stream->looping);
			}
		}
	}
}

void S_Stream_f (void)
{
	char	name[MAX_OSPATH];
	char	*command;

	if (!snd_initialized || !sound_started)
	{
		Com_Printf("Sound engine not started\n");
		return;
	}

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: stream {play | pause | resume | stop | status}\n");
		return;
	}

	command = Cmd_Argv (1);

	if (Q_strcasecmp(command, "play") == 0) {
		Com_sprintf(name, sizeof(name), "music/%s", Cmd_Argv(2));
		S_StartStreamBackgroundTrack(name);
		return;
	}

	if (Q_strcasecmp(command, "pause") == 0) {
		stream_channels[0].active = false;
		return;
	}

	if (Q_strcasecmp(command, "resume") == 0) {
		if (stream_channels[0].handle)
			stream_channels[0].active = true;
		return;
	}

	if (Q_strcasecmp(command, "stop") == 0) {
		S_Destroy_Stream(&stream_channels[0]);
		return;
	}

	if (Q_strcasecmp(command, "status") == 0) {
		S_StreamStatus ();
		return;
	}
}
