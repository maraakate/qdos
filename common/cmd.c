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
// cmd.c -- Quake script command processing module

#include <ctype.h>
#include "quakedef.h"
#include "cmd.h"

cvar_t	*cl_warncmd;

#ifdef QUAKEWORLD
void Cmd_ForwardToServer_f (void);
#endif
void Cmd_ForwardToServer (void);

#define CMDLINE_LENGTH 256 //johnfitz -- mirrored in common.c

cmdalias_t	*cmd_alias;

qboolean	cmd_wait;

//=============================================================================

/*
============
Cmd_Wait_f

Causes execution of the remainder of the command buffer to be delayed until
next frame.  This allows commands like:
bind g "impulse 5 ; +attack ; wait ; -attack ; impulse 2"
============
*/
void Cmd_Wait_f (void)
{
	cmd_wait = true;
}

/*
=============================================================================

						COMMAND BUFFER

=============================================================================
*/

sizebuf_t	cmd_text;
byte		cmd_text_buf[16384]; /* FS: Was 8192.  Needed for a larger config. */

/*
============
Cbuf_Init
============
*/
void Cbuf_Init (void)
{
	cmd_text.data = cmd_text_buf;
	cmd_text.maxsize = sizeof(cmd_text_buf);
}

/*
============
Cbuf_AddText

Adds command text at the end of the buffer
============
*/
void Cbuf_AddText (char *text)
{
	int		l;

	l = Q_strlen (text);

	if (cmd_text.cursize + l >= cmd_text.maxsize)
	{
		Com_Printf ("Cbuf_AddText: overflow\n");
		return;
	}
	SZ_Write (&cmd_text, text, Q_strlen (text));
}


/*
============
Cbuf_InsertText

Adds command text immediately after the current command
Adds a \n to the text
FIXME: actually change the command buffer to do less copying
============
*/
void Cbuf_InsertText (char *text)
{
	char	*temp;
	int		templen;

// copy off any commands still remaining in the exec buffer
	templen = cmd_text.cursize;
	if (templen)
	{
		temp = calloc (templen, 1);
		memcpy (temp, cmd_text.data, templen);
		SZ_Clear (&cmd_text);
	}
	else
		temp = NULL;	// shut up compiler

// add the entire text of the file
	Cbuf_AddText (text);
	SZ_Write (&cmd_text, "\n", 1);
// add the copied off data
	if (templen)
	{
		SZ_Write (&cmd_text, temp, templen);
		free (temp);
	}
}

/*
============
Cbuf_Execute
============
*/
void Cbuf_Execute (void)
{
	int		i;
	char	*text;
	char	line[1024];
	int		quotes;

	while (cmd_text.cursize)
	{
// find a \n or ; line break
		text = (char *)cmd_text.data;

		quotes = 0;
		for (i=0 ; i< cmd_text.cursize ; i++)
		{
			if (text[i] == '"')
				quotes++;
			if ( !(quotes&1) &&  text[i] == ';')
				break;	// don't break if inside a quoted string
			if (text[i] == '\n')
				break;
		}

		if (i > (int)sizeof(line) - 1)
		{
			memcpy (line, text, sizeof(line) - 1);
			line[sizeof(line) - 1] = 0;
		}
		else
		{
			memcpy (line, text, i);
			line[i] = 0;
		}

// delete the text from the command buffer and move remaining commands down
// this is necessary because commands (exec, alias) can insert data at the
// beginning of the text buffer
		if (i == cmd_text.cursize)
			cmd_text.cursize = 0;
		else
		{
			i++;
			cmd_text.cursize -= i;
			memmove (text, text+i, cmd_text.cursize);
		}

// execute the command line
		Cmd_ExecuteString (line, src_command);
		if (cmd_wait)
		{	// skip out while text still remains in buffer, leaving it
			// for next frame
			cmd_wait = false;
			break;
		}
	}
}

/*
==============================================================================

						SCRIPT COMMANDS

==============================================================================
*/

