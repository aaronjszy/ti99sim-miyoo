//----------------------------------------------------------------------------
//
// File:        osk-retro.cpp
// Date:        31-Oct-2026
// Programmer:  Aaron Szymanski
//
// Description: Built-in translucent TI-99/4A On-Screen Keyboard for Libretro
//
// SPDX-License-Identifier: GPL-2.0-or-later
//
//----------------------------------------------------------------------------

#include <algorithm>
#include <cstring>
#include "osk-retro.hpp"

namespace
{
	constexpr int OSK_KEYSYM_SLOT = 490;
	constexpr int OSK_MOD_SHIFT   = 491;
	constexpr int OSK_MOD_FCTN    = 492;
	constexpr int OSK_MOD_CTRL    = 493;

	enum class eOskKeyType
	{
		NORMAL,
		MOD_SHIFT,
		MOD_FCTN,
		MOD_CTRL,
		MOD_ALPHA,
		COMBO
	};

	struct sOskKey
	{
		const char    *label;
		const char    *fctnLabel;
		eOskKeyType    type;
		VIRTUAL_KEY_E  vkey1;
		VIRTUAL_KEY_E  vkey2;
		int            colSpan;
	};

	constexpr int NUM_ROWS = 5;
	constexpr int MAX_COLS = 11;

