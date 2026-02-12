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
// r_misc.c

#include "quakedef.h"


/*
==================
R_InitTextures
==================
*/
void	R_InitTextures (void)
{
	int		x,y, m;
	byte	*dest;

// create a simple checkerboard texture for the default
	r_notexture_mip = Hunk_AllocName (sizeof(texture_t) + 16*16+8*8+4*4+2*2, "notexture");
	
	r_notexture_mip->width = r_notexture_mip->height = 16;
	r_notexture_mip->offsets[0] = sizeof(texture_t);
	r_notexture_mip->offsets[1] = r_notexture_mip->offsets[0] + 16*16;
	r_notexture_mip->offsets[2] = r_notexture_mip->offsets[1] + 8*8;
	r_notexture_mip->offsets[3] = r_notexture_mip->offsets[2] + 4*4;
	
	for (m=0 ; m<4 ; m++)
	{
		dest = (byte *)r_notexture_mip + r_notexture_mip->offsets[m];
		for (y=0 ; y< (16>>m) ; y++)
			for (x=0 ; x< (16>>m) ; x++)
			{
				if (  (y< (8>>m) ) ^ (x< (8>>m) ) )
					*dest++ = 0;
				else
					*dest++ = 0xff;
			}
	}	
}

byte	dottexture[8][8] =
{
	{0,1,1,0,0,0,0,0},
	{1,1,1,1,0,0,0,0},
	{1,1,1,1,0,0,0,0},
	{0,1,1,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
};
void R_InitParticleTexture (void)
{
	int		x,y;
	byte	data[8][8][4];

	//
	// particle texture
	//
	particletexture = texture_extension_number++;
    GL_Bind(particletexture);

	for (x=0 ; x<8 ; x++)
	{
		for (y=0 ; y<8 ; y++)
		{
			data[y][x][0] = 255;
			data[y][x][1] = 255;
			data[y][x][2] = 255;
			data[y][x][3] = dottexture[x][y]*255;
		}
	}
	glTexImage2D_fp (GL_TEXTURE_2D, 0, gl_alpha_format, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

	glTexEnvf_fp(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	glTexParameterf_fp(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf_fp(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

/*
===============
R_Envmap_f

Grab six views for environment mapping tests
===============
*/
void R_Envmap_f (void)
{
	byte	buffer[256*256*4];

	glDrawBuffer_fp  (GL_FRONT);
	glReadBuffer_fp  (GL_FRONT);
	envmap = true;

	r_refdef.vrect.x = 0;
	r_refdef.vrect.y = 0;
	r_refdef.vrect.width = 256;
	r_refdef.vrect.height = 256;

	r_refdef.viewangles[0] = 0;
	r_refdef.viewangles[1] = 0;
	r_refdef.viewangles[2] = 0;
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	R_RenderView ();
	glReadPixels_fp (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile ("env0.rgb", buffer, sizeof(buffer));		

	r_refdef.viewangles[1] = 90;
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	R_RenderView ();
	glReadPixels_fp (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile ("env1.rgb", buffer, sizeof(buffer));		

	r_refdef.viewangles[1] = 180;
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	R_RenderView ();
	glReadPixels_fp (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile ("env2.rgb", buffer, sizeof(buffer));		

	r_refdef.viewangles[1] = 270;
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	R_RenderView ();
	glReadPixels_fp (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile ("env3.rgb", buffer, sizeof(buffer));		

	r_refdef.viewangles[0] = -90;
	r_refdef.viewangles[1] = 0;
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	R_RenderView ();
	glReadPixels_fp (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile ("env4.rgb", buffer, sizeof(buffer));		

	r_refdef.viewangles[0] = 90;
	r_refdef.viewangles[1] = 0;
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	R_RenderView ();
	glReadPixels_fp (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile ("env5.rgb", buffer, sizeof(buffer));		

	envmap = false;
	glDrawBuffer_fp  (GL_BACK);
	glReadBuffer_fp  (GL_BACK);
	GL_EndRendering ();
}

/*
===============
R_Init
===============
*/
void R_Init (void)
{	
	Cmd_AddCommand ("timerefresh", R_TimeRefresh_f);	
	Cmd_AddCommand ("envmap", R_Envmap_f);	
	Cmd_AddCommand ("pointfile", R_ReadPointFile_f);	

	r_norefresh = Cvar_Get("r_norefresh", "0", 0);
	r_lightmap = Cvar_Get("r_lightmap", "0", 0);
	r_lightmap->description = "Changes textures to white to show the lightmap blending.";
	r_fullbright = Cvar_Get("r_fullbright", "0", 0);
	r_fullbright->description = "Full-bright lights.  Disabled in Multiplayer.";
	r_drawentities = Cvar_Get("r_drawentities", "1", 0);
	r_drawviewmodel = Cvar_Get("r_drawviewmodel", "1", 0);
	r_shadows = Cvar_Get("r_shadows", "0", 0);
	r_mirroralpha = Cvar_Get("r_mirroralpha", "1", 0);
	r_mirroralpha->description = "Level of mirror transparency.  Values lower than 1 (in decimal ranges i.e. 0.5) will adjust the transparency of the stain-glass window in start.bsp; showing a mirror effect.  Disabled in QuakeWorld.";
	r_wateralpha = Cvar_Get("r_wateralpha", "1", 0);
	r_wateralpha->description = "Level of water transparency.  Values lower than 1 (in decimal ranges i.e. 0.5) will have transparent water in maps compiled with transparent water.";
	r_dynamic = Cvar_Get("r_dynamic", "1", 0);
	r_dynamic->description = "Dynamic Lighting.";
	r_novis = Cvar_Get("r_novis", "0", 0);
	r_novis->description = "Skip loading of VIS data.  Useful for seeing transparent water in maps not compiled with it.  Very slow.";
	r_speeds = Cvar_Get("r_speeds", "0", 0);
#ifdef QUAKEWORLD
	r_netgraph = Cvar_Get("r_netgraph", "0", 0);
	r_netgraph->description = "Draw a network graph showing latency and packet loss.";
#endif
	r_gunfov = Cvar_Get("r_gunfov", "75", CVAR_ARCHIVE); /* FS */

	gl_clear = Cvar_Get("gl_clear", "0", 0);
	gl_clear->description = "Use a clear colour to avoid Hall of Mirror (HOM) effects.  Useful for noclipping, spectating, and finding leaks in maps.";
	gl_texsort = Cvar_Get("gl_texsort", "1", 0);

 	if (gl_mtexable)
		Cvar_SetValue ("gl_texsort", 0.0);

	gl_cull = Cvar_Get("gl_cull", "1", 0);
	gl_cull->description = "Cull (i.e. skip) rendering of objects not in the viewport.";
	gl_smoothmodels = Cvar_Get("gl_smoothmodels", "1", 0);
	gl_smoothmodels->description = "Smooth textures on models.  Recommended to leave enabled as it can cause visual artifacts if disabled.";
	gl_affinemodels = Cvar_Get("gl_affinemodels", "0", 0);
	gl_polyblend = Cvar_Get("gl_polyblend","1", CVAR_ARCHIVE);
	gl_polyblend->description = "Show palette blends i.e. screen turning red from damage.";
	gl_flashblend = Cvar_Get("gl_flashblend","1", CVAR_ARCHIVE);
	gl_flashblend->description = "Set to 0 to use fake dynamic lights that have a mushroom cloud effect.  Can improve performance on slower machines.";
	gl_playermip = Cvar_Get("gl_playermip", "0", 0);
	gl_nocolors = Cvar_Get("gl_nocolors", "0", 0);
	gl_nocolors->description = "Don't use top/bottom colours for players.";
	gl_finish = Cvar_Get("gl_finish", "0", 0);
	gl_finish->description = "Send a gl_finish at the end of every frame.  Some older cards may require this.";

	gl_keeptjunctions = Cvar_Get("gl_keeptjunctions","1", CVAR_ARCHIVE|CVAR_LATCH);
	gl_keeptjunctions->description = "Keep co-linear points/t-junctions.  Disabling may show some gaps/lines in architecture, but may improve performance on slow machines.";

#ifdef QUAKE1
	gl_doubleeyes = Cvar_Get("gl_doubleeys", "1", CVAR_ARCHIVE);
	gl_doubleeyes->description = "Double size of model eyes, since they are really hard to see in GL.";
#endif

	r_waterwarp = Cvar_Get("r_waterwarp", "1", CVAR_ARCHIVE); /* FS: TODO FIXME dummy */

	gl_texturemode = Cvar_Get("gl_texturemode", "GL_LINEAR_MIPMAP_NEAREST", CVAR_ARCHIVE); /* FS: Now a CVAR so we can do +set gl_texturemode blah blah at cmdline or autoexec.cfg */

	/* FS */
	gl_zfar_dist = Cvar_Get("gl_zfar_dist", "8192", CVAR_ARCHIVE);
	gl_zfar_dist->description = "Adjusts the zFar distance.  May need to be inreased for larger maps.";

	R_InitBubble();

	R_InitParticles ();
	R_InitParticleTexture ();

#ifdef GLTEST
	Test_Init ();
#endif

#ifdef QUAKEWORLD
	netgraphtexture = texture_extension_number;
	texture_extension_number++;
#endif

	playertextures = texture_extension_number;
#ifdef QUAKE1
	texture_extension_number += 16;
#else
	texture_extension_number += MAX_CLIENTS;
#endif
}

/*
===============
R_TranslatePlayerSkin

Translates a skin texture by the per-player color lookup
===============
*/
#ifdef QUAKE1
void R_TranslatePlayerSkin (int playernum)
{
	int		top, bottom;
	byte	translate[256];
	unsigned	translate32[256];
	int		i, j, s;
	model_t	*model;
	aliashdr_t *paliashdr;
	byte	*original;
	unsigned	pixels[512*256], *out;
	unsigned	scaled_width, scaled_height;
	int			inwidth, inheight;
	byte		*inrow;
	unsigned	frac, fracstep;

	GL_DisableMultitexture();

	top = cl.scores[playernum].colors & 0xf0;
	bottom = (cl.scores[playernum].colors &15)<<4;

	for (i=0 ; i<256 ; i++)
		translate[i] = i;

	for (i=0 ; i<16 ; i++)
	{
		if (top < 128)	// the artists made some backwards ranges.  sigh.
			translate[TOP_RANGE+i] = top+i;
		else
			translate[TOP_RANGE+i] = top+15-i;
				
		if (bottom < 128)
			translate[BOTTOM_RANGE+i] = bottom+i;
		else
			translate[BOTTOM_RANGE+i] = bottom+15-i;
	}

	//
	// locate the original skin pixels
	//
	currententity = &cl_entities[1+playernum];
	model = currententity->model;
	if (!model)
		return;		// player doesn't have a model yet
	if (model->type != mod_alias)
		return; // only translate skins on alias models

	paliashdr = (aliashdr_t *)Mod_Extradata (model);
	s = paliashdr->skinwidth * paliashdr->skinheight;
	if (currententity->skinnum < 0 || currententity->skinnum >= paliashdr->numskins) {
		Con_Printf("(%d): Invalid player skin #%d\n", playernum, currententity->skinnum);
		original = (byte *)paliashdr + paliashdr->texels[0];
	} else
		original = (byte *)paliashdr + paliashdr->texels[currententity->skinnum];
	if (s & 3)
		Sys_Error ("R_TranslateSkin: s&3");

	inwidth = paliashdr->skinwidth;
	inheight = paliashdr->skinheight;

	// because this happens during gameplay, do it fast
	// instead of sending it through gl_upload 8
    GL_Bind(playertextures + playernum);

	scaled_width = gl_max_size->value < 512 ? gl_max_size->value : 512;
	scaled_height = gl_max_size->value < 256 ? gl_max_size->value : 256;

	// allow users to crunch sizes down even more if they want
	scaled_width >>= (int)gl_playermip->value;
	scaled_height >>= (int)gl_playermip->value;

	if (VID_Is8bit()) { // 8bit texture upload
		byte *out2;

		out2 = (byte *)pixels;
		memset(pixels, 0, sizeof(pixels));
		fracstep = inwidth*0x10000/scaled_width;
		for (i=0 ; i<scaled_height ; i++, out2 += scaled_width)
		{
			inrow = original + inwidth*(i*inheight/scaled_height);
			frac = fracstep >> 1;
			for (j=0 ; j<scaled_width ; j+=4)
			{
				out2[j] = translate[inrow[frac>>16]];
				frac += fracstep;
				out2[j+1] = translate[inrow[frac>>16]];
				frac += fracstep;
				out2[j+2] = translate[inrow[frac>>16]];
				frac += fracstep;
				out2[j+3] = translate[inrow[frac>>16]];
				frac += fracstep;
			}
		}

		GL_Upload8_EXT ((byte *)pixels, scaled_width, scaled_height, false, false);
		return;
	}

	for (i=0 ; i<256 ; i++)
		translate32[i] = d_8to24table[translate[i]];

	out = pixels;
	fracstep = inwidth*0x10000/scaled_width;
	for (i=0 ; i<scaled_height ; i++, out += scaled_width)
	{
		inrow = original + inwidth*(i*inheight/scaled_height);
		frac = fracstep >> 1;
		for (j=0 ; j<scaled_width ; j+=4)
		{
			out[j] = translate32[inrow[frac>>16]];
			frac += fracstep;
			out[j+1] = translate32[inrow[frac>>16]];
			frac += fracstep;
			out[j+2] = translate32[inrow[frac>>16]];
			frac += fracstep;
			out[j+3] = translate32[inrow[frac>>16]];
			frac += fracstep;
		}
	}

	glTexImage2D_fp (GL_TEXTURE_2D, 0, gl_solid_format,
		scaled_width, scaled_height, 0, GL_RGBA,
		GL_UNSIGNED_BYTE, pixels);

	glTexEnvf_fp(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexParameterf_fp(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf_fp(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

}

#else

void R_TranslatePlayerSkin (int playernum)
{
	int		top, bottom;
	byte	translate[256];
	unsigned	translate32[256];
	int		i, j;
	byte	*original;
	unsigned	pixels[512*256], *out;
	unsigned	scaled_width, scaled_height;
	int			inwidth;
	int			tinwidth, tinheight;
	byte		*inrow;
	unsigned	frac, fracstep;
	player_info_t *player;
	extern	byte		player_8bit_texels[320*200];
	char s[512];

	GL_DisableMultitexture();

	player = &cl.players[playernum];
	if (!player->name[0])
		return;

	strcpy(s, Info_ValueForKey(player->userinfo, "skin"));
	COM_StripExtension(s, s);
	if (player->skin && !stricmp(s, player->skin->name))
		player->skin = NULL;

	if (player->_topcolor != player->topcolor ||
		player->_bottomcolor != player->bottomcolor || !player->skin) {
		player->_topcolor = player->topcolor;
		player->_bottomcolor = player->bottomcolor;

		top = player->topcolor;
		bottom = player->bottomcolor;
		top = (top < 0) ? 0 : ((top > 13) ? 13 : top);
		bottom = (bottom < 0) ? 0 : ((bottom > 13) ? 13 : bottom);
		top *= 16;
		bottom *= 16;

		for (i=0 ; i<256 ; i++)
			translate[i] = i;

		for (i=0 ; i<16 ; i++)
		{
			if (top < 128)	// the artists made some backwards ranges.  sigh.
				translate[TOP_RANGE+i] = top+i;
			else
				translate[TOP_RANGE+i] = top+15-i;
					
			if (bottom < 128)
				translate[BOTTOM_RANGE+i] = bottom+i;
			else
				translate[BOTTOM_RANGE+i] = bottom+15-i;
		}

		//
		// locate the original skin pixels
		//
		// real model width
		tinwidth = 296;
		tinheight = 194;

		if (!player->skin)
			Skin_Find(player);
		if ((original = Skin_Cache(player->skin)) != NULL) {
			//skin data width
			inwidth = 320;
		} else {
			original = player_8bit_texels;
			inwidth = 296;
		}


		// because this happens during gameplay, do it fast
		// instead of sending it through gl_upload 8
		GL_Bind(playertextures + playernum);

		scaled_width = gl_max_size->value < 512 ? gl_max_size->value : 512;
		scaled_height = gl_max_size->value < 256 ? gl_max_size->value : 256;
		// allow users to crunch sizes down even more if they want
		scaled_width >>= (int)gl_playermip->value;
		scaled_height >>= (int)gl_playermip->value;

		if (VID_Is8bit()) { // 8bit texture upload
			byte *out2;

			out2 = (byte *)pixels;
			memset(pixels, 0, sizeof(pixels));
			fracstep = tinwidth*0x10000/scaled_width;
			for (i=0 ; i<scaled_height ; i++, out2 += scaled_width)
			{
				inrow = original + inwidth*(i*tinheight/scaled_height);
				frac = fracstep >> 1;
				for (j=0 ; j<scaled_width ; j+=4)
				{
					out2[j] = translate[inrow[frac>>16]];
					frac += fracstep;
					out2[j+1] = translate[inrow[frac>>16]];
					frac += fracstep;
					out2[j+2] = translate[inrow[frac>>16]];
					frac += fracstep;
					out2[j+3] = translate[inrow[frac>>16]];
					frac += fracstep;
				}
			}

			GL_Upload8_EXT ((byte *)pixels, scaled_width, scaled_height, false, false);
			return;
		}

		for (i=0 ; i<256 ; i++)
			translate32[i] = d_8to24table[translate[i]];

		out = pixels;
		memset(pixels, 0, sizeof(pixels)); /* FS: FIXME: Should this happen in QDOS too? */
		fracstep = tinwidth*0x10000/scaled_width;
		for (i=0 ; i<scaled_height ; i++, out += scaled_width)
		{
			inrow = original + inwidth*(i*tinheight/scaled_height);
			frac = fracstep >> 1;
			for (j=0 ; j<scaled_width ; j+=4)
			{
				out[j] = translate32[inrow[frac>>16]];
				frac += fracstep;
				out[j+1] = translate32[inrow[frac>>16]];
				frac += fracstep;
				out[j+2] = translate32[inrow[frac>>16]];
				frac += fracstep;
				out[j+3] = translate32[inrow[frac>>16]];
				frac += fracstep;
			}
		}

		glTexImage2D_fp (GL_TEXTURE_2D, 0, gl_solid_format, 
			scaled_width, scaled_height, 0, GL_RGBA, 
			GL_UNSIGNED_BYTE, pixels);

		glTexEnvf_fp(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glTexParameterf_fp(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf_fp(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
}
#endif // QUAKE1
/*
===============
R_NewMap
===============
*/
void R_NewMap (void)
{
	int		i;
	
	for (i=0 ; i<256 ; i++)
		d_lightstylevalue[i] = 264;		// normal light value

	memset (&r_worldentity, 0, sizeof(r_worldentity));
	r_worldentity.model = cl.worldmodel;

// clear out efrags in case the level hasn't been reloaded
// FIXME: is this one short?
	for (i=0 ; i<cl.worldmodel->numleafs ; i++)
		cl.worldmodel->leafs[i].efrags = NULL;
		 	
	r_viewleaf = NULL;
	R_ClearParticles ();

	GL_BuildLightmaps ();

	// identify sky texture
	skytexturenum = -1;
	mirrortexturenum = -1;
	for (i=0 ; i<cl.worldmodel->numtextures ; i++)
	{
		if (!cl.worldmodel->textures[i])
			continue;
		if (!Q_strncmp(cl.worldmodel->textures[i]->name,"sky",3) )
			skytexturenum = i;
		if (!Q_strncmp(cl.worldmodel->textures[i]->name,"window02_1",10) )
			mirrortexturenum = i;
 		cl.worldmodel->textures[i]->texturechain = NULL;
	}
#ifdef QUAKE2
	R_LoadSkys ();
#endif
}


/*
====================
R_TimeRefresh_f

For program optimization
====================
*/
void R_TimeRefresh_f (void)
{
	int			i;
	float		start, stop, time;

	glDrawBuffer_fp  (GL_FRONT);
	glFinish_fp ();

	start = Sys_DoubleTime();
	for (i=0 ; i<128 ; i++)
	{
		r_refdef.viewangles[1] = i/128.0*360.0;
		R_RenderView ();
	}

	glFinish_fp ();
	stop = Sys_DoubleTime();
	time = stop-start;
	Con_Printf ("%f seconds (%f fps)\n", time, 128/time);

	glDrawBuffer_fp  (GL_BACK);
	GL_EndRendering ();
}

void D_FlushCaches (void)
{
}


