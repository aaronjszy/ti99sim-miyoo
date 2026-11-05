//----------------------------------------------------------------------------
//
// File:        libretro-core.cpp
// Date:        31-Oct-2026
// Programmer:  Aaron Szymanski
//
// Description: Libretro API implementation for TI-99/Sim
//
// SPDX-License-Identifier: GPL-2.0-or-later
//
//----------------------------------------------------------------------------

#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

#include "libretro.h"
#include "libretro-core-options.hpp"

#include "common.hpp"
#include "support.hpp"
#include "cartridge.hpp"
#include "device-support.hpp"
#include "tms5220.hpp"
#include "tms9918a-retro.hpp"
#include "tms9919-retro.hpp"
#include "ti994a-retro.hpp"
#include "osk-retro.hpp"

#ifndef VERSION
#define VERSION "0.16.0"
#endif

namespace
{
	// Libretro frontend callbacks
	retro_environment_t        g_EnvironCb    = nullptr;
	retro_video_refresh_t      g_VideoCb      = nullptr;
	retro_audio_sample_t       g_AudioCb      = nullptr;
	retro_audio_sample_batch_t g_AudioBatchCb = nullptr;
	retro_input_poll_t         g_InputPollCb  = nullptr;
	retro_input_state_t        g_InputStateCb = nullptr;
	retro_log_printf_t         g_LogCb        = nullptr;

	bool g_UseBitmasks = false;
	bool g_UseRGB565   = true;

	std::filesystem::path g_SystemDir{ };
	std::filesystem::path g_SaveDir{ };
	std::filesystem::path g_RomPath{ };

	// Emulator instances
	cRefPtr<cRetroTMS9918A> g_VDP;
	cRefPtr<cRetroTMS9919>  g_Sound;
	cRefPtr<iTMS5220>       g_Speech;
	cRefPtr<cRetroTI994A>   g_Computer;
	cRetroOSK               g_OSK;

	int16_t g_AudioBuffer[ 2048 * 2 ];

	// Action binding model for the 10 configurable RetroPad buttons
	enum class ePadActionType
	{
		NONE,
		JOY_FIRE,
		TOGGLE_OSK,
		TOGGLE_ALPHALOCK,
		TI_KEY
	};

	struct sPadBinding
	{
		ePadActionType type  = ePadActionType::NONE;
		VIRTUAL_KEY_E  vkey1 = VK_NONE;
		VIRTUAL_KEY_E  vkey2 = VK_NONE;
	};

	enum ePadSlot
	{
		SLOT_A = 0,
		SLOT_B,
		SLOT_Y,
		SLOT_X,
		SLOT_START,
		SLOT_SELECT,
		SLOT_L,
		SLOT_R,
		SLOT_L2,
		SLOT_R2,
		SLOT_COUNT
	};

	constexpr unsigned SlotRetroIds[ SLOT_COUNT ] =
	{
		RETRO_DEVICE_ID_JOYPAD_A,
		RETRO_DEVICE_ID_JOYPAD_B,
		RETRO_DEVICE_ID_JOYPAD_Y,
		RETRO_DEVICE_ID_JOYPAD_X,
		RETRO_DEVICE_ID_JOYPAD_START,
		RETRO_DEVICE_ID_JOYPAD_SELECT,
		RETRO_DEVICE_ID_JOYPAD_L,
		RETRO_DEVICE_ID_JOYPAD_R,
		RETRO_DEVICE_ID_JOYPAD_L2,
		RETRO_DEVICE_ID_JOYPAD_R2
	};

	constexpr const char *SlotOptionKeys[ SLOT_COUNT ] =
	{
		"ti99sim_pad_a",
		"ti99sim_pad_b",
		"ti99sim_pad_y",
		"ti99sim_pad_x",
		"ti99sim_pad_start",
		"ti99sim_pad_select",
		"ti99sim_pad_l",
		"ti99sim_pad_r",
		"ti99sim_pad_l2",
		"ti99sim_pad_r2"
	};

	constexpr const char *SlotDefaultValues[ SLOT_COUNT ] =
	{
		"JOY_FIRE",
		"1",
		"2",
		"3",
		"ENTER",
		"OSK",
		"FCTN_REDO",
		"FCTN_BACK",
		"Y",
		"N"
	};

	// Keysym slots in cTMS9901::m_KSLinkTable (0..511)
	//   0       : Alpha Lock (VK_CAPSLOCK)
	//   1..399  : Host keyboard (retro_key mapped into 1..399)
	//   450..459: RetroPad buttons (SLOT_A..SLOT_R2)
	//   480..483: D-pad cursor keys (Up/Down/Left/Right)
	//   490..493: Built-in OSK
	constexpr int KEYSYM_PAD_BASE  = 450;
	constexpr int KEYSYM_DPAD_BASE = 480;

	sPadBinding g_PadBindings[ SLOT_COUNT ];

	enum class eDpadCursorMode
	{
		DISABLED,
		ESDX,
		ARROWS
	};

	enum class eDpadGateMode
	{
		GATE_4WAY,
		GATE_8WAY
	};

	enum class eLastAxis
	{
		NONE = 0,
		HORIZONTAL,
		VERTICAL
	};

	eDpadCursorMode g_DpadCursorMode = eDpadCursorMode::DISABLED;
	eDpadGateMode   g_DpadGateMode   = eDpadGateMode::GATE_4WAY;

	// Input state tracking for edge detection, inter-axis gate, and Last-Input-Wins SOCD
	bool      g_PrevSlotPressed[ SLOT_COUNT ] = { };
	bool      g_PrevDpadCursor[ 4 ]           = { };
	bool      g_PrevLeft[ 2 ]                 = { };
	bool      g_PrevRight[ 2 ]                = { };
	bool      g_PrevUp[ 2 ]                   = { };
	bool      g_PrevDown[ 2 ]                 = { };
	int       g_LastHorizDir[ 2 ]             = { };
	int       g_LastVertDir[ 2 ]              = { };
	eLastAxis g_LastAxis[ 2 ]                 = { };

	void FallbackLog( enum retro_log_level level, const char *fmt, ... )
	{
		( void ) level;
		va_list args;
		va_start( args, fmt );
		vfprintf( stderr, fmt, args );
		va_end( args );
	}

	void ShowNotification( const char *message, unsigned durationMs = 3000, enum retro_log_level level = RETRO_LOG_INFO )
	{
		if(( g_EnvironCb == nullptr ) || ( message == nullptr ))
		{
			return;
		}

		unsigned msgVer = 0;
		if( g_EnvironCb( RETRO_ENVIRONMENT_GET_MESSAGE_INTERFACE_VERSION, &msgVer ) && ( msgVer >= 1 ))
		{
			struct retro_message_ext ext =
			{
				message,
				durationMs,
				1,
				level,
				RETRO_MESSAGE_TARGET_ALL,
				RETRO_MESSAGE_TYPE_NOTIFICATION,
				-1
			};
			if( g_EnvironCb( RETRO_ENVIRONMENT_SET_MESSAGE_EXT, &ext ))
			{
				return;
			}
		}

		struct retro_message msg = { message, 180 };
		g_EnvironCb( RETRO_ENVIRONMENT_SET_MESSAGE, &msg );
	}

