//----------------------------------------------------------------------------
//
// File:        tms9919-retro.cpp
// Date:        31-Oct-2026
// Programmer:  Aaron Szymanski
//
// Description: Libretro synchronous PSG + TMS5220 audio synthesizer
//
// SPDX-License-Identifier: GPL-2.0-or-later
//
//----------------------------------------------------------------------------

#include <algorithm>
#include <cstring>
#include "common.hpp"
#include "logger.hpp"
#include "itms9900.hpp"
#include "itms5220.hpp"
#include "tms9919-retro.hpp"

DBG_REGISTER( __FILE__ );

#define NOISE_RESET              0x00F35
#define NOISE_WHITE_GENERATOR    0x12000
#define NOISE_PERIODIC_GENERATOR 0x08000
#define CLOCK_FREQUENCY          3579545

cRetroTMS9919::cRetroTMS9919( int sampleFreq ) :
	cBaseObject( "cRetroTMS9919" ),
	m_SampleRate( sampleFreq ),
	m_MasterVolume( 75 ),
	m_VolumeTable( ),
	m_Info( ),
	m_ShiftRegister( NOISE_RESET ),
	m_NoiseGenerator( NOISE_WHITE_GENERATOR ),
	m_pCPU( nullptr ),
	m_FrameStartClocks( 0 ),
	m_FrameTotalClocks( 50000 ),
	m_FrameTargetSamples( 735 ),
	m_RenderedSamples( 0 ),
	m_MonoBuffer( )
{
	float volume = 32768.0f / 4.0f;
	for( unsigned int i = 0; i < SIZE( m_VolumeTable ) - 1; i++ )
	{
		m_VolumeTable[ i ] = static_cast<int>( volume );
		volume = volume / 1.258925412f; // Reduce volume by 2dB per step
	}
	m_VolumeTable[ 15 ] = 0;

	uint8_t color = m_NoiseColor;
	uint8_t type  = m_NoiseType;
	m_NoiseColor  = static_cast<uint8_t>( -1 );
	m_NoiseType   = static_cast<uint8_t>( -1 );
	SetNoise( color, type );
}

cRetroTMS9919::~cRetroTMS9919( )
{
}

int cRetroTMS9919::GetPlaybackFrequency( )
{
	return m_SampleRate;
}

void cRetroTMS9919::SetMasterVolume( int volume )
{
	m_MasterVolume = std::clamp( volume, 0, 100 );
}

void cRetroTMS9919::BeginFrame( uint32_t startClocks, uint32_t totalClocks, int targetSamples )
{
	m_FrameStartClocks   = startClocks;
	m_FrameTotalClocks   = ( totalClocks > 0 ) ? totalClocks : 50000;
	m_FrameTargetSamples = std::clamp( targetSamples, 1, static_cast<int>( SIZE( m_MonoBuffer )));
	m_RenderedSamples    = 0;
}

void cRetroTMS9919::CatchUpToClocks( uint32_t currentClocks )
{
	uint32_t elapsed = currentClocks - m_FrameStartClocks;
	if( static_cast<int32_t>( elapsed ) <= 0 )
	{
		return;
	}

	int targetSample = static_cast<int>(( static_cast<uint64_t>( elapsed ) * m_FrameTargetSamples ) / m_FrameTotalClocks );
	targetSample = std::clamp( targetSample, m_RenderedSamples, m_FrameTargetSamples );

	int count = targetSample - m_RenderedSamples;
	if( count > 0 )
	{
		RenderSamples( m_MonoBuffer + m_RenderedSamples, count );
		m_RenderedSamples = targetSample;
	}
}

size_t cRetroTMS9919::EndFrame( int16_t *stereoOut )
{
	int remaining = m_FrameTargetSamples - m_RenderedSamples;
	if( remaining > 0 )
	{
		RenderSamples( m_MonoBuffer + m_RenderedSamples, remaining );
		m_RenderedSamples = m_FrameTargetSamples;
	}

	for( int i = 0; i < m_FrameTargetSamples; i++ )
	{
		int32_t sample = ( static_cast<int32_t>( m_MonoBuffer[ i ] ) * m_MasterVolume ) / 100;
		int16_t clamped = static_cast<int16_t>( std::clamp( sample, -32768, 32767 ));
		stereoOut[ i * 2 + 0 ] = clamped;
		stereoOut[ i * 2 + 1 ] = clamped;
	}

	return static_cast<size_t>( m_FrameTargetSamples );
}

