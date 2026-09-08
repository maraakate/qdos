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
// cl_main.c  -- client main loop

#include <ctype.h>
#include "quakedef.h"
#ifdef GAMESPY
#include "Goa/CEngine/goaceng.h" /* FS: For Gamespy */
#endif

// we need to declare some mouse variables here, because the menu system
// references them even when on a unix system.

// these two are not intended to be set directly
cvar_t	*cl_name;
cvar_t	*cl_color;

cvar_t	*cl_shownet; // can be 0, 1, or 2
cvar_t	*cl_nolerp;

cvar_t	*lookspring;
cvar_t	*lookstrafe;
cvar_t	*sensitivity;

cvar_t	*m_pitch;
cvar_t	*m_yaw;
cvar_t	*m_forward;
cvar_t	*m_side;

/* FS: New stuff */
cvar_t	*console_old_complete;
cvar_t	*cl_autorepeat_allkeys;
cvar_t	*cl_sleep;

client_static_t	cls;
client_state_t cl;
// FIXME: put these on hunk?
efrag_t			cl_efrags[MAX_EFRAGS];
entity_t		cl_static_entities[MAX_STATIC_ENTITIES];
lightstyle_t	cl_lightstyle[MAX_LIGHTSTYLES];
dlight_t		cl_dlights[MAX_DLIGHTS];

entity_t		*cl_entities; //johnfitz -- was a static array, now on hunk
int				cl_max_edicts; //johnfitz -- only changes when new map loads

int				cl_numvisedicts;
entity_t		*cl_visedicts[MAX_VISEDICTS];

/* FS: Flashlight */
qboolean bFlashlight;
void CL_Flashlight_f (void);

#ifdef GAMESPY
/* FS: Gamespy CVARs */
cvar_t	*cl_master_server_ip;
cvar_t	*cl_master_server_port;
cvar_t	*cl_master_server_queries;
cvar_t	*cl_master_server_timeout;
cvar_t	*cl_master_server_retries;
cvar_t	*cl_master_server_optout;
cvar_t	*snd_gamespy_sounds;

/* FS: Gamespy prototypes */
static	GServerList	serverlist = NULL;
static	int		gspyCur;
gamespyBrowser_t browserList[MAX_SERVERS]; /* FS: Browser list for active servers */
gamespyBrowser_t browserListAll[MAX_SERVERS]; /* FS: Browser list for ALL servers */
void GameSpy_Async_Think(void); /* FS: Not static in Q1 since it is called from host.c */
static void ListCallBack(GServerList serverlist, int msg, void *instance, void *param1, void *param2);
static void CL_Gspystop_f (void);
       void CL_PingNetServers_f (void);
static void CL_PrintBrowserList_f (void);
#endif

#ifdef __DJGPP__
void Sys_Memory_Stats_f (void); /* FS: Added */
#endif

extern void CL_PlayBackgroundTrack (int track);

/*
=====================
CL_ClearState

=====================
*/
void CL_ClearState (void)
{
	int i;

	if (!sv.active)
		Host_ClearMemory ();

// wipe the entire cl structure
	memset (&cl, 0, sizeof(cl));

	SZ_Clear (&cls.message);

// clear other arrays
	memset (cl_efrags, 0, sizeof(cl_efrags));
	memset (cl_dlights, 0, sizeof(cl_dlights));
	memset (cl_lightstyle, 0, sizeof(cl_lightstyle));
	memset (cl_temp_entities, 0, sizeof(cl_temp_entities));
	memset (cl_beams, 0, sizeof(cl_beams));

	//johnfitz -- cl_entities is now dynamically allocated
	cl_max_edicts = CLAMP (MIN_EDICTS,max_edicts->intValue,MAX_EDICTS);
	cl_entities = Hunk_AllocName (cl_max_edicts*sizeof(entity_t), "cl_entities");
	//johnfitz

//
// allocate the efrags and chain together into a free list
//
	cl.free_efrags = cl_efrags;
	for (i=0 ; i<MAX_EFRAGS-1 ; i++)
		cl.free_efrags[i].entnext = &cl.free_efrags[i+1];
	cl.free_efrags[i].entnext = NULL;
}

