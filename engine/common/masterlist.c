/*
masterlist.c - multi-master list
Copyright (C) 2018 mittorn

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/
#include "common.h"
#include "netchan.h"
#include "server.h"
#include "client.h"

typedef struct master_s
{
	struct master_s *next;
	qboolean sent; // TODO: get rid of this internal state
	qboolean save;
	qboolean v6only;
	string address;
	netadr_t adr; // temporary, rewritten after each send

	uint heartbeat_challenge;
	double last_heartbeat;
} master_t;

typedef struct masterstatic_s
{
	struct masterstatic_s *next;
	qboolean save;
	char base[MAX_SYSPATH];
} masterstatic_t;

static struct masterlist_s
{
	master_t *list;
	masterstatic_t *static_list;
	int static_in_flight; // counter instead of per-source flag, so clearmasters can't leave a dangling callback pointer
	qboolean modified;
} ml;

static CVAR_DEFINE_AUTO( sv_verbose_heartbeats, "0", 0, "print every heartbeat to console" );

#define HEARTBEAT_SECONDS	((sv_nat.value > 0.0f) ? 60.0f : 300.0f)  	// 1 or 5 minutes

/*
========================
NET_GetMasterHostByName
========================
*/
static net_gai_state_t NET_GetMasterHostByName( master_t *m )
{
	net_gai_state_t res = NET_StringToAdrNB( m->address, &m->adr, m->v6only );

	if( res == NET_EAI_OK )
		return res;

	m->adr.type = 0;
	if( res == NET_EAI_NONAME )
		Con_Reportf( "Can't resolve adr: %s\n", m->address );

	return res;
}

/*
========================
NET_SendToMasters

Send request to all masterservers list
return true if would block
========================
*/
qboolean NET_SendToMasters( netsrc_t sock, size_t len, const void *data )
{
	master_t *list;
	qboolean wait = false;

	for( list = ml.list; list; list = list->next )
	{
		if( list->sent )
			continue;

		switch( NET_GetMasterHostByName( list ))
		{
		case NET_EAI_AGAIN:
			list->sent = false;
			wait = true;
			break;
		case NET_EAI_NONAME:
			list->sent = true;
			break;
		case NET_EAI_OK:
			list->sent = true;
			NET_SendPacket( sock, len, data, list->adr );
			break;
		}
	}

	if( !wait )
	{
		// reset sent state
		for( list = ml.list; list; list = list->next )
			list->sent = false;
	}

	return wait;
}

/*
========================
NET_AnnounceToMaster

========================
*/
static void NET_AnnounceToMaster( master_t *m )
{
	sizebuf_t msg;
	char buf[16];

	m->heartbeat_challenge = COM_RandomLong( 0, INT_MAX );

	MSG_Init( &msg, "Master Join", buf, sizeof( buf ));
	MSG_WriteBytes( &msg, "q\xFF", 2 );
	MSG_WriteDword( &msg, m->heartbeat_challenge );

	NET_SendPacket( NS_SERVER, MSG_GetNumBytesWritten( &msg ), MSG_GetData( &msg ), m->adr );

	if( sv_verbose_heartbeats.value )
	{
		Con_Printf( S_NOTE "sent heartbeat to %s (%s, 0x%x)\n",
			m->address, NET_AdrToString( m->adr ), m->heartbeat_challenge );
	}
}

/*
========================
NET_AnnounceToMaster

========================
*/
void NET_MasterClear( void )
{
	master_t *m;

	for( m = ml.list; m; m = m->next )
		m->last_heartbeat = MAX_HEARTBEAT;
}

/*
========================
NET_QueryServerByAddress

========================
*/
void NET_QueryServerByAddress( netadr_t adr, connprotocol_t proto )
{
	if( proto == PROTO_GOLDSRC )
		Netchan_OutOfBand( NS_CLIENT, adr, sizeof( A2S_GOLDSRC_INFO ), (const byte *)A2S_GOLDSRC_INFO ); // includes null terminator
	else
		Netchan_OutOfBandPrint( NS_CLIENT, adr, A2A_INFO " %i", PROTOCOL_VERSION );
}

