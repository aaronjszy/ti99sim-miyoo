//----------------------------------------------------------------------------
//
// File:        libretro-core-options.hpp
// Date:        31-Oct-2026
// Programmer:  Aaron Szymanski
//
// Description: Libretro Core Options v2 definitions with v1/v0 fallback
//
// SPDX-License-Identifier: GPL-2.0-or-later
//
//----------------------------------------------------------------------------

#ifndef LIBRETRO_CORE_OPTIONS_HPP_
#define LIBRETRO_CORE_OPTIONS_HPP_

#include <cstring>
#include <string>
#include "libretro.h"

#define TI99SIM_BUTTON_OPTION_VALUES \
	{ "JOY_FIRE",     "Joystick Fire" }, \
	{ "OSK",          "Toggle On-Screen Keyboard" }, \
	{ "ENTER",        "ENTER" }, \
	{ "SPACE",        "SPACE" }, \
	{ "1",            "Key 1" }, \
	{ "2",            "Key 2" }, \
	{ "3",            "Key 3" }, \
	{ "4",            "Key 4" }, \
	{ "5",            "Key 5" }, \
	{ "6",            "Key 6" }, \
	{ "7",            "Key 7" }, \
	{ "8",            "Key 8" }, \
	{ "9",            "Key 9" }, \
	{ "0",            "Key 0" }, \
	{ "Y",            "Key Y" }, \
	{ "N",            "Key N" }, \
	{ "FCTN_PROCEED", "FCTN+6 (Proceed)" }, \
	{ "FCTN_REDO",    "FCTN+8 (Redo)" }, \
	{ "FCTN_BACK",    "FCTN+9 (Back)" }, \
	{ "FCTN_AID",     "FCTN+7 (Aid)" }, \
	{ "FCTN_CLEAR",   "FCTN+4 (Clear)" }, \
	{ "FCTN_DEL",     "FCTN+1 (Delete)" }, \
	{ "FCTN_INS",     "FCTN+2 (Insert)" }, \
	{ "FCTN_ERASE",   "FCTN+3 (Erase)" }, \
	{ "FCTN_BEGIN",   "FCTN+5 (Begin)" }, \
	{ "FCTN_QUIT",    "FCTN+= (Quit / Reset)" }, \
	{ "ALPHA_LOCK",   "Toggle Alpha Lock" }, \
	{ "A", "Key A" }, { "B", "Key B" }, { "C", "Key C" }, { "D", "Key D" }, \
	{ "E", "Key E" }, { "F", "Key F" }, { "G", "Key G" }, { "H", "Key H" }, \
	{ "I", "Key I" }, { "J", "Key J" }, { "K", "Key K" }, { "L", "Key L" }, \
	{ "M", "Key M" }, { "O", "Key O" }, { "P", "Key P" }, { "Q", "Key Q" }, \
	{ "R", "Key R" }, { "S", "Key S" }, { "T", "Key T" }, { "U", "Key U" }, \
	{ "V", "Key V" }, { "W", "Key W" }, { "X", "Key X" }, { "Z", "Key Z" }, \
	{ "EQUALS",       "Key =" }, \
	{ "PERIOD",       "Key ." }, \
	{ "COMMA",        "Key ," }, \
	{ "SEMICOLON",    "Key ;" }, \
	{ "DIVIDE",       "Key /" }, \
	{ "SHIFT",        "Modifier SHIFT" }, \
	{ "FCTN",         "Modifier FCTN" }, \
	{ "CTRL",         "Modifier CTRL" }, \
	{ "NONE",         "None" }, \
	{ nullptr, nullptr }

static struct retro_core_option_v2_category option_cats_us[] =
{
	{
		"system",
		"System",
		"Configure TI-99/4A hardware region, 32K RAM expansion, TMS5220 speech synthesizer, and Alpha Lock behavior."
	},
	{
		"video",
		"Video",
		"Configure TMS9918A color palette and overscan border display."
	},
	{
		"audio",
		"Audio",
		"Configure TMS9919 PSG sound effects and TMS5220 speech synthesizer volume levels."
	},
	{
		"input",
		"Input",
		"Configure RetroPad button-to-keyboard bindings and D-pad cursor keys."
	},
	{ nullptr, nullptr, nullptr }
};