	std::string Trim( const std::string &s )
	{
		size_t b = 0;
		while(( b < s.size( )) && std::isspace( static_cast<unsigned char>( s[ b ] )))
		{
			b++;
		}
		size_t e = s.size( );
		while(( e > b ) && std::isspace( static_cast<unsigned char>( s[ e - 1 ] )))
		{
			e--;
		}
		return s.substr( b, e - b );
	}

	std::string ToUpper( std::string s )
	{
		for( char &c : s )
		{
			c = static_cast<char>( std::toupper( static_cast<unsigned char>( c )));
		}
		return s;
	}

	VIRTUAL_KEY_E ParseSingleVKey( const std::string &token )
	{
		std::string u = ToUpper( Trim( token ));
		if( u.rfind( "VK_", 0 ) == 0 )
		{
			u = u.substr( 3 );
		}

		if(( u.size( ) == 1 ) && ( u[ 0 ] >= 'A' ) && ( u[ 0 ] <= 'Z' ))
		{
			return static_cast<VIRTUAL_KEY_E>( VK_A + ( u[ 0 ] - 'A' ));
		}
		if(( u.size( ) == 1 ) && ( u[ 0 ] >= '0' ) && ( u[ 0 ] <= '9' ))
		{
			return static_cast<VIRTUAL_KEY_E>( VK_0 + ( u[ 0 ] - '0' ));
		}

		if( u == "ENTER" || u == "RETURN" )         return VK_ENTER;
		if( u == "SPACE" )                          return VK_SPACE;
		if( u == "COMMA" || u == "," )              return VK_COMMA;
		if( u == "PERIOD" || u == "." )             return VK_PERIOD;
		if( u == "DIVIDE" || u == "SLASH" || u == "/" ) return VK_DIVIDE;
		if( u == "SEMICOLON" || u == ";" )          return VK_SEMICOLON;
		if( u == "EQUALS" || u == "=" )             return VK_EQUALS;
		if( u == "CAPSLOCK" || u == "ALPHALOCK" )   return VK_CAPSLOCK;
		if( u == "SHIFT" )                          return VK_SHIFT;
		if( u == "CTRL" || u == "CONTROL" )         return VK_CTRL;
		if( u == "FCTN" || u == "FUNCTION" || u == "ALT" ) return VK_FCTN;

		return VK_NONE;
	}

	sPadBinding ParseBindingValue( const char *val )
	{
		sPadBinding b;
		if(( val == nullptr ) || ( val[ 0 ] == '\0' ))
		{
			return b;
		}

		std::string u = ToUpper( Trim( val ));

		if( u == "NONE" )
		{
			return b;
		}
		if( u == "JOY_FIRE" || u == "FIRE" )
		{
			b.type = ePadActionType::JOY_FIRE;
			return b;
		}
		if( u == "OSK" || u == "KEYBOARD" )
		{
			b.type = ePadActionType::TOGGLE_OSK;
			return b;
		}
		if( u == "ALPHA_LOCK" || u == "ALPHALOCK_TOGGLE" )
		{
			b.type = ePadActionType::TOGGLE_ALPHALOCK;
			return b;
		}

		// Named FCTN shortcuts from Core Options
		if( u == "FCTN_DEL" )     { b.type = ePadActionType::TI_KEY; b.vkey1 = VK_FCTN; b.vkey2 = VK_1;      return b; }
		if( u == "FCTN_INS" )     { b.type = ePadActionType::TI_KEY; b.vkey1 = VK_FCTN; b.vkey2 = VK_2;      return b; }
		if( u == "FCTN_ERASE" )   { b.type = ePadActionType::TI_KEY; b.vkey1 = VK_FCTN; b.vkey2 = VK_3;      return b; }
		if( u == "FCTN_CLEAR" )   { b.type = ePadActionType::TI_KEY; b.vkey1 = VK_FCTN; b.vkey2 = VK_4;      return b; }
		if( u == "FCTN_BEGIN" )   { b.type = ePadActionType::TI_KEY; b.vkey1 = VK_FCTN; b.vkey2 = VK_5;      return b; }
		if( u == "FCTN_PROCEED" ) { b.type = ePadActionType::TI_KEY; b.vkey1 = VK_FCTN; b.vkey2 = VK_6;      return b; }
		if( u == "FCTN_AID" )     { b.type = ePadActionType::TI_KEY; b.vkey1 = VK_FCTN; b.vkey2 = VK_7;      return b; }
		if( u == "FCTN_REDO" )    { b.type = ePadActionType::TI_KEY; b.vkey1 = VK_FCTN; b.vkey2 = VK_8;      return b; }
		if( u == "FCTN_BACK" )    { b.type = ePadActionType::TI_KEY; b.vkey1 = VK_FCTN; b.vkey2 = VK_9;      return b; }
		if( u == "FCTN_QUIT" )    { b.type = ePadActionType::TI_KEY; b.vkey1 = VK_FCTN; b.vkey2 = VK_EQUALS; return b; }

		// Plus-separated combo from .ini files (e.g. "FCTN+8" or "SHIFT+C")
		auto plusPos = u.find( '+' );
		if( plusPos != std::string::npos )
		{
			VIRTUAL_KEY_E k1 = ParseSingleVKey( u.substr( 0, plusPos ));
			VIRTUAL_KEY_E k2 = ParseSingleVKey( u.substr( plusPos + 1 ));
			if( k1 != VK_NONE )
			{
				b.type  = ePadActionType::TI_KEY;
				b.vkey1 = k1;
				b.vkey2 = k2;
			}
			return b;
		}

		VIRTUAL_KEY_E k = ParseSingleVKey( u );
		if( k != VK_NONE )
		{
			b.type  = ePadActionType::TI_KEY;
			b.vkey1 = k;
		}
		return b;
	}

