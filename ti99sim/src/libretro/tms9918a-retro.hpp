//----------------------------------------------------------------------------
//
// File:        tms9918a-retro.hpp
// Date:        31-Oct-2026
// Programmer:  Aaron Szymanski
//
// Description: Libretro software VDP renderer for the TMS9918A
//
// SPDX-License-Identifier: GPL-2.0-or-later
//
//----------------------------------------------------------------------------

#ifndef TMS9918A_RETRO_HPP_
#define TMS9918A_RETRO_HPP_

#include <cstdint>
#include "tms9918a.hpp"

#define RETRO_VDP_BORDER_X      16
#define RETRO_VDP_BORDER_Y      12
#define RETRO_VDP_BORDER_WIDTH  ( VDP_WIDTH + 2 * RETRO_VDP_BORDER_X )
#define RETRO_VDP_BORDER_HEIGHT ( VDP_HEIGHT + 2 * RETRO_VDP_BORDER_Y )

enum class eRetroPalette
{
	DEFAULT = 0,
	NTSC    = 1,
	RGB     = 2,
	MONO    = 3
};

class cRetroTMS9918A :
	public cTMS9918A
{
	friend class cRetroTI994A;

	uint32_t        m_ColorTable8888[ 17 ];
	uint16_t        m_ColorTable565 [ 17 ];
	eRetroPalette   m_Palette;
	bool            m_ShowBorders;
	bool            m_UseRGB565;

	bool            m_TextMode;
	bool            m_ChangesMade;
	bool            m_BlankChanged;
	bool            m_ColorsChanged;
	bool            m_SpritesChanged;

	bool            m_ScreenChanged[ 0x03C0 ];
	bool            m_PatternChanged[ 256 * 3 ];
	int             m_CharUse[ 256 * 3 ];
	int             m_SpriteCharUse[ 256 ];

	UINT8           m_CharacterPattern[ 3 * 256 ][ 8 * 8 ];
	UINT8           m_BitmapScreen[ VDP_WIDTH * VDP_HEIGHT ];
	UINT8           m_BitmapSpriteScreen[ VDP_WIDTH * VDP_HEIGHT ];
	const UINT8    *m_ScreenSource;

	uint16_t        m_FrameBuffer565 [ RETRO_VDP_BORDER_WIDTH * RETRO_VDP_BORDER_HEIGHT ];
	uint32_t        m_FrameBuffer8888[ RETRO_VDP_BORDER_WIDTH * RETRO_VDP_BORDER_HEIGHT ];

public:

	cRetroTMS9918A( int refreshRate = 60 );

	// iStateObject methods
	virtual bool ParseState( const sStateSection &state ) override;

	// iTMS9918A methods
	virtual void Reset( ) override;
	virtual void WriteData( UINT8 data ) override;
	virtual void WriteRegister( size_t reg, UINT8 value ) override;
	virtual bool Retrace( ) override;
	virtual void Render( ) override;

	void SetPalette( eRetroPalette palette );
	eRetroPalette GetPalette( ) const { return m_Palette; }

	void SetShowBorders( bool show );
	bool GetShowBorders( ) const { return m_ShowBorders; }

	void SetUseRGB565( bool rgb565 );
	bool GetUseRGB565( ) const { return m_UseRGB565; }

	void SetRefreshRate( int refreshRate );

	unsigned GetOutputWidth( ) const  { return m_ShowBorders ? RETRO_VDP_BORDER_WIDTH  : VDP_WIDTH; }
	unsigned GetOutputHeight( ) const { return m_ShowBorders ? RETRO_VDP_BORDER_HEIGHT : VDP_HEIGHT; }
	size_t   GetOutputPitch( ) const  { return GetOutputWidth( ) * ( m_UseRGB565 ? sizeof( uint16_t ) : sizeof( uint32_t )); }

	const void *GetFrameBuffer( ) const { return m_UseRGB565 ? static_cast<const void *>( m_FrameBuffer565 ) : static_cast<const void *>( m_FrameBuffer8888 ); }
	uint16_t *GetFrameBuffer565( )  { return m_FrameBuffer565; }
	uint32_t *GetFrameBuffer8888( ) { return m_FrameBuffer8888; }

	void MarkAllDirty( );

protected:

	void RebuildPalette( );
	void ComposeFrameBuffer( );

	void DrawSprite( int index );
	const UINT8 *UpdateSprites( );

	void UpdateCharacterPatternGraphics( int ch, UINT8 fore, UINT8 back, UINT8 *pattern );
	void UpdateCharacterPatternBitMap( int ch, UINT8 *pattern );

	void UpdateScreenGraphics( int x, int y, int ch );
	void UpdateScreenText( int x, int y, int ch );
	void UpdateScreenMultiColor( int x, int y, int ch );

	int ScreenSize( ) const      { return m_TextMode ? 24 * 40 : 24 * 32; }
	int GetCellWidth( ) const    { return m_TextMode ? 6 : 8; }
	int GetScreenWidth( ) const  { return m_TextMode ? 40 : 32; }
	int GetScreenHeight( ) const { return 24; }

	void MarkScreenChanges( int ch );

	bool RefreshInvalid( );
	bool RefreshGraphics( );
	bool RefreshBitMap( );
	bool RefreshMultiColor( );

	void UpdateScreen( );

	// cTMS9918A protected methods
	virtual bool SetMode( int mode ) override;
	virtual void FlipAddressing( ) override;

	virtual ~cRetroTMS9918A( ) override;

private:

	cRetroTMS9918A( const cRetroTMS9918A & ) = delete;
	void operator =( const cRetroTMS9918A & ) = delete;
};

#endif