/*
=====================
CL_Disconnect

Sends a disconnect message to the server
This is also called on Host_Error, so it shouldn't cause any errors
=====================
*/
void CL_Disconnect (void)
{
// stop sounds (especially looping!)
	S_StopAllSounds ();
	
// bring the console down and fade the colors back to normal
// SCR_BringDownConsole ();

// if running a local server, shut it down
	if (cls.demoplayback)
		CL_StopPlayback ();
	else if (cls.state == ca_connected)
	{
		if (cls.demorecording)
			CL_Stop_f ();

		Com_DPrintf (DEVELOPER_MSG_NET, "Sending clc_disconnect\n");
		SZ_Clear (&cls.message);
		MSG_WriteByte (&cls.message, clc_disconnect);
		NET_SendUnreliableMessage (cls.netcon, &cls.message);
		SZ_Clear (&cls.message);
		NET_Close (cls.netcon);

		cls.state = ca_disconnected;
		if (sv.active)
			Host_ShutdownServer(false);
	}

	cls.demoplayback = cls.timedemo = false;
	cls.signon = 0;
	key_dest = 0; /* FS: Fix so main menu still works after disconnect */
	cl.intermission = 0; /* FS: Baker fix */

	/* FS: Clear these three for snd_restart. */
	cl.cdtrack = 0;
	cls.forcetrack = -1;
	cl.numsounds = 0;
}

void CL_Disconnect_f (void)
{
	if(cls.state > ca_disconnected) /* FS: Added message. */
		Com_Printf("*** Disconnected from server *** \n");

	CL_Disconnect();

	if (sv.active)
		Host_ShutdownServer (false);

}

/*
=====================
CL_EstablishConnection

Host should be either "local" or a net address to be passed on
=====================
*/
void CL_EstablishConnection (char *host)
{
	if (dedicated->intValue)
		return;

	if (cls.demoplayback)
		return;

	CL_Disconnect ();

	cls.netcon = NET_Connect (host);
	if (!cls.netcon)
		Host_Error ("CL_Connect: connect failed\n");
	Com_DPrintf (DEVELOPER_MSG_NET, "CL_EstablishConnection: connected to %s\n", host);
	
	cls.demonum = -1;		 // not in the demo loop now
	cls.state = ca_connected;
	cls.signon = 0;				// need all the signon messages before playing

	MSG_WriteByte (&cls.message, clc_nop); /* sezero: fixes connection getting stuck after displaying the "Connection accepted" msg.. */
}

/*
=====================
CL_SignonReply

An svc_signonnum has been received, perform a client side setup
=====================
*/
void CL_SignonReply (void)
{
	char  str[8192];

	Com_DPrintf (DEVELOPER_MSG_NET, "CL_SignonReply: %i\n", cls.signon);

	switch (cls.signon)
	{
	case 1:
		MSG_WriteByte (&cls.message, clc_stringcmd);
		MSG_WriteString (&cls.message, "prespawn");
		break;
		
	case 2:	  
		MSG_WriteByte (&cls.message, clc_stringcmd);
		MSG_WriteString (&cls.message, va("name \"%s\"\n", cl_name->string));
	
		MSG_WriteByte (&cls.message, clc_stringcmd);
		MSG_WriteString (&cls.message, va("color %i %i\n", (cl_color->intValue)>>4, (cl_color->intValue)&15));
	
		MSG_WriteByte (&cls.message, clc_stringcmd);
		Com_sprintf (str, sizeof(str), "spawn %s", cls.spawnparms);
		MSG_WriteString (&cls.message, str);
		break;
		
	case 3:  
		MSG_WriteByte (&cls.message, clc_stringcmd);
		MSG_WriteString (&cls.message, "begin");
		Cache_Report ();	  // print remaining memory
		break;
		
	case 4:
		SCR_EndLoadingPlaque ();		// allow normal screen updates
		break;
	}
}

/*
=====================
CL_NextDemo

Called to play the next demo in the demo loop
=====================
*/
void CL_NextDemo (void)
{
	char  str[1024];

	if (cls.demonum == -1)
		return;		// don't play demos

	if (!cls.demos[cls.demonum][0] || cls.demonum == MAX_DEMOS)
	{
		cls.demonum = 0;
		if (!cls.demos[cls.demonum][0])
		{
			if (!cl_demos->intValue || nostartupdemos) /* FS: Disable startup demos */
				Com_DPrintf(DEVELOPER_MSG_STANDARD, "Startup demos disabled.");
			else
				Com_Printf ("No demos listed with startdemos\n");

			cls.demonum = -1;
			CL_Disconnect();
			return;
		}
	}

	SCR_BeginLoadingPlaque ();

	Com_sprintf (str, sizeof(str), "playdemo %s\n", cls.demos[cls.demonum]);
	Cbuf_InsertText (str);
	cls.demonum++;
}

/*
==============
CL_PrintEntities_f
==============
*/
void CL_PrintEntities_f (void)
{
	entity_t *ent;
	int			i;
	
	for (i=0,ent=cl_entities ; i<cl.num_entities ; i++,ent++)
	{
		Com_Printf ("%3i:",i);
		if (!ent->model)
		{
			Com_Printf ("EMPTY\n");
			continue;
		}
		Com_Printf ("%s:%2i  (%5.1f,%5.1f,%5.1f) [%5.1f %5.1f %5.1f]\n"
		,ent->model->name,ent->frame, ent->origin[0], ent->origin[1], ent->origin[2], ent->angles[0], ent->angles[1], ent->angles[2]);
	}
}

