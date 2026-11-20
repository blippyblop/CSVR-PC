/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//
// menu.cpp
//
// generic menu handler
//
#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"

#include <string.h>
#include <stdio.h>
#include "draw_util.h"

//#include "vgui_TeamFortressViewport.h"

#define MAX_MENU_STRING	512

char g_szMenuString[MAX_MENU_STRING];
char g_szPrelocalisedMenuString[MAX_MENU_STRING];

int KB_ConvertString( char *in, char **ppout );

void Touch_CloseMenu()
{
	gMobileAPI.pfnTouchRemoveButton( "_menu_*" );
	gMobileAPI.pfnTouchSetClientOnly( 0 );
}

int CHudMenu :: Init( void )
{
	gHUD.AddHudElem( this );

	HOOK_MESSAGE( gHUD.m_Menu, ShowMenu );
	HOOK_MESSAGE( gHUD.m_Menu, VGUIMenu );
	HOOK_MESSAGE( gHUD.m_Menu, BuyClose );
	HOOK_MESSAGE( gHUD.m_Menu, AllowSpec );
	HOOK_COMMAND( gHUD.m_Menu, "client_buy_open", OldStyleMenuOpen );
	HOOK_COMMAND( gHUD.m_Menu, "client_buy_close", OldStyleMenuClose );
	HOOK_COMMAND( gHUD.m_Menu, "showvguimenu", ShowVGUIMenu );

	_extended_menus = CVAR_CREATE("_extended_menus",
#ifdef __ANDROID__
		"1",
#else
		"0",
#endif
		FCVAR_ARCHIVE);
	CVAR_CREATE("vr_menu_slots", "0", 0);
	CVAR_CREATE("vr_menu_selection", "0", 0);

	InitHUDData();

	m_bAllowSpec = true; // by default, spectating is allowed

	return 1;
}

void CHudMenu :: InitHUDData( void )
{
	m_fMenuDisplayed = 0;
	m_bitsValidSlots = 0;
	gEngfuncs.Cvar_SetValue("vr_menu_slots", 0);
	Reset();
}

void CHudMenu :: Reset( void )
{
	g_szPrelocalisedMenuString[0] = 0;
	m_fWaitingForMore = FALSE;
}

int CHudMenu :: VidInit( void )
{
	return 1;
}

int CHudMenu :: Draw( float flTime )
{
	// check for if menu is set to disappear
	if ( m_flShutoffTime > 0 )
	{
		if ( m_flShutoffTime <= gHUD.m_flTime )
		{  // times up, shutoff
			UserCmd_OldStyleMenuClose();
			return 1;
		}
	}

	// don't draw the menu if the scoreboard is being shown
	//if ( gViewPort && gViewPort->IsScoreBoardVisible() )
		//return 1;

	// draw the menu, along the left-hand side of the screen

	// count the number of newlines
	int nlc = 0;
	int i;
	for ( i = 0; i < MAX_MENU_STRING && g_szMenuString[i] != '\0'; i++ )
	{
		if ( g_szMenuString[i] == '\n' )
			nlc++;
	}

	// center it
	int y = (ScreenHeight/2) - ((nlc/2)*12) - 40; // make sure it is above the say text
	int x = 20;
	int lineHeight=24, maxX=320;
	float menuScale=0;
#ifndef __ANDROID__
	if (gEngfuncs.pfnGetCvarFloat("vr_enabled") > 0) {
		lineHeight=32; menuScale=2;
		x=(ScreenWidth>704 ? (ScreenWidth-640)/2 : 32);
		y=(ScreenHeight-nlc*lineHeight)/2-40;
		maxX=ScreenWidth-32;
	}
#endif

	i = 0;
	while ( i < MAX_MENU_STRING && g_szMenuString[i] != '\0' )
	{
#ifndef __ANDROID__
		if (gEngfuncs.pfnGetCvarFloat("vr_enabled") > 0) {
			const char* entry=g_szMenuString+i;
			while (*entry=='\\' && entry[1]) entry+=2;
			while (*entry==' ') ++entry;
			int slot=*entry=='0' ? 10 : *entry>='1' && *entry<='9' ? *entry-'0' : 0;
			if (slot && slot==(int)gEngfuncs.pfnGetCvarFloat("vr_menu_selection"))
				DrawUtils::DrawHudString(x-20, y, maxX, ">", 255, 200, 0, menuScale);
		}
#endif
		DrawUtils::DrawHudString( x, y, maxX, g_szMenuString + i, 255, 255, 255, menuScale );
		y += lineHeight;

		while ( i < MAX_MENU_STRING && g_szMenuString[i] != '\0' && g_szMenuString[i] != '\n' )
			i++;
		if ( g_szMenuString[i] == '\n' )
			i++;
	}
	
	return 1;
}

