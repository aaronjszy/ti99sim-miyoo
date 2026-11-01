//----------------------------------------------------------------------------
//
// File:        ti994a-retro.cpp
// Date:        31-Oct-2026
// Programmer:  Aaron Szymanski
//
// Description: Libretro TI-99/4A machine coordinator and binary serialization
//
// SPDX-License-Identifier: GPL-2.0-or-later
//
//----------------------------------------------------------------------------

#include <algorithm>
#include <cstring>
#include "common.hpp"
#include "logger.hpp"
#include "memory.hpp"
#include "opcodes.hpp"
#include "tms9900.hpp"
#include "tms9901.hpp"
#include "tms5220.hpp"
#include "cartridge.hpp"
#include "ti994a-retro.hpp"

DBG_REGISTER( __FILE__ );

extern cTI994A  *CRU_Object;
extern cTMS9901 *pic;
extern void    (*TimerHook)( );

namespace
{
	// CPU banks used by the TI-99/4A 32K Memory Expansion peripheral:
	// 0x2000-0x3FFF (banks 2, 3) and 0xA000-0xFFFF (banks 10..15)
	constexpr int MASK_32K_EXPANSION = 0x0000FC0C;
	constexpr int MASK_CONSOLE_ALL   = 0x00FFFFFF;
	constexpr int MASK_CONSOLE_NO32K = MASK_CONSOLE_ALL & ~MASK_32K_EXPANSION;

	constexpr char     SAVESTATE_MAGIC[ 8 ] = { 'T', 'I', '9', '9', 'R', 'E', 'T', '1' };
	constexpr uint32_t SAVESTATE_VERSION    = 2;

	#pragma pack( push, 1 )
	struct sRetroSaveState
	{
		char        magic[ 8 ];
		uint32_t    version;

		// TMS9900 CPU state
		uint16_t    wp;
		uint16_t    pc;
		uint16_t    st;
		uint16_t    interruptFlag;
		uint32_t    instructionCounter;
		uint32_t    clockCycleCounter;

		// TI-99/4A console state
		uint16_t    gromAddress;
		int32_t     gromReadShift;
		int32_t     gromWriteShift;
		uint16_t    activeCRU;
		uint32_t    lastRetraceDelta;
		uint8_t     enable32K;
		uint8_t     scratchpad[ 256 ];
		uint8_t     videoMemory[ 0x4000 ];

		// TMS9901 PIC state
		uint8_t     picTimerActive;
		int32_t     picReadRegister;
		int32_t     picDecrementer;
		int32_t     picClockRegister;
		uint8_t     picPinState[ 32 ][ 2 ];
		int32_t     picInterruptRequested;
		int32_t     picActiveInterrupts;
		int32_t     picLastDelta;
		uint32_t    picDecrementClock;
		uint8_t     picCapsLock;
		int32_t     picColumnSelect;
		int32_t     picHideShift;

		// TMS9918A VDP state
		uint16_t    vdpAddress;
		uint16_t    vdpTransfer;
		uint16_t    vdpShift;
		uint8_t     vdpStatus;
		uint8_t     vdpRegister[ 8 ];
		uint8_t     vdpMode;
		uint8_t     vdpReadAhead;
		uint8_t     vdpMaxSprite[ 256 ];
		uint8_t     vdpSpritesDirty;
		uint8_t     vdpSpritesRefreshed;
		uint8_t     vdpCoincidenceFlag;
		uint8_t     vdpFifthSpriteFlag;
		int32_t     vdpFifthSpriteIndex;

		// TMS9919 PSG state
		uint8_t     psgLastData;
		uint16_t    psgFrequency[ 4 ];
		uint8_t     psgAttenuation[ 4 ];
		uint8_t     psgNoiseColor;
		uint8_t     psgNoiseType;
		float       psgVoicePeriod[ 4 ];
		float       psgVoiceToggle[ 4 ];
		int32_t     psgVoiceSetting[ 4 ];
		int32_t     psgShiftRegister;
		int32_t     psgNoiseGenerator;

		// TMS5220 Speech Synthesizer state
		uint8_t     speechPresent;
		uint8_t     speechReadByte;
		uint8_t     speechSpeakExternal;
		uint8_t     speechReadingEnergy;
		int32_t     speechLoadPointer;
		uint32_t    speechAddress;
		int32_t     speechBitOffset;
		int32_t     speechFifoGet;
		int32_t     speechFifoPut;
		int32_t     speechFifoBitsLeft;
		uint8_t     speechFifo[ 16 ];
		uint8_t     speechTalkStatus;
		int32_t     speechInterpolationStage;
		int32_t     speechPitchIndex;
		double      speechFilterHistory[ 2 ][ 11 ];
		double      speechRawDataBuffer[ 25 ];
		double      speechStartReflection[ 10 ];
		int32_t     speechStartEnergy;
		int32_t     speechStartPitch;
		uint8_t     speechStartRepeat;
		uint8_t     speechStartStop;
		double      speechTargetReflection[ 10 ];
		int32_t     speechTargetEnergy;
		int32_t     speechTargetPitch;
		uint8_t     speechTargetRepeat;
		uint8_t     speechTargetStop;

