//----------------------------------------------------------------------------
//
// File:        tms9918a-retro.cpp
// Date:        31-Oct-2026
// Programmer:  Aaron Szymanski
//
// Description: Libretro software VDP renderer for the TMS9918A
//
// SPDX-License-Identifier: GPL-2.0-or-later
//
//----------------------------------------------------------------------------

#include <algorithm>
#include <cmath>
#include <cstring>
#include "common.hpp"
#include "logger.hpp"
#include "tms9918a-retro.hpp"

DBG_REGISTER( __FILE__ );

namespace
{
	struct sPaletteRGB
	{
		uint8_t r;
		uint8_t g;
		uint8_t b;
	};

	constexpr sPaletteRGB BasePalettes[ 3 ][ 17 ] =
	{
		// Palette 0: Default
		{
			{ 0x00, 0x00, 0x00 }, // 0x00 TI_TRANSPARENT - Background
			{ 0x00, 0x00, 0x00 }, // 0x01 TI_BLACK
			{ 0x48, 0x9C, 0x08 }, // 0x02 TI_MEDIUM_GREEN
			{ 0x70, 0xBF, 0x88 }, // 0x03 TI_LIGHT_GREEN
			{ 0x28, 0x3C, 0x8A }, // 0x04 TI_DARK_BLUE
			{ 0x50, 0x6C, 0xCF }, // 0x05 TI_LIGHT_BLUE
			{ 0xD0, 0x48, 0x00 }, // 0x06 TI_DARK_RED
			{ 0x00, 0xCC, 0xFF }, // 0x07 TI_CYAN
			{ 0xD0, 0x58, 0x28 }, // 0x08 TI_MEDIUM_RED
			{ 0xFF, 0xA0, 0x40 }, // 0x09 TI_LIGHT_RED
			{ 0xFC, 0xF0, 0x50 }, // 0x0A TI_DARK_YELLOW
			{ 0xFF, 0xFF, 0x80 }, // 0x0B TI_LIGHT_YELLOW
			{ 0x00, 0x80, 0x00 }, // 0x0C TI_DARK_GREEN
			{ 0xCD, 0x58, 0xCD }, // 0x0D TI_MAGENTA
			{ 0xE0, 0xE0, 0xE0 }, // 0x0E TI_GRAY
			{ 0xFF, 0xFF, 0xFF }, // 0x0F TI_WHITE
			{ 0xFF, 0xFF, 0xFF }  // 0x10 Text Mode Foreground
		},
		// Palette 1: NTSC
		{
			{ 0x00, 0x00, 0x00 }, // 0x00 TI_TRANSPARENT - Background
			{ 0x00, 0x00, 0x00 }, // 0x01 TI_BLACK
			{   33,  200,   66 }, // 0x02 TI_MEDIUM_GREEN
			{   94,  220,  120 }, // 0x03 TI_LIGHT_GREEN
			{   84,   85,  237 }, // 0x04 TI_DARK_BLUE
			{  125,  118,  252 }, // 0x05 TI_LIGHT_BLUE
			{  212,   82,   77 }, // 0x06 TI_DARK_RED
			{   66,  235,  245 }, // 0x07 TI_CYAN
			{  252,   85,   84 }, // 0x08 TI_MEDIUM_RED
			{  255,  121,  120 }, // 0x09 TI_LIGHT_RED
			{  212,  193,   84 }, // 0x0A TI_DARK_YELLOW
			{  230,  206,  128 }, // 0x0B TI_LIGHT_YELLOW
			{   33,  176,   59 }, // 0x0C TI_DARK_GREEN
			{  201,   91,  186 }, // 0x0D TI_MAGENTA
			{  204,  204,  204 }, // 0x0E TI_GRAY
			{ 0xFF, 0xFF, 0xFF }, // 0x0F TI_WHITE
			{ 0xFF, 0xFF, 0xFF }  // 0x10 Text Mode Foreground
		},
		// Palette 2: RGB
		{
			{ 0x00, 0x00, 0x00 }, // 0x00 TI_TRANSPARENT - Background
			{ 0x00, 0x00, 0x00 }, // 0x01 TI_BLACK
			{ 0x00, 0xCC, 0x00 }, // 0x02 TI_MEDIUM_GREEN
			{ 0x00, 0xFF, 0x00 }, // 0x03 TI_LIGHT_GREEN
			{ 0x00, 0x00, 0x80 }, // 0x04 TI_DARK_BLUE
			{ 0x00, 0x00, 0xFF }, // 0x05 TI_LIGHT_BLUE
			{ 0x80, 0x00, 0x00 }, // 0x06 TI_DARK_RED
			{ 0x00, 0xFF, 0xFF }, // 0x07 TI_CYAN
			{ 0xCC, 0x00, 0x00 }, // 0x08 TI_MEDIUM_RED
			{ 0xFF, 0x00, 0x00 }, // 0x09 TI_LIGHT_RED
			{ 0xB0, 0xB0, 0x00 }, // 0x0A TI_DARK_YELLOW
			{ 0xFF, 0xFF, 0x00 }, // 0x0B TI_LIGHT_YELLOW
			{ 0x00, 0x80, 0x00 }, // 0x0C TI_DARK_GREEN
			{ 0xB0, 0x00, 0xB0 }, // 0x0D TI_MAGENTA
			{ 0xCC, 0xCC, 0xCC }, // 0x0E TI_GRAY
			{ 0xFF, 0xFF, 0xFF }, // 0x0F TI_WHITE
			{ 0xFF, 0xFF, 0xFF }  // 0x10 Text Mode Foreground
		}
	};