// selects an item from the menu
void CHudMenu :: SelectMenuItem( int menu_item )
{
	// if menu_item is in a valid slot,  send a menuselect command to the server
	if ( (menu_item > 0) && (m_bitsValidSlots & (1 << (menu_item-1))) )
	{
		char szbuf[32];
		sprintf( szbuf, "menuselect %d\n", menu_item );
		ClientCmd( szbuf );

		UserCmd_OldStyleMenuClose();
	}
}


// Message handler for ShowMenu message
// takes four values:
//		short: a bitfield of keys that are valid input
//		char : the duration, in seconds, the menu should stay up. -1 means is stays until something is chosen.
//		byte : a boolean, TRUE if there is more string yet to be received before displaying the menu, FALSE if it's the last string
//		string: menu string to display
// if this message is never received, then scores will simply be the combined totals of the players.
int CHudMenu :: MsgFunc_ShowMenu( const char *pszName, int iSize, void *pbuf )
{
	char *temp = NULL, *menustring;

	BufferReader reader( pszName, pbuf, iSize );

	m_bitsValidSlots = reader.ReadShort();
	int DisplayTime = reader.ReadChar();
	int NeedMore = reader.ReadByte();

	if ( DisplayTime > 0 )
		m_flShutoffTime = DisplayTime + gHUD.m_flTime;
	else
		m_flShutoffTime = -1;

	if ( !m_bitsValidSlots )
	{
		UserCmd_OldStyleMenuClose(); // no valid slots means that the menu should be turned off
		return 1;
	}

	menustring = reader.ReadString();

	// menu will be replaced by scripted touch config
	// so execute it and exit
	if( _extended_menus->value != 0.0f )
	{
		if( !strncmp(menustring, "#Radio", 6 ) )
		{
			if( menustring[6] == 'A' )
			{
				ShowVGUIMenu(MENU_RADIOA); return 1;
			}
			else if( menustring[6] == 'B' )
			{
				ShowVGUIMenu(MENU_RADIOB); return 1;
			}
			else if( menustring[6] == 'C' )
			{
				ShowVGUIMenu(MENU_RADIOC); return 1;
			}
			else ShowVGUIMenu( MENU_NUMERICAL_MENU ); // we just show touch screen numbers
		}
		else ShowVGUIMenu(MENU_NUMERICAL_MENU);
	}
	else ShowVGUIMenu(MENU_NUMERICAL_MENU);

	if ( !m_fWaitingForMore ) // this is the start of a new menu
	{
		strncpy( g_szPrelocalisedMenuString, menustring, MAX_MENU_STRING - 1 );
	}
	else
	{  // append to the current menu string
		strncat( g_szPrelocalisedMenuString, menustring, MAX_MENU_STRING - strlen(g_szPrelocalisedMenuString) - 1 );
	}
	g_szPrelocalisedMenuString[MAX_MENU_STRING-1] = 0;  // ensure null termination (strncat/strncpy does not)

	if ( !NeedMore )
	{  // we have the whole string, so we can localise it now
		strncpy( g_szMenuString, gHUD.m_TextMessage.BufferedLocaliseTextString( g_szPrelocalisedMenuString ), MAX_MENU_STRING );
		g_szMenuString[MAX_MENU_STRING-1] = 0;
		// Swap in characters
		if ( KB_ConvertString( g_szMenuString, &temp ) )
		{
			strncpy( g_szMenuString, temp, MAX_MENU_STRING );
			g_szMenuString[MAX_MENU_STRING-1] = 0;
			free( temp );
		}
	}

	m_fMenuDisplayed = 1;
	m_iFlags |= HUD_DRAW;

	m_fWaitingForMore = NeedMore;
	gEngfuncs.Cvar_SetValue("vr_menu_slots", NeedMore ? 0 : (m_bitsValidSlots & 0x3ff));
	gEngfuncs.Cvar_SetValue("vr_menu_selection", 0);

	return 1;
}