		// Console 32K RAM (banks 0..15)
		uint8_t     consoleCpuCurBank[ 16 ];
		uint8_t     consoleGromCurBank[ 8 ];
		uint8_t     consoleRamBanks[ 8 ][ ROM_BANK_SIZE ]; // Banks 2, 3, 10, 11, 12, 13, 14, 15

		// Cartridge state (bank indices + up to 32KB cartridge RAM + 16KB GRAM)
		uint8_t     cartCpuCurBank[ 16 ];
		uint8_t     cartGromCurBank[ 8 ];
		uint8_t     cartCpuRam[ 8 ][ ROM_BANK_SIZE ];
		uint8_t     cartGromRam[ 2 ][ GROM_BANK_SIZE ];
	};
	#pragma pack( pop )

	constexpr int ConsoleRamBankIndices[ 8 ] = { 2, 3, 10, 11, 12, 13, 14, 15 };
}

cRetroTI994A::cRetroTI994A( iCartridge *console, cRetroTMS9918A *vdp, cRetroTMS9919 *sound, iTMS5220 *speech, bool enable32K ) :
	cBaseObject( "cRetroTI994A" ),
	cTI994A( console, vdp, sound, speech ),
	m_RetroVDP( vdp ),
	m_RetroSound( sound ),
	m_Enable32K( true ),
	m_SpeechEnabled( true ),
	m_RefreshRate( vdp ? static_cast<int>( vdp->GetRefreshRate( )) : 60 ),
	m_Fallback32K( )
{
	// Disable instruction-counter TimerHook; RunFrame drives VideoRetrace deterministically
	TimerHook = nullptr;

	if( m_RetroSound != nullptr )
	{
		m_RetroSound->SetCPU( m_CPU );
	}

	EnsureConsole32KBanks( );

	if( !enable32K )
	{
		Set32KExpansion( false );
	}

	if( m_PIC != nullptr )
	{
		m_PIC->SetAlphaLockFix( true );
		// Default Alpha Lock ON so TI BASIC and uppercase game prompts work out-of-the-box
		m_PIC->VKeyDown( 0, VK_CAPSLOCK );
	}
}

cRetroTI994A::~cRetroTI994A( )
{
	if( m_CPU != nullptr )
	{
		for( UINT8 i = 1; i < 16; i++ )
		{
			m_CPU->DeRegisterTrapHandler( i );
		}
	}

	for( auto &ptr : m_Fallback32K )
	{
		delete [] ptr;
		ptr = nullptr;
	}

	TimerHook          = nullptr;
	CRU_Object         = nullptr;
	pic                = nullptr;
	WorkspacePtr       = 0;
	ProgramCounter     = 0;
	Status             = 0;
	InterruptFlag      = 0;
	InstructionCounter = 0;
	ClockCycleCounter  = 0;

	cpuMemory.SetMemory( 0x0000, 0x10000, nullptr, true );
	gplMemory.SetMemory( 0x0000, 0x10000, nullptr, true );
}

void cRetroTI994A::EnsureConsole32KBanks( )
{
	if( m_Console == nullptr )
	{
		return;
	}

	bool addedAny = false;
	for( int bankIdx : ConsoleRamBankIndices )
	{
		sMemoryRegion *region = m_Console->GetCpuMemory( bankIdx );
		if(( region != nullptr ) && ( region->NumBanks == 0 ))
		{
			m_Fallback32K[ bankIdx ] = new UINT8[ ROM_BANK_SIZE ];
			memset( m_Fallback32K[ bankIdx ], 0, ROM_BANK_SIZE );
			region->NumBanks        = 1;
			region->Bank[ 0 ].Type  = BANK_RAM;
			region->Bank[ 0 ].Flags = 0;
			region->Bank[ 0 ].Data  = m_Fallback32K[ bankIdx ];
			region->CurBank         = &region->Bank[ 0 ];
			addedAny                = true;
		}
	}

	if( addedAny )
	{
		RemoveCartridge( m_Console, MASK_CONSOLE_ALL );
		AddCartridge( m_Console, m_Enable32K ? MASK_CONSOLE_ALL : MASK_CONSOLE_NO32K );
	}
}