	inline uint16_t PackRGB565( uint8_t r, uint8_t g, uint8_t b )
	{
		return static_cast<uint16_t>((( r >> 3 ) << 11 ) | (( g >> 2 ) << 5 ) | ( b >> 3 ));
	}

	inline uint32_t PackXRGB8888( uint8_t r, uint8_t g, uint8_t b )
	{
		return ( static_cast<uint32_t>( r ) << 16 ) | ( static_cast<uint32_t>( g ) << 8 ) | static_cast<uint32_t>( b );
	}
}

cRetroTMS9918A::cRetroTMS9918A( int refreshRate ) :
	cBaseObject( "cRetroTMS9918A" ),
	cTMS9918A( refreshRate ),
	m_ColorTable8888( ),
	m_ColorTable565( ),
	m_Palette( eRetroPalette::DEFAULT ),
	m_ShowBorders( false ),
	m_UseRGB565( true ),
	m_TextMode( false ),
	m_ChangesMade( true ),
	m_BlankChanged( true ),
	m_ColorsChanged( true ),
	m_SpritesChanged( true ),
	m_ScreenChanged( ),
	m_PatternChanged( ),
	m_CharUse( ),
	m_SpriteCharUse( ),
	m_CharacterPattern( ),
	m_BitmapScreen( ),
	m_BitmapSpriteScreen( ),
	m_ScreenSource( nullptr ),
	m_FrameBuffer565( ),
	m_FrameBuffer8888( )
{
	RebuildPalette( );
	Reset( );
}

cRetroTMS9918A::~cRetroTMS9918A( )
{
}

void cRetroTMS9918A::RebuildPalette( )
{
	int index = static_cast<int>( m_Palette );
	const sPaletteRGB *src = BasePalettes[( index >= 0 && index < 3 ) ? index : 0 ];

	for( int i = 0; i < 17; i++ )
	{
		uint8_t r = src[ i ].r;
		uint8_t g = src[ i ].g;
		uint8_t b = src[ i ].b;

		if( m_Palette == eRetroPalette::MONO )
		{
			uint8_t y = static_cast<uint8_t>( std::lrint( 0.299 * r + 0.587 * g + 0.114 * b ));
			r = g = b = y;
		}

		m_ColorTable8888[ i ] = PackXRGB8888( r, g, b );
		m_ColorTable565 [ i ] = PackRGB565( r, g, b );
	}

	int bg = ( m_Register[ 7 ] & 0x0F ) ? ( m_Register[ 7 ] & 0x0F ) : TI_BLACK;
	int fg = ( m_Register[ 7 ] & 0xF0 ) ? ( m_Register[ 7 ] >> 4   ) : bg;

	m_ColorTable8888[  0 ] = m_ColorTable8888[ bg ];
	m_ColorTable8888[ 16 ] = m_ColorTable8888[ fg ];
	m_ColorTable565 [  0 ] = m_ColorTable565 [ bg ];
	m_ColorTable565 [ 16 ] = m_ColorTable565 [ fg ];
}

void cRetroTMS9918A::SetPalette( eRetroPalette palette )
{
	if( m_Palette != palette )
	{
		m_Palette = palette;
		RebuildPalette( );
		MarkAllDirty( );
	}
}

void cRetroTMS9918A::SetShowBorders( bool show )
{
	if( m_ShowBorders != show )
	{
		m_ShowBorders = show;
		MarkAllDirty( );
	}
}

void cRetroTMS9918A::SetUseRGB565( bool rgb565 )
{
	if( m_UseRGB565 != rgb565 )
	{
		m_UseRGB565 = rgb565;
		MarkAllDirty( );
	}
}

void cRetroTMS9918A::SetRefreshRate( int refreshRate )
{
	m_RefreshRate = refreshRate;
}

