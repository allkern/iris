<div align="center" text-align="center" width="100%">
    <img width="55%" src="https://github.com/user-attachments/assets/d59e2d95-5791-4497-9985-442ca5115ac6">
</div>

# 🐣 Iris
Sony PlayStation 2 emulator for Windows, Linux and macOS 

## Screenshots
<div align="center" class="grid" markdown>
    <img width="47%" alt="Metal Gear Solid 3 - Snake Eater (Japan)" src="https://github.com/user-attachments/assets/9ffd0131-5fa0-4e2f-97ee-6d180f7dcdcc" />
    <img width="47%" alt="Resident Evil 4 (USA)" src="https://github.com/user-attachments/assets/d2f68c22-c6c9-4e00-9430-ecf609f9b269" />
    <img width="47%" alt="God of War II (USA)" src="https://github.com/user-attachments/assets/40747140-1b0c-4834-b3ab-e5439cc1272d" />
    <img width="47%" alt="Kingdom Hearts II (USA)" src="https://github.com/user-attachments/assets/53aa9486-958f-49c6-aa88-cd435260bb0a" />
    <img width="47%" alt="Ace Combat Zero - The Belkan War (USA)" src="https://github.com/user-attachments/assets/1adfaf21-305d-4d2c-a828-00cbbbfba920" />
    <img width="47%" alt="Virtua Fighter 4 (USA)" src="https://github.com/user-attachments/assets/1d1662ed-177d-48de-b21f-300a035da1b3" />
    <img width="47%" alt="Devil May Cry 3 - Dante's Awakening (USA)" src="https://github.com/user-attachments/assets/a7fc4654-cdb1-4a00-8187-a99ab5b4f7ea" />
    <img width="47%" alt="Ico (USA)" src="https://github.com/user-attachments/assets/419b76a4-5208-4e18-bebe-ab762c0e08ed" />
</div>

## Usage
> [!WARNING]  
> This emulator is under development, most games WILL run at very low/unplayable framerates.

### GUI
Navigate over to `Iris > Open...` and choose a disc image or ELF executable, drag-and-drop is also supported

### CLI
```
Usage: iris [OPTION]... <path-to-disc-image>

  -b, --bios               Specify a PlayStation 2 BIOS dump file
      --rom1               Specify a DVD player dump file
      --rom2               Specify a ROM2 dump file
  -d, --boot               Specify a direct kernel boot path
  -i, --disc               Specify a path to a disc image file
  -x, --executable         Specify a path to an ELF executable to be
                             loaded on system startup
      --slot1              Specify a path to a memory card file to
                             be inserted on slot 1
      --slot2              Specify a path to a memory card file to
                             be inserted on slot 2
  -h, --help               Display this help and exit
  -v, --version            Output version information and exit
```

## Features
- Support for ISO, BIN/CUE, CHD and CSO/ZSO disc image formats
- Hardware-accelerated Vulkan GS renderer with support for up to 16x SSAA
- Feature-packed debugger
- Easy to use graphical interface
- Game controller support with input remapping
- Support for post-processing shaders

## Building
> [!WARNING]  
> Building requires CMake and the Vulkan SDK on all supported platforms

### Linux
Building on Linux requires installing SDL3 dependencies and FUSE if you wish to generate AppImages.
```
sudo apt update
sudo apt upgrade
sudo add-apt-repository universe
sudo apt-get install build-essential git make \
    pkg-config cmake ninja-build gnome-desktop-testing libasound2-dev libpulse-dev \
    libaudio-dev libjack-dev libsndio-dev libx11-dev libxext-dev \
    libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev libxss-dev libxtst-dev \
    libxkbcommon-dev libdrm-dev libgbm-dev libgl1-mesa-dev libgles2-mesa-dev \
    libegl1-mesa-dev libdbus-1-dev libibus-1.0-dev libudev-dev libfuse2t64
```
Then clone the repository and run CMake:
```
git clone https://github.com/allkern/iris --recursive
cd iris
cmake -S . -B build
cmake --build build -j8
```
Optionally run `cmake --install build` to generate an AppImage.

### Windows
We currently only support GCC as a compiler on Windows, this is because MSVC doesn't have an inline assembler, which we need to embed resources into the executable. This might eventually be fixed though!
```
git clone https://github.com/allkern/iris --recursive
cd iris
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build -j8
```

### macOS

```
git clone https://github.com/allkern/iris --recursive
cd iris
cmake -S . -B build
cmake --build build -j8
```
Optionally run `sudo cmake --install build` to generate a macOS App Bundle