/*
===============
CL_AllocDlight

===============
*/
dlight_t *CL_AllocDlight (int key)
{
	int		i;
	dlight_t *dl;

// first look for an exact key match
	if (key)
	{
		dl = cl_dlights;
		for (i=0 ; i<MAX_DLIGHTS ; i++, dl++)
		{
			if (dl->key == key)
			{
				memset (dl, 0, sizeof(*dl));
				dl->key = key;
				dl->color[0] = dl->color[1] = dl->color[2] = 1; //johnfitz -- lit support via lordhavoc
				return dl;
			}
		}
	}

// then look for anything else
	dl = cl_dlights;
	for (i=0 ; i<MAX_DLIGHTS ; i++, dl++)
	{
		if (dl->die < cl.time)
		{
			memset (dl, 0, sizeof(*dl));
			dl->key = key;
			dl->color[0] = dl->color[1] = dl->color[2] = 1; //johnfitz -- lit support via lordhavoc
			return dl;
		}
	}

	dl = &cl_dlights[0];
	memset (dl, 0, sizeof(*dl));
	dl->key = key;
	dl->color[0] = dl->color[1] = dl->color[2] = 1; //johnfitz -- lit support via lordhavoc
	return dl;
}


/*
===============
CL_DecayLights

===============
*/
void CL_DecayLights (void)
{
	int			i;
	dlight_t *dl;
	float	 time;
	
	time = cl.time - cl.oldtime;

	dl = cl_dlights;
	for (i=0 ; i<MAX_DLIGHTS ; i++, dl++)
	{
		if (dl->die < cl.time || !dl->radius)
			continue;
		
		dl->radius -= time*dl->decay;
		if (dl->radius < 0)
			dl->radius = 0;
	}
}


/*
===============
CL_LerpPoint

Determines the fraction between the last two messages that the objects
should be put at.
===============
*/
float CL_LerpPoint (void)
{
	float f, frac;

	f = cl.mtime[0] - cl.mtime[1];
	
	if (!f || cl_nolerp->intValue || cls.timedemo || sv.active)
	{
		cl.time = cl.mtime[0];
		return 1;
	}
		
	if (f > 0.1)
	{  // dropped packet, or start of demo
		cl.mtime[1] = cl.mtime[0] - 0.1;
		f = 0.1;
	}
	frac = (cl.time - cl.mtime[1]) / f;
//Com_Printf ("frac: %f\n",frac);
	if (frac < 0)
	{
		if (frac < -0.01)
		{
			cl.time = cl.mtime[1];
//			 Com_Printf ("low frac\n");
		}
		frac = 0;
	}
	else if (frac > 1)
	{
		if (frac > 1.01)
		{
			cl.time = cl.mtime[0];
//			 Com_Printf ("high frac\n");
		}
		frac = 1;
	}
	else
	{
	}
	return frac;
}


