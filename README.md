<div align="center" text-align="center" width="100%">
    <img width="55%" src="https://github.com/user-attachments/assets/d59e2d95-5791-4497-9985-442ca5115ac6">
</div>

# 🐣 Iris
An experimental Sony PlayStation 2 emulator and debugger

## Screenshots
<div align="center" class="grid" markdown>
  <img width="45%" src="https://github.com/user-attachments/assets/39106951-9d45-484f-b4ae-13197305bf06"/>
  <img width="45%" src="https://github.com/user-attachments/assets/e7d24d24-ccac-4239-baba-80d880db35bf"/>
  <img width="45%" src="https://github.com/user-attachments/assets/3d2499fd-304e-4f2c-a1ce-677912f13753"/>
  <img width="45%" src="https://github.com/user-attachments/assets/de37505e-efea-4d3a-94fe-3438b2e9722b"/>
  <img width="45%" src="https://github.com/user-attachments/assets/d97b16fe-f59f-4174-97eb-f4dadf4c4df0"/>
  <img width="45%" src="https://github.com/user-attachments/assets/f061db57-96f3-4fad-94ea-8b023a5875ad"/>
  <img width="45%" src="https://github.com/user-attachments/assets/5ac202f5-eb74-493f-bb35-c6acf752a50b"/>
  <img width="45%" src="https://github.com/user-attachments/assets/099ddda9-4f7f-4d8d-8071-40741bbd3bfc"/>
</div>

## Usage
> [!WARNING]  
> This emulator is under development, it can only run a small number of commercial games

Iris has a graphical user interface and also supports launching from the command line:
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

Launching a game or executable through the GUI is also very easy, just go to Iris > Open... and pick a disc image or ELF executable.

## Building
Building the emulator should be pretty straightforward, just recursively clone the repository and follow the steps:

### Linux
```
git clone https://github.com/allkern/iris --recursive
cd iris
./setup-gl3w.sh
make -j8
```

### Windows
```
git clone https://github.com/allkern/iris --recursive
cd iris
./build-deps.ps1
./build-win.ps1
```

### macOS
> [!WARNING]  
> Iris should support macOS but hasn't been fully tested yet
```
git clone https://github.com/allkern/iris --recursive
cd iris
./setup-gl3w.sh
./build.sh
```

## Progress
### Commercial games
Booting a small number of commercial games in-game, and a slightly bigger set of games can boot to the title screen. Most of them do nothing though, an the ones that do usually run way too slow to be playable.

### BIOS
Pretty much all BIOSes I've tried work just fine, even some obscure ones like the Chinese BIOS and the PSX DESR BIOS (more on this later).

It is also possible to specify paths to ROM1 (DVD player) and ROM2 (Chinese extensions, required for the Chinese BIOS).

The only caveat is that none of them render any background graphics when booting, and the little orbs and 3D models. This is probably due to my terrible VU emulation and might be fixed soon.

## PSX DESR
Support for the PSX DESR console is early but somewhat functional. The DESR BIOS plays the boot animation but later fails some sort of diagnostic test. The DESR requires Flash, ATA and MagicGate emulation, which Iris doesn't yet support.

Booting to the XMB should be possible once these features are implemented, and is one of my medium-term goals for this project.

# Special thanks and acknowledgements
I would like to thank the emudev Discord server, Ziemas, Nelson (ncarrillo), cakehonolulu, PSI-rockin and the PCSX2 team for their kind support.

This project makes use of ImGui, gl3w, toml++, Portable File Dialogs and stb_image

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
  = 🔴 Micro mode
  = 🔴 VIF0
- 🔴 VU1 (always micro mode)
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
🟡 SIF
```