	constexpr sOskKey OskLayout[ NUM_ROWS ][ MAX_COLS ] =
	{
		// Row 0: Number row
		{
			{ "1", "DEL", eOskKeyType::NORMAL, VK_1,      VK_NONE, 1 },
			{ "2", "INS", eOskKeyType::NORMAL, VK_2,      VK_NONE, 1 },
			{ "3", "ERA", eOskKeyType::NORMAL, VK_3,      VK_NONE, 1 },
			{ "4", "CLR", eOskKeyType::NORMAL, VK_4,      VK_NONE, 1 },
			{ "5", "BGN", eOskKeyType::NORMAL, VK_5,      VK_NONE, 1 },
			{ "6", "PRO", eOskKeyType::NORMAL, VK_6,      VK_NONE, 1 },
			{ "7", "AID", eOskKeyType::NORMAL, VK_7,      VK_NONE, 1 },
			{ "8", "RDO", eOskKeyType::NORMAL, VK_8,      VK_NONE, 1 },
			{ "9", "BCK", eOskKeyType::NORMAL, VK_9,      VK_NONE, 1 },
			{ "0", "0",   eOskKeyType::NORMAL, VK_0,      VK_NONE, 1 },
			{ "=", "QIT", eOskKeyType::NORMAL, VK_EQUALS, VK_NONE, 1 }
		},
		// Row 1: QWERTY row
		{
			{ "Q", "Q",   eOskKeyType::NORMAL, VK_Q,      VK_NONE, 1 },
			{ "W", "~",   eOskKeyType::NORMAL, VK_W,      VK_NONE, 1 },
			{ "E", "UP",  eOskKeyType::NORMAL, VK_E,      VK_NONE, 1 },
			{ "R", "[",   eOskKeyType::NORMAL, VK_R,      VK_NONE, 1 },
			{ "T", "]",   eOskKeyType::NORMAL, VK_T,      VK_NONE, 1 },
			{ "Y", "Y",   eOskKeyType::NORMAL, VK_Y,      VK_NONE, 1 },
			{ "U", "_",   eOskKeyType::NORMAL, VK_U,      VK_NONE, 1 },
			{ "I", "?",   eOskKeyType::NORMAL, VK_I,      VK_NONE, 1 },
			{ "O", "'",   eOskKeyType::NORMAL, VK_O,      VK_NONE, 1 },
			{ "P", "\"",  eOskKeyType::NORMAL, VK_P,      VK_NONE, 1 },
			{ "/", "-",   eOskKeyType::NORMAL, VK_DIVIDE, VK_NONE, 1 }
		},
		// Row 2: ASDF row
		{
			{ "A", "|",   eOskKeyType::NORMAL, VK_A,         VK_NONE, 1 },
			{ "S", "LFT", eOskKeyType::NORMAL, VK_S,         VK_NONE, 1 },
			{ "D", "RGT", eOskKeyType::NORMAL, VK_D,         VK_NONE, 1 },
			{ "F", "{",   eOskKeyType::NORMAL, VK_F,         VK_NONE, 1 },
			{ "G", "}",   eOskKeyType::NORMAL, VK_G,         VK_NONE, 1 },
			{ "H", "H",   eOskKeyType::NORMAL, VK_H,         VK_NONE, 1 },
			{ "J", "J",   eOskKeyType::NORMAL, VK_J,         VK_NONE, 1 },
			{ "K", "K",   eOskKeyType::NORMAL, VK_K,         VK_NONE, 1 },
			{ "L", "L",   eOskKeyType::NORMAL, VK_L,         VK_NONE, 1 },
			{ ";", ":",   eOskKeyType::NORMAL, VK_SEMICOLON, VK_NONE, 1 },
			{ "ENT","ENT",eOskKeyType::NORMAL, VK_ENTER,     VK_NONE, 1 }
		},
		// Row 3: ZXCV row
		{
			{ "SHF","SHF",eOskKeyType::MOD_SHIFT, VK_SHIFT,  VK_NONE, 1 },
			{ "Z", "\\",  eOskKeyType::NORMAL,    VK_Z,      VK_NONE, 1 },
			{ "X", "DWN", eOskKeyType::NORMAL,    VK_X,      VK_NONE, 1 },
			{ "C", "`",   eOskKeyType::NORMAL,    VK_C,      VK_NONE, 1 },
			{ "V", "V",   eOskKeyType::NORMAL,    VK_V,      VK_NONE, 1 },
			{ "B", "B",   eOskKeyType::NORMAL,    VK_B,      VK_NONE, 1 },
			{ "N", "N",   eOskKeyType::NORMAL,    VK_N,      VK_NONE, 1 },
			{ "M", "M",   eOskKeyType::NORMAL,    VK_M,      VK_NONE, 1 },
			{ ",", ",",   eOskKeyType::NORMAL,    VK_COMMA,  VK_NONE, 1 },
			{ ".", ".",   eOskKeyType::NORMAL,    VK_PERIOD, VK_NONE, 1 },
			{ "FCT","FCT",eOskKeyType::MOD_FCTN,  VK_FCTN,   VK_NONE, 1 }
		},
		// Row 4: Modifiers, Space, and TI Function shortcuts
		{
			{ "ALP","ALP",eOskKeyType::MOD_ALPHA, VK_CAPSLOCK, VK_NONE,   1 },
			{ "CTL","CTL",eOskKeyType::MOD_CTRL,  VK_CTRL,     VK_NONE,   1 },
			{ "SPACE","SPACE",eOskKeyType::NORMAL,VK_SPACE,    VK_NONE,   3 },
			{ "AID","AID",eOskKeyType::COMBO,     VK_FCTN,     VK_7,      1 },
			{ "CLR","CLR",eOskKeyType::COMBO,     VK_FCTN,     VK_4,      1 },
			{ "PRO","PRO",eOskKeyType::COMBO,     VK_FCTN,     VK_6,      1 },
			{ "RDO","RDO",eOskKeyType::COMBO,     VK_FCTN,     VK_8,      1 },
			{ "BCK","BCK",eOskKeyType::COMBO,     VK_FCTN,     VK_9,      1 },
			{ "QIT","QIT",eOskKeyType::COMBO,     VK_FCTN,     VK_EQUALS, 1 },
			{ nullptr, nullptr, eOskKeyType::NORMAL, VK_NONE, VK_NONE, 0 },
			{ nullptr, nullptr, eOskKeyType::NORMAL, VK_NONE, VK_NONE, 0 }
		}
	};

	constexpr int RowColCount[ NUM_ROWS ] = { 11, 11, 11, 11, 9 };