void cRetroTI994A::Set32KExpansion( bool enable )
{
	if( m_Enable32K == enable )
	{
		return;
	}

	if( m_Console != nullptr )
	{
		RemoveCartridge( m_Console, m_Enable32K ? MASK_CONSOLE_ALL : MASK_CONSOLE_NO32K );
		m_Enable32K = enable;
		AddCartridge( m_Console, m_Enable32K ? MASK_CONSOLE_ALL : MASK_CONSOLE_NO32K );

		// Re-apply cartridge on top if one is inserted
		if( m_Cartridge != nullptr )
		{
			RemoveCartridge( m_Cartridge, INFO_MASK_CARTRIDGE );
			AddCartridge( m_Cartridge, INFO_MASK_CARTRIDGE );
		}
	}
	else
	{
		m_Enable32K = enable;
	}
}

void cRetroTI994A::SetSpeechEnabled( bool enable )
{
	m_SpeechEnabled = enable;
	if( m_RetroSound != nullptr )
	{
		m_RetroSound->SetSpeechSynthesizer( enable ? m_SpeechSynthesizer.get( ) : nullptr );
	}
}

void cRetroTI994A::SetAlphaLockFix( bool enable )
{
	if( m_PIC != nullptr )
	{
		m_PIC->SetAlphaLockFix( enable );
	}
}

void cRetroTI994A::SetRefreshRate( int hz )
{
	if(( hz != 50 ) && ( hz != 60 ))
	{
		hz = 60;
	}

	m_RefreshRate = hz;
	if( m_RetroVDP != nullptr )
	{
		m_RetroVDP->SetRefreshRate( hz );
	}
	m_RetraceInterval = m_ClockSpeed / static_cast<UINT32>( m_RefreshRate );
}

void cRetroTI994A::TimerHookProc( UINT32 )
{
	// No-op: VideoRetrace is invoked explicitly at the scanline/VBlank boundary in RunFrame
}

UINT8 cRetroTI994A::SpeechWriteBreakPoint( ADDRESS address, UINT8 data )
{
	if( !m_SpeechEnabled )
	{
		return data;
	}
	return cTI994A::SpeechWriteBreakPoint( address, data );
}

UINT8 cRetroTI994A::SpeechReadBreakPoint( ADDRESS address, UINT8 data )
{
	if( !m_SpeechEnabled )
	{
		return 0;
	}
	return cTI994A::SpeechReadBreakPoint( address, data );
}

void cRetroTI994A::Reset( )
{
	cTI994A::Reset( );
	m_LastRetrace = m_CPU ? m_CPU->GetClocks( ) : 0;
}

bool cRetroTI994A::Sleep( int cycles, UINT32 )
{
	m_CPU->AddClocks( cycles );
	if( m_RetroSound != nullptr )
	{
		m_RetroSound->CatchUpToClocks( m_CPU->GetClocks( ));
	}
	return false;
}

bool cRetroTI994A::WakeCPU( UINT32 )
{
	// When TMS5220 Speak-External FIFO is low on bits during AudioCallback,
	// step the CPU briefly so active speech loops can feed the next byte.
	if( m_CPU != nullptr )
	{
		UINT32 target = m_CPU->GetClocks( ) + 250;
		while( static_cast<int32_t>( m_CPU->GetClocks( ) - target ) < 0 )
		{
			m_CPU->Step( );
		}
	}
	return false;
}

void cRetroTI994A::RunFrame( int16_t *audioOut, size_t *audioFramesOut )
{
	UINT32 startClocks     = m_CPU->GetClocks( );
	UINT32 frameClocks     = m_RetraceInterval ? m_RetraceInterval : 50000;
	int    targetSamples   = ( m_RefreshRate == 50 ) ? 882 : 735;
	UINT32 activeScanClocks = ( frameClocks * 192 ) / 262; // ~36,641 clocks for 192 active lines

	if( m_RetroSound != nullptr )
	{
		m_RetroSound->BeginFrame( startClocks, frameClocks, targetSamples );
	}

	// Execute active display in 4 slices so TMS5220 FIFO and PSG updates stay tightly synced
	for( int slice = 1; slice <= 4; slice++ )
	{
		UINT32 sliceTarget = startClocks + ( activeScanClocks * slice ) / 4;
		while( static_cast<int32_t>( m_CPU->GetClocks( ) - sliceTarget ) < 0 )
		{
			m_CPU->Step( );
		}
		if( m_RetroSound != nullptr )
		{
			m_RetroSound->CatchUpToClocks( m_CPU->GetClocks( ));
		}
	}

	// Trigger VDP vertical retrace (renders scanlines/sprites & raises VDP INT on TMS9901)
	m_LastRetrace = m_CPU->GetClocks( );
	VideoRetrace( );

	// Execute VBlank interval (TI console ISR scans keyboard/joystick CRU & updates sound lists)
	UINT32 frameEndTarget = startClocks + frameClocks;
	while( static_cast<int32_t>( m_CPU->GetClocks( ) - frameEndTarget ) < 0 )
	{
		m_CPU->Step( );
	}

	if(( m_RetroSound != nullptr ) && ( audioOut != nullptr ))
	{
		size_t rendered = m_RetroSound->EndFrame( audioOut );
		if( audioFramesOut != nullptr )
		{
			*audioFramesOut = rendered;
		}
	}
	else if( audioFramesOut != nullptr )
	{
		*audioFramesOut = 0;
	}
}