/*
===============
CL_RelinkEntities
===============
*/
void CL_RelinkEntities (void)
{
	entity_t		  *ent;
	int			i, j;
	float	 frac, f, d;
	vec3_t		delta;
	float	 bobjrotate;
	vec3_t		oldorg;
	dlight_t *dl;

// determine partial update time 
	frac = CL_LerpPoint ();

	cl_numvisedicts = 0;

//
// interpolate player info
//
	for (i=0 ; i<3 ; i++)
		cl.velocity[i] = cl.mvelocity[1][i] + 
			frac * (cl.mvelocity[0][i] - cl.mvelocity[1][i]);

	if(bFlashlight) /* FS: Flashlight */
		cl_entities[cl.viewentity].effects |= EF_BRIGHTLIGHT;

	if (cls.demoplayback)
	{
	// interpolate the angles  
		for (j=0 ; j<3 ; j++)
		{
			d = cl.mviewangles[0][j] - cl.mviewangles[1][j];
			if (d > 180)
				d -= 360;
			else if (d < -180)
				d += 360;
			cl.viewangles[j] = cl.mviewangles[1][j] + frac*d;
		}
	}
	
	bobjrotate = anglemod(100*cl.time);
	
// start on the entity after the world
	for (i=1,ent=cl_entities+1 ; i<cl.num_entities ; i++,ent++)
	{
		if (!ent->model)
		{  // empty slot
			if (ent->forcelink)
				R_RemoveEfrags (ent);	// just became empty
			continue;
		}

// if the object wasn't included in the last packet, remove it
		if (ent->msgtime != cl.mtime[0])
		{
			ent->model = NULL;
			continue;
		}

		VectorCopy (ent->origin, oldorg);

		if (ent->forcelink)
		{  // the entity was not updated in the last message
			// so move to the final spot
			VectorCopy (ent->msg_origins[0], ent->origin);
			VectorCopy (ent->msg_angles[0], ent->angles);
		}
		else
		{  // if the delta is large, assume a teleport and don't lerp
			f = frac;
			for (j=0 ; j<3 ; j++)
			{
				delta[j] = ent->msg_origins[0][j] - ent->msg_origins[1][j];
				if (delta[j] > 100 || delta[j] < -100)
					f = 1;		// assume a teleportation, not a motion
			}

		// interpolate the origin and angles
			for (j=0 ; j<3 ; j++)
			{
				ent->origin[j] = ent->msg_origins[1][j] + f*delta[j];

				d = ent->msg_angles[0][j] - ent->msg_angles[1][j];
				if (d > 180)
					d -= 360;
				else if (d < -180)
					d += 360;
				ent->angles[j] = ent->msg_angles[1][j] + f*d;
			}
			
		}

// rotate binary objects locally
		if (ent->model->flags & EF_ROTATE)
			ent->angles[1] = bobjrotate;

		if (ent->effects & EF_BRIGHTFIELD)
			R_EntityParticles (ent);

		if (ent->effects & EF_MUZZLEFLASH)
		{
			vec3_t		fv, rv, uv;

			dl = CL_AllocDlight (i);
			VectorCopy (ent->origin,  dl->origin);
			dl->origin[2] += 16;
			AngleVectors (ent->angles, fv, rv, uv);
			 
			VectorMA (dl->origin, 18, fv, dl->origin);
			dl->radius = 200 + (rand()&31);
			dl->minlight = 32;
			dl->die = cl.time + 0.1;
		}
		if (ent->effects & EF_BRIGHTLIGHT)
		{		  
			dl = CL_AllocDlight (i);
			VectorCopy (ent->origin,  dl->origin);
			dl->origin[2] += 16;
			dl->radius = 400 + (rand()&31);
			dl->die = cl.time + 0.001;
		}
		if (ent->effects & EF_DIMLIGHT) 
		{		  
			dl = CL_AllocDlight (i);
			VectorCopy (ent->origin,  dl->origin);
			dl->radius = 200 + (rand()&31);
			dl->die = cl.time + 0.001;
		}

		if (ent->model->flags & EF_GIB)
			R_RocketTrail (oldorg, ent->origin, 2);
		else if (ent->model->flags & EF_ZOMGIB)
			R_RocketTrail (oldorg, ent->origin, 4);
		else if (ent->model->flags & EF_TRACER)
			R_RocketTrail (oldorg, ent->origin, 3);
		else if (ent->model->flags & EF_TRACER2)
			R_RocketTrail (oldorg, ent->origin, 5);
		else if (ent->model->flags & EF_ROCKET)
		{
			R_RocketTrail (oldorg, ent->origin, 0);
			dl = CL_AllocDlight (i);
			VectorCopy (ent->origin, dl->origin);
			dl->radius = 200;
			dl->die = cl.time + 0.01;
		}
		else if (ent->model->flags & EF_GRENADE)
			R_RocketTrail (oldorg, ent->origin, 1);
		else if (ent->model->flags & EF_TRACER3)
			R_RocketTrail (oldorg, ent->origin, 6);

		ent->forcelink = false;

		if (i == cl.viewentity && !chase_active->intValue)
			continue;

		if (cl_numvisedicts < MAX_VISEDICTS)
		{
			cl_visedicts[cl_numvisedicts] = ent;
			cl_numvisedicts++;
		}
	}

}


/*
===============
CL_ReadFromServer

Read all incoming data from the server
===============
*/
int CL_ReadFromServer (void)
{
	int		ret;

	cl.oldtime = cl.time;
	cl.time += host_frametime;
	
	do
	{
		ret = CL_GetMessage ();
		if (ret == -1)
			Host_Error ("CL_ReadFromServer: lost server connection");
		if (!ret)
			break;
		
		cl.last_received_message = realtime;
		CL_ParseServerMessage ();
	} while (ret && cls.state == ca_connected);
	
	if (cl_shownet->intValue)
		Com_Printf ("\n");

	CL_RelinkEntities ();
	CL_UpdateBeams ();

//
// bring the links up to date
//
	return 0;
}

