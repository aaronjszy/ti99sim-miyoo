//----------------------------------------------------------------------------
//
// File:        keymap.hpp
//
// Description: Configurable host-key -> TI-99/4A action mapping (used by the
//              Miyoo Mini build, where the buttons arrive as keyboard events)
//
// SPDX-License-Identifier: GPL-2.0-or-later
//
//----------------------------------------------------------------------------

#ifndef KEYMAP_HPP_
#define KEYMAP_HPP_

#include <filesystem>
#include <istream>
#include <map>
#include <SDL.h>
#include "itms9901.hpp"

struct sKeyAction
{
	enum TYPE { KEYS, FIRE, RESET };

	TYPE          type{ KEYS };
	VIRTUAL_KEY_E key1{ VK_NONE };		// VK_NONE for both keys == ignore the host key
	VIRTUAL_KEY_E key2{ VK_NONE };
};

class cKeyMap
{
	std::map<SDL_Keycode,sKeyAction> m_Actions;

	void Parse( std::istream &, const char *source );

public:

	// Starts with the built-in defaults
	cKeyMap( );

	// Overrides/extends the mapping from a file, if it exists
	void Load( const std::filesystem::path &file );

	const sKeyAction *Find( SDL_Keycode ) const;
};

#endif
