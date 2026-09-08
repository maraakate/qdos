/******
goautil.c
GameSpy Developer SDK

Copyright 1999 GameSpy Industries, Inc

Suite E-204
2900 Bristol Street
Costa Mesa, CA 92626
(714)549-7689
Fax(714)549-0757

******

 Please see the  GameSpy Developer SDK documentation for more 
 information

 Updated 6/9/99 - DDW
  Added get_sockaddrin function, and use for resolving
  Made portable with additions from CEngine code
  Double check that we don't bind to localhost (instead use INADDR_ANY)

******/

/********
INCLUDES
********/
#include "../nonport.h"
#include "quakedef.h"
#include "goautil.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

extern cvar_t	*public_server; /* should heartbeats be sent */
#ifdef __cplusplus
extern "C" {
#endif

/********
DEFINES
********/
#define HB_TIME 300000 /* 5 minutes */
#define MAX_DATA_SIZE 1400
#define INBUF_LEN 256
#define BUF_SIZE 1400

/********
GLOBAL VARS
********/
goa_querycallback_t goa_basic_callback = NULL;
goa_querycallback_t goa_info_callback = NULL;
goa_querycallback_t goa_rules_callback = NULL;
goa_querycallback_t goa_players_callback = NULL;
char goa_secret_key[256];

/********
TYPEDEFS
********/
typedef unsigned char uchar;

typedef enum {qtunknown, qtbasic, qtinfo, qtrules, qtplayers, qtstatus, qtpackets, qtecho, qtsecure} query_t;


/********
VARS
********/
static const char *queries[]={"","basic","info","rules","players","status","packets", "echo", "secure"};

static unsigned long lastheartbeat = 0;
static int queryid = 1;
static int packetnumber;
static int qport;
static void *udata = NULL;
static char gname[24];

static SOCKET querysock = INVALID_SOCKET;
static SOCKET hbsock = INVALID_SOCKET;


/********
PROTOTYPES
********/
void send_heartbeat(int statechanged);
void parse_query(char *query, struct sockaddr *sender);
int get_sockaddrin(char *host, int port, struct sockaddr_in *saddr, struct hostent **savehent);


/****************************************************************************/
/* PUBLIC FUNCTIONS */
/****************************************************************************/

/* goa_init: Initializes the sockets, etc. Returns an error value
if an error occured, or 0 otherwise */
int goa_init(int queryport, char *gamename, char *ip, void *userdata)
{
	int lasterror;
	struct sockaddr_in saddr;

	Com_Printf("Initializing GameSpy\n");

	/* FS: Now CVARs */
	if (sv_master_server_ip->string[0] == '\0')
	{
		Com_Printf("Error: sv_master_server_ip is blank!  Setting to default: %s\n", SV_MASTER_ADDR);
		Cvar_Set("sv_master_server_ip", CL_MASTER_ADDR);
	}
	if (sv_master_server_port->intValue <= 0)
	{
		Com_Printf("Error: sv_master_server_port is invalid!  Setting to default: %s\n", SV_MASTER_PORT);
		Cvar_Set("sv_master_server_port", CL_MASTER_PORT);
	}

	//save off query port,gamename and userdata
	qport = queryport;
	strncpy(gname, gamename, 23);
	udata = userdata;

	//create our sockets
	SocketStartUp();

	querysock = socket(PF_INET, SOCK_DGRAM, 0);
	hbsock = socket(PF_INET, SOCK_DGRAM, 0);
	if (INVALID_SOCKET == querysock || INVALID_SOCKET == hbsock)
	{
		return E_GOA_WSOCKERROR;
	}

	get_sockaddrin(ip,queryport,&saddr,NULL);
	if (saddr.sin_addr.s_addr == 0x0100007F) //localhost -- we don't want that!
		saddr.sin_addr.s_addr = INADDR_ANY;

	lasterror = bind(querysock, (struct sockaddr *)&saddr, sizeof(saddr));
	if (lasterror)
	{
		return E_GOA_BINDERROR;
	}

	get_sockaddrin(sv_master_server_ip->string,sv_master_server_port->intValue,&saddr,NULL);

	lasterror = connect (hbsock, (struct sockaddr *) &saddr, sizeof(saddr));
	
	if (lasterror)
	{
		return E_GOA_CONNERROR;
	}

	return 0;
}

/* goa_process_queries: Processes any waiting queries, and sends a
heartbeat if 5 minutes have elapsed */
void goa_process_queries(void)
{
	static char indata[INBUF_LEN]; //256 byte input buffer
	struct sockaddr saddr;
	int saddrlen = sizeof(struct sockaddr);
	fd_set set; 
	struct timeval timeout = {0,0};
	int error;
	unsigned int tc = current_time();

	if (INVALID_SOCKET == querysock || INVALID_SOCKET == hbsock)
		return; //no sockets to work with!

	//check if we need to send a heartbet
	if (tc - lastheartbeat > HB_TIME)
		send_heartbeat(0);

	FD_ZERO ( &set );
	FD_SET ( querysock, &set );

	while (1)
	{
		error = selectsocket(FD_SETSIZE, &set, NULL, NULL, &timeout);
		if (SOCKET_ERROR == error || 0 == error)
			return;
		//else we have data
		error = recvfrom(querysock, indata, INBUF_LEN - 1, 0, &saddr, &saddrlen);
		if (error != SOCKET_ERROR)
		{
			indata[error] = '\0';
			Com_DPrintf(DEVELOPER_MSG_GAMESPY, "Query parse: %s\n", indata);
			parse_query(indata, &saddr);
		}
	}
}

/* goa_send_statechanged: Sends a statechanged heartbeat, call when
your gamemode changes */
void goa_send_statechanged(int statechanged)
{
	send_heartbeat(statechanged);
}

/* goa_shutdown: Cleans up the sockets and shuts down */
void goa_shutdown(void)
{
	if (INVALID_SOCKET != querysock)
	{
		closesocket(querysock);
		querysock = INVALID_SOCKET;
	}
		
	if (INVALID_SOCKET != hbsock)
	{
		closesocket(hbsock);
		hbsock = INVALID_SOCKET;
	}

	SocketShutDown();
}

/****************************************************************************/

/* Return a sockaddrin for the given host (numeric or DNS) and port)
Returns the hostent in savehent if it is not NULL */
int get_sockaddrin(char *host, int port, struct sockaddr_in *saddr, struct hostent **savehent)
{
	struct hostent *hent = NULL; /* FS: Compiler Warning */

	saddr->sin_family = AF_INET;
	saddr->sin_port = htons((unsigned short)port);
	if (host == NULL)
		saddr->sin_addr.s_addr = INADDR_ANY;
	else
		saddr->sin_addr.s_addr = inet_addr(host);
	
	if (saddr->sin_addr.s_addr == INADDR_NONE && strcmp(host,"255.255.255.255") != 0)
	{
		hent = gethostbyname(host);
		if (!hent)
			return 0;
		saddr->sin_addr.s_addr = *(u_long *)hent->h_addr_list[0];
	}
	if (savehent != NULL)
		*savehent = hent;
	return 1;
}


/* value_for_key: this returns a value for a certain key in s, where s is a string
containing key\value pairs. If the key does not exist, it returns  NULL. 
Note: the value is stored in a common buffer. If you want to keep it, make a copy! */
char *value_for_key(const char *s, const char *key)
{
	static int valueindex;
	char *pos2;
	const char *pos; 
	char keyspec[256]="\\";
	static char value[2][256];

	valueindex ^= 1;
	strcat(keyspec,key);
	strcat(keyspec,"\\");
	pos = strstr(s,keyspec);
	if (!pos)
		return NULL;
	pos += strlen(keyspec);
	pos2 = value[valueindex];
	while (*pos && *pos != '\\')
		*pos2++ = *pos++;
	*pos2 = '\0';
	return value[valueindex];
}

/*****************************************************************************/
/* Various encryption / encoding routines */

void swap_byte (uchar *a, uchar *b)
{
	uchar swapByte;

	swapByte = *a;
	*a = *b;
	*b = swapByte;
}

uchar encode_ct (uchar c)
{
	if (c <  26) return ('A'+c);
	if (c <  52) return ('a'+c-26);
	if (c <  62) return ('0'+c-52);
	if (c == 62) return ('+');
	if (c == 63) return ('/');

	return 0;
}

void gs_encode (uchar *ins, int size, uchar *result)
{
	int    i,pos;
	uchar  trip[3];
	uchar  kwart[4];

	i=0;
	memset(&trip, 0, sizeof(trip));

	while (i < size)
	{
		for (pos=0 ; pos <= 2 ; pos++, i++)
		{
			if (i < size) trip[pos] = *ins++;
			else trip[pos] = '\0';
			kwart[0] =   (trip[0])       >> 2;
			kwart[1] = (((trip[0]) &  3) << 4) + ((trip[1]) >> 4);
			kwart[2] = (((trip[1]) & 15) << 2) + ((trip[2]) >> 6);
			kwart[3] =   (trip[2]) & 63;
			for (pos=0; pos <= 3; pos++) *result++ = encode_ct(kwart[pos]);
		}
	}
	*result='\0';
}

void gs_encrypt (uchar *key, int key_len, uchar *buffer_ptr, int buffer_len)
{
	short counter;
	uchar x, y, xorIndex;
	uchar state[256];

	for ( counter = 0; counter < 256; counter++) state[counter] = (uchar) counter;

	x = 0; y = 0;
	for ( counter = 0; counter < 256; counter++)
	{
		y = (key[x] + state[counter] + y) & 255;
		x = (x + 1) % key_len;
		swap_byte ( &state[counter], &state[y] );
	}

	x = 0; y = 0;
	for ( counter = 0; counter < buffer_len; counter ++)
	{
		x = (x + buffer_ptr[counter] + 1)& 255;
		y = (state[x] + y) & 255;
		swap_byte ( &state[x], &state[y] );
		xorIndex = (state[x] + state[y])& 255;
		buffer_ptr[counter] ^= state[xorIndex];
	}
}

/*****************************************************************************/

/* packet_send: sends a key\value packet. Appends the queryid
key\value pair. Clears the buffer */
void packet_send(struct sockaddr *addr, char *buffer)
{
	char keyvalue[80];

	if (strlen(buffer) == 0)
		return; //dont need to send an empty one!
	packetnumber++; //packet numbers start at 1
	Com_sprintf(keyvalue,sizeof(keyvalue),"\\queryid\\%d.%d",queryid, packetnumber);
	strcat(buffer,keyvalue);
	sendto(querysock, buffer, strlen(buffer), 0, addr, sizeof(struct sockaddr));
	buffer[0]='\0';
}

/* buffer_send: appends buffer with newdata. If the combined
size would be too large, it flushes buffer first. Space is reserved
on the total size to allow for the queryid key\value */
void buffer_send(struct sockaddr *sender, char *buffer, char *newdata)
{
	char *pos, *lastkey;
	int bcount = 0;

	if (strlen(buffer) + strlen(newdata) < MAX_DATA_SIZE - 50)
	{
		strcat(buffer, newdata);
	} else
	{//test this!
		if (strlen(newdata) > MAX_DATA_SIZE - 50) //incoming data is too big already!
		{
			lastkey = pos = newdata;
			while (pos - newdata < MAX_DATA_SIZE-50)
			{
				if ('\\' == *pos)
				{
					if ((bcount & 1) == 0)
						lastkey = pos;
					bcount++;
				}
				pos++;
			}
			if (lastkey == newdata)
				return; //endless loop - single key was too big!
			*lastkey = '\0';
			buffer_send(sender, buffer, newdata);
			*lastkey = '\\';
			buffer_send(sender, buffer, lastkey); //send the rest!
		} else
		{
			packet_send(sender, buffer);
			strcpy(buffer,newdata);
		}
	}
}

/* send_basic: sends a response to the basic query */
void send_basic(struct sockaddr *sender, char *outbuf)
{
	char keyvalue[BUF_SIZE] = "";

	if(!goa_basic_callback)
		Sys_Error("send_basic: goa_basic_callback is NULL.");

	goa_basic_callback(keyvalue, sizeof(keyvalue), udata);

	buffer_send(sender, outbuf, keyvalue);
}

/* send_info: sends a response to the info query */
void send_info(struct sockaddr *sender, char *outbuf)
{
	char keyvalue[BUF_SIZE] = "";

	if(!goa_info_callback)
		Sys_Error("send_info: goa_info_callback is NULL.");

	goa_info_callback(keyvalue, sizeof(keyvalue), udata);
	buffer_send(sender, outbuf, keyvalue);

}

/* send_rules: sends a response to the rules query. */
void send_rules(struct sockaddr *sender, char *outbuf)
{
	char keyvalue[BUF_SIZE] = "";

	if(!goa_rules_callback)
		Sys_Error("send_rules: goa_rules_callback is NULL.");

	goa_rules_callback(keyvalue, sizeof(keyvalue), udata);
	buffer_send(sender, outbuf, keyvalue);

}

/* send_players: sends the players and their information.*/
void send_players(struct sockaddr *sender, char *outbuf)
{
	char keyvalue[BUF_SIZE] = "";

	if(!goa_players_callback)
		Sys_Error("send_players: goa_players_callback is NULL.");

	goa_players_callback(keyvalue, sizeof(keyvalue), udata);
	buffer_send(sender, outbuf, keyvalue);

}

/* send_echo: bounces the echostr back to sender 
Note: you should always make sure that your echostr doesn't exceed the MAX_DATA_SIZE*/
void send_echo(struct sockaddr *sender, char *outbuf,char *echostr)
{
	char keyvalue[MAX_DATA_SIZE];

	if (strlen(echostr) > MAX_DATA_SIZE - 50)
		return;
	Com_sprintf(keyvalue,sizeof(keyvalue),"\\echo\\%s",echostr);
	buffer_send(sender, outbuf, keyvalue);
}

/* send_final: sends the remaining data in outbuf. Appends the final
key\value to the end. Also adds validation if required. */
void send_final(struct sockaddr *sender, char *outbuf,char *validation)
{
	char keyvalue[256];
	char encrypted_val[128]; //don't need to null terminate
	char encoded_val[200];
	int keylen;
	
	if (validation[0])
	{
		keylen = strlen(validation);
		if (keylen > 128) return;
		strcpy(encrypted_val, validation);
		gs_encrypt((unsigned char *)goa_secret_key, strlen(goa_secret_key), (unsigned char *)encrypted_val, keylen);
		gs_encode((unsigned char *)encrypted_val,keylen, (unsigned char *)encoded_val);
		Com_sprintf(keyvalue,sizeof(keyvalue),"\\validate\\%s",encoded_val);
		buffer_send(sender, outbuf, keyvalue);
	}
	
	Com_sprintf(keyvalue,sizeof(keyvalue),"\\final\\");
	buffer_send(sender, outbuf, keyvalue);
	packet_send(sender, outbuf);
}


/* parse_query: parse an incoming query (which may contain 1 or more
individual queries) and reply to each query */
void parse_query(char *query, struct sockaddr *sender)
{
	query_t querytype;
	char buffer[MAX_DATA_SIZE]="";
	char *value;
	char validation[256] = "";

	queryid++;
	packetnumber = 0;

	for (querytype = qtbasic; querytype <= qtsecure ; querytype = (query_t)((int)querytype + 1))
	{
		if ((value = value_for_key(query, queries[querytype])))
			switch (querytype)
			{
				case qtbasic:
					send_basic(sender,buffer);
					break;
				case qtinfo:
					send_info(sender,buffer);
					break;
				case qtrules:
					send_rules(sender,buffer);
					break;
				case qtplayers:
					send_players(sender,buffer);
					break;
				case qtstatus:
					send_basic(sender,buffer);
					send_info(sender,buffer);
					send_rules(sender,buffer);
					send_players(sender,buffer);
					break;
				case qtpackets:
				/*note: "packets" is NOT a real query type. It is simply here to illustrate
					how a large query would look if broken into packets */
					send_basic(sender,buffer); packet_send(sender, buffer);
					send_info(sender,buffer); packet_send(sender, buffer);
					send_rules(sender,buffer); packet_send(sender, buffer);
					send_players(sender,buffer);
					break;
				case qtecho:
					//note: \echo\value is the syntax here
					send_echo(sender,buffer,value);
					break;
				case qtsecure:
					strcpy(validation, value);
					break;
				case qtunknown:
					break;
			}
	}
	send_final(sender,buffer,validation);
}

/* send_heartbeat: Sends a heartbeat to the gamemaster,
adds \statechanged\ if statechanged != 0 */
void send_heartbeat(int statechanged)
{
	char buf[256];
	extern cvar_t *dedicated;

	if (!dedicated || !dedicated->value)
		return;		// only dedicated servers send heartbeats
  
	if (!public_server->value)
		return;		// a private dedicated game

	Com_DPrintf(DEVELOPER_MSG_SERVER, "Sending a heartbeat to the GameSpy Master Server.\n");
	sprintf(buf,"\\heartbeat\\%d\\gamename\\%s",qport, gname);

	if (statechanged)
	{
		char statechangedBuf[16];

		sprintf(statechangedBuf, "\\statechanged\\%i", statechanged);
		strcat(buf,statechangedBuf);
	}
	send(hbsock, buf, strlen(buf), 0);
	lastheartbeat = current_time();
}

#ifdef __cplusplus
}
#endif
