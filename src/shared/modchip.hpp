/*
    This file is part of the Iris PlayStation 2 emulator project.
    Hosted at https://github.com/allkern/iris

    MIT License

    Copyright (c) 2026 Allkern/Lycoder (Lisandro Alarcón)

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

/*  shared/modchip.h - Matrix Infinity/Modbo modchip emulation

    Notes: Matrix Infinity (MI) and its clones (like Modbo 5.0) are
           basically just boards with an FPGA and some flash on them.
           These modchips overlay a fixed patch (stage 0) over a piece
           of the BIOS. In my case, the chip overlays a patch over sprintf
           which then acts as a bootloader for two more boot stages.

           The patch (stage 0 bootloader) talks to the chip by writing
           to 1FC00000h (BIOS area), which is normally read-only, the chip
           intercepts these writes and interprets them as commands.

           The chip answers with data by pulling down data lines on reads
           to (1FC00000 +) 6FAh, 6FBh and 6FEh, I explain these addresses
           down below.

           Worth noting that the chip will only dump its flash contents
           once, after boot, the chip returns 0

           Command format
           --------------
           A command is a 16-bit value sent as three bytes to 1FC00000h

           Byte     Description
           0        4Dh ('M'), starts a command
           1        Command group (high byte)
           2        Operand (low byte)

           The byte that gets transferred depends on the size of the access,
           SB acts normally, but 16/32/64/128-bit writes act differently.
           For 16/32-bit writes, the MS byte is transferred, for 64/128-bit
           writes, the MS byte of *every 32-bit word* is transferred, that
           means an SQ can transfer up to 4 bytes at once.

           Software keeps a retry flag in bit 16 of the command, it's never
           sent to the chip.

           Replies
           -------
           The chip puts reply bytes in 1FC006FBh, the code determines
           whether a reply is valid by comparing the halfword at 6FAh with
           the one at 6FEh (they have to be equal)

           Commands
           --------
           Commands are made up of a command byte and an operand byte:

           Cmd      Description                       Reply
           A0h      Chip select
                    - 00h: Set CS                     00h
                    - 01h: Reset CS                   FFh
           A1h      SPI transfer byte                 XXh (SPI reply)
           A2h      Same as A1h                       XXh
           A9h      Same as A1h                       XXh
           A3h      Repeat the last reply             XXh
           A4h      Get chip version                  10h?
           A5h      Reset key stream                  00h
           A6h      Read key stream                   XXh
           A7h      Set key stream increment to 1     00h
           AEh      Set mode/region?                  00h
           ACh      Set key stream increment to 2     00h

           SPI flash
           ---------
           Seems to be a PMC Pm25LV? Sending a command requires first setting
           CS, then resetting it after sending all commands

           Command  Description
           01h      Write status reg (WEL has to be set)
           02h      Page program
           03h      Read
           04h      Write disable
           05h      Read status reg (bit 0 - busy, bit 1 - WEL)
           06h      Write enable
           0Bh      Fast read
           20h      Sector erase (4 KiB)
           60h      Chip erase
           81h      Page erase (256 bytes)
           ABh      Read ID
           C7h      Same as 60h
           D7h      Same as 20h
           D8h      Block erase (64 KiB)

           Flash layout (MI 1.93/Modbo 5.0)
           --------------------------------

           Offset   Description
           00000h   Header, "Infinity v1.93"
           0001Ch   Stage 1 entry point
           003FCh   Rest of stage 1. Scrambled, in blocks 400h apart.
                    Each block is two big endian checksums followed by
                    3FCh bytes XORed with t >> 5, t = (t * 7FEDh + 7F89h) % 7EF1h,
                    seeded with 1337h (lol)
           0E000h   Matrix logo
           0F500h   Copy of the kernel stub (SCPH-90001 variant), Modbo has
                    more at 1FF00h and 7FF00h
           0FC00h   MI settings, 400h bytes whose words add up to 12345678h
           17C00h   Modbo settings, same format
           20000h   Modbo boot menu, an NRV2E packed ELF

           Boot process
           ------------
           The stage 0 bootloader (sprintf patch mentioned before) reads
           1 KiB from flash into 100000h. Every byte is XORed with a key
           that starts at 2Dh, then the next value is calculated with the
           formula k = ((k ^ A7h) + i), where i is the byte index.
           THe bootloader then runs a simple XOR checksum, if it passes
           it then jumps to stage 1 at 80100020h, otherwise it just
           retries.

           Stage 1 reads the rest of itself over SPI, runs the anti-clone
           check (see below), LZMA unpacks stage 2 to 110000h and jumps to it.
           
           Stage 2 then reads input and performs different actions.
           Triangle+Circle will start up the configuration menu, holding R1
           will boot mc0:/BOOT/BOOT.ELF, etc.

           Anti-clone check
           ----------------
           The FPGA on real MI chips generates a bitstream that's used as a
           key to detect clones. Even though my Modbo modchip won't output
           its flash contents after boot no matter what I do, it will happily
           spit out this security bitstream for whatever reason, I'll take it.
           
           The bitstream is checked during stage 1, it first picks a number N
           based on the RTC, the stream is then reset (with cmd A5) and the
           byte index increment is selected with A7/AC as described above,
           The 0x400-byte block to read is selected with N, thus, stage 1
           clocks the output 1024 * N times before reading.

           Stage 1 then reads bytes out of the bitstream with A6 until the
           read byte matches against sync[N] (a table at 108B40h). Finally
           it reads 1 KiB out of the bitstream, SHA-256 hashes it and compares
           the first 8 bytes of the hash against a table at 108340h.

           The stream is simply a byte wide LFSR of order 22

           s[i] = s[i-1] ^ s[i-2] ^ s[i-3] ^ s[i-6] ^ s[i-7] ^ s[i-8] ^
                   s[i-9] ^ s[i-13] ^ s[i-14] ^ s[i-16] ^ s[i-17] ^
                   s[i-18] ^ s[i-19] ^ s[i-22]

           seeded with STREAM_SEED. Block N starts at s[N * 400h] and A600h
           wraps back to the start of the block every 1 KiB.

           As mentioned before I recovered this stream from my real Modbo
           chip, one caveat is that this clone only ever outputs block 0,
           it essentially "ignores" the initial block selection.

           Modbo
           -----
           Modbo hooks stage 1 at 10A35Ch to copy its own code to 8009F000h
           and move the settings to 17C00h. That code patches the check at
           105B20h so N is always 0, which is how it passes on a chip that
           can't clock. Its loader at 9F248h then sends A000h, A903h, three
           address bytes with A9h and 1024 A100h (retry flag set) per KiB,
           copying 250 KiB from flash 20000h to 1CC1EE0h, and jumps to
           1D00008h, which unpacks the boot menu ELF to 100000h.

           Note: I still don't know what a non-clone MI outputs on blocks
                 other than 0, so using real MI firmwares on Iris will likely
                 not work
*/