/*
========================
NET_ParseMasterStaticBody

Format, one directive per line, '#' and '//' start comments:
ip <address>   Xash3D server (protocol 49)
gs <address>   GoldSrc server (protocol 48)
Lines with unknown directives are skipped for forward compatibility.
========================
*/
static int NET_ParseMasterStaticBody( const byte *body, size_t size )
{
	char line[1024];
	int offset = 0;
	int count = 0;

	while( COM_MemFgets( (byte *)body, size, &offset, line, sizeof( line )) != NULL )
	{
		char token[MAX_TOKEN];
		char *pfile = line;
		netadr_t adr = { 0 };
		qboolean gs;

		pfile = COM_ParseFileSafe( pfile, token, sizeof( token ), PFILE_IGNOREHASHCMT, NULL, NULL );
		if( !pfile || token[0] == '\0' )
			continue;

		if( !Q_strcmp( token, "ip" ))
			gs = false;
		else if( !Q_strcmp( token, "gs" ))
			gs = true;
		else
			continue;

		pfile = COM_ParseFileSafe( pfile, token, sizeof( token ), PFILE_IGNOREHASHCMT, NULL, NULL );
		if( !pfile )
			continue;

		if( !NET_StringToAdr( token, &adr ))
		{
			Con_Reportf( S_WARN "masterstatic: can't parse address \"%s\"\n", token );
			continue;
		}

		if( adr.port == 0 )
			adr.port = MSG_BigShort( PORT_SERVER );

		NET_QueryServerByAddress( adr, gs ? PROTO_GOLDSRC : PROTO_CURRENT );
		count++;
	}

	return count;
}

/*
========================
NET_MasterStaticResponse

========================
*/
static void NET_MasterStaticResponse( const char *url, qboolean success, const byte *data, size_t size, void *userdata )
{
	int count;

	if( ml.static_in_flight > 0 )
		ml.static_in_flight--;

	if( !success || !data || size == 0 )
	{
		Con_Reportf( "masterstatic: %s returned no data\n", url );
		return;
	}

#if !XASH_DEDICATED
	// reset ping base before the queries go out, HTTP latency must not count
	CL_NotifyServerListResponse();
#endif

	NET_Config( true, false ); // allow remote sends

	count = NET_ParseMasterStaticBody( data, size );

	Con_Reportf( "masterstatic: %s yielded %d server(s)\n", url, count );
}

/*
========================
NET_MasterStaticQuery

Request server lists from all static HTTP masters
========================
*/
void NET_MasterStaticQuery( void )
{
	const char *gamedir = GI ? GI->gamefolder : "valve";
	masterstatic_t *ms;

	if( ml.static_in_flight > 0 )
		return;

	for( ms = ml.static_list; ms; ms = ms->next )
	{
		char url[MAX_SYSPATH + 64];

		Q_snprintf( url, sizeof( url ), "%s/v1/servers/%s", ms->base, gamedir );

		if( HTTP_GetToMemory( url, NET_MasterStaticResponse, NULL ))
			ml.static_in_flight++;
	}
}

/*
========================
NET_MasterHeartbeat

========================
*/
void NET_MasterHeartbeat( void )
{
	master_t *m;

	if(( !public_server.value && !sv_nat.value ) || svs.maxclients == 1 )
		return; // only public servers send heartbeats

	for( m = ml.list; m; m = m->next )
	{
		if( host.realtime - m->last_heartbeat < HEARTBEAT_SECONDS )
			continue;

		switch( NET_GetMasterHostByName( m ))
		{
		case NET_EAI_AGAIN:
			m->last_heartbeat = MAX_HEARTBEAT; // retry on next frame
			if( sv_verbose_heartbeats.value )
				Con_Printf( S_NOTE "delay heartbeat to next frame until %s resolves\n", m->address );

			break;
		case NET_EAI_NONAME:
			m->last_heartbeat = host.realtime; // try to resolve again on next heartbeat
			break;
		case NET_EAI_OK:
			m->last_heartbeat = host.realtime;
			NET_AnnounceToMaster( m );
			break;
		}
	}
}