void cRetroTMS9918A::MarkAllDirty( )
{
	m_ChangesMade    = true;
	m_BlankChanged   = true;
	m_ColorsChanged  = true;
	m_SpritesChanged = true;

	memset( m_ScreenChanged, true, sizeof( m_ScreenChanged ));
	memset( m_PatternChanged, true, sizeof( m_PatternChanged ));

	if( m_Memory != nullptr )
	{
		memset( m_CharUse, 0, sizeof( m_CharUse ));
		UINT8 *chr = m_Memory + m_ImageTableIndex;
		for( unsigned int i = 0; i < m_ImageTableSize; i++ )
		{
			int index = ( m_Mode & VDP_M3 ) ? ( i & 0xFF00 ) : 0;
			m_CharUse[ index + *chr++ ]++;
		}

		memset( m_SpriteCharUse, 0, sizeof( m_SpriteCharUse ));
		int count = ( m_Register[ 1 ] & VDP_SPRITE_SIZE ) ? 4 : 1;
		for( int s = 0; s < 32; s++ )
		{
			sSpriteAttributeEntry *sprite = &reinterpret_cast<sSpriteAttribute *>( m_Memory + m_SpriteAttrTableIndex )->data[ s ];
			for( int i = 0; i < count; i++ )
			{
				m_SpriteCharUse[( i + sprite->patternIndex ) % 256 ]++;
			}
		}
	}
}

bool cRetroTMS9918A::ParseState( const sStateSection &state )
{
	if( cTMS9918A::ParseState( state ) == true )
	{
		RebuildPalette( );
		MarkAllDirty( );
		return true;
	}

	return false;
}

void cRetroTMS9918A::Reset( )
{
	cTMS9918A::Reset( );

	m_TextMode       = false;
	m_ChangesMade    = true;
	m_BlankChanged   = true;
	m_ColorsChanged  = true;
	m_SpritesChanged = false;
	m_ScreenSource   = nullptr;

	memset( m_ScreenChanged, false, sizeof( m_ScreenChanged ));
	memset( m_PatternChanged, false, sizeof( m_PatternChanged ));
	memset( m_CharUse, 0, sizeof( m_CharUse ));
	memset( m_SpriteCharUse, 0, sizeof( m_SpriteCharUse ));
	memset( m_BitmapScreen, 0, sizeof( m_BitmapScreen ));
	memset( m_BitmapSpriteScreen, 0, sizeof( m_BitmapSpriteScreen ));

	if( m_Mode & VDP_M3 )
	{
		m_CharUse[ 0x0000 ] = m_ImageTableSize / 3;
		m_CharUse[ 0x0100 ] = m_ImageTableSize / 3;
		m_CharUse[ 0x0200 ] = m_ImageTableSize / 3;
	}
	else
	{
		m_CharUse[ 0x0000 ] = m_ImageTableSize;
	}

	m_SpriteCharUse[ 0 ] = 32;
	RebuildPalette( );
}

void cRetroTMS9918A::WriteData( UINT8 data )
{
	if( m_Memory != nullptr )
	{
		unsigned address = m_Address & 0x3FFF;
		UINT8 *MemPtr = &m_Memory[ address ];

		if( data != *MemPtr )
		{
			int type = m_MemoryType[ address ];

			if( type )
			{
				if( type & MEM_IMAGE_TABLE )
				{
					unsigned offset = address - m_ImageTableIndex;
					if( offset < m_ImageTableSize )
					{
						unsigned index = ( m_Mode & VDP_M3 ) ? offset & 0xFF00 : 0;
						m_ChangesMade             = true;
						m_ScreenChanged[ offset ] = true;
						m_CharUse[ index + *MemPtr ]--;
						m_CharUse[ index + data ]++;
					}
				}

				if( type & MEM_PATTERN_TABLE )
				{
					unsigned offset = address - m_PatternTableIndex;
					if( offset < m_PatternTableSize )
					{
						m_ChangesMade                  = true;
						m_PatternChanged[ offset / 8 ] = true;
					}
				}

				if( !m_TextMode )
				{
					if( type & MEM_COLOR_TABLE )
					{
						unsigned offset = address - m_ColorTableIndex;
						if( offset < m_ColorTableSize )
						{
							m_ChangesMade = true;
							if( m_Mode & VDP_M3 )
							{
								m_PatternChanged[ offset / 8 ] = true;
							}
							else if( offset * 8 + 8 <= sizeof( m_PatternChanged ))
							{
								memset( &m_PatternChanged[ offset * 8 ], true, 8 );
							}
						}
					}

					if( type & MEM_SPRITE_ATTR_TABLE )
					{
						unsigned index = ( address - m_SpriteAttrTableIndex ) / sizeof( sSpriteAttributeEntry );
						if( index < 32 )
						{
							sSpriteAttributeEntry *sprite = &reinterpret_cast<sSpriteAttribute *>( m_Memory + m_SpriteAttrTableIndex )->data[ index ];
							if( MemPtr == &sprite->patternIndex )
							{
								int count = ( m_Register[ 1 ] & VDP_SPRITE_SIZE ) ? 4 : 1;
								for( int i = 0; i < count; i++ )
								{
									m_SpriteCharUse[( i + *MemPtr ) % 256 ]--;
									m_SpriteCharUse[( i + data ) % 256 ]++;
								}
							}
							m_SpritesChanged = true;
						}
					}

					if( type & MEM_SPRITE_DESC_TABLE )
					{
						unsigned index = ( address - m_SpriteDescTableIndex ) / 8;
						if(( index < 256 ) && ( m_SpriteCharUse[ index ] > 0 ))
						{
							m_SpritesChanged = true;
						}
					}
				}
			}
		}
	}

	cTMS9918A::WriteData( data );
}