static struct retro_core_option_v2_definition option_defs_us[] =
{
	{
		"ti99sim_region",
		"System Region (Video Refresh Rate)",
		"Region (Refresh Rate)",
		"Select NTSC (60 Hz) or PAL (50 Hz) VDP refresh timing.",
		nullptr,
		"system",
		{
			{ "ntsc", "NTSC (60 Hz)" },
			{ "pal",  "PAL (50 Hz)" },
			{ nullptr, nullptr }
		},
		"ntsc"
	},
	{
		"ti99sim_cpu_speed",
		"System > CPU Execution Speed",
		"CPU Execution Speed",
		"Adjust virtual CPU execution rate per frame. Lower values (e.g. 75%) simulate authentic hardware execution bottlenecks for games like Parsec where enemy dives are tied to CPU loops. Higher values reduce slowdown.",
		nullptr,
		"system",
		{
			{ "60",  "60%" },
			{ "65",  "65%" },
			{ "70",  "70%" },
			{ "75",  "75% (Authentic Parsec / Stock)" },
			{ "80",  "80%" },
			{ "85",  "85%" },
			{ "90",  "90%" },
			{ "95",  "95%" },
			{ "100", "100% (Default)" },
			{ "110", "110%" },
			{ "120", "120%" },
			{ "130", "130%" },
			{ "150", "150% (Fast)" },
			{ nullptr, nullptr }
		},
		"100"
	},
	{
		"ti99sim_ram_32k",
		"System > 32K Memory Expansion",
		"32K Memory Expansion",
		"Enable the TI 32K RAM expansion peripheral at 0x2000-0x3FFF and 0xA000-0xFFFF.",
		nullptr,
		"system",
		{
			{ "enabled",  "Enabled" },
			{ "disabled", "Disabled" },
			{ nullptr, nullptr }
		},
		"enabled"
	},
	{
		"ti99sim_speech",
		"System > TMS5220 Speech Synthesizer",
		"TMS5220 Speech Synthesizer",
		"Enable the TI Solid State Speech Synthesizer (requires spchrom.bin in BIOS/ti99sim for phrase ROM games).",
		nullptr,
		"system",
		{
			{ "enabled",  "Enabled" },
			{ "disabled", "Disabled" },
			{ nullptr, nullptr }
		},
		"enabled"
	},
	{
		"ti99sim_alphalock_fix",
		"System > Alpha Lock + Joystick Up Diode Fix",
		"Alpha Lock + Joystick Up Fix",
		"Prevent active Alpha Lock from blocking Joystick Up (column 6/7) on the TMS9901 CRU matrix.",
		nullptr,
		"system",
		{
			{ "enabled",  "Enabled" },
			{ "disabled", "Disabled (Authentic Hardware Bug)" },
			{ nullptr, nullptr }
		},
		"enabled"
	},
	{
		"ti99sim_palette",
		"Video > Color Palette",
		"Color Palette",
		"Select the TMS9918A color palette.",
		nullptr,
		"video",
		{
			{ "default", "Default (TI-99/Sim)" },
			{ "ntsc",    "NTSC" },
			{ "rgb",     "RGB" },
			{ "mono",    "Monochrome" },
			{ nullptr, nullptr }
		},
		"default"
	},
	{
		"ti99sim_borders",
		"Video > Overscan Borders",
		"Overscan Borders",
		"Show the TMS9918A backdrop border around the 256x192 active display area (outputs 288x216).",
		nullptr,
		"video",
		{
			{ "disabled", "Disabled (256x192)" },
			{ "enabled",  "Enabled (288x216)" },
			{ nullptr, nullptr }
		},
		"disabled"
	},
	{
		"ti99sim_osk_position",
		"Video > On-Screen Keyboard Position",
		"OSK Position",
		"Default docking position for the built-in on-screen keyboard.",
		nullptr,
		"video",
		{
			{ "bottom", "Bottom" },
			{ "top",    "Top" },
			{ nullptr, nullptr }
		},
		"bottom"
	},
	{
		"ti99sim_volume_psg",
		"Audio > PSG Sound Effects Volume",
		"PSG Volume",
		"Set the volume level for TMS9919 PSG sound generator voices.",
		nullptr,
		"audio",
		{
			{ "100", "100% (Default)" },
			{ "90",  "90%" },
			{ "80",  "80%" },
			{ "70",  "70%" },
			{ "60",  "60%" },
			{ "50",  "50%" },
			{ "40",  "40%" },
			{ "30",  "30%" },
			{ "20",  "20%" },
			{ "10",  "10%" },
			{ "0",   "Muted (0%)" },
			{ nullptr, nullptr }
		},
		"100"
	},
	{
		"ti99sim_volume_speech",
		"Audio > TMS5220 Speech Volume",
		"Speech Volume",
		"Set the volume level for TMS5220 LPC speech synthesis relative to PSG.",
		nullptr,
		"audio",
		{
			{ "200", "200% (+6 dB)" },
			{ "175", "175%" },
			{ "150", "150% (+3.5 dB)" },
			{ "125", "125%" },
			{ "100", "100% (Default)" },
			{ "75",  "75%" },
			{ "50",  "50%" },
			{ "25",  "25%" },
			{ "0",   "Muted (0%)" },
			{ nullptr, nullptr }
		},
		"100"
	},
	{
		"ti99sim_dpad_gate",
		"Input > D-Pad Directional Gate",
		"D-Pad Gate",
		"Select 4-way mode (inter-axis Last-Input-Wins, ideal for Parsec, Munch Man, and maze games) or full 8-way diagonal mode.",
		nullptr,
		"input",
		{
			{ "4way", "4-Way (Last-Input Priority)" },
			{ "8way", "8-Way (Diagonals Allowed)" },
			{ nullptr, nullptr }
		},
		"4way"
	},
	{
		"ti99sim_dpad_cursor",
		"Input > D-Pad Also Presses Cursor Keys",
		"D-Pad Also Presses Cursor Keys",
		"Allow the D-pad to simultaneously trigger TI keyboard movement keys alongside Joystick 1.",
		nullptr,
		"input",
		{
			{ "disabled", "Disabled (Joystick Only)" },
			{ "esdx",     "E / S / D / X" },
			{ "arrows",   "FCTN + E / S / D / X (Arrow Keys)" },
			{ nullptr, nullptr }
		},
		"disabled"
	},
	{
		"ti99sim_pad_a",
		"Input > Button A Mapping",
		"Button A Mapping",
		"TI-99/4A action or key bound to RetroPad A.",
		nullptr,
		"input",
		{ TI99SIM_BUTTON_OPTION_VALUES },
		"JOY_FIRE"
	},
	{
		"ti99sim_pad_b",
		"Input > Button B Mapping",
		"Button B Mapping",
		"TI-99/4A action or key bound to RetroPad B.",
		nullptr,
		"input",
		{ TI99SIM_BUTTON_OPTION_VALUES },
		"1"
	},
	{
		"ti99sim_pad_y",
		"Input > Button Y Mapping",
		"Button Y Mapping",
		"TI-99/4A action or key bound to RetroPad Y.",
		nullptr,
		"input",
		{ TI99SIM_BUTTON_OPTION_VALUES },
		"2"
	},
	{
		"ti99sim_pad_x",
		"Input > Button X Mapping",
		"Button X Mapping",
		"TI-99/4A action or key bound to RetroPad X.",
		nullptr,
		"input",
		{ TI99SIM_BUTTON_OPTION_VALUES },
		"3"
	},
	{
		"ti99sim_pad_start",
		"Input > Button Start Mapping",
		"Button Start Mapping",
		"TI-99/4A action or key bound to RetroPad Start.",
		nullptr,
		"input",
		{ TI99SIM_BUTTON_OPTION_VALUES },
		"ENTER"
	},
	{
		"ti99sim_pad_select",
		"Input > Button Select Mapping",
		"Button Select Mapping",
		"TI-99/4A action or key bound to RetroPad Select.",
		nullptr,
		"input",
		{ TI99SIM_BUTTON_OPTION_VALUES },
		"OSK"
	},
	{
		"ti99sim_pad_l",
		"Input > Button L (L1) Mapping",
		"Button L (L1) Mapping",
		"TI-99/4A action or key bound to RetroPad L.",
		nullptr,
		"input",
		{ TI99SIM_BUTTON_OPTION_VALUES },
		"FCTN_REDO"
	},
	{
		"ti99sim_pad_r",
		"Input > Button R (R1) Mapping",
		"Button R (R1) Mapping",
		"TI-99/4A action or key bound to RetroPad R.",
		nullptr,
		"input",
		{ TI99SIM_BUTTON_OPTION_VALUES },
		"FCTN_BACK"
	},
	{
		"ti99sim_pad_l2",
		"Input > Button L2 Mapping",
		"Button L2 Mapping",
		"TI-99/4A action or key bound to RetroPad L2.",
		nullptr,
		"input",
		{ TI99SIM_BUTTON_OPTION_VALUES },
		"Y"
	},
	{
		"ti99sim_pad_r2",
		"Input > Button R2 Mapping",
		"Button R2 Mapping",
		"TI-99/4A action or key bound to RetroPad R2.",
		nullptr,
		"input",
		{ TI99SIM_BUTTON_OPTION_VALUES },
		"N"
	},
	{ nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, {{0}}, nullptr }
};