void *cRetroTI994A::GetSaveRAMPtr( ) const
{
	if( m_Cartridge != nullptr )
	{
		for( size_t i = 0; i < NUM_ROM_BANKS; i++ )
		{
			sMemoryRegion *region = m_Cartridge->GetCpuMemory( i );
			for( int j = 0; j < region->NumBanks; j++ )
			{
				if(( region->Bank[ j ].Flags & FLAG_BATTERY_BACKED ) && ( region->Bank[ j ].Data != nullptr ))
				{
					return region->Bank[ j ].Data;
				}
			}
		}
	}
	return nullptr;
}

size_t cRetroTI994A::GetSaveRAMSize( ) const
{
	if( GetSaveRAMPtr( ) != nullptr )
	{
		return ROM_BANK_SIZE;
	}
	return 0;
}

size_t cRetroTI994A::SerializeSize( ) const
{
	return sizeof( sRetroSaveState );
}

bool cRetroTI994A::Serialize( void *data, size_t size ) const
{
	if(( data == nullptr ) || ( size < sizeof( sRetroSaveState )))
	{
		return false;
	}

	auto *st = static_cast<sRetroSaveState *>( data );
	memset( st, 0, sizeof( sRetroSaveState ));

	memcpy( st->magic, SAVESTATE_MAGIC, sizeof( SAVESTATE_MAGIC ));
	st->version = SAVESTATE_VERSION;

	// CPU
	st->wp                 = WorkspacePtr;
	st->pc                 = ProgramCounter;
	st->st                 = Status;
	st->interruptFlag      = InterruptFlag;
	st->instructionCounter = InstructionCounter;
	st->clockCycleCounter  = ClockCycleCounter;

	// Console
	st->gromAddress        = m_GromAddress;
	st->gromReadShift      = m_GromReadShift;
	st->gromWriteShift     = m_GromWriteShift;
	st->activeCRU          = m_ActiveCRU;
	st->lastRetraceDelta   = ClockCycleCounter - m_LastRetrace;
	st->enable32K          = m_Enable32K ? 1 : 0;
	memcpy( st->scratchpad, m_Scratchpad, sizeof( m_Scratchpad ));
	memcpy( st->videoMemory, m_VideoMemory, 0x4000 );

	// TMS9901 PIC
	if( auto *p9901 = dynamic_cast<cTMS9901 *>( static_cast<iTMS9901 *>( m_PIC )))
	{
		st->picTimerActive        = p9901->m_TimerActive ? 1 : 0;
		st->picReadRegister       = p9901->m_ReadRegister;
		st->picDecrementer        = p9901->m_Decrementer;
		st->picClockRegister      = p9901->m_ClockRegister;
		memcpy( st->picPinState, p9901->m_PinState, sizeof( st->picPinState ));
		st->picInterruptRequested = p9901->m_InterruptRequested;
		st->picActiveInterrupts   = p9901->m_ActiveInterrupts;
		st->picLastDelta          = p9901->m_LastDelta;
		st->picDecrementClock     = p9901->m_DecrementClock;
		st->picCapsLock           = p9901->m_CapsLock ? 1 : 0;
		st->picColumnSelect       = p9901->m_ColumnSelect;
		st->picHideShift          = p9901->m_HideShift;
	}

	// TMS9918A VDP
	if( m_RetroVDP != nullptr )
	{
		st->vdpAddress          = m_RetroVDP->m_Address;
		st->vdpTransfer         = m_RetroVDP->m_Transfer;
		st->vdpShift            = m_RetroVDP->m_Shift;
		st->vdpStatus           = m_RetroVDP->m_Status;
		memcpy( st->vdpRegister, m_RetroVDP->m_Register, sizeof( st->vdpRegister ));
		st->vdpMode             = m_RetroVDP->m_Mode;
		st->vdpReadAhead        = m_RetroVDP->m_ReadAhead;
		memcpy( st->vdpMaxSprite, m_RetroVDP->m_MaxSprite, sizeof( st->vdpMaxSprite ));
		st->vdpSpritesDirty     = m_RetroVDP->m_SpritesDirty ? 1 : 0;
		st->vdpSpritesRefreshed = m_RetroVDP->m_SpritesRefreshed ? 1 : 0;
		st->vdpCoincidenceFlag  = m_RetroVDP->m_CoincidenceFlag ? 1 : 0;
		st->vdpFifthSpriteFlag  = m_RetroVDP->m_FifthSpriteFlag ? 1 : 0;
		st->vdpFifthSpriteIndex = m_RetroVDP->m_FifthSpriteIndex;
	}

	// TMS9919 PSG
	if( m_RetroSound != nullptr )
	{
		st->psgLastData   = m_RetroSound->m_LastData;
		memcpy( st->psgFrequency, m_RetroSound->m_Frequency, sizeof( st->psgFrequency ));
		memcpy( st->psgAttenuation, m_RetroSound->m_Attenuation, sizeof( st->psgAttenuation ));
		st->psgNoiseColor = m_RetroSound->m_NoiseColor;
		st->psgNoiseType  = m_RetroSound->m_NoiseType;
		for( int i = 0; i < 4; i++ )
		{
			st->psgVoicePeriod[ i ]  = m_RetroSound->m_Info[ i ].period;
			st->psgVoiceToggle[ i ]  = m_RetroSound->m_Info[ i ].toggle;
			st->psgVoiceSetting[ i ] = m_RetroSound->m_Info[ i ].setting;
		}
		st->psgShiftRegister  = m_RetroSound->m_ShiftRegister;
		st->psgNoiseGenerator = m_RetroSound->m_NoiseGenerator;
	}

	// TMS5220 Speech
	if( auto *p5220 = dynamic_cast<cTMS5220 *>( static_cast<iTMS5220 *>( m_SpeechSynthesizer )))
	{
		st->speechPresent            = 1;
		st->speechReadByte           = p5220->m_State.ReadByte ? 1 : 0;
		st->speechSpeakExternal      = p5220->m_State.SpeakExternal ? 1 : 0;
		st->speechReadingEnergy      = p5220->m_State.ReadingEnergy ? 1 : 0;
		st->speechLoadPointer        = p5220->m_State.vsm.LoadPointer;
		st->speechAddress            = p5220->m_State.vsm.Address;
		st->speechBitOffset          = p5220->m_State.vsm.BitOffset;
		st->speechFifoGet            = p5220->m_State.fifo.GetIndex;
		st->speechFifoPut            = p5220->m_State.fifo.PutIndex;
		st->speechFifoBitsLeft       = p5220->m_State.fifo.BitsLeft;
		memcpy( st->speechFifo, p5220->m_FIFO, sizeof( st->speechFifo ));
		st->speechTalkStatus         = p5220->m_TalkStatus ? 1 : 0;
		st->speechInterpolationStage = p5220->m_InterpolationStage;
		st->speechPitchIndex         = p5220->m_PitchIndex;
		memcpy( st->speechFilterHistory, p5220->m_FilterHistory, sizeof( st->speechFilterHistory ));
		memcpy( st->speechRawDataBuffer, p5220->m_RawDataBuffer, sizeof( st->speechRawDataBuffer ));
		memcpy( st->speechStartReflection, p5220->m_StartParams.Reflection, sizeof( st->speechStartReflection ));
		st->speechStartEnergy        = p5220->m_StartParams.Energy;
		st->speechStartPitch         = p5220->m_StartParams.Pitch;
		st->speechStartRepeat        = p5220->m_StartParams.Repeat ? 1 : 0;
		st->speechStartStop          = p5220->m_StartParams.Stop ? 1 : 0;
		memcpy( st->speechTargetReflection, p5220->m_TargetParams.Reflection, sizeof( st->speechTargetReflection ));
		st->speechTargetEnergy       = p5220->m_TargetParams.Energy;
		st->speechTargetPitch        = p5220->m_TargetParams.Pitch;
		st->speechTargetRepeat       = p5220->m_TargetParams.Repeat ? 1 : 0;
		st->speechTargetStop         = p5220->m_TargetParams.Stop ? 1 : 0;
	}

	// Console 32K RAM
	if( m_Console != nullptr )
	{
		for( size_t i = 0; i < 16; i++ )
		{
			sMemoryRegion *r = m_Console->GetCpuMemory( i );
			st->consoleCpuCurBank[ i ] = ( r && r->CurBank ) ? static_cast<uint8_t>( r->CurBank - r->Bank ) : 0;
		}
		for( size_t i = 0; i < 8; i++ )
		{
			sMemoryRegion *r = m_Console->GetGromMemory( i );
			st->consoleGromCurBank[ i ] = ( r && r->CurBank ) ? static_cast<uint8_t>( r->CurBank - r->Bank ) : 0;
		}
		for( size_t idx = 0; idx < SIZE( ConsoleRamBankIndices ); idx++ )
		{
			int bankIdx = ConsoleRamBankIndices[ idx ];
			sMemoryRegion *r = m_Console->GetCpuMemory( bankIdx );
			if(( r != nullptr ) && ( r->NumBanks > 0 ) && ( r->Bank[ 0 ].Data != nullptr ))
			{
				memcpy( st->consoleRamBanks[ idx ], r->Bank[ 0 ].Data, ROM_BANK_SIZE );
			}
		}
	}

	// Cartridge banks & RAM
	if( m_Cartridge != nullptr )
	{
		size_t cpuRamSlot = 0;
		for( size_t i = 0; i < 16; i++ )
		{
			sMemoryRegion *r = m_Cartridge->GetCpuMemory( i );
			st->cartCpuCurBank[ i ] = ( r && r->CurBank ) ? static_cast<uint8_t>( r->CurBank - r->Bank ) : 0;
			if( r != nullptr )
			{
				for( int b = 0; b < r->NumBanks; b++ )
				{
					if(( r->Bank[ b ].Type == BANK_RAM ) && ( r->Bank[ b ].Data != nullptr ) && ( cpuRamSlot < 8 ))
					{
						memcpy( st->cartCpuRam[ cpuRamSlot++ ], r->Bank[ b ].Data, ROM_BANK_SIZE );
					}
				}
			}
		}

		size_t gromRamSlot = 0;
		for( size_t i = 0; i < 8; i++ )
		{
			sMemoryRegion *r = m_Cartridge->GetGromMemory( i );
			st->cartGromCurBank[ i ] = ( r && r->CurBank ) ? static_cast<uint8_t>( r->CurBank - r->Bank ) : 0;
			if( r != nullptr )
			{
				for( int b = 0; b < r->NumBanks; b++ )
				{
					if(( r->Bank[ b ].Type == BANK_RAM ) && ( r->Bank[ b ].Data != nullptr ) && ( gromRamSlot < 2 ))
					{
						memcpy( st->cartGromRam[ gromRamSlot++ ], r->Bank[ b ].Data, GROM_BANK_SIZE );
					}
				}
			}
		}
	}

	return true;
}