	// Compact 5x7 bitmap font for ASCII 32..95 (5 columns, 7 bits high, bit 0 = top row)
	constexpr uint8_t Font5x7[ 64 ][ 5 ] =
	{
		{ 0x00, 0x00, 0x00, 0x00, 0x00 }, // ' '
		{ 0x00, 0x00, 0x5F, 0x00, 0x00 }, // '!'
		{ 0x00, 0x07, 0x00, 0x07, 0x00 }, // '"'
		{ 0x14, 0x7F, 0x14, 0x7F, 0x14 }, // '#'
		{ 0x24, 0x2A, 0x7F, 0x2A, 0x12 }, // '$'
		{ 0x23, 0x13, 0x08, 0x64, 0x62 }, // '%'
		{ 0x36, 0x49, 0x55, 0x22, 0x50 }, // '&'
		{ 0x00, 0x05, 0x03, 0x00, 0x00 }, // '''
		{ 0x00, 0x1C, 0x22, 0x41, 0x00 }, // '('
		{ 0x00, 0x41, 0x22, 0x1C, 0x00 }, // ')'
		{ 0x14, 0x08, 0x3E, 0x08, 0x14 }, // '*'
		{ 0x08, 0x08, 0x3E, 0x08, 0x08 }, // '+'
		{ 0x00, 0x50, 0x30, 0x00, 0x00 }, // ','
		{ 0x08, 0x08, 0x08, 0x08, 0x08 }, // '-'
		{ 0x00, 0x60, 0x60, 0x00, 0x00 }, // '.'
		{ 0x20, 0x10, 0x08, 0x04, 0x02 }, // '/'
		{ 0x3E, 0x51, 0x49, 0x45, 0x3E }, // '0'
		{ 0x00, 0x42, 0x7F, 0x40, 0x00 }, // '1'
		{ 0x42, 0x61, 0x51, 0x49, 0x46 }, // '2'
		{ 0x21, 0x41, 0x45, 0x4B, 0x31 }, // '3'
		{ 0x18, 0x14, 0x12, 0x7F, 0x10 }, // '4'
		{ 0x27, 0x45, 0x45, 0x45, 0x39 }, // '5'
		{ 0x3C, 0x4A, 0x49, 0x49, 0x30 }, // '6'
		{ 0x01, 0x71, 0x09, 0x05, 0x03 }, // '7'
		{ 0x36, 0x49, 0x49, 0x49, 0x36 }, // '8'
		{ 0x06, 0x49, 0x49, 0x29, 0x1E }, // '9'
		{ 0x00, 0x36, 0x36, 0x00, 0x00 }, // ':'
		{ 0x00, 0x56, 0x36, 0x00, 0x00 }, // ';'
		{ 0x08, 0x14, 0x22, 0x41, 0x00 }, // '<'
		{ 0x14, 0x14, 0x14, 0x14, 0x14 }, // '='
		{ 0x00, 0x41, 0x22, 0x14, 0x08 }, // '>'
		{ 0x02, 0x01, 0x51, 0x09, 0x06 }, // '?'
		{ 0x32, 0x49, 0x79, 0x41, 0x3E }, // '@'
		{ 0x7E, 0x11, 0x11, 0x11, 0x7E }, // 'A'
		{ 0x7F, 0x49, 0x49, 0x49, 0x36 }, // 'B'
		{ 0x3E, 0x41, 0x41, 0x41, 0x22 }, // 'C'
		{ 0x7F, 0x41, 0x41, 0x22, 0x1C }, // 'D'
		{ 0x7F, 0x49, 0x49, 0x49, 0x41 }, // 'E'
		{ 0x7F, 0x09, 0x09, 0x09, 0x01 }, // 'F'
		{ 0x3E, 0x41, 0x49, 0x49, 0x7A }, // 'G'
		{ 0x7F, 0x08, 0x08, 0x08, 0x7F }, // 'H'
		{ 0x00, 0x41, 0x7F, 0x41, 0x00 }, // 'I'
		{ 0x20, 0x40, 0x41, 0x3F, 0x01 }, // 'J'
		{ 0x7F, 0x08, 0x14, 0x22, 0x41 }, // 'K'
		{ 0x7F, 0x40, 0x40, 0x40, 0x40 }, // 'L'
		{ 0x7F, 0x02, 0x0C, 0x02, 0x7F }, // 'M'
		{ 0x7F, 0x04, 0x08, 0x10, 0x7F }, // 'N'
		{ 0x3E, 0x41, 0x41, 0x41, 0x3E }, // 'O'
		{ 0x7F, 0x09, 0x09, 0x09, 0x06 }, // 'P'
		{ 0x3E, 0x41, 0x51, 0x21, 0x5E }, // 'Q'
		{ 0x7F, 0x09, 0x19, 0x29, 0x46 }, // 'R'
		{ 0x46, 0x49, 0x49, 0x49, 0x31 }, // 'S'
		{ 0x01, 0x01, 0x7F, 0x01, 0x01 }, // 'T'
		{ 0x3F, 0x40, 0x40, 0x40, 0x3F }, // 'U'
		{ 0x1F, 0x20, 0x40, 0x20, 0x1F }, // 'V'
		{ 0x3F, 0x40, 0x38, 0x40, 0x3F }, // 'W'
		{ 0x63, 0x14, 0x08, 0x14, 0x63 }, // 'X'
		{ 0x07, 0x08, 0x70, 0x08, 0x07 }, // 'Y'
		{ 0x61, 0x51, 0x49, 0x45, 0x43 }, // 'Z'
		{ 0x00, 0x7F, 0x41, 0x41, 0x00 }, // '['
		{ 0x02, 0x04, 0x08, 0x10, 0x20 }, // '\'
		{ 0x00, 0x41, 0x41, 0x7F, 0x00 }, // ']'
		{ 0x04, 0x02, 0x01, 0x02, 0x04 }, // '^'
		{ 0x40, 0x40, 0x40, 0x40, 0x40 }  // '_'
	};