/*
=================
CL_SendCmd
=================
*/
void CL_SendCmd (void)
{
	usercmd_t		cmd;

	if (cls.state != ca_connected)
		return;

	if (cls.signon == SIGNONS)
	{
	// get basic movement from keyboard
		CL_BaseMove (&cmd);

	// allow mice or other external controllers to add to the move
		IN_Move (&cmd);

	// send the unreliable message
		CL_SendMove (&cmd);
	
	}

	if (cls.demoplayback)
	{
		SZ_Clear (&cls.message);
		return;
	}
	
// send the reliable message
	if (!cls.message.cursize)
		return;	  // no message at all
	
	if (!NET_CanSendMessage (cls.netcon))
	{
		Com_DPrintf (DEVELOPER_MSG_NET, "CL_WriteToServer: can't send\n");
		return;
	}

	if (NET_SendMessage (cls.netcon, &cls.message) == -1)
		Host_Error ("CL_WriteToServer: lost server connection");

	SZ_Clear (&cls.message);
}

void CL_Flashlight_f (void) /* FS: Flashlight */
{
	if(bFlashlight)
	{
		bFlashlight = false;
		Com_Printf("Flashlight OFF\n");
	}
	else
	{
		bFlashlight = true;
		Com_Printf("Flashlight ON\n");
	}
}

// Knightmare added
/*
===============
CL_WriteConfig_f

===============
*/
void CL_WriteConfig_f (void)
{
	char cfgName[MAX_QPATH];

	if ((Cmd_Argc() == 1) || (Cmd_Argc() == 2))
	{
		if(Cmd_Argc() == 1)
			Com_sprintf(cfgName, sizeof(cfgName), "qdos");
		else
			Q_strlcpy (cfgName, Cmd_Argv(1), sizeof(cfgName));

		Host_WriteConfiguration (cfgName);
		Com_Printf ("Wrote config file %s/%s.cfg.\n", com_gamedir, cfgName);
	}
	else
		Com_Printf ("Usage: writeconfig <name>\n");
}

void CL_Snd_Shutdown_f (void)
{
	S_StopAllSounds();
	S_Shutdown();
}

void CL_Snd_Restart_f (void)
{
	int i;

	S_StopAllSounds();
	S_Shutdown();
	S_Init();

	if (cl.numsounds)
	{
		for (i = 1; i < cl.numsounds; i++) /* FS: First one is always blank, so skip it. */
		{
			S_TouchSound (cl.sound_precache_str[i]);
			cl.sound_precache[i] = S_PrecacheSound (cl.sound_precache_str[i]);
		}
	}

	if ( (cls.demoplayback || cls.demorecording) && (cls.forcetrack != -1) )
	{
		CL_PlayBackgroundTrack(cls.forcetrack);
	}
	else
	{
		CL_PlayBackgroundTrack(cl.cdtrack);
	}
}

