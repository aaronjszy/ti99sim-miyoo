//----------------------------------------------------------------------------
//
// File:        osk-retro.hpp
// Date:        31-Oct-2026
// Programmer:  Aaron Szymanski
//
// Description: Built-in translucent TI-99/4A On-Screen Keyboard for Libretro
//
// SPDX-License-Identifier: GPL-2.0-or-later
//
//----------------------------------------------------------------------------

#ifndef OSK_RETRO_HPP_
#define OSK_RETRO_HPP_

#include <cstdint>
#include "itms9901.hpp"

class cRetroOSK
{
public:

	struct sPadInput
	{
		bool up;
		bool down;
		bool left;
		bool right;
		bool a;
		bool b;
		bool x;
		bool y;
		bool start;
		bool l;
		bool r;
	};

private:

	bool            m_Active;
	bool            m_TopPosition;
	int             m_CursorRow;
	int             m_CursorCol;

	bool            m_StickyShift;
	bool            m_StickyFctn;
	bool            m_StickyCtrl;
	bool            m_AlphaLock;

	int             m_ActiveKeySym;
	VIRTUAL_KEY_E   m_ActiveVKey1;
	VIRTUAL_KEY_E   m_ActiveVKey2;
	bool            m_ClearStickyOnRelease;

	sPadInput       m_PrevInput;
	int             m_RepeatTimer;

	void ReleaseActiveKey( iTMS9901 *pic );
	void PressSelectedKey( iTMS9901 *pic );

public:

	cRetroOSK( );
	~cRetroOSK( ) = default;

	void Reset( iTMS9901 *pic );

	bool IsActive( ) const { return m_Active; }
	void SetActive( bool active, iTMS9901 *pic );
	void Toggle( iTMS9901 *pic );

	bool IsTopPosition( ) const { return m_TopPosition; }
	void SetTopPosition( bool top ) { m_TopPosition = top; }

	bool IsAlphaLock( ) const { return m_AlphaLock; }
	void SetAlphaLock( bool enabled, iTMS9901 *pic );

	void Update( const sPadInput &input, iTMS9901 *pic );

	void RenderRGB565( uint16_t *fb, unsigned width, unsigned height, unsigned pitchPixels ) const;
	void RenderXRGB8888( uint32_t *fb, unsigned width, unsigned height, unsigned pitchPixels ) const;
};

#endif