	void UpdateCoreOptions( bool isInitialLoad )
	{
		if( g_EnvironCb == nullptr )
		{
			return;
		}

		bool geometryChanged = false;
		bool timingChanged   = false;

		struct retro_variable var = { nullptr, nullptr };

		// 1. System Region
		var.key   = "ti99sim_region";
		var.value = nullptr;
		if( g_EnvironCb( RETRO_ENVIRONMENT_GET_VARIABLE, &var ) && ( var.value != nullptr ))
		{
			int newHz = ( strcmp( var.value, "pal" ) == 0 ) ? 50 : 60;
			if(( g_Computer != nullptr ) && ( g_Computer->GetRefreshRate( ) != newHz ))
			{
				g_Computer->SetRefreshRate( newHz );
				timingChanged = true;
			}
		}

		// CPU Execution Speed (Clock Rate)
		var.key   = "ti99sim_cpu_speed";
		var.value = nullptr;
		if( g_EnvironCb( RETRO_ENVIRONMENT_GET_VARIABLE, &var ) && ( var.value != nullptr ))
		{
			int speed = std::atoi( var.value );
			if( g_Computer != nullptr )
			{
				g_Computer->SetCPUSpeed( speed );
			}
		}

		// 2. 32K RAM Expansion
		var.key   = "ti99sim_ram_32k";
		var.value = nullptr;
		if( g_EnvironCb( RETRO_ENVIRONMENT_GET_VARIABLE, &var ) && ( var.value != nullptr ))
		{
			bool enable32K = ( strcmp( var.value, "disabled" ) != 0 );
			if( g_Computer != nullptr )
			{
				g_Computer->Set32KExpansion( enable32K );
			}
		}

		// 3. Speech Synthesizer
		var.key   = "ti99sim_speech";
		var.value = nullptr;
		if( g_EnvironCb( RETRO_ENVIRONMENT_GET_VARIABLE, &var ) && ( var.value != nullptr ))
		{
			bool enableSpeech = ( strcmp( var.value, "disabled" ) != 0 );
			if( g_Computer != nullptr )
			{
				g_Computer->SetSpeechEnabled( enableSpeech );
			}
		}

		// 4. Alpha Lock + Joystick Up Diode Fix
		var.key   = "ti99sim_alphalock_fix";
		var.value = nullptr;
		if( g_EnvironCb( RETRO_ENVIRONMENT_GET_VARIABLE, &var ) && ( var.value != nullptr ))
		{
			bool enableFix = ( strcmp( var.value, "disabled" ) != 0 );
			if( g_Computer != nullptr )
			{
				g_Computer->SetAlphaLockFix( enableFix );
			}
		}

		// 5. Color Palette
		var.key   = "ti99sim_palette";
		var.value = nullptr;
		if( g_EnvironCb( RETRO_ENVIRONMENT_GET_VARIABLE, &var ) && ( var.value != nullptr ))
		{
			eRetroPalette pal = eRetroPalette::DEFAULT;
			if( strcmp( var.value, "ntsc" ) == 0 )      pal = eRetroPalette::NTSC;
			else if( strcmp( var.value, "rgb" ) == 0 )  pal = eRetroPalette::RGB;
			else if( strcmp( var.value, "mono" ) == 0 ) pal = eRetroPalette::MONO;

			if( g_VDP != nullptr )
			{
				g_VDP->SetPalette( pal );
			}
		}

		// 6. Overscan Borders
		var.key   = "ti99sim_borders";
		var.value = nullptr;
		if( g_EnvironCb( RETRO_ENVIRONMENT_GET_VARIABLE, &var ) && ( var.value != nullptr ))
		{
			bool showBorders = ( strcmp( var.value, "enabled" ) == 0 );
			if(( g_VDP != nullptr ) && ( g_VDP->GetShowBorders( ) != showBorders ))
			{
				g_VDP->SetShowBorders( showBorders );
				geometryChanged = true;
			}
		}

		// 7. On-Screen Keyboard Default Position
		var.key   = "ti99sim_osk_position";
		var.value = nullptr;
		if( g_EnvironCb( RETRO_ENVIRONMENT_GET_VARIABLE, &var ) && ( var.value != nullptr ))
		{
			g_OSK.SetTopPosition( strcmp( var.value, "top" ) == 0 );
		}

		// 8. PSG Sound Effects Volume
		var.key   = "ti99sim_volume_psg";
		var.value = nullptr;
		if( g_EnvironCb( RETRO_ENVIRONMENT_GET_VARIABLE, &var ) && ( var.value != nullptr ))
		{
			int psgVol = std::atoi( var.value );
			if( g_Sound != nullptr )
			{
				g_Sound->SetPsgVolume( psgVol );
			}
		}

		// 9. TMS5220 Speech Synthesizer Volume
		var.key   = "ti99sim_volume_speech";
		var.value = nullptr;
		if( g_EnvironCb( RETRO_ENVIRONMENT_GET_VARIABLE, &var ) && ( var.value != nullptr ))
		{
			int speechVol = std::atoi( var.value );
			if( g_Sound != nullptr )
			{
				g_Sound->SetSpeechVolume( speechVol );
			}
		}

		// 10. D-Pad Directional Gate
		var.key   = "ti99sim_dpad_gate";
		var.value = nullptr;
		if( g_EnvironCb( RETRO_ENVIRONMENT_GET_VARIABLE, &var ) && ( var.value != nullptr ))
		{
			if( strcmp( var.value, "8way" ) == 0 )
			{
				g_DpadGateMode = eDpadGateMode::GATE_8WAY;
			}
			else
			{
				g_DpadGateMode = eDpadGateMode::GATE_4WAY;
			}
		}

		// 11. D-Pad Cursor Mode
		var.key   = "ti99sim_dpad_cursor";
		var.value = nullptr;
		if( g_EnvironCb( RETRO_ENVIRONMENT_GET_VARIABLE, &var ) && ( var.value != nullptr ))
		{
			if( strcmp( var.value, "esdx" ) == 0 )        g_DpadCursorMode = eDpadCursorMode::ESDX;
			else if( strcmp( var.value, "arrows" ) == 0 ) g_DpadCursorMode = eDpadCursorMode::ARROWS;
			else                                          g_DpadCursorMode = eDpadCursorMode::DISABLED;
		}

		// 12. Button Mappings
		for( int i = 0; i < SLOT_COUNT; i++ )
		{
			var.key   = SlotOptionKeys[ i ];
			var.value = nullptr;
			const char *optVal = SlotDefaultValues[ i ];
			if( g_EnvironCb( RETRO_ENVIRONMENT_GET_VARIABLE, &var ) && ( var.value != nullptr ))
			{
				optVal = var.value;
			}

			// Release any currently held virtual key on this slot before rebinding
			if(( g_Computer != nullptr ) && ( g_Computer->GetPIC( ) != nullptr ))
			{
				g_Computer->GetPIC( )->VKeyUp( KEYSYM_PAD_BASE + i );
			}

			g_PadBindings[ i ] = ParseBindingValue( optVal );
		}

		if( !isInitialLoad && ( geometryChanged || timingChanged ))
		{
			struct retro_system_av_info av_info;
			retro_get_system_av_info( &av_info );
			if( timingChanged )
			{
				g_EnvironCb( RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &av_info );
			}
			else
			{
				g_EnvironCb( RETRO_ENVIRONMENT_SET_GEOMETRY, &av_info.geometry );
			}
		}
	}