	inline uint16_t BlendRGB565( uint16_t dst, uint16_t src )
	{
		// Fast 75% src + 25% dst blend
		uint32_t dR = ( dst >> 11 ) & 0x1F;
		uint32_t dG = ( dst >> 5  ) & 0x3F;
		uint32_t dB = ( dst >> 0  ) & 0x1F;

		uint32_t sR = ( src >> 11 ) & 0x1F;
		uint32_t sG = ( src >> 5  ) & 0x3F;
		uint32_t sB = ( src >> 0  ) & 0x1F;

		uint32_t r = ( sR * 3 + dR ) >> 2;
		uint32_t g = ( sG * 3 + dG ) >> 2;
		uint32_t b = ( sB * 3 + dB ) >> 2;

		return static_cast<uint16_t>(( r << 11 ) | ( g << 5 ) | b );
	}

	inline uint32_t BlendXRGB8888( uint32_t dst, uint32_t src )
	{
		uint32_t dR = ( dst >> 16 ) & 0xFF;
		uint32_t dG = ( dst >> 8  ) & 0xFF;
		uint32_t dB = ( dst >> 0  ) & 0xFF;

		uint32_t sR = ( src >> 16 ) & 0xFF;
		uint32_t sG = ( src >> 8  ) & 0xFF;
		uint32_t sB = ( src >> 0  ) & 0xFF;

		uint32_t r = ( sR * 3 + dR ) >> 2;
		uint32_t g = ( sG * 3 + dG ) >> 2;
		uint32_t b = ( sB * 3 + dB ) >> 2;

		return ( r << 16 ) | ( g << 8 ) | b;
	}

	template <typename PixelT>
	void DrawChar5x7( PixelT *fb, unsigned width, unsigned height, unsigned pitchPixels, int x, int y, char c, PixelT color )
	{
		if(( c >= 'a' ) && ( c <= 'z' ))
		{
			c = static_cast<char>( c - 'a' + 'A' );
		}
		if(( c < 32 ) || ( c > 95 ))
		{
			c = '?';
		}

		const uint8_t *glyph = Font5x7[ c - 32 ];
		for( int col = 0; col < 5; col++ )
		{
			int px = x + col;
			if(( px < 0 ) || ( px >= static_cast<int>( width )))
			{
				continue;
			}
			uint8_t bits = glyph[ col ];
			for( int row = 0; row < 7; row++ )
			{
				int py = y + row;
				if(( py >= 0 ) && ( py < static_cast<int>( height )) && ( bits & ( 1 << row )))
				{
					fb[ py * pitchPixels + px ] = color;
				}
			}
		}
	}