void cRetroTMS9918A::WriteRegister( size_t reg, UINT8 value )
{
	UINT8 oldReg = m_Register[ reg ];
	cTMS9918A::WriteRegister( reg, value );
	UINT8 newReg = m_Register[ reg ];

	if( oldReg == newReg )
	{
		return;
	}

	bool patternChanged = false;

	switch( reg )
	{
		case 0 :
			patternChanged = true;
			break;

		case 1 :
			if(( oldReg ^ newReg ) & VDP_BLANK_MASK )
			{
				m_BlankChanged = true;
				if(( oldReg ^ newReg ) == VDP_BLANK_MASK )
				{
					break;
				}
			}
			if(( oldReg ^ newReg ) & VDP_SPRITE_SIZE )
			{
				memset( m_SpriteCharUse, 0, sizeof( m_SpriteCharUse ));
				if( m_Memory != nullptr )
				{
					int count = ( newReg & VDP_SPRITE_SIZE ) ? 4 : 1;
					for( int s = 0; s < 32; s++ )
					{
						sSpriteAttributeEntry *sprite = &reinterpret_cast<sSpriteAttribute *>( m_Memory + m_SpriteAttrTableIndex )->data[ s ];
						for( int i = 0; i < count; i++ )
						{
							m_SpriteCharUse[( i + sprite->patternIndex ) % 256 ]++;
						}
					}
				}
				m_SpritesChanged = true;
			}
			// Fall through

		case 2 :
		{
			m_ChangesMade = true;
			memset( m_CharUse, 0, sizeof( m_CharUse ));
			if( m_Memory != nullptr )
			{
				UINT8 *chr = m_Memory + m_ImageTableIndex;
				for( unsigned int i = 0; i < m_ImageTableSize; i++ )
				{
					int index = ( m_Mode & VDP_M3 ) ? ( i & 0xFF00 ) : 0;
					m_CharUse[ index + *chr++ ]++;
				}
			}
			memset( m_ScreenChanged, true, m_ImageTableSize );
			break;
		}

		case 7 :
		{
			m_ColorsChanged = true;

			int bg = ( newReg & 0x0F ) ? ( newReg & 0x0F ) : TI_BLACK;
			int fg = ( newReg & 0xF0 ) ? ( newReg >> 4   ) : bg;

			m_ColorTable8888[  0 ] = m_ColorTable8888[ bg ];
			m_ColorTable8888[ 16 ] = m_ColorTable8888[ fg ];
			m_ColorTable565 [  0 ] = m_ColorTable565 [ bg ];
			m_ColorTable565 [ 16 ] = m_ColorTable565 [ fg ];

			// Fall through
		}
		case 3 :
		case 4 :
			patternChanged = true;
			break;
	}

	if( patternChanged )
	{
		m_ChangesMade = true;
		memset( m_PatternChanged, true, sizeof( m_PatternChanged ));
	}
}

bool cRetroTMS9918A::Retrace( )
{
	cTMS9918A::Retrace( );

	if( BlankEnabled( ) == true )
	{
		m_BlankChanged = false;
		m_ScreenSource = nullptr;
		ComposeFrameBuffer( );
		return true;
	}

	if( m_ChangesMade || m_SpritesChanged || m_BlankChanged || m_ColorsChanged )
	{
		bool bNeedsUpdate = false;

		if(( m_Mode & VDP_MODE_ILLEGAL ) == VDP_MODE_ILLEGAL )
		{
			bNeedsUpdate = RefreshInvalid( );
		}
		else if( m_Mode & VDP_M3 )
		{
			bNeedsUpdate = RefreshBitMap( );
		}
		else if( m_Mode & VDP_M2 )
		{
			bNeedsUpdate = RefreshMultiColor( );
		}
		else
		{
			bNeedsUpdate = RefreshGraphics( );
		}

		m_ChangesMade   = false;
		m_ColorsChanged = false;

		if( bNeedsUpdate || m_SpritesChanged || m_BlankChanged || ( m_ScreenSource == nullptr ))
		{
			UpdateScreen( );
			m_BlankChanged = false;
		}
	}

	ComposeFrameBuffer( );

	return true;
}

