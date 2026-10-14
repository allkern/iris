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

#pragma once

#include "iop.hpp"

#include "hle/ioman.hpp"
#include "hle/loadcore.hpp"
#include "hle/sysmem.hpp"
#include "hle/thbase.hpp"
#include <cstdint>
#include "logger.hpp"

namespace iris::iop {

#define MODULE_UNKNOWN  0
#define MODULE_IOMAN    1
#define MODULE_IOMANX   2
#define MODULE_LOADCORE 3
#define MODULE_SYSMEM   4

#define IOMAN_OPEN      4
#define IOMAN_CLOSE     5
#define IOMAN_READ      6
#define IOMAN_WRITE     7
#define IOMAN_LSEEK     8
#define IOMAN_IOCTL     9
#define IOMAN_REMOVE    10
#define IOMAN_MKDIR     11
#define IOMAN_RMDIR     12
#define IOMAN_DOPEN     13
#define IOMAN_DCLOSE    14
#define IOMAN_DREAD     15
#define IOMAN_GETSTAT   16
#define IOMAN_CHSTAT    17
#define IOMAN_FORMAT    18
#define IOMAN_ADDDRV    20
#define IOMAN_DELDRV    21
#define IOMAN_STDIOINIT 23
#define IOMAN_RENAME    25
#define IOMAN_CHDIR     26
#define IOMAN_SYNC      27
#define IOMAN_MOUNT     28
#define IOMAN_UMOUNT    29
#define IOMAN_LSEEK64   30
#define IOMAN_DEVCTL    31
#define IOMAN_SYMLINK   32
#define IOMAN_READLINK  33
#define IOMAN_IOCTL2    34

#define LOADCORE_REG_LIB_ENT 6

uint32_t irx_import_table_addr(Iop* iop, int entry);
int get_module(Iop* iop, int itable);
int delegate_ioman(Iop* iop, int slot, int iomanx);
int delegate_loadcore(Iop* iop, int slot);
int delegate_sysmem(Iop* iop, int slot);
int test_module_hooks(Iop* iop);
int get_module_for_address(Iop* iop, uint32_t addr, uint32_t* slot);
void set_return(Iop* iop, int ret);

}