	// Map Libretro keyboard events (physical keyboard / Game Focus mode) to TI-99/4A matrix
	void RETRO_CALLCONV KeyboardCallback( bool down, unsigned keycode, uint32_t character, uint16_t key_modifiers )
	{
		( void ) character;
		( void ) key_modifiers;

		if(( g_Computer == nullptr ) || ( g_Computer->GetPIC( ) == nullptr ))
		{
			return;
		}

		iTMS9901 *pic = g_Computer->GetPIC( );
		int sym = static_cast<int>( keycode );
		if(( sym <= 0 ) || ( sym >= 400 ))
		{
			return;
		}

		if( keycode == RETROK_CAPSLOCK )
		{
			if( down )
			{
				bool nextState = !g_OSK.IsAlphaLock( );
				g_OSK.SetAlphaLock( nextState, pic );
				ShowNotification( nextState ? "Alpha Lock: ON" : "Alpha Lock: OFF", 1500 );
			}
			return;
		}

		if( !down )
		{
			pic->VKeyUp( sym );
			return;
		}

		if(( keycode >= RETROK_a ) && ( keycode <= RETROK_z ))
		{
			pic->VKeysDown( sym, static_cast<VIRTUAL_KEY_E>( VK_A + ( keycode - RETROK_a )));
			return;
		}
		if(( keycode >= RETROK_0 ) && ( keycode <= RETROK_9 ))
		{
			pic->VKeysDown( sym, static_cast<VIRTUAL_KEY_E>( VK_0 + ( keycode - RETROK_0 )));
			return;
		}

		switch( keycode )
		{
			case RETROK_RETURN :
			case RETROK_KP_ENTER :
				pic->VKeysDown( sym, VK_ENTER );
				break;
			case RETROK_SPACE :
				pic->VKeysDown( sym, VK_SPACE );
				break;
			case RETROK_COMMA :
				pic->VKeysDown( sym, VK_COMMA );
				break;
			case RETROK_PERIOD :
			case RETROK_KP_PERIOD :
				pic->VKeysDown( sym, VK_PERIOD );
				break;
			case RETROK_SLASH :
			case RETROK_KP_DIVIDE :
				pic->VKeysDown( sym, VK_DIVIDE );
				break;
			case RETROK_MINUS :
			case RETROK_KP_MINUS :
				pic->VKeysDown( sym, VK_SHIFT, VK_DIVIDE );
				break;
			case RETROK_SEMICOLON :
				pic->VKeysDown( sym, VK_SEMICOLON );
				break;
			case RETROK_EQUALS :
			case RETROK_KP_EQUALS :
				pic->VKeysDown( sym, VK_EQUALS );
				break;
			case RETROK_LSHIFT :
			case RETROK_RSHIFT :
				pic->VKeysDown( sym, VK_SHIFT );
				break;
			case RETROK_LCTRL :
			case RETROK_RCTRL :
				pic->VKeysDown( sym, VK_CTRL );
				break;
			case RETROK_LALT :
			case RETROK_RALT :
			case RETROK_LSUPER :
			case RETROK_RSUPER :
				pic->VKeysDown( sym, VK_FCTN );
				break;
			case RETROK_UP :
				pic->VKeysDown( sym, VK_FCTN, VK_E );
				break;
			case RETROK_DOWN :
				pic->VKeysDown( sym, VK_FCTN, VK_X );
				break;
			case RETROK_LEFT :
			case RETROK_BACKSPACE :
				pic->VKeysDown( sym, VK_FCTN, VK_S );
				break;
			case RETROK_RIGHT :
				pic->VKeysDown( sym, VK_FCTN, VK_D );
				break;
			case RETROK_ESCAPE :
				pic->VKeysDown( sym, VK_FCTN, VK_9 ); // FCTN+9 (BACK)
				break;
			case RETROK_F1 :
				pic->VKeysDown( sym, VK_FCTN, VK_1 );
				break;
			case RETROK_F2 :
				pic->VKeysDown( sym, VK_FCTN, VK_2 );
				break;
			case RETROK_F3 :
				pic->VKeysDown( sym, VK_FCTN, VK_3 );
				break;
			case RETROK_F4 :
				pic->VKeysDown( sym, VK_FCTN, VK_4 );
				break;
			case RETROK_F5 :
				pic->VKeysDown( sym, VK_FCTN, VK_5 );
				break;
			case RETROK_F6 :
				pic->VKeysDown( sym, VK_FCTN, VK_6 );
				break;
			case RETROK_F7 :
				pic->VKeysDown( sym, VK_FCTN, VK_7 );
				break;
			case RETROK_F8 :
				pic->VKeysDown( sym, VK_FCTN, VK_8 );
				break;
			case RETROK_F9 :
				pic->VKeysDown( sym, VK_FCTN, VK_9 );
				break;
			case RETROK_F10 :
				pic->VKeysDown( sym, VK_FCTN, VK_EQUALS );
				break;
			default :
				break;
		}
	}

	uint32_t ReadJoypadMask( unsigned port )
	{
		if( g_InputStateCb == nullptr )
		{
			return 0;
		}

		if( g_UseBitmasks )
		{
			return static_cast<uint32_t>( static_cast<uint16_t>(
				g_InputStateCb( port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK )));
		}

		uint32_t mask = 0;
		for( unsigned id = 0; id <= RETRO_DEVICE_ID_JOYPAD_R3; id++ )
		{
			if( g_InputStateCb( port, RETRO_DEVICE_JOYPAD, 0, id ))
			{
				mask |= ( 1u << id );
			}
		}
		return mask;
	}