	template <typename PixelT>
	void DrawText5x7( PixelT *fb, unsigned width, unsigned height, unsigned pitchPixels, int x, int y, const char *text, PixelT color )
	{
		if( text == nullptr )
		{
			return;
		}
		for( int i = 0; text[ i ] != '\0'; i++ )
		{
			DrawChar5x7( fb, width, height, pitchPixels, x + i * 6, y, text[ i ], color );
		}
	}
}

cRetroOSK::cRetroOSK( ) :
	m_Active( false ),
	m_TopPosition( false ),
	m_CursorRow( 0 ),
	m_CursorCol( 0 ),
	m_StickyShift( false ),
	m_StickyFctn( false ),
	m_StickyCtrl( false ),
	m_AlphaLock( true ),
	m_ActiveKeySym( 0 ),
	m_ActiveVKey1( VK_NONE ),
	m_ActiveVKey2( VK_NONE ),
	m_ClearStickyOnRelease( false ),
	m_PrevInput( ),
	m_RepeatTimer( 0 )
{
}

void cRetroOSK::Reset( iTMS9901 *pic )
{
	ReleaseActiveKey( pic );
	if( pic != nullptr )
	{
		pic->VKeyUp( OSK_MOD_SHIFT );
		pic->VKeyUp( OSK_MOD_FCTN );
		pic->VKeyUp( OSK_MOD_CTRL );
	}
	m_StickyShift = false;
	m_StickyFctn  = false;
	m_StickyCtrl  = false;
	m_Active      = false;
	m_PrevInput   = { };
	m_RepeatTimer = 0;
}

void cRetroOSK::SetActive( bool active, iTMS9901 *pic )
{
	if( m_Active == active )
	{
		return;
	}

	if( !active )
	{
		ReleaseActiveKey( pic );
		if( pic != nullptr )
		{
			pic->VKeyUp( OSK_MOD_SHIFT );
			pic->VKeyUp( OSK_MOD_FCTN );
			pic->VKeyUp( OSK_MOD_CTRL );
		}
		m_StickyShift = false;
		m_StickyFctn  = false;
		m_StickyCtrl  = false;
	}

	m_Active      = active;
	m_RepeatTimer = 0;
}

void cRetroOSK::Toggle( iTMS9901 *pic )
{
	SetActive( !m_Active, pic );
}

void cRetroOSK::SetAlphaLock( bool enabled, iTMS9901 *pic )
{
	m_AlphaLock = enabled;
	if( pic != nullptr )
	{
		if( enabled )
		{
			if( pic->GetKeyState( VK_CAPSLOCK ) == 0 )
			{
				pic->VKeyDown( 0, VK_CAPSLOCK );
			}
		}
		else
		{
			pic->VKeyUp( 0 );
		}
	}
}

void cRetroOSK::ReleaseActiveKey( iTMS9901 *pic )
{
	if(( m_ActiveKeySym != 0 ) && ( pic != nullptr ))
	{
		pic->VKeyUp( m_ActiveKeySym );
	}
	m_ActiveKeySym = 0;
	m_ActiveVKey1  = VK_NONE;
	m_ActiveVKey2  = VK_NONE;

	if( m_ClearStickyOnRelease && ( pic != nullptr ))
	{
		pic->VKeyUp( OSK_MOD_SHIFT );
		pic->VKeyUp( OSK_MOD_FCTN );
		pic->VKeyUp( OSK_MOD_CTRL );
		m_StickyShift          = false;
		m_StickyFctn           = false;
		m_StickyCtrl           = false;
		m_ClearStickyOnRelease = false;
	}
}