/*
=================
CL_Init
=================
*/
void CL_Init (void)
{  
	SZ_Alloc (&cls.message, 1024);

	CL_InitInput ();
	CL_InitTEnts ();
	
//
// register our commands
//
	cl_name = Cvar_Get("_cl_name", "player", CVAR_ARCHIVE);
	Cvar_Set_Description("cl_name", "Internal CVAR for setting player name.  Use cvar \"name\" to set.");

	cl_color = Cvar_Get("_cl_color", "0", CVAR_ARCHIVE);
	Cvar_Set_Description("cl_color", "Internal CVAR for setting player colour.  Use cvar \"color\" to set.");

	cl_upspeed = Cvar_Get("cl_upspeed","200", 0);
	cl_forwardspeed = Cvar_Get("cl_forwardspeed","400", CVAR_ARCHIVE);
	cl_backspeed = Cvar_Get("cl_backspeed","400", CVAR_ARCHIVE);
	cl_sidespeed = Cvar_Get("cl_sidespeed","350", 0);
	cl_movespeedkey = Cvar_Get("cl_movespeedkey","2.0", 0);
	cl_yawspeed = Cvar_Get("cl_yawspeed","140", 0);
	cl_pitchspeed = Cvar_Get("cl_pitchspeed","150", 0);
	cl_anglespeedkey = Cvar_Get("cl_anglespeedkey","1.5", 0);
	cl_fullpitch = Cvar_Get("cl_fullpitch", "0", 0); /* FS: ProQuake Shit */

	cl_shownet = Cvar_Get("cl_shownet", "0", 0); // can be 0, 1, or 2
	cl_nolerp = Cvar_Get("cl_nolerp", "0", 0); 
	Cvar_Set_Description("cl_nolerp", "Disable animation lerping.");
	lookspring = Cvar_Get("lookspring", "0", CVAR_ARCHIVE);
	lookstrafe = Cvar_Get("lookstrafe", "0", CVAR_ARCHIVE);
	sensitivity = Cvar_Get("sensitivity", "3", CVAR_ARCHIVE);

	m_pitch = Cvar_Get("m_pitch", "0.022", CVAR_ARCHIVE);
	m_yaw = Cvar_Get("m_yaw", "0.022", CVAR_ARCHIVE);
	m_forward = Cvar_Get("m_forward", "1", CVAR_ARCHIVE);
	m_side = Cvar_Get("m_side", "0.8", CVAR_ARCHIVE);

#ifdef GAMESPY /* FS: GameSpy CVARs */
	cl_master_server_ip = Cvar_Get("cl_master_server_ip", CL_MASTER_ADDR, CVAR_ARCHIVE);
	Cvar_Set_Description("cl_master_server_ip", "GameSpy Master Server IP.");
	cl_master_server_port = Cvar_Get("cl_master_server_port", CL_MASTER_PORT, CVAR_ARCHIVE); 
	Cvar_Set_Description("cl_master_server_port", "GameSpy Master Server Port.");
	cl_master_server_queries = Cvar_Get("cl_master_server_queries", "10", CVAR_ARCHIVE);
	Cvar_Set_Description("cl_master_server_queries", "Number of sockets to allocate for GameSpy.");
	cl_master_server_timeout = Cvar_Get("cl_master_server_timeout", "3000", CVAR_ARCHIVE);
	Cvar_Set_Description("cl_master_server_timeout", "Timeout (in milliseconds) to give up on pinging a server.");
	cl_master_server_retries = Cvar_Get("cl_master_server_retries", "20", CVAR_ARCHIVE);
	Cvar_Set_Description("cl_master_server_retries", "Number of retries to attempt for receiving the server list.  Formula is 50ms + 10ms for each retry.");
	cl_master_server_optout = Cvar_Get("cl_master_server_optout", "0", CVAR_ARCHIVE);
	Cvar_Set_Description("cl_master_server_optout", "Opt-out of sending your Quake Username in GameSpy list requests.");
	snd_gamespy_sounds = Cvar_Get("snd_gamespy_sounds", "0", CVAR_ARCHIVE);
	Cvar_Set_Description("snd_gamespy_sounds", "Play the complete.wav and abort.wav from GameSpy3D if it exists in sounds/gamespy.");
#endif

	/* FS: New stuff */
	console_old_complete = Cvar_Get("console_old_complete", "0", CVAR_ARCHIVE);
	Cvar_Set_Description("console_old_complete", "Use the legacy style console tab completion.");
	cl_autorepeat_allkeys = Cvar_Get("cl_autorepeat_allkeys", "0", CVAR_ARCHIVE);
	Cvar_Set_Description("cl_autorepeat_allkeys", "Allow to autorepeat any key, not just Backspace, Pause, PgUp, and PgDn keys.");
	cl_sleep = Cvar_Get("cl_sleep", "0", CVAR_ARCHIVE);
	Cvar_Set_Description("cl_sleep", "Reduce CPU usage by issuing sleep commands between extra frames.");

	Cmd_AddCommand ("entities", CL_PrintEntities_f);
	Cmd_AddCommand ("disconnect", CL_Disconnect_f);
	Cmd_AddCommand ("record", CL_Record_f);
	Cmd_AddCommand ("stop", CL_Stop_f);
	Cmd_AddCommand ("playdemo", CL_PlayDemo_f);
	Cmd_AddCommand ("timedemo", CL_TimeDemo_f);
	Cmd_AddCommand ("flashlight", CL_Flashlight_f);
	Cmd_AddCommand ("writeconfig", CL_WriteConfig_f);

#ifndef GLQUAKE
	Cmd_AddCommand ("r_restart", R_Restart_f); /* FS: Unfinished */
#endif

#ifdef GAMESPY /* FS: Gamespy stuff */
	Cmd_AddCommand ("slist2", CL_PingNetServers_f);
	Cmd_AddCommand ("srelist", CL_PrintBrowserList_f);
	Cmd_AddCommand ("gspystop", CL_Gspystop_f);

	memset(&browserList, 0, sizeof(browserList));
	memset(&browserListAll, 0, sizeof(browserListAll));
#endif
	Cmd_AddCommand("snd_shutdown", CL_Snd_Shutdown_f); /* FS */
	Cmd_AddCommand("snd_restart", CL_Snd_Restart_f); /* FS */

#ifdef __DJGPP__
	Cmd_AddCommand ("memstats", Sys_Memory_Stats_f); /* FS: Added to keep track of memory usage in DOS */
#endif
}