bool cRetroTI994A::Unserialize( const void *data, size_t size )
{
	if(( data == nullptr ) || ( size < sizeof( sRetroSaveState )))
	{
		return false;
	}

	const auto *st = static_cast<const sRetroSaveState *>( data );
	if(( memcmp( st->magic, SAVESTATE_MAGIC, sizeof( SAVESTATE_MAGIC )) != 0 ) ||
	   ( st->version != SAVESTATE_VERSION ))
	{
		return false;
	}

	// CPU
	WorkspacePtr       = st->wp;
	ProgramCounter     = st->pc;
	Status             = st->st;
	InterruptFlag      = st->interruptFlag;
	InstructionCounter = st->instructionCounter;
	ClockCycleCounter  = st->clockCycleCounter;

	// Console
	m_GromAddress      = st->gromAddress;
	m_GromReadShift    = st->gromReadShift;
	m_GromWriteShift   = st->gromWriteShift;
	m_ActiveCRU        = st->activeCRU;
	m_LastRetrace      = ClockCycleCounter - st->lastRetraceDelta;

	Set32KExpansion( st->enable32K != 0 );

	memcpy( m_Scratchpad, st->scratchpad, sizeof( m_Scratchpad ));
	memcpy( m_VideoMemory, st->videoMemory, 0x4000 );

	cpuMemory.SetMemory( 0x8000, 0x0100, m_Scratchpad, false );
	cpuMemory.SetMemory( 0x8100, 0x0100, m_Scratchpad, false );
	cpuMemory.SetMemory( 0x8200, 0x0100, m_Scratchpad, false );
	cpuMemory.SetMemory( 0x8300, 0x0100, m_Scratchpad, false );

	// TMS9901 PIC
	if( auto *p9901 = dynamic_cast<cTMS9901 *>( static_cast<iTMS9901 *>( m_PIC )))
	{
		p9901->m_TimerActive        = ( st->picTimerActive != 0 );
		p9901->m_ReadRegister       = st->picReadRegister;
		p9901->m_Decrementer        = st->picDecrementer;
		p9901->m_ClockRegister      = st->picClockRegister;
		memcpy( p9901->m_PinState, st->picPinState, sizeof( p9901->m_PinState ));
		p9901->m_InterruptRequested = st->picInterruptRequested;
		p9901->m_ActiveInterrupts   = st->picActiveInterrupts;
		p9901->m_LastDelta          = st->picLastDelta;
		p9901->m_DecrementClock     = st->picDecrementClock;
		p9901->m_CapsLock           = ( st->picCapsLock != 0 );
		p9901->m_ColumnSelect       = st->picColumnSelect;
		p9901->m_HideShift          = st->picHideShift;
	}

	// TMS9918A VDP
	if( m_RetroVDP != nullptr )
	{
		m_RetroVDP->m_Mode = 0xFF;
		for( size_t i = 0; i < 8; i++ )
		{
			m_RetroVDP->WriteRegister( i, st->vdpRegister[ i ] );
		}
		m_RetroVDP->m_Address          = st->vdpAddress;
		m_RetroVDP->m_Transfer         = st->vdpTransfer;
		m_RetroVDP->m_Shift            = st->vdpShift;
		m_RetroVDP->m_Status           = st->vdpStatus;
		m_RetroVDP->m_Mode             = st->vdpMode;
		m_RetroVDP->m_ReadAhead        = st->vdpReadAhead;
		memcpy( m_RetroVDP->m_MaxSprite, st->vdpMaxSprite, sizeof( m_RetroVDP->m_MaxSprite ));
		m_RetroVDP->m_SpritesDirty     = ( st->vdpSpritesDirty != 0 );
		m_RetroVDP->m_SpritesRefreshed = ( st->vdpSpritesRefreshed != 0 );
		m_RetroVDP->m_CoincidenceFlag  = ( st->vdpCoincidenceFlag != 0 );
		m_RetroVDP->m_FifthSpriteFlag  = ( st->vdpFifthSpriteFlag != 0 );
		m_RetroVDP->m_FifthSpriteIndex = st->vdpFifthSpriteIndex;
		m_RetroVDP->RebuildPalette( );
		m_RetroVDP->MarkAllDirty( );
	}

	// TMS9919 PSG
	if( m_RetroSound != nullptr )
	{
		m_RetroSound->m_LastData   = st->psgLastData;
		memcpy( m_RetroSound->m_Frequency, st->psgFrequency, sizeof( m_RetroSound->m_Frequency ));
		memcpy( m_RetroSound->m_Attenuation, st->psgAttenuation, sizeof( m_RetroSound->m_Attenuation ));
		m_RetroSound->m_NoiseColor = st->psgNoiseColor;
		m_RetroSound->m_NoiseType  = st->psgNoiseType;
		m_RetroSound->RefreshVoices( );
		for( int i = 0; i < 4; i++ )
		{
			m_RetroSound->m_Info[ i ].period  = st->psgVoicePeriod[ i ];
			m_RetroSound->m_Info[ i ].toggle  = st->psgVoiceToggle[ i ];
			m_RetroSound->m_Info[ i ].setting = st->psgVoiceSetting[ i ];
		}
		m_RetroSound->m_ShiftRegister  = st->psgShiftRegister;
		m_RetroSound->m_NoiseGenerator = st->psgNoiseGenerator;
	}

	// TMS5220 Speech
	if(( st->speechPresent != 0 ) && ( m_SpeechSynthesizer != nullptr ))
	{
		if( auto *p5220 = dynamic_cast<cTMS5220 *>( m_SpeechSynthesizer.get( )))
		{
			p5220->Reset( );
			p5220->m_State.ReadByte          = ( st->speechReadByte != 0 );
			p5220->m_State.SpeakExternal     = ( st->speechSpeakExternal != 0 );
			p5220->m_State.ReadingEnergy     = ( st->speechReadingEnergy != 0 );
			p5220->m_State.vsm.LoadPointer   = st->speechLoadPointer;
			p5220->m_State.vsm.Address       = st->speechAddress;
			p5220->m_State.vsm.BitOffset     = st->speechBitOffset;
			p5220->m_State.fifo.GetIndex     = st->speechFifoGet;
			p5220->m_State.fifo.PutIndex     = st->speechFifoPut;
			p5220->m_State.fifo.BitsLeft     = st->speechFifoBitsLeft;
			memcpy( p5220->m_FIFO, st->speechFifo, sizeof( p5220->m_FIFO ));
			p5220->m_TalkStatus              = ( st->speechTalkStatus != 0 );
			p5220->m_InterpolationStage      = st->speechInterpolationStage;
			p5220->m_PitchIndex              = st->speechPitchIndex;
			memcpy( p5220->m_FilterHistory, st->speechFilterHistory, sizeof( p5220->m_FilterHistory ));
			memcpy( p5220->m_RawDataBuffer, st->speechRawDataBuffer, sizeof( p5220->m_RawDataBuffer ));
			memcpy( p5220->m_StartParams.Reflection, st->speechStartReflection, sizeof( p5220->m_StartParams.Reflection ));
			p5220->m_StartParams.Energy      = st->speechStartEnergy;
			p5220->m_StartParams.Pitch       = st->speechStartPitch;
			p5220->m_StartParams.Repeat      = ( st->speechStartRepeat != 0 );
			p5220->m_StartParams.Stop        = ( st->speechStartStop != 0 );
			memcpy( p5220->m_TargetParams.Reflection, st->speechTargetReflection, sizeof( p5220->m_TargetParams.Reflection ));
			p5220->m_TargetParams.Energy     = st->speechTargetEnergy;
			p5220->m_TargetParams.Pitch      = st->speechTargetPitch;
			p5220->m_TargetParams.Repeat     = ( st->speechTargetRepeat != 0 );
			p5220->m_TargetParams.Stop       = ( st->speechTargetStop != 0 );
		}
	}

	// Console 32K RAM
	if( m_Console != nullptr )
	{
		for( size_t i = 0; i < 16; i++ )
		{
			sMemoryRegion *r = m_Console->GetCpuMemory( i );
			if(( r != nullptr ) && ( r->NumBanks > 0 ))
			{
				int bank = std::clamp<int>( st->consoleCpuCurBank[ i ], 0, r->NumBanks - 1 );
				r->CurBank = &r->Bank[ bank ];
			}
		}
		for( size_t i = 0; i < 8; i++ )
		{
			sMemoryRegion *r = m_Console->GetGromMemory( i );
			if(( r != nullptr ) && ( r->NumBanks > 0 ))
			{
				int bank = std::clamp<int>( st->consoleGromCurBank[ i ], 0, r->NumBanks - 1 );
				r->CurBank = &r->Bank[ bank ];
			}
		}
		for( size_t idx = 0; idx < SIZE( ConsoleRamBankIndices ); idx++ )
		{
			int bankIdx = ConsoleRamBankIndices[ idx ];
			sMemoryRegion *r = m_Console->GetCpuMemory( bankIdx );
			if(( r != nullptr ) && ( r->NumBanks > 0 ) && ( r->Bank[ 0 ].Data != nullptr ))
			{
				memcpy( r->Bank[ 0 ].Data, st->consoleRamBanks[ idx ], ROM_BANK_SIZE );
			}
		}
	}

	// Cartridge banks & RAM
	if( m_Cartridge != nullptr )
	{
		size_t cpuRamSlot = 0;
		for( size_t i = 0; i < 16; i++ )
		{
			sMemoryRegion *r = m_Cartridge->GetCpuMemory( i );
			if(( r != nullptr ) && ( r->NumBanks > 0 ))
			{
				int bank = std::clamp<int>( st->cartCpuCurBank[ i ], 0, r->NumBanks - 1 );
				r->CurBank = &r->Bank[ bank ];
				for( int b = 0; b < r->NumBanks; b++ )
				{
					if(( r->Bank[ b ].Type == BANK_RAM ) && ( r->Bank[ b ].Data != nullptr ) && ( cpuRamSlot < 8 ))
					{
						memcpy( r->Bank[ b ].Data, st->cartCpuRam[ cpuRamSlot++ ], ROM_BANK_SIZE );
					}
				}
			}
		}

		size_t gromRamSlot = 0;
		for( size_t i = 0; i < 8; i++ )
		{
			sMemoryRegion *r = m_Cartridge->GetGromMemory( i );
			if(( r != nullptr ) && ( r->NumBanks > 0 ))
			{
				int bank = std::clamp<int>( st->cartGromCurBank[ i ], 0, r->NumBanks - 1 );
				r->CurBank = &r->Bank[ bank ];
				for( int b = 0; b < r->NumBanks; b++ )
				{
					if(( r->Bank[ b ].Type == BANK_RAM ) && ( r->Bank[ b ].Data != nullptr ) && ( gromRamSlot < 2 ))
					{
						memcpy( r->Bank[ b ].Data, st->cartGromRam[ gromRamSlot++ ], GROM_BANK_SIZE );
					}
				}
			}
		}
	}

	UpdateMemory( MASK_CONSOLE_ALL );

	cpuMemory.SetMemory( 0x8000, 0x0100, m_Scratchpad, false );
	cpuMemory.SetMemory( 0x8100, 0x0100, m_Scratchpad, false );
	cpuMemory.SetMemory( 0x8200, 0x0100, m_Scratchpad, false );
	cpuMemory.SetMemory( 0x8300, 0x0100, m_Scratchpad, false );
	cpuMemory.SetMemory( 0x9000, ROM_BANK_SIZE, nullptr, true );

	return true;
}