	void ApplyPortDirection( unsigned port, uint32_t padMask, int &outX, int &outY, bool &rawUp, bool &rawDown, bool &rawLeft, bool &rawRight )
	{
		constexpr int16_t ANALOG_DEADZONE = 0x3000;

		int16_t analogX = 0;
		int16_t analogY = 0;
		if( g_InputStateCb != nullptr )
		{
			analogX = g_InputStateCb( port, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X );
			analogY = g_InputStateCb( port, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y );
		}

		rawUp    = (( padMask & ( 1u << RETRO_DEVICE_ID_JOYPAD_UP    )) != 0 ) || ( analogY < -ANALOG_DEADZONE );
		rawDown  = (( padMask & ( 1u << RETRO_DEVICE_ID_JOYPAD_DOWN  )) != 0 ) || ( analogY >  ANALOG_DEADZONE );
		rawLeft  = (( padMask & ( 1u << RETRO_DEVICE_ID_JOYPAD_LEFT  )) != 0 ) || ( analogX < -ANALOG_DEADZONE );
		rawRight = (( padMask & ( 1u << RETRO_DEVICE_ID_JOYPAD_RIGHT )) != 0 ) || ( analogX >  ANALOG_DEADZONE );

		// Detect new edge presses on each axis for inter-axis Last-Input-Wins resolution
		bool newHoriz = ( rawLeft && !g_PrevLeft[ port ] ) || ( rawRight && !g_PrevRight[ port ] );
		bool newVert  = ( rawUp && !g_PrevUp[ port ] ) || ( rawDown && !g_PrevDown[ port ] );

		if( newHoriz && newVert )
		{
			if( std::abs( static_cast<int>( analogX )) > std::abs( static_cast<int>( analogY )))
			{
				g_LastAxis[ port ] = eLastAxis::HORIZONTAL;
			}
			else
			{
				g_LastAxis[ port ] = eLastAxis::VERTICAL;
			}
		}
		else if( newHoriz )
		{
			g_LastAxis[ port ] = eLastAxis::HORIZONTAL;
		}
		else if( newVert )
		{
			g_LastAxis[ port ] = eLastAxis::VERTICAL;
		}

		// Last-Input-Wins SOCD resolution (opposing directions on same axis)
		if( rawLeft && !g_PrevLeft[ port ] )
		{
			g_LastHorizDir[ port ] = -1;
		}
		if( rawRight && !g_PrevRight[ port ] )
		{
			g_LastHorizDir[ port ] = 1;
		}
		if( rawUp && !g_PrevUp[ port ] )
		{
			g_LastVertDir[ port ] = 1;
		}
		if( rawDown && !g_PrevDown[ port ] )
		{
			g_LastVertDir[ port ] = -1;
		}

		g_PrevLeft [ port ] = rawLeft;
		g_PrevRight[ port ] = rawRight;
		g_PrevUp   [ port ] = rawUp;
		g_PrevDown [ port ] = rawDown;

		if( rawLeft && rawRight )
		{
			outX = ( g_LastHorizDir[ port ] != 0 ) ? g_LastHorizDir[ port ] : 0;
		}
		else if( rawLeft )
		{
			outX = -1;
			g_LastHorizDir[ port ] = -1;
		}
		else if( rawRight )
		{
			outX = 1;
			g_LastHorizDir[ port ] = 1;
		}
		else
		{
			outX = 0;
			g_LastHorizDir[ port ] = 0;
		}

		// Note: cTMS9901::ReadCRU expects y_Axis > 0 for Joystick Up (row 7) and y_Axis < 0 for Joystick Down (row 6)
		if( rawUp && rawDown )
		{
			outY = ( g_LastVertDir[ port ] != 0 ) ? g_LastVertDir[ port ] : 0;
		}
		else if( rawUp )
		{
			outY = 1;
			g_LastVertDir[ port ] = 1;
		}
		else if( rawDown )
		{
			outY = -1;
			g_LastVertDir[ port ] = -1;
		}
		else
		{
			outY = 0;
			g_LastVertDir[ port ] = 0;
		}

		// 4-Way Directional Gate (Inter-axis Last-Input-Wins for 4-way games like Parsec & Munch Man)
		if( !rawLeft && !rawRight && !rawUp && !rawDown )
		{
			g_LastAxis[ port ] = eLastAxis::NONE;
		}
		else if(( g_DpadGateMode == eDpadGateMode::GATE_4WAY ) && ( outX != 0 ) && ( outY != 0 ))
		{
			if( g_LastAxis[ port ] == eLastAxis::HORIZONTAL )
			{
				outY = 0;
			}
			else
			{
				outX = 0;
			}
		}
	}

