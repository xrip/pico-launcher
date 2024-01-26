# pico-launcher - Bootloader Firmware Flasher / Launcher for MURMULATOR Devboard
Bootloader firmware flasher / launcher for MURMULATOR devboard

# How to Use
1) Copy all your Pico firmwares to the SD card.
2) Reboot PICO by holding down SELECT on the Gamepad or F11 on the keyboard and select default firmware.

Additionally, you can:
- Hold F12 or START button while booting to enter SD card firmware update mode.
- Use F10 or the A button in the launcher to enter SD card-reader mode.

# Compiling
To compile it yourself:

1. Copy the file ``boot2\exit_from_boot2.S`` from the repository to your Pico SDK at ``pico-sdk\src\rp2_common\boot_stage2\asminclude\boot2_helpers\exit_from_boot2.S``
2. Build using the "MinSizeRel" build type.