/*
===============
Cmd_StuffCmds_f

Adds command line parameters as script statements
Commands lead with a +, and continue until a - or another +
quake +prog jctest.qp +cmd amlev1
quake -nosound +cmd amlev1
===============
*/
void Cmd_StuffCmds_f (void)
{
#ifdef QUAKE1
// johnfitz -- rewritten to read the "cmdline" cvar, for use with dynamic mod loading
	extern	cvar_t	*cmdline;
	char	cmds[CMDLINE_LENGTH];
	int		i, j, plus;

	plus = true;
	j = 0;
	for (i=0; cmdline->string[i]; i++)
	{
		if (cmdline->string[i] == '+')
		{
			plus = true;
			if (j > 0)
			{
				cmds[j-1] = ';';
				cmds[j++] = ' ';
			}
		}
		else if (cmdline->string[i] == '-' &&
			(i==0 || cmdline->string[i-1] == ' ')) //johnfitz -- allow hypenated map names with +map
				plus = false;
		else if (plus)
			cmds[j++] = cmdline->string[i];
	}
	cmds[j] = 0;

	Cbuf_InsertText (cmds);
#else
	int		i, j;
	int		s;
	char	*text, *build, c;
	size_t	len;

// build the combined string to parse from
	s = 0;
	for (i=1 ; i<com_argc ; i++)
	{
		if (!com_argv[i])
			continue;		// NEXTSTEP nulls out -NXHost
		s += Q_strlen (com_argv[i]) + 1;
	}
	if (!s)
		return;

	len = s + 1;

	text = calloc (len, 1);
	text[0] = 0;
	for (i=1 ; i<com_argc ; i++)
	{
		if (!com_argv[i])
			continue;		// NEXTSTEP nulls out -NXHost
		Q_strlcat (text, com_argv[i], len);
		if (i != com_argc-1)
			Q_strlcat (text, " ", len);
	}

// pull out the commands
	build = calloc (len, 1);
	build[0] = 0;

	for (i=0 ; i<s-1 ; i++)
	{
		if (text[i] == '+')
		{
			i++;

			for (j=i ; (text[j] != '+') && (text[j] != '-') && (text[j] != 0) ; j++)
				;

			c = text[j];
			text[j] = 0;

			Q_strlcat (build, text+i, len);
			Q_strlcat (build, "\n", len);
			text[j] = c;
			i = j-1;
		}
	}

	if (build[0])
		Cbuf_InsertText (build);

	free (text);
	free (build);
#endif
}


/*
===============
Cmd_Exec_f
===============
*/
void Cmd_Exec_f (void)
{
	char	*f;
	char	*s;

	if (Cmd_Argc () != 2)
	{
		Com_Printf ("exec <filename> : execute a script file\n");
		return;
	}

	s = Cmd_Argv(1);

	if (!strncmp(s,"default.cfg",11) || quakerc_init) /* FS: unbindall protection hack */
	{
		Com_DPrintf (DEVELOPER_MSG_VERBOSE, "default.cfg unbindall protection hack\n");
		Cvar_SetValue("cl_unbindall_protection", 0); /* FS: disable the warning if it's default.cfg */
	}

	if (quakerc_init)
	{
		if (!strncmp(s, "config.cfg", 10)) /* FS: Intercept config.cfg from quake.rc */
		{
			s = "qdos.cfg";
		}
	}

	f = (char *)COM_LoadFile(s, 0);
	if (!f)
	{
		Com_Printf ("couldn't exec %s\n",s);
		return;
	}

	if (!Cvar_Command () && (cl_warncmd->value || developer->value))
		Com_Printf ("execing %s\n",s);

	Cbuf_InsertText (f);
	Z_Free(f);
}


/*
===============
Cmd_Echo_f

Just prints the rest of the line to the console
===============
*/
void Cmd_Echo_f (void)
{
	int		i;

	for (i=1 ; i<Cmd_Argc() ; i++)
		Com_Printf ("%s ",Cmd_Argv(i));
	Com_Printf ("\n");
}