void cRetroOSK::PressSelectedKey( iTMS9901 *pic )
{
	if( pic == nullptr )
	{
		return;
	}

	const sOskKey &k = OskLayout[ m_CursorRow ][ m_CursorCol ];
	if( k.label == nullptr )
	{
		return;
	}

	switch( k.type )
	{
		case eOskKeyType::MOD_SHIFT :
			m_StickyShift = !m_StickyShift;
			if( m_StickyShift )
			{
				pic->VKeysDown( OSK_MOD_SHIFT, VK_SHIFT );
			}
			else
			{
				pic->VKeyUp( OSK_MOD_SHIFT );
			}
			break;

		case eOskKeyType::MOD_FCTN :
			m_StickyFctn = !m_StickyFctn;
			if( m_StickyFctn )
			{
				pic->VKeysDown( OSK_MOD_FCTN, VK_FCTN );
			}
			else
			{
				pic->VKeyUp( OSK_MOD_FCTN );
			}
			break;

		case eOskKeyType::MOD_CTRL :
			m_StickyCtrl = !m_StickyCtrl;
			if( m_StickyCtrl )
			{
				pic->VKeysDown( OSK_MOD_CTRL, VK_CTRL );
			}
			else
			{
				pic->VKeyUp( OSK_MOD_CTRL );
			}
			break;

		case eOskKeyType::MOD_ALPHA :
			SetAlphaLock( !m_AlphaLock, pic );
			break;

		case eOskKeyType::COMBO :
			ReleaseActiveKey( pic );
			m_ActiveKeySym = OSK_KEYSYM_SLOT;
			m_ActiveVKey1  = k.vkey1;
			m_ActiveVKey2  = k.vkey2;
			pic->VKeysDown( OSK_KEYSYM_SLOT, k.vkey1, k.vkey2 );
			break;

		case eOskKeyType::NORMAL :
			ReleaseActiveKey( pic );
			m_ActiveKeySym         = OSK_KEYSYM_SLOT;
			m_ActiveVKey1          = k.vkey1;
			m_ActiveVKey2          = k.vkey2;
			m_ClearStickyOnRelease = ( m_StickyShift || m_StickyFctn || m_StickyCtrl );
			pic->VKeysDown( OSK_KEYSYM_SLOT, k.vkey1, k.vkey2 );
			break;
	}
}

void cRetroOSK::Update( const sPadInput &input, iTMS9901 *pic )
{
	if( !m_Active )
	{
		m_PrevInput = input;
		return;
	}

	// L / R toggles top/bottom placement of the keyboard overlay
	if(( input.l && !m_PrevInput.l ) || ( input.r && !m_PrevInput.r ))
	{
		m_TopPosition = !m_TopPosition;
	}

	// B closes the OSK
	if( input.b && !m_PrevInput.b )
	{
		SetActive( false, pic );
		m_PrevInput = input;
		return;
	}

	// D-pad navigation with auto-repeat
	bool anyDir     = input.up || input.down || input.left || input.right;
	bool prevAnyDir = m_PrevInput.up || m_PrevInput.down || m_PrevInput.left || m_PrevInput.right;
	bool stepDir    = false;

	if( anyDir )
	{
		if( !prevAnyDir )
		{
			stepDir       = true;
			m_RepeatTimer = 18;
		}
		else if( --m_RepeatTimer <= 0 )
		{
			stepDir       = true;
			m_RepeatTimer = 5;
		}
	}
	else
	{
		m_RepeatTimer = 0;
	}

	if( stepDir )
	{
		if( input.up )
		{
			m_CursorRow = ( m_CursorRow + NUM_ROWS - 1 ) % NUM_ROWS;
		}
		else if( input.down )
		{
			m_CursorRow = ( m_CursorRow + 1 ) % NUM_ROWS;
		}

		int maxCols = RowColCount[ m_CursorRow ];
		if( m_CursorCol >= maxCols )
		{
			m_CursorCol = maxCols - 1;
		}

		if( input.left )
		{
			m_CursorCol = ( m_CursorCol + maxCols - 1 ) % maxCols;
		}
		else if( input.right )
		{
			m_CursorCol = ( m_CursorCol + 1 ) % maxCols;
		}
	}

	// A presses selected key; X presses Backspace (FCTN+S); Y presses Space; Start presses Enter
	bool anyKeyHeld = input.a || input.x || input.y || input.start;
	if( input.a && !m_PrevInput.a )
	{
		PressSelectedKey( pic );
	}
	else if( input.x && !m_PrevInput.x )
	{
		ReleaseActiveKey( pic );
		m_ActiveKeySym = OSK_KEYSYM_SLOT;
		m_ActiveVKey1  = VK_FCTN;
		m_ActiveVKey2  = VK_S;
		if( pic != nullptr )
		{
			pic->VKeysDown( OSK_KEYSYM_SLOT, VK_FCTN, VK_S );
		}
	}
	else if( input.y && !m_PrevInput.y )
	{
		ReleaseActiveKey( pic );
		m_ActiveKeySym = OSK_KEYSYM_SLOT;
		m_ActiveVKey1  = VK_SPACE;
		if( pic != nullptr )
		{
			pic->VKeysDown( OSK_KEYSYM_SLOT, VK_SPACE );
		}
	}
	else if( input.start && !m_PrevInput.start )
	{
		ReleaseActiveKey( pic );
		m_ActiveKeySym = OSK_KEYSYM_SLOT;
		m_ActiveVKey1  = VK_ENTER;
		if( pic != nullptr )
		{
			pic->VKeysDown( OSK_KEYSYM_SLOT, VK_ENTER );
		}
	}
	else if( !anyKeyHeld && ( m_ActiveKeySym != 0 ))
	{
		ReleaseActiveKey( pic );
	}

	m_PrevInput = input;
}