/*
=================
NET_MasterShutdown

Informs all masters that this server is going down
(ignored by master servers in current implementation)
=================
*/
void NET_MasterShutdown( void )
{
	NET_Config( true, false ); // allow remote
	while( NET_SendToMasters( NS_SERVER, 2, "\x62\x0A" ));
}


/*
========================
NET_GetMasterFromAdr

========================
*/
static master_t *NET_GetMasterFromAdr( netadr_t adr )
{
	master_t *master;

	for( master = ml.list; master; master = master->next )
	{
		if( NET_CompareAdr( adr, master->adr ))
			return master;
	}

	return NULL;
}


/*
========================
NET_GetMaster
========================
*/
qboolean NET_GetMaster( netadr_t from, uint *challenge, double *last_heartbeat )
{
	master_t *m;

	m = NET_GetMasterFromAdr( from );

	if( m )
	{
		*challenge = m->heartbeat_challenge;
		*last_heartbeat = m->last_heartbeat;
	}

	return m != NULL;
}

/*
========================
NET_IsMasterAdr

========================
*/
qboolean NET_IsMasterAdr( netadr_t adr )
{
	return NET_GetMasterFromAdr( adr ) != NULL;
}

/*
========================
NET_AddMaster

Add master to the list
========================
*/
static void NET_AddMaster( const char *addr, qboolean save, qboolean v6only )
{
	master_t *master, *last;

	for( last = ml.list; last && last->next; last = last->next )
	{
		if( !Q_strcmp( last->address, addr ) ) // already exists
			return;
	}

	master = Mem_Malloc( host.mempool, sizeof( *master ) );
	Q_strncpy( master->address, addr, sizeof( master->address ));
	master->sent = false;
	master->save = save;
	master->v6only = v6only;
	master->next = NULL;
	master->adr.type = 0;

	// link in
	if( last )
		last->next = master;
	else
		ml.list = master;
}

static void NET_AddMaster_f( void )
{
	if( Cmd_Argc() != 2 )
	{
		Msg( S_USAGE "addmaster <address>\n");
		return;
	}

	NET_AddMaster( Cmd_Argv( 1 ), true, false ); // save them into config
	ml.modified = true; // save config
}

/*
========================
NET_AddMasterStatic

Add static HTTP master base URL to the list
========================
*/
static masterstatic_t *NET_AddMasterStatic( const char *base )
{
	masterstatic_t *ms, *last = NULL;
	size_t len = Q_strlen( base );

	// strip trailing slashes, the query appends its own
	while( len > 0 && base[len - 1] == '/' )
		len--;

	if( len == 0 || len >= sizeof( ms->base ))
		return NULL;

	for( ms = ml.static_list; ms; ms = ms->next )
	{
		if( !Q_strncmp( ms->base, base, len ) && ms->base[len] == '\0' )
			return ms; // already exists

		last = ms;
	}

	ms = Mem_Calloc( host.mempool, sizeof( *ms ));
	Q_strncpy( ms->base, base, len + 1 );

	if( last )
		last->next = ms;
	else
		ml.static_list = ms;

	return ms;
}

static void NET_AddMasterStatic_f( void )
{
	masterstatic_t *ms;

	if( Cmd_Argc() != 2 )
	{
		Msg( S_USAGE "addmasterstatic <base url>\n" );
		return;
	}

	ms = NET_AddMasterStatic( Cmd_Argv( 1 ));

	if( !ms )
	{
		Con_Printf( S_ERROR "invalid static master URL \"%s\"\n", Cmd_Argv( 1 ));
		return;
	}

	ms->save = true;
	ml.modified = true;
}

/*
========================
NET_ClearMasters

Clear master list
========================
*/
static void NET_ClearMasters_f( void )
{
	while( ml.list )
	{
		master_t *prev = ml.list;
		ml.list = ml.list->next;
		Mem_Free( prev );
	}

	while( ml.static_list )
	{
		masterstatic_t *prev = ml.static_list;
		ml.static_list = ml.static_list->next;
		Mem_Free( prev );
	}
}