/*
===============
Cmd_Alias_f -- johnfitz -- rewritten

Creates a new command that executes a command string (possibly ; seperated)
===============
*/
void Cmd_Alias_f (void)
{
	cmdalias_t	*a;
	char		cmd[1024];
	int			i, c;
	char		*s;


	switch (Cmd_Argc())
	{
	case 1: //list all aliases
		for (a = cmd_alias, i = 0; a; a=a->next, i++)
			Com_SafePrintf ("   %s: %s", a->name, a->value);
		if (i)
			Com_SafePrintf ("%i alias command(s)\n", i);
		else
			Com_SafePrintf ("no alias commands found\n");
		break;
	case 2: //output current alias string
		for (a = cmd_alias ; a ; a=a->next)
			if (!strcmp(Cmd_Argv(1), a->name))
				Com_Printf ("   %s: %s", a->name, a->value);
		break;
	default: //set alias string
		s = Cmd_Argv(1);
		if (strlen(s) >= MAX_ALIAS_NAME)
		{
			Com_Printf ("Alias name is too long\n");
			return;
		}

		// if the alias allready exists, reuse it
		for (a = cmd_alias ; a ; a=a->next)
		{
			if (!strcmp(s, a->name))
			{
				free (a->value);
				break;
			}
		}

		if (!a)
		{
			a = malloc (sizeof(cmdalias_t));
			if (!a)
			{
				Sys_Error("Cmd_Alias_f: out of memory");
				return;
			}
			a->next = cmd_alias;
			cmd_alias = a;
		}
		Q_strlcpy (a->name, s, sizeof(a->name));

		// copy the rest of the command line
		cmd[0] = 0;		// start out with a null string
		c = Cmd_Argc();
		for (i=2 ; i< c ; i++)
		{
			Q_strlcat (cmd, Cmd_Argv(i), sizeof(cmd));
			if (i != c)
				Q_strlcat (cmd, " ", sizeof(cmd));
		}
		Q_strlcat (cmd, "\n", sizeof(cmd));

		a->value = strdup (cmd);
		if (!a->value)
		{
			Sys_Error("Cmd_Alias_f: out of memory");
			return;
		}
		break;
	}
}

/*
===============
Cmd_Unalias_f -- johnfitz
===============
*/
void Cmd_Unalias_f (void)
{
	cmdalias_t	*a, *prev;

	switch (Cmd_Argc())
	{
	default:
	case 1:
		Com_Printf("unalias <name> : delete alias\n");
		break;
	case 2:
		for (prev = a = cmd_alias; a; a = a->next)
		{
			if (!strcmp(Cmd_Argv(1), a->name))
			{
				prev->next = a->next;
				free (a->value);
				free (a);
				prev = a;
				return;
			}
			prev = a;
		}
		break;
	}
}

/*
===============
Cmd_Unaliasall_f -- johnfitz
===============
*/
void Cmd_Unaliasall_f (void)
{
	cmdalias_t	*blah;

	while (cmd_alias)
	{
		blah = cmd_alias->next;
		free(cmd_alias->value);
		free(cmd_alias);
		cmd_alias = blah;
	}
}

/*
=============================================================================

					COMMAND EXECUTION

=============================================================================
*/

#define	MAX_ARGS		80

static	int			cmd_argc;
static	char		*cmd_argv[MAX_ARGS];
static	char		*cmd_null_string = "";
static	char		*cmd_args = NULL;

cmd_source_t	cmd_source;
cmd_function_t	*cmd_functions;		// possible commands to execute

