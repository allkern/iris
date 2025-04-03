#ifndef IOMAN_H
#define IOMAN_H

#include "../iop.h"
#include "../iop_export.h"

#define IOMAN_DRIVE_UNKNOWN 0

// BIOS ROM
// DVD ROM
// CDVD drive
// Host machine
// Memory card slot 1
// Memory card slot 2
// USB drive
#define IOMAN_DRIVE_ROM0      1
#define IOMAN_DRIVE_ROM1      2
#define IOMAN_DRIVE_CDROM0    3
#define IOMAN_DRIVE_HOST      4
#define IOMAN_DRIVE_MC0       5
#define IOMAN_DRIVE_MC1       6
#define IOMAN_DRIVE_MASS      7

extern "C" {
int ioman_open(struct iop_state* iop);
int ioman_close(struct iop_state* iop);
int ioman_read(struct iop_state* iop);
int ioman_write(struct iop_state* iop);
int ioman_lseek(struct iop_state* iop);
int ioman_ioctl(struct iop_state* iop);
int ioman_remove(struct iop_state* iop);
int ioman_mkdir(struct iop_state* iop);
int ioman_rmdir(struct iop_state* iop);
int ioman_dopen(struct iop_state* iop);
int ioman_dclose(struct iop_state* iop);
int ioman_dread(struct iop_state* iop);
int ioman_getstat(struct iop_state* iop);
int ioman_chstat(struct iop_state* iop);
int ioman_format(struct iop_state* iop);
int ioman_adddrv(struct iop_state* iop);
int ioman_deldrv(struct iop_state* iop);
int ioman_stdioinit(struct iop_state* iop);
int ioman_rename(struct iop_state* iop);
int ioman_chdir(struct iop_state* iop);
int ioman_sync(struct iop_state* iop);
int ioman_mount(struct iop_state* iop);
int ioman_umount(struct iop_state* iop);
int ioman_lseek64(struct iop_state* iop);
int ioman_devctl(struct iop_state* iop);
int ioman_symlink(struct iop_state* iop);
int ioman_readlink(struct iop_state* iop);
int ioman_ioctl2(struct iop_state* iop);
}

#endif