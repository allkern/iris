#include "iop_export.hpp"
#include "iop_def.hpp"
#include "logger.hpp"

namespace iris::iop {

uint32_t irx_import_table_addr(Iop* iop, int entry) {
    uint32_t i = entry - 0x18;

    while ((entry - i) < 0x2000) {
        if (read32(iop, i) == 0x41e00000)
            return i;

        i -= 4;
    }

    return 0;
}

int get_module(Iop* iop, int itable) {
    char buf[9];

    for (int i = 0; i < 8; i++)
        buf[i] = read8(iop, itable + 12 + i);

    if (!strncmp(buf, "ioman", 8)) return MODULE_IOMAN;
    if (!strncmp(buf, "iomanx", 8)) return MODULE_IOMANX;
    if (!strncmp(buf, "loadcore", 8)) return MODULE_LOADCORE;
    if (!strncmp(buf, "sysmem", 8)) return MODULE_SYSMEM;

    return MODULE_UNKNOWN;
}

int delegate_ioman(Iop* iop, int slot, int iomanx) {
    switch (slot & 0xffff) {
        case IOMAN_OPEN: return iop::hle::ioman::open(iop, iomanx);
        case IOMAN_CLOSE: return iop::hle::ioman::close(iop, iomanx);
        case IOMAN_READ: return iop::hle::ioman::read(iop, iomanx);
        case IOMAN_WRITE: return iop::hle::ioman::write(iop, iomanx);
        case IOMAN_LSEEK: return iop::hle::ioman::lseek(iop, iomanx);
        case IOMAN_IOCTL: return iop::hle::ioman::ioctl(iop, iomanx);
        case IOMAN_REMOVE: return iop::hle::ioman::remove(iop, iomanx);
        case IOMAN_MKDIR: return iop::hle::ioman::mkdir(iop, iomanx);
        case IOMAN_RMDIR: return iop::hle::ioman::rmdir(iop, iomanx);
        case IOMAN_DOPEN: return iop::hle::ioman::dopen(iop, iomanx);
        case IOMAN_DCLOSE: return iop::hle::ioman::dclose(iop, iomanx);
        case IOMAN_DREAD: return iop::hle::ioman::dread(iop, iomanx);
        case IOMAN_GETSTAT: return iop::hle::ioman::getstat(iop, iomanx);
        case IOMAN_CHSTAT: return iop::hle::ioman::chstat(iop, iomanx);
        case IOMAN_FORMAT: return iop::hle::ioman::format(iop, iomanx);
        case IOMAN_ADDDRV: return iop::hle::ioman::adddrv(iop, iomanx);
        case IOMAN_DELDRV: return iop::hle::ioman::deldrv(iop, iomanx);
        case IOMAN_STDIOINIT: return iop::hle::ioman::stdioinit(iop, iomanx);
        case IOMAN_RENAME: return iop::hle::ioman::rename(iop, iomanx);
        case IOMAN_CHDIR: return iop::hle::ioman::chdir(iop, iomanx);
        case IOMAN_SYNC: return iop::hle::ioman::sync(iop, iomanx);
        case IOMAN_MOUNT: return iop::hle::ioman::mount(iop, iomanx);
        case IOMAN_UMOUNT: return iop::hle::ioman::umount(iop, iomanx);
        case IOMAN_LSEEK64: return iop::hle::ioman::lseek64(iop, iomanx);
        case IOMAN_DEVCTL: return iop::hle::ioman::devctl(iop, iomanx);
        case IOMAN_SYMLINK: return iop::hle::ioman::symlink(iop, iomanx);
        case IOMAN_READLINK: return iop::hle::ioman::readlink(iop, iomanx);
        case IOMAN_IOCTL2: return iop::hle::ioman::ioctl2(iop, iomanx);
    }

    return 0;
}

int delegate_loadcore(Iop* iop, int slot) {
    switch (slot & 0xffff) {
        case LOADCORE_REG_LIB_ENT: return iop::hle::loadcore::reg_lib_ent(iop);
    }

    return 0;
}

int delegate_sysmem(Iop* iop, int slot) {
    switch (slot & 0xffff) {
        // SYSMEM kprintf
        case 14: return iop::hle::sysmem::kprintf(iop);
    }

    return 0;
}

int test_module_hooks(Iop* iop) {
    uint32_t slot = read32(iop, iop->pc);

    if ((slot >> 16) != 0x2400)
        return 0;

    uint32_t itable = irx_import_table_addr(iop, iop->pc);

    if (!itable)
        return 0;

    int module = get_module(iop, itable);

    if (!module)
        return 0;

    switch (module) {
        case MODULE_IOMAN: return delegate_ioman(iop, slot, 0);
        case MODULE_IOMANX: return delegate_ioman(iop, slot, 1);
        case MODULE_LOADCORE: return delegate_loadcore(iop, slot);
        case MODULE_SYSMEM: return delegate_sysmem(iop, slot);
    }

    return 0;
}

int get_module_for_address(Iop* iop, uint32_t addr, uint32_t* slot) {
    *slot = read32(iop, addr);

    if ((*slot >> 16) != 0x2400)
        return 0;

    uint32_t itable = irx_import_table_addr(iop, addr);

    if (!itable)
        return 0;

    int module = get_module(iop, itable);

    if (!module)
        return 0;

    return module;
}

void set_return(Iop* iop, int ret) {
    // printf("hle: ret=%d pc=%08x ra=%08x\n", ret, iop->saved_pc, iop->r[31]);
    // Set v0 (return register) to ret
    iop->r[2] = ret;

    // // Emulate jal ra
    // iop->pc = iop->r[31];
    iop->next_pc = iop->r[31];
}

}