#ifdef GAMESPY /* FS: Gamespy Stuff */
static void CL_Gamespy_Check_Error(GServerList lst, int error)
{
	if (error != GE_NOERROR) /* FS: Grab the error code */
	{
		Com_Printf("\x02GameSpy Error: ");
		Com_Printf("%s.\n", ServerListErrorDesc(lst, error));
	}
}

void GameSpy_Async_Think(void)
{
	int error;

	if (!serverlist)
		return;

	if(ServerListState(serverlist) == sl_idle && cls.gamespyupdate)
	{
		if (key_dest != key_menu) /* FS: Only print this from an slist2 command, not the server browser. */
		{
			Com_Printf("Found %i active servers out of %i in %i seconds.\n", gspyCur, cls.gamespytotalservers, (((int)Sys_DoubleTime()-cls.gamespystarttime)) );
		}
		else
		{
			Update_Gamespy_Menu();
		}
		cls.gamespyupdate = 0;
		cls.gamespypercent = 0;
		ServerListClear(serverlist);
		ServerListFree(serverlist);
		serverlist = NULL; /* FS: This is on purpose so future ctrl+c's won't try to close empty serverlists */
	}
	else
	{
		error = ServerListThink(serverlist);
		CL_Gamespy_Check_Error(serverlist, error);
	}
}

static void CL_Gspystop_f (void)
{
	if(serverlist != NULL && cls.gamespyupdate) /* FS: Immediately abort gspy scans */
	{
		Com_Printf("\x02Server scan aborted!\n");
		S_GamespySound ("gamespy/abort.wav");
		ServerListHalt(serverlist);
	}
}

static void CL_PrintBrowserList_f (void)
{
	int i;
	int num_active_servers = 0;
	qboolean showAll = false;

	if(Cmd_Argc() > 1)
	{
		showAll = true;
	}

	for (i = 0; i < MAX_SERVERS; i++)
	{
		if(browserList[i].hostname[0] != 0)
		{
			if (browserList[i].curPlayers > 0)
			{
				Com_Printf("%02d:  %s:%d [%d] %s ", num_active_servers+1, browserList[i].ip, browserList[i].port, browserList[i].ping, browserList[i].hostname);
				Com_Printf("\x02%d", browserList[i].curPlayers); /* FS: Show the current players number in the green font */
				Com_Printf("/%d %s\n", browserList[i].maxPlayers, browserList[i].mapname);
				num_active_servers++;
			}
		}
		else /* FS: if theres nothing there the rest of the list is old garbage, bye. */
		{
			break;
		}
	}

	if (showAll)
	{
		int skip = 0;

		for (i = 0; i < MAX_SERVERS; i++)
		{
			if(browserListAll[i].hostname[0] != 0)
			{
				Com_Printf("%02d:  %s:%d [%d] %s %d/%d %s\n", (i+num_active_servers+1)-(skip), browserListAll[i].ip, browserListAll[i].port, browserListAll[i].ping, browserListAll[i].hostname, browserListAll[i].curPlayers, browserListAll[i].maxPlayers, browserListAll[i].mapname);
			}
			else /* FS: The next one could be 0 if we skipped over it previously in GameSpy_Sort_By_Ping.  So increment the number of skips counter so the server number shows sequentially */
			{
				skip++;
				continue;
			}
		}
	}
}

static void GameSpy_Sort_By_Ping(GServerList lst)
{
	int i;
	gspyCur = 0;

	for (i = 0; i < cls.gamespytotalservers; i++)
	{
		GServer server = ServerListGetServer(lst, i);
		if (server)
		{
			if(ServerGetIntValue(server, "numplayers", 0) <= 0)
				continue;

			if(i == MAX_SERVERS)
				break;

			Q_strlcpy(browserList[gspyCur].ip, ServerGetAddress(server), sizeof(browserList[gspyCur].ip));
			browserList[gspyCur].port = ServerGetQueryPort(server);
			browserList[gspyCur].ping = ServerGetPing(server);
			Q_strlcpy(browserList[gspyCur].hostname, ServerGetStringValue(server, "hostname","(NONE)"), sizeof(browserList[gspyCur].hostname));
			Q_strlcpy(browserList[gspyCur].mapname, ServerGetStringValue(server,"map","(NO MAP)"), sizeof(browserList[gspyCur].mapname));
			browserList[gspyCur].curPlayers = ServerGetIntValue(server,"numplayers",0);
			browserList[gspyCur].maxPlayers = ServerGetIntValue(server,"maxclients",0);

			gspyCur++;
		}
	}

	for (i = 0; i < cls.gamespytotalservers; i++)
	{
		GServer server = ServerListGetServer(lst, i);

		if (server)
		{
			if(ServerGetIntValue(server, "numplayers", 0) > 0) /* FS: We already added this so skip it */
			{
				continue;
			}

			if(Q_strncmp(ServerGetStringValue(server, "hostname","(NONE)"), "(NONE)", 6) == 0) /* FS: A server that timed-out or we aborted early */
			{
				continue;
			}

			if(i == MAX_SERVERS)
				break;

			Q_strlcpy(browserListAll[i].ip, ServerGetAddress(server), sizeof(browserListAll[i].ip));
			browserListAll[i].port = ServerGetQueryPort(server);
			browserListAll[i].ping = ServerGetPing(server);
			Q_strlcpy(browserListAll[i].hostname, ServerGetStringValue(server, "hostname","(NONE)"), sizeof(browserListAll[i].hostname));
			Q_strlcpy(browserListAll[i].mapname, ServerGetStringValue(server,"map","(NO MAP)"), sizeof(browserListAll[i].mapname));
			browserListAll[i].curPlayers = ServerGetIntValue(server,"numplayers",0);
			browserListAll[i].maxPlayers = ServerGetIntValue(server,"maxclients",0);
		}
	}
}