## Progress/Insights
Iris can boot/run a fairly large number of commercial games, playability may be all over the place though, some games run fairly smoothly, while others can't break the 1 digit FPS mark, this is due to the lack of EE/VU JITs which will be addressed soon.

The PlayStation 2 can have up to three processors running simultaneously at ~300 MHz, plus the IOP running at 33 MHz, not counting all the different peripherals/chips doing their own work, such as the GS rendering massive amounts of graphics, the IPU decoding MPEG-2 video, the SPU2 rendering up to 48 ADPCM audio channels at 48 KHz, and more. You can see how emulating this system can become a pretty complex task once you factor in all the processing that's done per-frame.

In order to alleviate the struggles of emulating the PS2, we have a number of optimization techniques at our disposal, some of which have already been implemented:
- Scheduling (done)
- Software fastmem (done)
- EE interpreter caching (done)
- Hardware-accelerated GS rendering (done)
- EE JIT/Dynarec (coming up)
- VU JIT/Dynarec (soon)
- Hardware fastmem (eventually)
- etc.

Integrating Parallel-GS was a big milestone for Iris, now we need to work on JITing the EE. Once that's done, I'd expect the emulator to start running a lot more games at playable speeds.

## Preservation
Iris aims to emulate not only the retail PlayStation 2, but also other systems based on the PS2, such as the PSX DESR (Japanese PS2/DVR hybrid) and all or most of the PS2-based arcade systems, work towards this goal is ongoing, in fact, Iris was the first PS2 emulator to boot [the PSX DESR BIOS/bootrom](https://www.youtube.com/watch?v=YtsoRjofYKA).

In order to emulate these systems, a pile of extra hardware needs to be implemented, such as the onboard flash memory on the PSX DESR, and the NAND storage on all of the Namco boards. This is not trivial and the lack of documentation makes it a pretty daunting task, but we're working on it.

It's worth to mention that PSX DESR support has been merged to the main branch, which means you (our beloved user) should be able to dump the BIOS out of your PSX DESR machine and run it on Iris. It won't boot past some error screens after running the boot animation but I'd say it's still pretty cool.

In addition to emulating other systems based on the PS2, Iris also aims to support **all** the features/capabilities of the retail system, this includes the often overlooked DVD player, PSBBN, Linux, and eventually all of the available external USB/SIO peripherals and input/output devices.

# Special thanks and acknowledgements
I would like to thank the emudev Discord server, Ziemas, Nelson (ncarrillo), cakehonolulu, PSI-rockin, noumi and the PCSX2 team for their kind support.

This project makes use of the following third-party libraries:
- [ImGui](https://github.com/ocornut/imgui)
- [ImPlot](https://github.com/epezent/implot)
- [SDL3](https://github.com/libsdl-org/SDL)
- [SDL_GameControllerDB](https://github.com/mdqinc/SDL_GameControllerDB)
- [incbin](https://github.com/graphitemaster/incbin)
- [Parallel-GS](https://github.com/Arntzen-software/parallel-gs)
- [libchdr](https://github.com/rtissera/libchdr)
- [libdeflate](https://github.com/ebiggers/libdeflate)
- [lz4](https://github.com/lz4/lz4)
- [toml++](https://marzer.github.io/tomlplusplus/)
- [Portable File Dialogs](https://github.com/samhocevar/portable-file-dialogs)
- [stb_image](https://github.com/nothings/stb)

Credit goes out to the developers of these libraries, Iris wouldn't have been possible without your outstanding work.

### Components
This console is significantly more complex compared to the PS1, here's a rough list of components:
```
🟡 EE (R5900) CPU
- 🟡 FPU
- 🟡 MMI (SIMD)
- 🟡 TLB
- 🟡 DMAC
- 🟢 INTC
- 🟡 Timers
- 🟢 GIF
- 🟡 GS
- 🟡 VU0
  = 🟡 Macro mode
  = 🟡 Micro mode
  = 🟡 VIF0
- 🟡 VU1 (always micro mode)
  = 🟡 VIF1
- 🟡 IPU
🟢 IOP (R3000) CPU
- 🟡 DMAC
- 🟢 INTC
- 🟡 Timers
- 🟢 CDVD
- 🟢 SIO2 (controllers and Memory Cards)
- 🟢 SPU2
- 🟡 DEV9
- 🟡 USB/FireWire?
- 🔴 Ethernet
- 🔴 PS1 backcompat (PS1 hardware)
🟢 SIF
```