template <typename PixelT, PixelT (*PackFn)( uint8_t, uint8_t, uint8_t ), PixelT (*BlendFn)( PixelT, PixelT )>
static void RenderOSKImpl(
	PixelT *fb,
	unsigned width,
	unsigned height,
	unsigned pitchPixels,
	bool topPos,
	int cursorRow,
	int cursorCol,
	bool stickyShift,
	bool stickyFctn,
	bool stickyCtrl,
	bool alphaLock,
	bool keyPressed )
{
	constexpr int panelW = 248;
	constexpr int panelH = 82;
	constexpr int cellW  = 22;
	constexpr int cellH  = 13;

	int startX = ( static_cast<int>( width ) - panelW ) / 2;
	int startY = topPos ? 4 : ( static_cast<int>( height ) - panelH - 4 );

	PixelT panelBg      = PackFn(  20,  24,  34 );
	PixelT panelBorder  = PackFn( 100, 130, 180 );
	PixelT keyBg        = PackFn(  44,  52,  70 );
	PixelT keyModBg     = PackFn(  60,  48,  82 );
	PixelT keyActiveBg  = PackFn(  40, 145,  85 );
	PixelT keyCursorBg  = PackFn( 235, 185,  35 );
	PixelT keyPressedBg = PackFn( 255, 110,  40 );
	PixelT textWhite    = PackFn( 245, 245, 250 );
	PixelT textDark     = PackFn(  16,  18,  24 );
	PixelT textHint     = PackFn( 160, 200, 255 );

	// Draw translucent panel background and border
	for( int y = 0; y < panelH; y++ )
	{
		int py = startY + y;
		if(( py < 0 ) || ( py >= static_cast<int>( height )))
		{
			continue;
		}
		PixelT *rowPtr = fb + py * pitchPixels;
		bool isBorderY = ( y == 0 ) || ( y == panelH - 1 );
		for( int x = 0; x < panelW; x++ )
		{
			int px = startX + x;
			if(( px < 0 ) || ( px >= static_cast<int>( width )))
			{
				continue;
			}
			bool isBorderX = ( x == 0 ) || ( x == panelW - 1 );
			if( isBorderX || isBorderY )
			{
				rowPtr[ px ] = panelBorder;
			}
			else
			{
				rowPtr[ px ] = BlendFn( rowPtr[ px ], panelBg );
			}
		}
	}

	// Header legend
	DrawText5x7( fb, width, height, pitchPixels, startX + 4, startY + 3,
		"A:KEY B:HIDE X:FCT Y:SPC ST:ENT L:POS", textHint );

	// Draw 5 rows of keys
	for( int r = 0; r < NUM_ROWS; r++ )
	{
		int ky   = startY + 13 + r * ( cellH + 1 );
		int colX = startX + 3;

		int numCols = RowColCount[ r ];
		for( int c = 0; c < numCols; c++ )
		{
			const sOskKey &k = OskLayout[ r ][ c ];
			if( k.label == nullptr )
			{
				continue;
			}

			int kw = k.colSpan * cellW - 2;
			int kh = cellH;

			bool isCursor = ( r == cursorRow ) && ( c == cursorCol );
			bool isSticky =
				(( k.type == eOskKeyType::MOD_SHIFT ) && stickyShift ) ||
				(( k.type == eOskKeyType::MOD_FCTN  ) && stickyFctn  ) ||
				(( k.type == eOskKeyType::MOD_CTRL  ) && stickyCtrl  ) ||
				(( k.type == eOskKeyType::MOD_ALPHA ) && alphaLock   );

			PixelT fill = keyBg;
			if( k.type != eOskKeyType::NORMAL )
			{
				fill = keyModBg;
			}
			if( isSticky )
			{
				fill = keyActiveBg;
			}
			if( isCursor )
			{
				fill = keyPressed ? keyPressedBg : keyCursorBg;
			}

			for( int yy = 0; yy < kh; yy++ )
			{
				int py = ky + yy;
				if(( py < 0 ) || ( py >= static_cast<int>( height )))
				{
					continue;
				}
				PixelT *rowPtr = fb + py * pitchPixels;
				for( int xx = 0; xx < kw; xx++ )
				{
					int px = colX + xx;
					if(( px >= 0 ) && ( px < static_cast<int>( width )))
					{
						rowPtr[ px ] = isCursor ? fill : BlendFn( rowPtr[ px ], fill );
					}
				}
			}

			const char *lbl = ( stickyFctn && ( k.fctnLabel != nullptr )) ? k.fctnLabel : k.label;
			int lblLen = static_cast<int>( strlen( lbl ));
			int textX  = colX + ( kw - ( lblLen * 6 - 1 )) / 2;
			int textY  = ky + ( kh - 7 ) / 2;

			DrawText5x7( fb, width, height, pitchPixels, textX, textY, lbl, isCursor ? textDark : textWhite );

			colX += k.colSpan * cellW;
		}
	}
}

