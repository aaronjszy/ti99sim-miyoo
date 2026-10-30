Custom Per-Game Keymaps
========================

You can customize button mappings for individual TI-99/4A cartridges by creating
a .ini file matching the cartridge filename (without .ctg).

Search order (first match is loaded and overrides default keymap.ini):
1. Next to the ROM file in Roms/TI99/:
     /mnt/SDCARD/Roms/TI99/<game>.ini
     /mnt/SDCARD/Roms/TI99/<game>.keymap.ini

2. In this keymaps/ directory:
     /mnt/SDCARD/Emu/TI99/keymaps/<game>.ini
     /mnt/SDCARD/Emu/TI99/keymaps/<game>.keymap.ini

3. In the emulator directory:
     /mnt/SDCARD/Emu/TI99/<game>.ini

Format:
  <SDL key name> = <action>      # comment

Host keys on Miyoo Mini:
  Space       = [A]
  Left Ctrl   = [B]
  Left Shift  = [X]
  Left Alt    = [Y]
  E           = [L1]
  T           = [R1]
  Tab         = [L2]
  Backspace   = [R2]
  Return      = [Start]
  Right Ctrl  = [Select]

Actions:
  FIRE, RESET, SAVE, LOAD, NONE, or TI keys (A-Z, 0-9, ENTER, SPACE, etc.),
  optionally with modifiers: FCTN+9, SHIFT+1, CTRL+C.