static struct retro_core_options_v2 options_us =
{
	option_cats_us,
	option_defs_us
};

inline void libretro_set_core_options( retro_environment_t environ_cb )
{
	if( !environ_cb )
	{
		return;
	}

	unsigned version = 0;
	if( !environ_cb( RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION, &version ))
	{
		version = 0;
	}

	if( version >= 2 )
	{
		struct retro_core_options_v2_intl core_options_intl = { &options_us, nullptr };
		if( environ_cb( RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL, &core_options_intl ))
		{
			return;
		}
		if( environ_cb( RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2, &options_us ))
		{
			return;
		}
	}

	// Fallback for frontends supporting only v1 or v0 core options
	size_t num_opts = 0;
	while( option_defs_us[ num_opts ].key != nullptr )
	{
		num_opts++;
	}

	if( version >= 1 )
	{
		static struct retro_core_option_definition v1_defs[ 48 ];
		memset( v1_defs, 0, sizeof( v1_defs ));
		for( size_t i = 0; i < num_opts && i < 47; i++ )
		{
			v1_defs[ i ].key           = option_defs_us[ i ].key;
			v1_defs[ i ].desc          = option_defs_us[ i ].desc;
			v1_defs[ i ].info          = option_defs_us[ i ].info;
			v1_defs[ i ].default_value = option_defs_us[ i ].default_value;
			memcpy( v1_defs[ i ].values, option_defs_us[ i ].values, sizeof( v1_defs[ i ].values ));
		}
		if( environ_cb( RETRO_ENVIRONMENT_SET_CORE_OPTIONS, &v1_defs ))
		{
			return;
		}
	}

	// Fallback for v0 frontends (RETRO_ENVIRONMENT_SET_VARIABLES)
	static std::string var_strings[ 48 ];
	static struct retro_variable vars[ 49 ];
	memset( vars, 0, sizeof( vars ));
	for( size_t i = 0; i < num_opts && i < 48; i++ )
	{
		std::string str = option_defs_us[ i ].desc ? option_defs_us[ i ].desc : "";
		str += "; ";
		bool first = true;
		for( size_t v = 0; option_defs_us[ i ].values[ v ].value != nullptr; v++ )
		{
			if( !first )
			{
				str += "|";
			}
			str += option_defs_us[ i ].values[ v ].value;
			first = false;
		}
		var_strings[ i ] = std::move( str );
		vars[ i ].key   = option_defs_us[ i ].key;
		vars[ i ].value = var_strings[ i ].c_str( );
	}
	environ_cb( RETRO_ENVIRONMENT_SET_VARIABLES, vars );
}

#endif