void cRetroTMS9918A::Render( )
{
	ComposeFrameBuffer( );
}

void cRetroTMS9918A::ComposeFrameBuffer( )
{
	if( m_UseRGB565 )
	{
		uint16_t bg = m_ColorTable565[ 0 ];

		if( m_ScreenSource == nullptr )
		{
			unsigned total = GetOutputWidth( ) * GetOutputHeight( );
			std::fill( m_FrameBuffer565, m_FrameBuffer565 + total, bg );
			return;
		}

		if( !m_ShowBorders )
		{
			const UINT8 *src = m_ScreenSource;
			uint16_t *dst = m_FrameBuffer565;
			for( int i = 0; i < VDP_WIDTH * VDP_HEIGHT; i++ )
			{
				dst[ i ] = m_ColorTable565[ src[ i ] & 0x1F ];
			}
		}
		else
		{
			uint16_t *dst = m_FrameBuffer565;
			for( int y = 0; y < RETRO_VDP_BORDER_Y; y++ )
			{
				std::fill( dst, dst + RETRO_VDP_BORDER_WIDTH, bg );
				dst += RETRO_VDP_BORDER_WIDTH;
			}

			const UINT8 *src = m_ScreenSource;
			for( int y = 0; y < VDP_HEIGHT; y++ )
			{
				for( int x = 0; x < RETRO_VDP_BORDER_X; x++ )
				{
					*dst++ = bg;
				}
				for( int x = 0; x < VDP_WIDTH; x++ )
				{
					*dst++ = m_ColorTable565[ *src++ & 0x1F ];
				}
				for( int x = 0; x < RETRO_VDP_BORDER_X; x++ )
				{
					*dst++ = bg;
				}
			}

			for( int y = 0; y < RETRO_VDP_BORDER_Y; y++ )
			{
				std::fill( dst, dst + RETRO_VDP_BORDER_WIDTH, bg );
				dst += RETRO_VDP_BORDER_WIDTH;
			}
		}
	}
	else
	{
		uint32_t bg = m_ColorTable8888[ 0 ];

		if( m_ScreenSource == nullptr )
		{
			unsigned total = GetOutputWidth( ) * GetOutputHeight( );
			std::fill( m_FrameBuffer8888, m_FrameBuffer8888 + total, bg );
			return;
		}

		if( !m_ShowBorders )
		{
			const UINT8 *src = m_ScreenSource;
			uint32_t *dst = m_FrameBuffer8888;
			for( int i = 0; i < VDP_WIDTH * VDP_HEIGHT; i++ )
			{
				dst[ i ] = m_ColorTable8888[ src[ i ] & 0x1F ];
			}
		}
		else
		{
			uint32_t *dst = m_FrameBuffer8888;
			for( int y = 0; y < RETRO_VDP_BORDER_Y; y++ )
			{
				std::fill( dst, dst + RETRO_VDP_BORDER_WIDTH, bg );
				dst += RETRO_VDP_BORDER_WIDTH;
			}

			const UINT8 *src = m_ScreenSource;
			for( int y = 0; y < VDP_HEIGHT; y++ )
			{
				for( int x = 0; x < RETRO_VDP_BORDER_X; x++ )
				{
					*dst++ = bg;
				}
				for( int x = 0; x < VDP_WIDTH; x++ )
				{
					*dst++ = m_ColorTable8888[ *src++ & 0x1F ];
				}
				for( int x = 0; x < RETRO_VDP_BORDER_X; x++ )
				{
					*dst++ = bg;
				}
			}

			for( int y = 0; y < RETRO_VDP_BORDER_Y; y++ )
			{
				std::fill( dst, dst + RETRO_VDP_BORDER_WIDTH, bg );
				dst += RETRO_VDP_BORDER_WIDTH;
			}
		}
	}
}

