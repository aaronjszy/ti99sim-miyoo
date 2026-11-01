//----------------------------------------------------------------------------
//
// File:        tms9919-retro.hpp
// Date:        31-Oct-2026
// Programmer:  Aaron Szymanski
//
// Description: Libretro synchronous PSG + TMS5220 audio synthesizer
//
// SPDX-License-Identifier: GPL-2.0-or-later
//
//----------------------------------------------------------------------------

#ifndef TMS9919_RETRO_HPP_
#define TMS9919_RETRO_HPP_

#include <cstddef>
#include <cstdint>
#include "tms9919.hpp"

struct iTMS9900;

class cRetroTMS9919 :
	public cTMS9919
{
	friend class cRetroTI994A;

	struct sVoiceInfo
	{
		float  period;
		float  toggle;
		int    setting;
	};

	int          m_SampleRate;
	int          m_MasterVolume;
	int          m_PsgVolume;
	int          m_SpeechVolume;
	int          m_VolumeTable[ 16 ];
	sVoiceInfo   m_Info[ 4 ];
	int          m_ShiftRegister;
	int          m_NoiseGenerator;

	iTMS9900    *m_pCPU;
	uint32_t     m_FrameStartClocks;
	uint32_t     m_FrameTotalClocks;
	int          m_FrameTargetSamples;
	int          m_RenderedSamples;

	INT16        m_MonoBuffer[ 2048 ];

	virtual void SetNoise( uint8_t color, uint8_t type ) override;
	virtual void SetFrequency( uint8_t tone, uint16_t freq ) override;
	virtual void SetAttenuation( uint8_t tone, uint8_t atten ) override;

	void RenderSamples( INT16 *dst, int count );

public:

	cRetroTMS9919( int sampleFreq = 44100 );

	void SetCPU( iTMS9900 *cpu ) { m_pCPU = cpu; }

	// iTMS9919 Methods
	virtual void WriteData( UINT8 data ) override;
	virtual int GetPlaybackFrequency( ) override;

	// Frame-sliced synchronous audio generation
	void BeginFrame( uint32_t startClocks, uint32_t totalClocks, int targetSamples );
	void CatchUpToClocks( uint32_t currentClocks );
	size_t EndFrame( int16_t *stereoOut );

	int  GetMasterVolume( ) const { return m_MasterVolume; }
	void SetMasterVolume( int volume );

	int  GetPsgVolume( ) const { return m_PsgVolume; }
	void SetPsgVolume( int volume );

	int  GetSpeechVolume( ) const { return m_SpeechVolume; }
	void SetSpeechVolume( int volume );

	void RefreshVoices( );

protected:

	virtual ~cRetroTMS9919( ) override;

private:

	cRetroTMS9919( const cRetroTMS9919 & ) = delete;
	void operator =( const cRetroTMS9919 & ) = delete;
};

#endif