int CHudMenu::MsgFunc_VGUIMenu( const char *pszName, int iSize, void *pbuf )
{
	BufferReader reader( pszName, pbuf, iSize );

	int menuType = reader.ReadByte();
	m_bitsValidSlots = reader.ReadShort(); // is ignored

	ShowVGUIMenu(menuType);
	return 1;
}

int CHudMenu::MsgFunc_BuyClose(const char *pszName, int iSize, void *pbuf)
{
#ifndef __ANDROID__
	UserCmd_OldStyleMenuClose();
#else
	Touch_CloseMenu();
#endif

	return 1;
}

int CHudMenu::MsgFunc_AllowSpec(const char *pszName, int iSize, void *pbuf)
{
	BufferReader reader( pszName, pbuf, iSize );

	m_bAllowSpec = (bool)reader.ReadByte();

	return 1;
}

void CHudMenu::UserCmd_OldStyleMenuOpen()
{
	m_flShutoffTime = -1; // stay open until user will not close it
	strncpy( g_szMenuString, gHUD.m_TextMessage.BufferedLocaliseTextString("Buy"), MAX_MENU_STRING );
	g_szMenuString[MAX_MENU_STRING-1] = 0;
}

void CHudMenu::UserCmd_OldStyleMenuClose()
{
	gEngfuncs.Cvar_SetValue("vr_menu_slots", 0);
	gEngfuncs.Cvar_SetValue("vr_menu_selection", 0);
	m_fMenuDisplayed = 0; // no valid slots means that the menu should be turned off
	m_iFlags &= ~HUD_DRAW;

	Touch_CloseMenu();
}

// lol, no real VGUI here
// it's really good only for touchscreen

