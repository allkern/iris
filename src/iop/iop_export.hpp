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
