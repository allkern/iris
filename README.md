# 🐣eegs
An attempt at Sony PlayStation 2 emulation. Right now under early development, expect to see more in the coming months!

| 3stars.elf | bytheway.elf | Crazy Taxi (SLUS_202.02) |
| ------------- | ------------- | ------------- |
| ![](https://github.com/user-attachments/assets/5702f174-24ea-40d0-b404-e26787a151c8) | ![](https://github.com/user-attachments/assets/9b99d4fc-2232-492b-a14d-3d7f472e6928) | ![](https://github.com/user-attachments/assets/887e8c08-e870-44e0-b9ef-ac41903e83d7) |
## Usage

> [!WARNING]  
> This emulator is under development, it can't run any commercial games yet

### Building (Unix)
```
git clone https://github.com/allkern/eegs
cd eegs
make
```

### Running
```
eegs -b <path-to-bios> -x <path-to-elf> -d <direct-boot-path>
```

## Progress
Booting the `SCPH10000.BIN` BIOS up to the point it starts loading the necessary modules for OSDSYS (the system menu), and the system menu itself.

When the BIOS gets to this point, it will instruct the IOP to load the system menu executable at path `rom0:OSDSYS`. We can patch this path in memory to make the IOP load and boot any file we want, for example, a game executable stored in CDVD.

By parsing the filesystem of an ISO 9660 game dump, we can extract the `SYSTEM.CNF` file, this file contains (among other things) a path to the game's executable:
```
BOOT2 = cdrom0:\SLUS_210.90;1
VER = 1.02
VMODE = NTSC
```
We can then replace the OSDSYS path with the path we got from SYSTEM.CNF, which will make the BIOS boot our game instead.

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
- 🟡 CDVD
- 🟡 SIO2 (controllers and Memory Cards)
- 🟡 SPU2
- 🔴 DEV9
- 🟡 USB/FireWire?
- 🔴 Ethernet
- 🔴 PS1 backcompat (PS1 hardware)
🟡 SIF
```