/*
============
Cmd_Init
============
*/
void Cmd_Init (void)
{
	cl_warncmd = Cvar_Get("cl_warncmd", "0", 0);
	Cvar_Set_Description("cl_warncmd", "Warn about unknown commands.");

//
// register our commands
//
	Cmd_AddCommand ("stuffcmds",Cmd_StuffCmds_f);
	Cmd_AddCommand ("exec",Cmd_Exec_f);
	Cmd_AddCommand ("echo",Cmd_Echo_f);
	Cmd_AddCommand ("alias",Cmd_Alias_f);
	Cmd_AddCommand ("wait", Cmd_Wait_f);
#ifdef QUAKE1
	Cmd_AddCommand ("cmd", Cmd_ForwardToServer);
#else
	Cmd_AddCommand ("cmd", Cmd_ForwardToServer_f);
#endif
	Cmd_AddCommand ("cmdlist", Cmd_List_f);
	Cmd_AddCommand ("unalias", Cmd_Unalias_f); //johnfitz
	Cmd_AddCommand ("unaliasall", Cmd_Unaliasall_f); //johnfitz
}

/*
============
Cmd_Argc
============
*/
int		Cmd_Argc (void)
{
	return cmd_argc;
}

/*
============
Cmd_Argv
============
*/
char	*Cmd_Argv (int arg)
{
	if ( arg >= cmd_argc )
		return cmd_null_string;
	return cmd_argv[arg];
}

/*
============
Cmd_Args

Returns a single string containing argv(1) to argv(argc()-1)
============
*/
char		*Cmd_Args (void)
{
	if (!cmd_args)
		return "";
	return cmd_args;
}


/*
============
Cmd_TokenizeString

Parses the given string into command line tokens.
============
*/
void Cmd_TokenizeString (char *text)
{
	int		i;

// clear the args from the last string
	for (i = 0; i < cmd_argc; i++)
	{
		free (cmd_argv[i]);
	}

	cmd_argc = 0;
	cmd_args = NULL;

	while (1)
	{
// skip whitespace up to a /n
		while (*text && *text <= ' ' && *text != '\n')
		{
			text++;
		}

		if (*text == '\n')
		{	// a newline seperates commands in the buffer
			text++;
			break;
		}

		if (!*text)
			return;

		if (cmd_argc == 1)
			 cmd_args = text;

		text = COM_Parse (text);
		if (!text)
			return;

		if (cmd_argc < MAX_ARGS)
		{
			size_t len = Q_strlen(com_token)+1;
			cmd_argv[cmd_argc] = malloc (len);
			if (!cmd_argv[cmd_argc])
			{
				Sys_Error("Cmd_TokenizeString: out of memory");
				return;
			}
			Q_strlcpy (cmd_argv[cmd_argc], com_token, len);
			cmd_argc++;
		}
	}

}


/*
============
Cmd_AddCommand
============
*/
void    Cmd_AddCommand (char *cmd_name, xcommand_t function)
{
	cmd_function_t	*cmd;

// fail if the command is a variable name
	if (Cvar_VariableString(cmd_name)[0])
	{
		Com_Printf ("Cmd_AddCommand: %s already defined as a var\n", cmd_name);
		return;
	}

// fail if the command already exists
	for (cmd=cmd_functions ; cmd ; cmd=cmd->next)
	{
		if (!Q_strcmp (cmd_name, cmd->name))
		{
			Com_Printf ("Cmd_AddCommand: %s already defined\n", cmd_name);
			return;
		}
	}

	cmd = malloc(sizeof(cmd_function_t));
	if (!cmd)
	{
		Sys_Error("Cmd_AddCommand: out of memory");
		return;
	}
	cmd->name = strdup(cmd_name);
	if (!cmd->name)
	{
		Sys_Error("Cmd_AddCommand: out of memory");
		return;
	}
	cmd->function = function;
	cmd->next = cmd_functions;
	cmd_functions = cmd;
}

/*
============
Cmd_RemoveCommand
============
*/
void	Cmd_RemoveCommand (char *cmd_name)
{
	cmd_function_t	*cmd, **back;

	back = &cmd_functions;
	while (1)
	{
		cmd = *back;
		if (!cmd)
		{
			Com_Printf ("Cmd_RemoveCommand: %s not added\n", cmd_name);
			return;
		}
		if (!strcmp (cmd_name, cmd->name))
		{
			*back = cmd->next;
			free (cmd);
			return;
		}
		back = &cmd->next;
	}
}