	void PollAndApplyInput( )
	{
		if( g_InputPollCb != nullptr )
		{
			g_InputPollCb( );
		}

		if(( g_Computer == nullptr ) || ( g_Computer->GetPIC( ) == nullptr ))
		{
			return;
		}

		iTMS9901 *pic = g_Computer->GetPIC( );

		uint32_t mask0 = ReadJoypadMask( 0 );
		uint32_t mask1 = ReadJoypadMask( 1 );

		int  joy0X = 0, joy0Y = 0;
		bool rawUp0 = false, rawDown0 = false, rawLeft0 = false, rawRight0 = false;
		ApplyPortDirection( 0, mask0, joy0X, joy0Y, rawUp0, rawDown0, rawLeft0, rawRight0 );

		int  joy1X = 0, joy1Y = 0;
		bool rawUp1 = false, rawDown1 = false, rawLeft1 = false, rawRight1 = false;
		ApplyPortDirection( 1, mask1, joy1X, joy1Y, rawUp1, rawDown1, rawLeft1, rawRight1 );

		// Check if any button bound to TOGGLE_OSK was just pressed on Port 0
		for( int i = 0; i < SLOT_COUNT; i++ )
		{
			if( g_PadBindings[ i ].type == ePadActionType::TOGGLE_OSK )
			{
				bool pressed = ( mask0 & ( 1u << SlotRetroIds[ i ] )) != 0;
				if( pressed && !g_PrevSlotPressed[ i ] )
				{
					// Release any held pad keys before entering/leaving OSK
					for( int s = 0; s < SLOT_COUNT; s++ )
					{
						pic->VKeyUp( KEYSYM_PAD_BASE + s );
					}
					for( int d = 0; d < 4; d++ )
					{
						pic->VKeyUp( KEYSYM_DPAD_BASE + d );
						g_PrevDpadCursor[ d ] = false;
					}
					g_OSK.Toggle( pic );
				}
				g_PrevSlotPressed[ i ] = pressed;
			}
		}

		// When OSK is active, route Port 0 inputs exclusively to the OSK
		if( g_OSK.IsActive( ))
		{
			pic->SetJoystickX( 0, 0 );
			pic->SetJoystickY( 0, 0 );
			pic->SetJoystickButton( 0, false );

			cRetroOSK::sPadInput oskInput =
			{
				joy0Y > 0,
				joy0Y < 0,
				joy0X < 0,
				joy0X > 0,
				( mask0 & ( 1u << RETRO_DEVICE_ID_JOYPAD_A     )) != 0,
				( mask0 & ( 1u << RETRO_DEVICE_ID_JOYPAD_B     )) != 0,
				( mask0 & ( 1u << RETRO_DEVICE_ID_JOYPAD_X     )) != 0,
				( mask0 & ( 1u << RETRO_DEVICE_ID_JOYPAD_Y     )) != 0,
				( mask0 & ( 1u << RETRO_DEVICE_ID_JOYPAD_START )) != 0,
				( mask0 & ( 1u << RETRO_DEVICE_ID_JOYPAD_L     )) != 0,
				( mask0 & ( 1u << RETRO_DEVICE_ID_JOYPAD_R     )) != 0
			};
			g_OSK.Update( oskInput, pic );

			for( int i = 0; i < SLOT_COUNT; i++ )
			{
				if( g_PadBindings[ i ].type != ePadActionType::TOGGLE_OSK )
				{
					g_PrevSlotPressed[ i ] = ( mask0 & ( 1u << SlotRetroIds[ i ] )) != 0;
				}
			}
			return;
		}

		// Apply Joystick 1 & Joystick 2 axes
		pic->SetJoystickX( 0, joy0X );
		pic->SetJoystickY( 0, joy0Y );

		pic->SetJoystickX( 1, joy1X );
		pic->SetJoystickY( 1, joy1Y );
		pic->SetJoystickButton( 1, ( mask1 & ( 1u << RETRO_DEVICE_ID_JOYPAD_A )) != 0 );

		// Optional D-pad cursor keys (E/S/D/X or FCTN+E/S/D/X)
		bool curDirs[ 4 ] =
		{
			( g_DpadCursorMode != eDpadCursorMode::DISABLED ) && ( joy0Y > 0 ), // Up    -> E
			( g_DpadCursorMode != eDpadCursorMode::DISABLED ) && ( joy0Y < 0 ), // Down  -> X
			( g_DpadCursorMode != eDpadCursorMode::DISABLED ) && ( joy0X < 0 ), // Left  -> S
			( g_DpadCursorMode != eDpadCursorMode::DISABLED ) && ( joy0X > 0 )  // Right -> D
		};
		constexpr VIRTUAL_KEY_E CursorVKeys[ 4 ] = { VK_E, VK_X, VK_S, VK_D };

		for( int d = 0; d < 4; d++ )
		{
			if( curDirs[ d ] != g_PrevDpadCursor[ d ] )
			{
				if( curDirs[ d ] )
				{
					if( g_DpadCursorMode == eDpadCursorMode::ARROWS )
					{
						pic->VKeysDown( KEYSYM_DPAD_BASE + d, VK_FCTN, CursorVKeys[ d ] );
					}
					else
					{
						pic->VKeysDown( KEYSYM_DPAD_BASE + d, CursorVKeys[ d ] );
					}
				}
				else
				{
					pic->VKeyUp( KEYSYM_DPAD_BASE + d );
				}
				g_PrevDpadCursor[ d ] = curDirs[ d ];
			}
		}

		// Process the 10 configurable Port 0 buttons
		bool joy0Fire = false;
		for( int i = 0; i < SLOT_COUNT; i++ )
		{
			const sPadBinding &binding = g_PadBindings[ i ];
			bool pressed = ( mask0 & ( 1u << SlotRetroIds[ i ] )) != 0;

			switch( binding.type )
			{
				case ePadActionType::JOY_FIRE :
					if( pressed )
					{
						joy0Fire = true;
					}
					g_PrevSlotPressed[ i ] = pressed;
					break;

				case ePadActionType::TOGGLE_ALPHALOCK :
					if( pressed && !g_PrevSlotPressed[ i ] )
					{
						bool nextState = !g_OSK.IsAlphaLock( );
						g_OSK.SetAlphaLock( nextState, pic );
						ShowNotification( nextState ? "Alpha Lock: ON" : "Alpha Lock: OFF", 1500 );
					}
					g_PrevSlotPressed[ i ] = pressed;
					break;

				case ePadActionType::TI_KEY :
					if( pressed != g_PrevSlotPressed[ i ] )
					{
						if( pressed )
						{
							pic->VKeysDown( KEYSYM_PAD_BASE + i, binding.vkey1, binding.vkey2 );
						}
						else
						{
							pic->VKeyUp( KEYSYM_PAD_BASE + i );
						}
						g_PrevSlotPressed[ i ] = pressed;
					}
					break;

				case ePadActionType::TOGGLE_OSK :
				case ePadActionType::NONE :
					g_PrevSlotPressed[ i ] = pressed;
					break;
			}
		}

		pic->SetJoystickButton( 0, joy0Fire );
	}