void cRetroTMS9919::WriteData( UINT8 data )
{
	if( m_pCPU != nullptr )
	{
		CatchUpToClocks( m_pCPU->GetClocks( ));
	}

	cTMS9919::WriteData( data );
}

void cRetroTMS9919::RenderSamples( INT16 *dst, int samples )
{
	if( samples <= 0 )
	{
		return;
	}

	memset( dst, 0, sizeof( INT16 ) * samples );

	for( int i = 0; i < 4; i++ )
	{
		sVoiceInfo *info = &m_Info[ i ];
		if(( m_Attenuation[ i ] != 15 ) && ( info->period >= 1.0f ))
		{
			int left = samples;
			int j = 0;
			do
			{
				int count = ( info->toggle < left ) ? static_cast<int>( info->toggle ) : left;
				left -= count;
				info->toggle -= count;
				while( count-- )
				{
					dst[ j++ ] += static_cast<INT16>( info->setting );
				}
				if( info->toggle < 1.0f )
				{
					info->toggle += info->period;
					if( i < 3 )
					{
						info->setting = -info->setting;
					}
					else
					{
						if( m_ShiftRegister & 1 )
						{
							m_ShiftRegister ^= m_NoiseGenerator;
							if( m_ShiftRegister == 0 )
							{
								m_ShiftRegister = NOISE_RESET;
							}
							info->setting = -info->setting;
						}
						m_ShiftRegister >>= 1;
					}
				}
			}
			while( left > 0 );
		}
	}

	if( m_pSpeechSynthesizer != nullptr )
	{
		m_pSpeechSynthesizer->AudioCallback( dst, samples );
	}
}

void cRetroTMS9919::SetNoise( uint8_t color, uint8_t type )
{
	bool reset = ( color != m_NoiseColor );

	cTMS9919::SetNoise( color, type );

	if( reset )
	{
		m_ShiftRegister = NOISE_RESET;
	}
	m_NoiseGenerator = ( color == NOISE_WHITE ) ? NOISE_WHITE_GENERATOR : NOISE_PERIODIC_GENERATOR;

	sVoiceInfo *info = &m_Info[ 3 ];

	if( m_Frequency[ 3 ] != 0 )
	{
		int volume    = m_VolumeTable[ m_Attenuation[ 3 ] & 0x0F ];
		info->period  = static_cast<float>( m_SampleRate ) / ( CLOCK_FREQUENCY / static_cast<float>( m_Frequency[ 3 ] )) / 2.0f;
		info->setting = ( info->setting > 0 ) ? volume : -volume;
	}
	else
	{
		info->period = 0.0f;
	}
}

void cRetroTMS9919::SetFrequency( uint8_t tone, uint16_t freq )
{
	cTMS9919::SetFrequency( tone, freq );

	if( tone < 3 )
	{
		sVoiceInfo *info = &m_Info[ tone ];

		if(( freq < m_SampleRate / 2 ) && ( freq != 0 ))
		{
			int volume    = m_VolumeTable[ m_Attenuation[ tone ] & 0x0F ];
			info->period  = static_cast<float>( m_SampleRate ) / ( CLOCK_FREQUENCY / static_cast<float>( freq )) / 2.0f;
			info->setting = ( info->setting > 0 ) ? volume : -volume;
		}
		else
		{
			info->period  = 512.0f;
			info->setting = 0;
		}
	}
}

void cRetroTMS9919::SetAttenuation( uint8_t tone, uint8_t atten )
{
	cTMS9919::SetAttenuation( tone, atten );

	if( tone < 4 )
	{
		sVoiceInfo *info = &m_Info[ tone ];
		int volume    = m_VolumeTable[ m_Attenuation[ tone ] & 0x0F ];
		info->setting = ( info->setting > 0 ) ? volume : -volume;
	}
}

void cRetroTMS9919::RefreshVoices( )
{
	for( uint8_t i = 0; i < 3; i++ )
	{
		SetFrequency( i, m_Frequency[ i ] );
		SetAttenuation( i, m_Attenuation[ i ] );
	}
	SetNoise( m_NoiseColor, m_NoiseType );
	SetAttenuation( 3, m_Attenuation[ 3 ] );
}