#pragma once

#include <cstddef>
#include <cstdint>

#include "bios.hpp"
#include "logger.hpp"
#include "u128.h"

namespace iris::modchip {

inline constexpr uint32_t WINDOW_BASE = 0x1fc00000;
inline constexpr uint32_t WINDOW_END = 0x1fc01fff;

inline constexpr int STREAM_ORDER = 22;
inline constexpr int STREAM_BLOCK_SIZE = 0x400;

struct Modchip {
    struct {
        bios::Bios* bios = nullptr;
    } hw;

    uint8_t* flash = nullptr;
    size_t flash_size = 0;
    char* flash_path = nullptr;
    uint8_t flash_status = 0;
    bool flash_dirty = false;
    bool write_enabled = false;

    int parse_state = 0;
    uint8_t command_hi = 0;

    bool selected = false;
    int spi_count = 0;
    uint8_t spi_command = 0;
    uint32_t spi_address = 0;

    uint8_t reply = 0xff;
    bool reply_pending = false;

    uint8_t mode = 0;

    bool clock_armed = false;
    uint8_t stream_state[STREAM_ORDER] = { 0 };
    uint8_t stream_block[STREAM_BLOCK_SIZE] = { 0 };
    bool stream_block_valid = false;
    int stream_index = 0;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Modchip* create(logger::Logger* logger, bios::Bios* bios);
bool load_flash(Modchip* chip, const char* path);
void unload_flash(Modchip* chip);
bool is_enabled(const Modchip* chip);
void reset(Modchip* chip);
void destroy(Modchip* chip);

uint64_t read8(Modchip* chip, uint32_t addr);
uint64_t read16(Modchip* chip, uint32_t addr);
uint64_t read32(Modchip* chip, uint32_t addr);
uint64_t read64(Modchip* chip, uint32_t addr);
uint128_t read128(Modchip* chip, uint32_t addr);
void write8(Modchip* chip, uint32_t addr, uint64_t data);
void write16(Modchip* chip, uint32_t addr, uint64_t data);
void write32(Modchip* chip, uint32_t addr, uint64_t data);
void write64(Modchip* chip, uint32_t addr, uint64_t data);
void write128(Modchip* chip, uint32_t addr, uint128_t data);

bool unpatch_bios(bios::Bios* bios);
bool install_stub(bios::Bios* bios);

}