void CHudMenu::ShowVGUIMenu( int menuType )
{
#ifndef __ANDROID__
	/* Servers can still send VGUIMenu despite the classic-menu preference.
	 * Resolve the same stock title strings instead of executing touch scripts. */
	if (menuType==MENU_NUMERICAL_MENU) return;
	bool ct=g_PlayerExtraInfo[gHUD.m_Scoreboard.m_iPlayerNum].teamnumber==TEAM_CT;
	const char* title=NULL;
	switch (menuType) {
	case MENU_TEAM: title=(m_bitsValidSlots & (1<<5)) ? "#IG_Team_Select_Spect" : (m_bitsValidSlots & (1<<9)) ? "#IG_Team_Select" : "#Team_Select"; break;
	case MENU_CLASS_T: title="#Terrorist_Select"; break;
	case MENU_CLASS_CT: title="#CT_Select"; break;
	case MENU_BUY: title="#Buy"; break;
	case MENU_BUY_PISTOL: title=ct ? "#CT_BuyPistol" : "#T_BuyPistol"; break;
	case MENU_BUY_SHOTGUN: title="#BuyShotgun"; break;
	case MENU_BUY_SUBMACHINEGUN: title=ct ? "#CT_BuySubMachineGun" : "#T_BuySubMachineGun"; break;
	case MENU_BUY_RIFLE: title=ct ? "#CT_BuyRifle" : "#T_BuyRifle"; break;
	case MENU_BUY_MACHINEGUN: title="#BuyMachineGun"; break;
	case MENU_BUY_ITEM: title=ct ? "#CT_BuyItem" : "#T_BuyItem"; break;
	default: UserCmd_OldStyleMenuClose(); return;
	}
	strncpy(g_szMenuString, gHUD.m_TextMessage.BufferedLocaliseTextString(title), MAX_MENU_STRING-1);
	g_szMenuString[MAX_MENU_STRING-1]=0;
	m_fMenuDisplayed=1; m_iFlags|=HUD_DRAW; m_flShutoffTime=-1; m_fWaitingForMore=FALSE;
	gEngfuncs.Cvar_SetValue("vr_menu_slots", m_bitsValidSlots & 0x3ff);
	gEngfuncs.Cvar_SetValue("vr_menu_selection", 0);
	return;
#endif
	const char *szCmd;

	switch(menuType)
	{
	case MENU_TEAM:
		szCmd = "exec touch/chooseteam.cfg";
		break;
	case MENU_CLASS_T:
		szCmd = "exec touch/chooseteam_tr.cfg";
		break;
	case MENU_CLASS_CT:
		szCmd = "exec touch/chooseteam_ct.cfg";
		break;
	case MENU_BUY:
		szCmd = "exec touch/buy.cfg";
		break;
	case MENU_BUY_PISTOL:
		if( g_PlayerExtraInfo[gHUD.m_Scoreboard.m_iPlayerNum].teamnumber == TEAM_TERRORIST )
			szCmd = "exec touch/buy_pistol_t.cfg";
		else szCmd = "exec touch/buy_pistol_ct.cfg";
		break;
	case MENU_BUY_SHOTGUN:
		if( g_PlayerExtraInfo[gHUD.m_Scoreboard.m_iPlayerNum].teamnumber == TEAM_TERRORIST )
			szCmd = "exec touch/buy_shotgun_t.cfg";
		else szCmd = "exec touch/buy_shotgun_ct.cfg";
		break;
	case MENU_BUY_RIFLE:
		if( g_PlayerExtraInfo[gHUD.m_Scoreboard.m_iPlayerNum].teamnumber == TEAM_TERRORIST )
			szCmd = "exec touch/buy_rifle_t.cfg";
		else szCmd ="exec touch/buy_rifle_ct.cfg";
		break;
	case MENU_BUY_SUBMACHINEGUN:
		if( g_PlayerExtraInfo[gHUD.m_Scoreboard.m_iPlayerNum].teamnumber == TEAM_TERRORIST )
			szCmd = "exec touch/buy_submachinegun_t.cfg";
		else szCmd = "exec touch/buy_submachinegun_ct.cfg";
		break;
	case MENU_BUY_MACHINEGUN:
		if( g_PlayerExtraInfo[gHUD.m_Scoreboard.m_iPlayerNum].teamnumber == TEAM_TERRORIST )
			szCmd = "exec touch/buy_machinegun_t.cfg";
		else szCmd = "exec touch/buy_machinegun_ct.cfg";
		break;
	case MENU_BUY_ITEM:
		if( g_PlayerExtraInfo[gHUD.m_Scoreboard.m_iPlayerNum].teamnumber == TEAM_TERRORIST )
			szCmd = "exec touch/buy_item_t.cfg";
		else szCmd = "exec touch/buy_item_ct.cfg";
		break;
	case MENU_RADIOA:
		szCmd = "exec touch/radioa.cfg";
		break;
	case MENU_RADIOB:
		szCmd = "exec touch/radiob.cfg";
		break;
	case MENU_RADIOC:
		szCmd = "exec touch/radioc.cfg";
		break;
	case MENU_RADIOSELECTOR:
		szCmd = "exec touch/radioselector.cfg";
		break;
	case MENU_NUMERICAL_MENU:
#ifdef __ANDROID__
		szCmd = "exec touch/numerical_menu.cfg";
		break;
#else
		return;
#endif
	default:
		UserCmd_OldStyleMenuClose();
		return;
	}

	m_fMenuDisplayed = 1;
	ClientCmd(szCmd);
}

void CHudMenu::UserCmd_ShowVGUIMenu()
{
	if( gEngfuncs.Cmd_Argc() < 2 )
	{
		ConsolePrint("usage: showvguimenu <menuType>\n");
		return;
	}

	int menuType = atoi(gEngfuncs.Cmd_Argv(1));
	ShowVGUIMenu(menuType);
}