void cRetroTMS9918A::UpdateCharacterPatternGraphics( int ch, UINT8 fore, UINT8 back, UINT8 *pattern )
{
	UINT8 *pData = m_CharacterPattern[ ch ];

	for( int y = 0; y < 8; y++ )
	{
		UINT8 row = *pattern++;
		*pData++ = ( row & 0x80 ) ? fore : back;
		*pData++ = ( row & 0x40 ) ? fore : back;
		*pData++ = ( row & 0x20 ) ? fore : back;
		*pData++ = ( row & 0x10 ) ? fore : back;
		*pData++ = ( row & 0x08 ) ? fore : back;
		*pData++ = ( row & 0x04 ) ? fore : back;
		*pData++ = ( row & 0x02 ) ? fore : back;
		*pData++ = ( row & 0x01 ) ? fore : back;
	}
}

void cRetroTMS9918A::UpdateCharacterPatternBitMap( int ch, UINT8 *pattern )
{
	UINT8 *pColor = reinterpret_cast<sBitmapColorTable *>( m_Memory + m_ColorTableIndex )->data[ ch & m_ColorTableMask ];
	UINT8 *pData = m_CharacterPattern[ ch ];

	for( int y = 0; y < 8; y++ )
	{
		UINT8 row  = *pattern++;
		UINT8 fore = static_cast<UINT8>( pColor[ y ] >> 4 );
		UINT8 back = static_cast<UINT8>( pColor[ y ] & 0x0F );
		*pData++ = ( row & 0x80 ) ? fore : back;
		*pData++ = ( row & 0x40 ) ? fore : back;
		*pData++ = ( row & 0x20 ) ? fore : back;
		*pData++ = ( row & 0x10 ) ? fore : back;
		*pData++ = ( row & 0x08 ) ? fore : back;
		*pData++ = ( row & 0x04 ) ? fore : back;
		*pData++ = ( row & 0x02 ) ? fore : back;
		*pData++ = ( row & 0x01 ) ? fore : back;
	}
}

void cRetroTMS9918A::UpdateScreenGraphics( int x, int y, int ch )
{
	UINT8 *pSrcData = m_CharacterPattern[ ch ];
	UINT8 *pDstData = m_BitmapScreen + (( y * 8 ) * VDP_WIDTH + ( x * 8 ));

	for( int row = 0; row < 8; row++ )
	{
		memcpy( pDstData, pSrcData, 8 );
		pSrcData += 8;
		pDstData += VDP_WIDTH;
	}
}

void cRetroTMS9918A::UpdateScreenText( int x, int y, int ch )
{
	UINT8 *pSrcData = m_CharacterPattern[ ch ];
	UINT8 *pDstData = m_BitmapScreen + ( y * 8 * VDP_WIDTH + x * 6 + 8 );

	for( int row = 0; row < 8; row++ )
	{
		memcpy( pDstData, pSrcData, 6 );
		pSrcData += 8;
		pDstData += VDP_WIDTH;
	}
}

void cRetroTMS9918A::UpdateScreenMultiColor( int x, int y, int ch )
{
	UINT8 back = static_cast<UINT8>( m_Register[ 7 ] & 0x0F );
	UINT8 *pSrcData = &reinterpret_cast<sPatternDescriptor *>( m_Memory + m_PatternTableIndex )->data[ ch ][( y & 0x03 ) * 2 ];
	UINT8 *pDstData = m_BitmapScreen + ( y * 8 * VDP_WIDTH + x * 8 );

	for( int i = 0; i < 2; i++ )
	{
		UINT8 leftColor  = ( *pSrcData & 0xF0 ) ? ( *pSrcData >> 4 ) : back;
		UINT8 rightColor = ( *pSrcData & 0x0F ) ? ( *pSrcData & 0x0F ) : back;

		for( int row = 0; row < 4; row++ )
		{
			UINT8 *pDst = pDstData;
			for( int col = 0; col < 4; col++ )
			{
				*pDst++ = leftColor;
			}
			for( int col = 0; col < 4; col++ )
			{
				*pDst++ = rightColor;
			}
			pDstData += VDP_WIDTH;
		}

		pSrcData++;
	}
}

void cRetroTMS9918A::MarkScreenChanges( int ch )
{
	if( m_Memory == nullptr )
	{
		return;
	}

	UINT8 *screen = m_Memory + m_ImageTableIndex;
	bool *changed = m_ScreenChanged;
	int count     = m_CharUse[ ch ];
	int max       = m_ImageTableSize;

	if( m_Mode & VDP_M3 )
	{
		int index = ch & 0xFF00;
		screen  += index;
		changed += index;
		ch  &= 0xFF;
		max /= 3;
	}

	for( int i = max; i; i-- )
	{
		if( *screen++ == ch )
		{
			*changed = true;
			if( --count == 0 )
			{
				break;
			}
		}
		changed++;
	}
}