/*
============
Cmd_Exists
============
*/
qboolean	Cmd_Exists (char *cmd_name)
{
	cmd_function_t	*cmd;

	for (cmd=cmd_functions ; cmd ; cmd=cmd->next)
	{
		if (!Q_strcmp (cmd_name,cmd->name))
			return true;
	}

	return false;
}



/*
============
Cmd_CompleteCommand
============
*/
char *Cmd_CompleteCommand (char *partial)
{
	cmd_function_t	*cmd;
	int				len;
	cmdalias_t		*a;

	len = Q_strlen(partial);

	if (!len)
		return NULL;

// check for exact match
	for (cmd=cmd_functions ; cmd ; cmd=cmd->next)
		if (!strcmp (partial,cmd->name))
			return cmd->name;
	for (a=cmd_alias ; a ; a=a->next)
		if (!strcmp (partial, a->name))
			return a->name;

// check for partial match
	for (cmd=cmd_functions ; cmd ; cmd=cmd->next)
		if (!strncmp (partial,cmd->name, len))
			return cmd->name;
	for (a=cmd_alias ; a ; a=a->next)
		if (!strncmp (partial, a->name, len))
			return a->name;

	return NULL;
}

#ifdef QUAKE1
/*
===================
Cmd_ForwardToServer

Sends the entire command line over to the server
===================
*/
void Cmd_ForwardToServer (void)
{
	if (cls.state != ca_connected)
	{
		Com_Printf ("Can't \"%s\", not connected\n", Cmd_Argv(0));
		return;
	}

	if (cls.demoplayback)
		return;		// not really connected

	MSG_WriteByte (&cls.message, clc_stringcmd);
	if (Q_strcasecmp(Cmd_Argv(0), "cmd") != 0)
	{
		SZ_Print (&cls.message, Cmd_Argv(0));
		SZ_Print (&cls.message, " ");
	}
	if (Cmd_Argc() > 1)
		SZ_Print (&cls.message, Cmd_Args());
	else
		SZ_Print (&cls.message, "\n");
}

#else

/*
===================
Cmd_ForwardToServer

adds the current command line as a clc_stringcmd to the client message.
things like godmode, noclip, etc, are commands directed to the server,
so when they are typed in at the console, they will need to be forwarded.
===================
*/
void Cmd_ForwardToServer (void)
{
	if (cls.state == ca_disconnected)
	{
		Com_Printf ("Can't \"%s\", not connected\n", Cmd_Argv(0));
		return;
	}

	if (cls.demoplayback)
		return;		// not really connected

	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
	SZ_Print (&cls.netchan.message, Cmd_Argv(0));
	if (Cmd_Argc() > 1)
	{
		SZ_Print (&cls.netchan.message, " ");
		SZ_Print (&cls.netchan.message, Cmd_Args());
	}
}

// don't forward the first argument
void Cmd_ForwardToServer_f (void)
{
	if (cls.state == ca_disconnected)
	{
		Com_Printf ("Can't \"%s\", not connected\n", Cmd_Argv(0));
		return;
	}

	if (Q_strcasecmp(Cmd_Argv(1), "snap") == 0) {
		Cbuf_InsertText ("snap\n");
		return;
	}

	if (cls.demoplayback)
		return;		// not really connected

	if (Cmd_Argc() > 1)
	{
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		SZ_Print (&cls.netchan.message, Cmd_Args());
	}
}
#endif