/*
========================
NET_ListMasters_f

Display current master linked list
========================
*/
static void NET_ListMasters_f( void )
{
	master_t *list;
	masterstatic_t *ms;
	int i;

	Con_Printf( "Master servers:\n" );

	for( i = 1, list = ml.list; list; i++, list = list->next )
	{
		Con_Printf( "%d\t%s", i, list->address );
		if( list->adr.type != 0 )
			Con_Printf( "\t%s\n", NET_AdrToString( list->adr ));
		else Con_Printf( "\n" );
	}

	if( ml.static_list )
		Con_Printf( "Static master base URLs:\n" );

	for( i = 1, ms = ml.static_list; ms; i++, ms = ms->next )
		Con_Printf( "%d\t%s\n", i, ms->base );
}

/*
========================
NET_LoadMasters

Load master server list from xashcomm.lst
========================
*/
static void NET_LoadMasters( void )
{
	byte *afile;
	char *pfile;
	char token[MAX_TOKEN];

	afile = FS_LoadFile( "xashcomm.lst", NULL, true );

	if( !afile ) // file doesn't exist yet
	{
		Con_Reportf( "Cannot load xashcomm.lst\n" );
		return;
	}

	pfile = (char*)afile;

	// format: master <addr>\n
	while( ( pfile = COM_ParseFile( pfile, token, sizeof( token ) ) ) )
	{
		if( !Q_strcmp( token, "master" )) // load addr
		{
			pfile = COM_ParseFile( pfile, token, sizeof( token ) );
			NET_AddMaster( token, true, false );
		}
		else if( !Q_strcmp( token, "master6" ))
		{
			pfile = COM_ParseFile( pfile, token, sizeof( token ) );
			NET_AddMaster( token, true, true );
		}
		else if( !Q_strcmp( token, "masterstatic" ))
		{
			pfile = COM_ParseFile( pfile, token, sizeof( token ) );
			if( pfile )
			{
				masterstatic_t *ms = NET_AddMasterStatic( token );
				if( ms )
					ms->save = true;
			}
		}
	}

	Mem_Free( afile );

	ml.modified = false;
}

/*
========================
NET_SaveMasters

Save master server list to xashcomm.lst, except for default
========================
*/
void NET_SaveMasters( void )
{
	file_t *f;
	master_t *m;
	masterstatic_t *ms;

	if( !ml.modified )
		return;

	f = FS_Open( "xashcomm.lst", "w", true );

	if( !f )
	{
		Con_Reportf( S_ERROR "Couldn't write xashcomm.lst\n" );
		return;
	}

	for( m = ml.list; m; m = m->next )
	{
		if( m->save )
			FS_Printf( f, "%s %s\n", m->v6only ? "master6" : "master", m->address );
	}

	for( ms = ml.static_list; ms; ms = ms->next )
	{
		if( ms->save )
			FS_Printf( f, "masterstatic \"%s\"\n", ms->base );
	}

	FS_Close( f );
}

/*
========================
NET_InitMasters

Initialize master server list
========================
*/
void NET_InitMasters( void )
{
	Cmd_AddRestrictedCommand( "addmaster", NET_AddMaster_f, "add address to masterserver list" );
	Cmd_AddRestrictedCommand( "addmasterstatic", NET_AddMasterStatic_f, "add static HTTP masterserver base URL" );
	Cmd_AddRestrictedCommand( "clearmasters", NET_ClearMasters_f, "clear masterserver list" );
	Cmd_AddCommand( "listmasters", NET_ListMasters_f, "list masterservers" );

	Cvar_RegisterVariable( &sv_verbose_heartbeats );

	// upstream FWGS replaced the dead UDP mentality.rip masters with this static list.
	// do not use fwgs.github.io, the org-wide GitHub Pages URL is xash.su
	NET_AddMasterStatic( "http://xash.su/server-list" );

	NET_LoadMasters( );
}