void cRetroTMS9918A::DrawSprite( int index )
{
	sSpriteAttributeEntry *sprite = &reinterpret_cast<sSpriteAttribute *>( m_Memory + m_SpriteAttrTableIndex )->data[ index ];

	UINT8 colorIndex = static_cast<UINT8>( sprite->earlyClock & 0x0F );
	if( colorIndex == 0 )
	{
		return;
	}

	int count = ( m_Register[ 1 ] & VDP_SPRITE_SIZE ) ? 4 : 1;
	int size  = ( m_Register[ 1 ] & VDP_SPRITE_MAGNIFY ) ? 16 : 8;

	UINT8 *pScreen = m_BitmapSpriteScreen;

	for( int i = 0; i < count; i++ )
	{
		int posX = static_cast<int>( sprite->posX ) + ( i / 2 ) * size;
		if( sprite->earlyClock & 0x80 )
		{
			posX -= 32;
		}

		if( posX >= VDP_WIDTH )
		{
			continue;
		}

		UINT8 row       = static_cast<UINT8>( sprite->posY + 1 + ( i % 2 ) * size );
		UINT8 *pDstData = pScreen + posX;
		UINT8 *pattern  = reinterpret_cast<sSpriteDescriptor *>( m_Memory + m_SpriteDescTableIndex )->data[( sprite->patternIndex + i ) % 256 ];

		for( int y = 0; y < size; y++, row++ )
		{
			if(( row < VDP_HEIGHT ) && ( index <= m_MaxSprite[ row ] ))
			{
				UINT8 bits   = *pattern;
				UINT8 *pData = pDstData + row * VDP_WIDTH;

				if( bits )
				{
					for( int x = 0, col = posX; x < size; x++, col++ )
					{
						if( col >= VDP_WIDTH )
						{
							break;
						}
						if(( bits & 0x80 ) && ( col >= 0 ))
						{
							*pData = colorIndex;
						}
						pData++;
						if(( size == 8 ) || ( x & 1 ))
						{
							bits <<= 1;
						}
					}
				}
			}

			if(( size == 8 ) || ( y & 1 ))
			{
				pattern++;
			}
		}
	}
}

bool cRetroTMS9918A::RefreshInvalid( )
{
	bool needsUpdate = m_ColorsChanged;

	if( needsUpdate )
	{
		m_ColorsChanged = false;

		UINT8 fore = static_cast<UINT8>( m_Register[ 7 ] >> 4 );
		UINT8 back = static_cast<UINT8>( m_Register[ 7 ] & 0x0F );

		memset( m_BitmapScreen, back, sizeof( m_BitmapScreen ));

		for( int y = 0; y < VDP_HEIGHT; y++ )
		{
			UINT8 *pDst = m_BitmapScreen + y * VDP_WIDTH + 8;
			for( int x = 0; x < 40 * 6; x++ )
			{
				pDst[ x ] = ( x % 6 < 4 ) ? fore : back;
			}
		}
	}

	return needsUpdate;
}

bool cRetroTMS9918A::RefreshGraphics( )
{
	if( m_Memory == nullptr )
	{
		return false;
	}

	UINT8 fore = static_cast<UINT8>( m_Register[ 7 ] >> 4 );
	UINT8 back = static_cast<UINT8>( m_Register[ 7 ] & 0x0F );

	for( int ch = 0; ch < 256; ch++ )
	{
		if( !m_TextMode && (( ch & 0x07 ) == 0 ))
		{
			fore = static_cast<UINT8>( reinterpret_cast<sColorTable *>( m_Memory + m_ColorTableIndex )->data[ ch / 8 ] >> 4 );
			back = static_cast<UINT8>( reinterpret_cast<sColorTable *>( m_Memory + m_ColorTableIndex )->data[ ch / 8 ] & 0x0F );
		}
		if( m_PatternChanged[ ch ] && m_CharUse[ ch ] )
		{
			UpdateCharacterPatternGraphics( ch, fore, back, reinterpret_cast<UINT8 *>( &reinterpret_cast<sPatternDescriptor *>( m_Memory + m_PatternTableIndex )->data[ ch ] ));
			m_PatternChanged[ ch ] = false;
			MarkScreenChanges( ch );
		}
	}

	bool needsUpdate = false;
	bool *changed = m_ScreenChanged;
	UINT8 *chr = m_Memory + m_ImageTableIndex;

	int height = GetScreenHeight( );
	int width  = GetScreenWidth( );
	for( int y = 0; y < height; y++ )
	{
		for( int x = 0; x < width; x++ )
		{
			if( *changed++ )
			{
				if( m_TextMode )
				{
					UpdateScreenText( x, y, *chr );
				}
				else
				{
					UpdateScreenGraphics( x, y, *chr );
				}
				needsUpdate = true;
			}
			chr++;
		}
	}

	if( needsUpdate )
	{
		memset( m_ScreenChanged, false, sizeof( m_ScreenChanged ));
	}

	if( m_TextMode && m_ColorsChanged )
	{
		for( int y = 0; y < height * 8; y++ )
		{
			memset( m_BitmapScreen + y * VDP_WIDTH, back, 8 );
			memset( m_BitmapScreen + y * VDP_WIDTH + ( 40 * 6 + 8 ), back, 8 );
		}

		m_ColorsChanged = false;
		needsUpdate = true;
	}

	return needsUpdate;
}