namespace
{
	inline uint16_t Pack565( uint8_t r, uint8_t g, uint8_t b )
	{
		return static_cast<uint16_t>((( r >> 3 ) << 11 ) | (( g >> 2 ) << 5 ) | ( b >> 3 ));
	}

	inline uint32_t Pack8888( uint8_t r, uint8_t g, uint8_t b )
	{
		return ( static_cast<uint32_t>( r ) << 16 ) | ( static_cast<uint32_t>( g ) << 8 ) | static_cast<uint32_t>( b );
	}
}

void cRetroOSK::RenderRGB565( uint16_t *fb, unsigned width, unsigned height, unsigned pitchPixels ) const
{
	if( !m_Active || ( fb == nullptr ))
	{
		return;
	}

	RenderOSKImpl<uint16_t, Pack565, BlendRGB565>(
		fb, width, height, pitchPixels,
		m_TopPosition, m_CursorRow, m_CursorCol,
		m_StickyShift, m_StickyFctn, m_StickyCtrl, m_AlphaLock,
		m_ActiveKeySym != 0 );
}

void cRetroOSK::RenderXRGB8888( uint32_t *fb, unsigned width, unsigned height, unsigned pitchPixels ) const
{
	if( !m_Active || ( fb == nullptr ))
	{
		return;
	}

	RenderOSKImpl<uint32_t, Pack8888, BlendXRGB8888>(
		fb, width, height, pitchPixels,
		m_TopPosition, m_CursorRow, m_CursorCol,
		m_StickyShift, m_StickyFctn, m_StickyCtrl, m_AlphaLock,
		m_ActiveKeySym != 0 );
}