	void SetInputDescriptors( )
	{
		if( g_EnvironCb == nullptr )
		{
			return;
		}

		static const struct retro_input_descriptor desc[] =
		{
			{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,   "Joystick 1 Left" },
			{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,     "Joystick 1 Up" },
			{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,   "Joystick 1 Down" },
			{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT,  "Joystick 1 Right" },
			{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,      "Joystick 1 Fire / Custom A" },
			{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,      "Key 1 (Start/Select) / Custom B" },
			{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,      "Key 2 (Start/Select) / Custom Y" },
			{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,      "Key 3 (Option) / Custom X" },
			{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START,  "ENTER / Custom Start" },
			{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Toggle On-Screen Keyboard / Custom Select" },
			{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,      "FCTN+8 (Redo) / Custom L" },
			{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,      "FCTN+9 (Back) / Custom R" },
			{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2,     "Key Y / Custom L2" },
			{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2,     "Key N / Custom R2" },
			{ 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X, "Joystick 1 Analog X" },
			{ 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y, "Joystick 1 Analog Y" },

			{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,   "Joystick 2 Left" },
			{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,     "Joystick 2 Up" },
			{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,   "Joystick 2 Down" },
			{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT,  "Joystick 2 Right" },
			{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,      "Joystick 2 Fire" },
			{ 0, 0, 0, 0, nullptr }
		};

		g_EnvironCb( RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, const_cast<struct retro_input_descriptor *>( desc ));
	}
}

void RetroLog( enum retro_log_level level, const char *fmt, ... )
{
	char buffer[ 512 ];
	va_list args;
	va_start( args, fmt );
	vsnprintf( buffer, sizeof( buffer ), fmt, args );
	va_end( args );

	if( g_LogCb != nullptr )
	{
		g_LogCb( level, "%s", buffer );
	}
	else
	{
		fputs( buffer, stderr );
	}
}

void RetroNotify( const char *message, unsigned durationMs, enum retro_log_level level )
{
	ShowNotification( message, durationMs, level );
}

extern "C"
{

RETRO_API unsigned retro_api_version( void )
{
	return RETRO_API_VERSION;
}

RETRO_API void retro_get_system_info( struct retro_system_info *info )
{
	if( info == nullptr )
	{
		return;
	}
	memset( info, 0, sizeof( *info ));
	info->library_name     = "TI-99Sim";
	info->library_version  = VERSION;
	info->valid_extensions = "ctg|zip|7z";
	info->need_fullpath    = true;
	info->block_extract    = false;
}

RETRO_API void retro_set_environment( retro_environment_t cb )
{
	g_EnvironCb = cb;
	if( cb == nullptr )
	{
		return;
	}

	bool noGame = true;
	cb( RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &noGame );

	libretro_set_core_options( cb );

	static const struct retro_controller_description controllers[] =
	{
		{ "TI-99/4A Joystick + Keypad", RETRO_DEVICE_JOYPAD },
		{ nullptr, 0 }
	};

	static const struct retro_controller_info ports[] =
	{
		{ controllers, 1 },
		{ controllers, 1 },
		{ nullptr, 0 }
	};

	cb( RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, const_cast<struct retro_controller_info *>( ports ));
}

RETRO_API void retro_set_video_refresh( retro_video_refresh_t cb )
{
	g_VideoCb = cb;
}

RETRO_API void retro_set_audio_sample( retro_audio_sample_t cb )
{
	g_AudioCb = cb;
}

RETRO_API void retro_set_audio_sample_batch( retro_audio_sample_batch_t cb )
{
	g_AudioBatchCb = cb;
}

RETRO_API void retro_set_input_poll( retro_input_poll_t cb )
{
	g_InputPollCb = cb;
}

RETRO_API void retro_set_input_state( retro_input_state_t cb )
{
	g_InputStateCb = cb;
}

RETRO_API void retro_set_controller_port_device( unsigned port, unsigned device )
{
	( void ) port;
	( void ) device;
}

RETRO_API void retro_init( void )
{
	struct retro_log_callback log = { nullptr };
	if(( g_EnvironCb != nullptr ) && g_EnvironCb( RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log ) && ( log.log != nullptr ))
	{
		g_LogCb = log.log;
	}
	else
	{
		g_LogCb = FallbackLog;
	}

	const char *sysDir  = nullptr;
	const char *saveDir = nullptr;
	if( g_EnvironCb != nullptr )
	{
		g_EnvironCb( RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &sysDir );
		g_EnvironCb( RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &saveDir );
	}

	g_SystemDir = ( sysDir && sysDir[ 0 ] )   ? std::filesystem::path( sysDir )  : std::filesystem::path( "." );
	g_SaveDir   = ( saveDir && saveDir[ 0 ] ) ? std::filesystem::path( saveDir ) : std::filesystem::path( "." );

	SetLibretroPaths( g_SystemDir, g_SaveDir );

	g_UseBitmasks = false;
	if( g_EnvironCb != nullptr )
	{
		bool bitmasks = false;
		if( g_EnvironCb( RETRO_ENVIRONMENT_GET_INPUT_BITMASKS, &bitmasks ) && bitmasks )
		{
			g_UseBitmasks = true;
		}
	}
}

RETRO_API void retro_deinit( void )
{
	g_Computer = nullptr;
	g_Speech   = nullptr;
	g_Sound    = nullptr;
	g_VDP      = nullptr;
}

RETRO_API void retro_get_system_av_info( struct retro_system_av_info *info )
{
	if( info == nullptr )
	{
		return;
	}

	unsigned width  = ( g_VDP != nullptr ) ? g_VDP->GetOutputWidth( )  : VDP_WIDTH;
	unsigned height = ( g_VDP != nullptr ) ? g_VDP->GetOutputHeight( ) : VDP_HEIGHT;
	int      hz     = ( g_Computer != nullptr ) ? g_Computer->GetRefreshRate( ) : 60;

	memset( info, 0, sizeof( *info ));
	info->geometry.base_width   = width;
	info->geometry.base_height  = height;
	info->geometry.max_width    = RETRO_VDP_BORDER_WIDTH;
	info->geometry.max_height   = RETRO_VDP_BORDER_HEIGHT;
	info->geometry.aspect_ratio = 4.0f / 3.0f;

	info->timing.fps         = static_cast<double>( hz );
	info->timing.sample_rate = 44100.0;
}

RETRO_API bool retro_load_game( const struct retro_game_info *game )
{
	// Negotiate pixel format (RGB565 preferred for Miyoo Mini, fallback to XRGB8888)
	g_UseRGB565 = true;
	if( g_EnvironCb != nullptr )
	{
		enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;
		if( !g_EnvironCb( RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt ))
		{
			fmt = RETRO_PIXEL_FORMAT_XRGB8888;
			if( g_EnvironCb( RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt ))
			{
				g_UseRGB565 = false;
			}
		}

		uint64_t quirks = 0;
		g_EnvironCb( RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS, &quirks );

		struct retro_keyboard_callback kb_cb = { KeyboardCallback };
		g_EnvironCb( RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK, &kb_cb );
	}

	SetInputDescriptors( );

	// Refresh system/save directories in case frontend set them after retro_init
	const char *sysDir  = nullptr;
	const char *saveDir = nullptr;
	if( g_EnvironCb != nullptr )
	{
		g_EnvironCb( RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &sysDir );
		g_EnvironCb( RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &saveDir );
	}
	if( sysDir && sysDir[ 0 ] )   g_SystemDir = sysDir;
	if( saveDir && saveDir[ 0 ] ) g_SaveDir   = saveDir;
	SetLibretroPaths( g_SystemDir, g_SaveDir );

	// Locate TI-994A.ctg console ROM
	std::string consolePath = LocateCartridge( "console", "TI-994A.ctg",
		{
			"0264512c7d9e7fa091a48e5c8734782ea031a52d",
			"16e275faae427465ba4dd4c2bf8569f6546d32dd"
		} );

	if( consolePath.empty( ))
	{
		g_LogCb( RETRO_LOG_ERROR, "[ti99sim] Unable to locate console ROM 'TI-994A.ctg' in '%s/ti99sim/' or '/mnt/SDCARD/Emu/TI99/console/'!\n", g_SystemDir.c_str( ));
		ShowNotification( "Missing TI-994A.ctg! Copy console ROMs to BIOS/ti99sim/", 5000, RETRO_LOG_ERROR );
		return false;
	}

	cRefPtr<cCartridge> consoleROM = new cCartridge( consolePath );
	if( !consoleROM->IsValid( ))
	{
		g_LogCb( RETRO_LOG_ERROR, "[ti99sim] Console ROM '%s' is invalid!\n", consolePath.c_str( ));
		ShowNotification( "Invalid TI-994A.ctg console ROM!", 5000, RETRO_LOG_ERROR );
		return false;
	}

	g_LogCb( RETRO_LOG_INFO, "[ti99sim] Loaded console ROM '%s' (%s)\n", consoleROM->GetFileName( ), consoleROM->GetTitle( ) ? consoleROM->GetTitle( ) : "" );

	g_VDP = new cRetroTMS9918A( 60 );
	g_VDP->SetUseRGB565( g_UseRGB565 );

	g_Sound  = new cRetroTMS9919( 44100 );
	g_Speech = new cTMS5220;

	g_Computer = new cRetroTI994A( consoleROM, g_VDP, g_Sound, g_Speech, true );

	// Load optional peripheral DSR ROMs (e.g. ti-disk.ctg) if present in BIOS
	LoadDevices( g_Computer, []( const char *name )
	{
		if( strcmp( name, "cf7+.ctg" ) == 0 )     return false;
		if( strcmp( name, "ti-pcard.ctg" ) == 0 ) return false;
		return true;
	} );

	g_RomPath.clear( );
	cRefPtr<cCartridge> gameCtg = nullptr;

	if(( game != nullptr ) && ( game->path != nullptr ) && ( game->path[ 0 ] != '\0' ))
	{
		g_RomPath = game->path;
		gameCtg   = new cCartridge( g_RomPath.string( ));
		if( !gameCtg->IsValid( ))
		{
			g_LogCb( RETRO_LOG_ERROR, "[ti99sim] Failed to load cartridge '%s'\n", game->path );
			ShowNotification( "Failed to load TI-99/4A cartridge (.ctg)!", 4000, RETRO_LOG_ERROR );
			g_Computer = nullptr;
			g_Speech   = nullptr;
			g_Sound    = nullptr;
			g_VDP      = nullptr;
			return false;
		}

		g_LogCb( RETRO_LOG_INFO, "[ti99sim] Inserted cartridge '%s' (%s)\n", gameCtg->GetFileName( ), gameCtg->GetTitle( ) ? gameCtg->GetTitle( ) : "" );
		g_Computer->InsertCartridge( gameCtg );
	}

	g_OSK.Reset( g_Computer->GetPIC( ));
	g_OSK.SetAlphaLock( true, g_Computer->GetPIC( ));

	memset( g_PrevSlotPressed, 0, sizeof( g_PrevSlotPressed ));
	memset( g_PrevDpadCursor, 0, sizeof( g_PrevDpadCursor ));
	memset( g_PrevLeft, 0, sizeof( g_PrevLeft ));
	memset( g_PrevRight, 0, sizeof( g_PrevRight ));
	memset( g_PrevUp, 0, sizeof( g_PrevUp ));
	memset( g_PrevDown, 0, sizeof( g_PrevDown ));
	memset( g_LastHorizDir, 0, sizeof( g_LastHorizDir ));
	memset( g_LastVertDir, 0, sizeof( g_LastVertDir ));
	memset( g_LastAxis, 0, sizeof( g_LastAxis ));

	UpdateCoreOptions( true );

	return true;
}

RETRO_API bool retro_load_game_special( unsigned game_type, const struct retro_game_info *info, size_t num_info )
{
	( void ) game_type;
	( void ) info;
	( void ) num_info;
	return false;
}

RETRO_API void retro_unload_game( void )
{
	if( g_Computer != nullptr )
	{
		g_OSK.Reset( g_Computer->GetPIC( ));
	}
	g_Computer = nullptr;
	g_Speech   = nullptr;
	g_Sound    = nullptr;
	g_VDP      = nullptr;
	g_RomPath.clear( );
}

RETRO_API void retro_reset( void )
{
	if( g_Computer != nullptr )
	{
		g_OSK.Reset( g_Computer->GetPIC( ));
		g_Computer->Reset( );
		g_OSK.SetAlphaLock( true, g_Computer->GetPIC( ));
	}
}

RETRO_API void retro_run( void )
{
	if( g_Computer == nullptr )
	{
		return;
	}

	bool updated = false;
	if(( g_EnvironCb != nullptr ) && g_EnvironCb( RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated ) && updated )
	{
		UpdateCoreOptions( false );
	}

	PollAndApplyInput( );

	size_t audioFrames = 0;
	g_Computer->RunFrame( g_AudioBuffer, &audioFrames );

	if(( g_VDP != nullptr ) && ( g_VideoCb != nullptr ))
	{
		unsigned width  = g_VDP->GetOutputWidth( );
		unsigned height = g_VDP->GetOutputHeight( );

		if( g_OSK.IsActive( ))
		{
			if( g_UseRGB565 )
			{
				g_OSK.RenderRGB565( g_VDP->GetFrameBuffer565( ), width, height, width );
			}
			else
			{
				g_OSK.RenderXRGB8888( g_VDP->GetFrameBuffer8888( ), width, height, width );
			}
		}

		g_VideoCb( g_VDP->GetFrameBuffer( ), width, height, g_VDP->GetOutputPitch( ));
	}

	if(( audioFrames > 0 ) && ( g_AudioBatchCb != nullptr ))
	{
		g_AudioBatchCb( g_AudioBuffer, audioFrames );
	}
}

RETRO_API size_t retro_serialize_size( void )
{
	return ( g_Computer != nullptr ) ? g_Computer->SerializeSize( ) : 0;
}

RETRO_API bool retro_serialize( void *data, size_t size )
{
	if( g_Computer == nullptr )
	{
		RetroLog( RETRO_LOG_ERROR, "[ti99sim] Save state save failed: core not initialized\n" );
		RetroNotify( "Save state failed: core not initialized", 3000, RETRO_LOG_ERROR );
		return false;
	}
	return g_Computer->Serialize( data, size );
}

RETRO_API bool retro_unserialize( const void *data, size_t size )
{
	if( g_Computer == nullptr )
	{
		RetroLog( RETRO_LOG_ERROR, "[ti99sim] Save state restore failed: core not initialized\n" );
		RetroNotify( "Save state load failed: core not initialized", 3000, RETRO_LOG_ERROR );
		return false;
	}
	return g_Computer->Unserialize( data, size );
}

RETRO_API void retro_cheat_reset( void )
{
}

RETRO_API void retro_cheat_set( unsigned index, bool enabled, const char *code )
{
	( void ) index;
	( void ) enabled;
	( void ) code;
}

RETRO_API unsigned retro_get_region( void )
{
	if(( g_Computer != nullptr ) && ( g_Computer->GetRefreshRate( ) == 50 ))
	{
		return RETRO_REGION_PAL;
	}
	return RETRO_REGION_NTSC;
}

RETRO_API void *retro_get_memory_data( unsigned id )
{
	if( g_Computer == nullptr )
	{
		return nullptr;
	}

	switch( id & RETRO_MEMORY_MASK )
	{
		case RETRO_MEMORY_SAVE_RAM :
			return g_Computer->GetSaveRAMPtr( );
		case RETRO_MEMORY_SYSTEM_RAM :
			return g_Computer->GetScratchpadPtr( );
		case RETRO_MEMORY_VIDEO_RAM :
			return g_Computer->GetVideoMemory( );
		default :
			return nullptr;
	}
}

RETRO_API size_t retro_get_memory_size( unsigned id )
{
	if( g_Computer == nullptr )
	{
		return 0;
	}

	switch( id & RETRO_MEMORY_MASK )
	{
		case RETRO_MEMORY_SAVE_RAM :
			return g_Computer->GetSaveRAMSize( );
		case RETRO_MEMORY_SYSTEM_RAM :
			return g_Computer->GetScratchpadSize( );
		case RETRO_MEMORY_VIDEO_RAM :
			return 0x4000;
		default :
			return 0;
	}
}

} // extern "C"