/*
============
Cmd_ExecuteString

A complete command line has been parsed, so try to execute it
FIXME: lookupnoadd the token to speed search?
============
*/
void	Cmd_ExecuteString (char *text, cmd_source_t src)
{
	cmd_function_t	*cmd;
	cmdalias_t		*a;

	cmd_source = src; /* QUAKE 1 */
	Cmd_TokenizeString (text);

// execute the command line
	if (!Cmd_Argc())
		return;		// no tokens

// check functions
	for (cmd=cmd_functions ; cmd ; cmd=cmd->next)
	{
		if (!Q_strcasecmp (cmd_argv[0],cmd->name))
		{
#ifdef QUAKEWORLD
			if (!cmd->function)
				Cmd_ForwardToServer ();
			else
#endif
				cmd->function ();
			return;
		}
	}

// check alias
	for (a=cmd_alias ; a ; a=a->next)
	{
		if (!Q_strcasecmp (cmd_argv[0], a->name))
		{
			Cbuf_InsertText (a->value);
			return;
		}
	}

// check cvars
	if (!Cvar_Command () && (cl_warncmd->intValue || developer->intValue))
	{
		if(!strncmp(Cmd_Argv(0), "init", 4))
			Com_DPrintf(DEVELOPER_MSG_VERBOSE, "Unknown Command init hack for some servers\n");
		else
			Com_Printf ("Unknown command \"%s\"\n", Cmd_Argv(0));
	}
}




/*
================
Cmd_CheckParm

Returns the position (1 to argc-1) in the command's argument list
where the given parameter apears, or 0 if not present
================
*/
int Cmd_CheckParm (char *parm)
{
	int i;

	if (!parm)
		Sys_Error ("Cmd_CheckParm: NULL");

	for (i = 1; i < Cmd_Argc (); i++)
		if (! Q_strcasecmp (parm, Cmd_Argv (i)))
			return i;

	return 0;
}

void Cmd_ChatInfo (int val)
{
#ifdef QUAKEWORLD
	if (net_broadcast_chat->intValue)
	{
		switch (val)
		{
			case EZQ_CHAT_TYPING:
			case EZQ_CHAT_AFK:
			case EZQ_CHAT_AFK_TYPING:
				Cbuf_AddText( va("setinfo chat %i\n",val) );
				Cvar_ForceSetValue("chat", val);
				afk = val;
				break;
			default:
				afk = 0;
				Cbuf_AddText("setinfo chat \"\"\n");
				Cvar_ForceSetValue("chat", EZQ_CHAT_OFF);
				break;
		}
	}
	else
	{
		afk = 0;
		Cbuf_AddText("setinfo chat \"\"\n");
		Cvar_ForceSetValue("chat", EZQ_CHAT_OFF);
	}
#endif
}

/*
===============
Cbuf_AddEarlyCommands

Adds command line parameters as script statements
Commands lead with a +, and continue until another +

Set commands are added early, so they are guaranteed to be set before
the client and server initialize for the first time.

Other commands are added late, after all initialization is complete.
===============
*/
void Cbuf_AddEarlyCommands (qboolean clear)
{
	int		i;
	char	*s;

	for (i=0 ; i<COM_Argc(); i++)
	{
		s = COM_Argv(i);

		if (strcmp (s, "+set"))
			continue;
		Cbuf_AddText (va("set %s %s\n", COM_Argv(i+1), COM_Argv(i+2)));
		if (clear)
		{
			COM_ClearArgv(i);
			COM_ClearArgv(i+1);
			COM_ClearArgv(i+2);
		}

		i+=2;
	}
}

void Cmd_RemoveAllCommands (void)
{
	cmdalias_t *alias, *aNext;
	cmd_function_t	*cmd, *next;
	int i;

	for (alias=cmd_alias; alias; alias = aNext)
	{
		aNext = alias->next;

		if (alias->value)
		{
			free(alias->value);
			alias->value = NULL;
		}

		free(alias);
		alias = NULL;
	}

	for (cmd = cmd_functions; cmd; cmd = next)
	{
		next = cmd->next;

		if (cmd->name)
		{
			free(cmd->name);
			cmd->name = NULL;
		}

		free(cmd);
		cmd = NULL;
	}

	for (i = 0; i < cmd_argc; i++)
	{
		free(cmd_argv[i]);
		cmd_argv[i] = NULL;
	}
}
