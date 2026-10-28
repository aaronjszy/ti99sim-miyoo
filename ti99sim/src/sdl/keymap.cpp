//----------------------------------------------------------------------------
//
// File:        keymap.cpp
//
// Description: Configurable host-key -> TI-99/4A action mapping
//
// SPDX-License-Identifier: GPL-2.0-or-later
//
//              File format, one entry per line:
//
//                  <SDL key name> = <action>      # comment
//
//              <SDL key name> is anything SDL_GetKeyFromName accepts (Space, Return,
//              Left Ctrl, E, ...). <action> is one of:
//
//                  FIRE          joystick 1 fire button
//                  RESET         reset the console
//                  NONE          ignore the key
//                  <key>         a TI key: A-Z, 0-9, ENTER, SPACE, COMMA, PERIOD,
//                                SLASH, SEMICOLON, EQUALS
//                  <mod>+<key>   <mod> is FCTN, SHIFT or CTRL (e.g. FCTN+9 is BACK)
//
//----------------------------------------------------------------------------

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include "keymap.hpp"

namespace
{
	// Matches the Miyoo Mini button layout (the physical button is in brackets) and
	// starts from the layout the GPi Case/RG351P handheld patches use
	const char DEFAULT_KEYMAP[ ] =
		"Space      = FIRE          # [A]\n"
		"Left Ctrl  = 1             # [B]\n"
		"Left Shift = 2             # [X]\n"
		"Left Alt   = 3             # [Y]\n"
		"E          = FCTN+EQUALS   # [L1] QUIT\n"
		"T          = FCTN+8        # [R1] REDO\n"
		"Tab        = SPACE         # [L2]\n"
		"Backspace  = FCTN+9        # [R2] BACK\n"
		"Return     = ENTER         # [Start]\n"
		"Right Ctrl = P             # [Select]\n";

	std::string Trim( const std::string &text )
	{
		auto first = text.find_first_not_of( " \t\r\n" );
		if( first == std::string::npos )
		{
			return "";
		}
		auto last = text.find_last_not_of( " \t\r\n" );
		return text.substr( first, last - first + 1 );
	}

	std::string Upper( std::string text )
	{
		std::transform( text.begin( ), text.end( ), text.begin( ), []( unsigned char c ) { return std::toupper( c ); } );
		return text;
	}

	std::optional<VIRTUAL_KEY_E> ParseKey( const std::string &name )
	{
		if( name.size( ) == 1 )
		{
			if( std::isalpha( static_cast<unsigned char>( name[ 0 ] )))
			{
				return static_cast<VIRTUAL_KEY_E>( VK_A + ( name[ 0 ] - 'A' ));
			}
			if( std::isdigit( static_cast<unsigned char>( name[ 0 ] )))
			{
				return static_cast<VIRTUAL_KEY_E>( VK_0 + ( name[ 0 ] - '0' ));
			}
		}

		static const std::map<std::string,VIRTUAL_KEY_E> names =
		{
			{ "ENTER",     VK_ENTER     },
			{ "SPACE",     VK_SPACE     },
			{ "COMMA",     VK_COMMA     },
			{ "PERIOD",    VK_PERIOD    },
			{ "SLASH",     VK_DIVIDE    },
			{ "SEMICOLON", VK_SEMICOLON },
			{ "EQUALS",    VK_EQUALS    },
			{ "FCTN",      VK_FCTN      },
			{ "SHIFT",     VK_SHIFT     },
			{ "CTRL",      VK_CTRL      }
		};

		auto it = names.find( name );
		if( it == names.end( ))
		{
			return std::nullopt;
		}
		return it->second;
	}

	std::optional<sKeyAction> ParseAction( const std::string &text )
	{
		sKeyAction action;
		auto name = Upper( text );

		if( name == "FIRE" )
		{
			action.type = sKeyAction::FIRE;
			return action;
		}
		if( name == "RESET" )
		{
			action.type = sKeyAction::RESET;
			return action;
		}
		if( name == "NONE" )
		{
			return action;
		}

		auto plus = name.find( '+' );
		if( plus == std::string::npos )
		{
			auto key = ParseKey( Trim( name ));
			if( ! key )
			{
				return std::nullopt;
			}
			action.key1 = *key;
			return action;
		}

		auto first  = ParseKey( Trim( name.substr( 0, plus )));
		auto second = ParseKey( Trim( name.substr( plus + 1 )));
		if( ! first || ! second )
		{
			return std::nullopt;
		}
		action.key1 = *first;
		action.key2 = *second;
		return action;
	}
}

cKeyMap::cKeyMap( )
{
	std::istringstream defaults( DEFAULT_KEYMAP );
	Parse( defaults, "built-in keymap" );
}

void cKeyMap::Load( const std::filesystem::path &file )
{
	std::ifstream stream( file );
	if( stream.is_open( ))
	{
		Parse( stream, file.c_str( ));
	}
}

const sKeyAction *cKeyMap::Find( SDL_Keycode key ) const
{
	auto it = m_Actions.find( key );
	return ( it == m_Actions.end( )) ? nullptr : &it->second;
}

void cKeyMap::Parse( std::istream &stream, const char *source )
{
	std::string line;
	int number = 0;

	while( std::getline( stream, line ))
	{
		number++;

		auto comment = line.find( '#' );
		if( comment != std::string::npos )
		{
			line.erase( comment );
		}

		line = Trim( line );
		if( line.empty( ))
		{
			continue;
		}

		auto equals = line.find( '=' );
		if( equals == std::string::npos )
		{
			fprintf( stderr, "%s:%d: expected '<key> = <action>'\n", source, number );
			continue;
		}

		auto keyName = Trim( line.substr( 0, equals ));
		auto key = SDL_GetKeyFromName( keyName.c_str( ));
		if( key == SDLK_UNKNOWN )
		{
			fprintf( stderr, "%s:%d: unknown key name '%s'\n", source, number, keyName.c_str( ));
			continue;
		}

		auto action = ParseAction( Trim( line.substr( equals + 1 )));
		if( ! action )
		{
			fprintf( stderr, "%s:%d: unknown action '%s'\n", source, number, Trim( line.substr( equals + 1 )).c_str( ));
			continue;
		}

		m_Actions[ key ] = *action;
	}
}
