/******
gutil.c
GameSpy C Engine SDK

Copyright 1999 GameSpy Industries, Inc

Suite E-204
2900 Bristol Street
Costa Mesa, CA 92626
(714)549-7689
Fax(714)549-0757

******/

#include "gutil.h"
#include <string.h>

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

	memset(trip, 0, sizeof(trip));
	memset(kwart, 0, sizeof(kwart));

	while (i < size)
	{
		for (pos = 0; pos <= 2; pos++, i++)
		{
			if (i < size)
			{
				trip[pos] = *ins++;
			}
			else
			{
				trip[pos] = '\0';
			}
			kwart[0] = (trip[0]) >> 2;
			kwart[1] = (((trip[0]) & 3) << 4) + ((trip[1]) >> 4);
			kwart[2] = (((trip[1]) & 15) << 2) + ((trip[2]) >> 6);
			kwart[3] = (trip[2]) & 63;
			for (pos = 0; pos <= 3; pos++)
			{
				*result++ = encode_ct(kwart[pos]);
			}
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
		y = (key[x] + state[counter] + y) % 256;
		x = (x + 1) % key_len;
		swap_byte ( &state[counter], &state[y] );
	}

	x = 0; y = 0;
	for ( counter = 0; counter < buffer_len; counter ++)
	{
		x = (x + buffer_ptr[counter] + 1) % 256;
		y = (state[x] + y) % 256;
		swap_byte ( &state[x], &state[y] );
		xorIndex = (state[x] + state[y]) % 256;
		buffer_ptr[counter] ^= state[xorIndex];
	}
}
/*****************************************************************************/
