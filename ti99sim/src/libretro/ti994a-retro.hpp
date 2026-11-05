//----------------------------------------------------------------------------
//
// File:        ti994a-retro.hpp
// Date:        31-Oct-2026
// Programmer:  Aaron Szymanski
//
// Description: Libretro TI-99/4A machine coordinator and binary serialization
//
// SPDX-License-Identifier: GPL-2.0-or-later
//
//----------------------------------------------------------------------------

#ifndef TI994A_RETRO_HPP_
#define TI994A_RETRO_HPP_

#include <cstddef>
#include <cstdint>
#include "libretro.h"
#include "ti994a.hpp"
#include "tms9918a-retro.hpp"
#include "tms9919-retro.hpp"

void RetroLog( enum retro_log_level level, const char *fmt, ... );
void RetroNotify( const char *message, unsigned durationMs = 3000, enum retro_log_level level = RETRO_LOG_INFO );

class cRetroTI994A :
	public cTI994A
{
	cRetroTMS9918A     *m_RetroVDP;
	cRetroTMS9919      *m_RetroSound;
	bool                m_Enable32K;
	bool                m_SpeechEnabled;
	int                 m_CPUSpeedPercent;
	int                 m_RefreshRate;
	UINT8              *m_Fallback32K[ 16 ];

public:

	cRetroTI994A( iCartridge *console, cRetroTMS9918A *vdp, cRetroTMS9919 *sound, iTMS5220 *speech, bool enable32K = true );
	virtual ~cRetroTI994A( ) override;

	void RunFrame( int16_t *audioOut, size_t *audioFramesOut );

	void SetRefreshRate( int hz );
	int  GetRefreshRate( ) const { return m_RefreshRate; }

	void SetCPUSpeed( int percent );
	int  GetCPUSpeed( ) const    { return m_CPUSpeedPercent; }

	void Set32KExpansion( bool enable );
	bool Get32KExpansion( ) const { return m_Enable32K; }

	void SetSpeechEnabled( bool enable );
	bool GetSpeechEnabled( ) const { return m_SpeechEnabled; }

	void SetAlphaLockFix( bool enable );

	iTMS9901       *GetPIC( ) const        { return m_PIC; }
	cRetroTMS9918A *GetRetroVDP( ) const   { return m_RetroVDP; }
	cRetroTMS9919  *GetRetroSound( ) const { return m_RetroSound; }
	iCartridge     *GetCartridge( ) const  { return m_Cartridge; }

	// RetroArch memory descriptors
	void  *GetSaveRAMPtr( ) const;
	size_t GetSaveRAMSize( ) const;
	void  *GetScratchpadPtr( )  { return m_Scratchpad; }
	size_t GetScratchpadSize( ) const { return sizeof( m_Scratchpad ); }

	// Fast deterministic binary savestates
	size_t SerializeSize( ) const;
	bool   Serialize( void *data, size_t size ) const;
	bool   Unserialize( const void *data, size_t size );

	// iComputer overrides
	virtual void Reset( ) override;
	virtual bool Sleep( int cycles, UINT32 timeout ) override;
	virtual bool WakeCPU( UINT32 timeout ) override;

protected:

	virtual void TimerHookProc( UINT32 clockCycles ) override;
	virtual UINT8 SpeechWriteBreakPoint( ADDRESS address, UINT8 data ) override;
	virtual UINT8 SpeechReadBreakPoint( ADDRESS address, UINT8 data ) override;

	void EnsureConsole32KBanks( );

private:

	cRetroTI994A( const cRetroTI994A & ) = delete;
	cRetroTI994A &operator =( const cRetroTI994A & ) = delete;
};

#endif