bool cRetroTMS9918A::RefreshBitMap( )
{
	if( m_Memory == nullptr )
	{
		return false;
	}

	for( int ch = 0; ch < 3 * 256; ch++ )
	{
		if( m_PatternChanged[ ch ] && m_CharUse[ ch ] )
		{
			UpdateCharacterPatternBitMap( ch, reinterpret_cast<UINT8 *>( reinterpret_cast<sPatternDescriptor *>( m_Memory + m_PatternTableIndex )->data[ ch & m_PatternTableMask ] ));
			m_PatternChanged[ ch ] = false;
			MarkScreenChanges( ch );
		}
	}

	bool needsUpdate = false;
	UINT8 *chr = m_Memory + m_ImageTableIndex;

	for( unsigned int i = 0; i < m_ImageTableSize; i++ )
	{
		if( m_ScreenChanged[ i ] )
		{
			if( m_TextMode )
			{
				UpdateScreenText( i % 40, i / 40, chr[ i ] );
			}
			else
			{
				UpdateScreenGraphics( i % 32, i / 32, ( i & 0xFF00 ) + chr[ i ] );
			}
			needsUpdate = true;
		}
	}

	if( needsUpdate )
	{
		memset( m_ScreenChanged, false, sizeof( m_ScreenChanged ));
	}

	return needsUpdate;
}

bool cRetroTMS9918A::RefreshMultiColor( )
{
	if( m_Memory == nullptr )
	{
		return false;
	}

	bool needsUpdate = false;
	UINT8 *chr = m_Memory + m_ImageTableIndex;

	for( unsigned int i = 0; i < m_ImageTableSize; i++ )
	{
		UINT8 index = chr[ i ];
		if( m_ScreenChanged[ i ] || m_PatternChanged[ index ] )
		{
			UpdateScreenMultiColor( i % 32, i / 32, index );
			needsUpdate = true;
		}
	}

	if( needsUpdate )
	{
		memset( m_ScreenChanged, false, sizeof( m_ScreenChanged ));
		memset( m_PatternChanged, false, sizeof( m_PatternChanged ));
	}

	return needsUpdate;
}

const UINT8 *cRetroTMS9918A::UpdateSprites( )
{
	if( m_Memory == nullptr )
	{
		return m_BitmapScreen;
	}

	sSpriteAttributeEntry *sprite = &reinterpret_cast<sSpriteAttribute *>( m_Memory + m_SpriteAttrTableIndex )->data[ 0 ];

	int i;
	for( i = 0; i < 32; i++ )
	{
		if( sprite[ i ].posY == 0xD0 )
		{
			break;
		}
	}

	if( i == 0 )
	{
		m_SpritesChanged = false;
		return m_BitmapScreen;
	}

	memcpy( m_BitmapSpriteScreen, m_BitmapScreen, sizeof( m_BitmapScreen ));

	while( --i >= 0 )
	{
		DrawSprite( i );
	}

	m_SpritesChanged = false;

	return m_BitmapSpriteScreen;
}

void cRetroTMS9918A::UpdateScreen( )
{
	m_ScreenSource = m_TextMode ? m_BitmapScreen : UpdateSprites( );
}

void cRetroTMS9918A::FlipAddressing( )
{
	cTMS9918A::FlipAddressing( );
	MarkAllDirty( );
}

bool cRetroTMS9918A::SetMode( int mode )
{
	if( cTMS9918A::SetMode( mode ) == false )
	{
		return false;
	}

	m_TextMode      = ( m_Mode & VDP_M1 ) ? true : false;
	m_ColorsChanged = true;
	m_ChangesMade   = true;

	memset( m_PatternChanged, true, sizeof( m_PatternChanged ));
	memset( m_ScreenChanged, true, sizeof( m_ScreenChanged ));
	memset( m_CharUse, 0, sizeof( m_CharUse ));

	if( m_Memory != nullptr )
	{
		UINT8 *chr = m_Memory + m_ImageTableIndex;

		for( unsigned int i = 0; i < m_ImageTableSize; i++ )
		{
			int index = ( m_Mode & VDP_M3 ) ? ( i & 0xFF00 ) : 0;
			m_CharUse[ index + *chr++ ]++;
		}
	}

	return true;
}