static void ListCallBack(GServerList lst, int msg, void *instance, void *param1, void *param2)
{
	GServer server;
	int percent;
	int numplayers;

	if (msg == LIST_PROGRESS)
	{
		server = (GServer)param1;
		numplayers = ServerGetIntValue(server,"numplayers",0);

		if(numplayers > 0) /* FS: Only show populated servers */
		{
			if (key_dest != key_menu) /* FS: Only print this from an slist2 command, not the server browser. */
			{
				Com_Printf("%s:%d [%d] %s ", ServerGetAddress(server), ServerGetQueryPort(server), ServerGetPing(server), ServerGetStringValue(server, "hostname","(NONE)"));
				Com_Printf("\x02%d", numplayers); /* FS: Show the current players number in the green font */
				Com_Printf("/%d %s\n", ServerGetIntValue(server,"maxclients",0), ServerGetStringValue(server,"map","(NO MAP)"));
			}
		}
		else if (cls.gamespyupdate == SHOW_ALL_SERVERS)
		{
			if (key_dest != key_menu) /* FS: Only print this from an slist2 command, not the server browser. */
			{
				Com_Printf("%s:%d [%d] %s %d/%d %s\n", ServerGetAddress(server), ServerGetQueryPort(server), ServerGetPing(server), ServerGetStringValue(server, "hostname","(NONE)"), ServerGetIntValue(server,"numplayers",0), ServerGetIntValue(server,"maxclients",0), ServerGetStringValue(server,"map","(NO MAP)"));
			}
		}

		if(param2)
		{
			percent = (int)(param2);
			cls.gamespypercent = percent;
		}
	}
	else if (msg == LIST_STATECHANGED)
	{
		switch(ServerListState(lst))
		{
			case sl_idle:
				ServerListSort(lst, true, "ping", cm_int);
				GameSpy_Sort_By_Ping(lst);
				break;
			default:
				break;
		}
	}
}

void CL_PingNetServers_f (void)
{
	char goa_secret_key[7];
	int error;
	int allocatedSockets;

	if(cls.gamespyupdate)
	{
		Com_Printf("Error: Already querying the GameSpy Master!\n");
		return;
	}

	gspyCur = 0;
	memset(&browserList, 0, sizeof(browserList));
	memset(&browserListAll, 0, sizeof(browserListAll));

	goa_secret_key[0] = '7';
	goa_secret_key[1] = 'W';
	goa_secret_key[2] = '7';
	goa_secret_key[3] = 'y';
	goa_secret_key[4] = 'Z';
	goa_secret_key[5] = 'z';
	goa_secret_key[6] = '\0';

	if ((Cmd_Argc() == 1) || (key_dest == key_menu))
	{
		cls.gamespyupdate = SHOW_POPULATED_SERVERS;;
		Com_Printf("\x02Grabbing populated server list from GameSpy master. . .\n");
	}
	else
	{
		cls.gamespyupdate = SHOW_ALL_SERVERS;
		Com_Printf("\x02Grabbing all servers from GameSpy master. . .\n");
	}

	cls.gamespypercent = 0;
	cls.gamespystarttime = (int)Sys_DoubleTime();
	cls.gamespytotalservers = 0;

	allocatedSockets = bound(5, cl_master_server_queries->intValue, 100);

	SCR_UpdateScreen(); /* FS: Force an update so the percentage bar shows some progress */

	serverlist = ServerListNew("quake1","quake1",goa_secret_key,allocatedSockets,ListCallBack,GCALLBACK_FUNCTION,NULL);
	error = ServerListUpdate(serverlist,true); /* FS: Use Async now! */

	CL_Gamespy_Check_Error(serverlist, error);
}
#endif
