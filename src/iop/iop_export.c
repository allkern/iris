#include "iop_export.h"

static inline uint32_t irx_import_table_addr(struct iop_state* iop, int entry) {
    uint32_t i = entry - 0x18;

    while ((entry - i) < 0x2000) {
        if (iop_read32(iop, i) == 0x41e00000)
            return i;

        i -= 4;
    }

    return 0;
}

static inline int iop_get_module(struct iop_state* iop, int itable) {
    char buf[9];

    for (int i = 0; i < 8; i++)
        buf[i] = iop_read8(iop, itable + 12 + i);

    if (!strncmp(buf, "ioman", 8)) return MODULE_IOMAN;
    if (!strncmp(buf, "iomanx", 8)) return MODULE_IOMANX;
    if (!strncmp(buf, "loadcore", 8)) return MODULE_LOADCORE;
    if (!strncmp(buf, "sysmem", 8)) return MODULE_SYSMEM;

    return MODULE_UNKNOWN;
}

static inline int iop_delegate_ioman(struct iop_state* iop, int slot, int iomanx) {
    switch (slot & 0xffff) {
        case IOMAN_OPEN: return ioman_open(iop, iomanx);
        case IOMAN_CLOSE: return ioman_close(iop, iomanx);
        case IOMAN_READ: return ioman_read(iop, iomanx);
        case IOMAN_WRITE: return ioman_write(iop, iomanx);
        case IOMAN_LSEEK: return ioman_lseek(iop, iomanx);
        case IOMAN_IOCTL: return ioman_ioctl(iop, iomanx);
        case IOMAN_REMOVE: return ioman_remove(iop, iomanx);
        case IOMAN_MKDIR: return ioman_mkdir(iop, iomanx);
        case IOMAN_RMDIR: return ioman_rmdir(iop, iomanx);
        case IOMAN_DOPEN: return ioman_dopen(iop, iomanx);
        case IOMAN_DCLOSE: return ioman_dclose(iop, iomanx);
        case IOMAN_DREAD: return ioman_dread(iop, iomanx);
        case IOMAN_GETSTAT: return ioman_getstat(iop, iomanx);
        case IOMAN_CHSTAT: return ioman_chstat(iop, iomanx);
        case IOMAN_FORMAT: return ioman_format(iop, iomanx);
        case IOMAN_ADDDRV: return ioman_adddrv(iop, iomanx);
        case IOMAN_DELDRV: return ioman_deldrv(iop, iomanx);
        case IOMAN_STDIOINIT: return ioman_stdioinit(iop, iomanx);
        case IOMAN_RENAME: return ioman_rename(iop, iomanx);
        case IOMAN_CHDIR: return ioman_chdir(iop, iomanx);
        case IOMAN_SYNC: return ioman_sync(iop, iomanx);
        case IOMAN_MOUNT: return ioman_mount(iop, iomanx);
        case IOMAN_UMOUNT: return ioman_umount(iop, iomanx);
        case IOMAN_LSEEK64: return ioman_lseek64(iop, iomanx);
        case IOMAN_DEVCTL: return ioman_devctl(iop, iomanx);
        case IOMAN_SYMLINK: return ioman_symlink(iop, iomanx);
        case IOMAN_READLINK: return ioman_readlink(iop, iomanx);
        case IOMAN_IOCTL2: return ioman_ioctl2(iop, iomanx);
    }

    return 0;
}

static inline int iop_delegate_loadcore(struct iop_state* iop, int slot) {
    switch (slot & 0xffff) {
        case LOADCORE_REG_LIB_ENT: return loadcore_reg_lib_ent(iop);
    }

    return 0;
}

static inline int iop_delegate_sysmem(struct iop_state* iop, int slot) {
    switch (slot & 0xffff) {
        // SYSMEM kprintf
        case 14: return sysmem_kprintf(iop);
    }

    return 0;
}

int iop_test_module_hooks(struct iop_state* iop) {
    uint32_t slot = iop_read32(iop, iop->pc);

    if ((slot >> 16) != 0x2400)
        return 0;

    uint32_t itable = irx_import_table_addr(iop, iop->pc);

    if (!itable)
        return 0;

    int module = iop_get_module(iop, itable);

    if (!module)
        return 0;

    switch (module) {
        case MODULE_IOMAN: return iop_delegate_ioman(iop, slot, 0);
        case MODULE_IOMANX: return iop_delegate_ioman(iop, slot, 1);
        case MODULE_LOADCORE: return iop_delegate_loadcore(iop, slot);
        case MODULE_SYSMEM: return iop_delegate_sysmem(iop, slot);
    }

    return 0;
}

void iop_return(struct iop_state* iop, int ret) {
    // Set v0 (return register) to ret
    iop->r[2] = ret;

    // Emulate jal ra
    iop->pc = iop->r[31];
    iop->next_pc = iop->pc + 4;
}
