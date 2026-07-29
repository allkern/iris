#ifndef IOMAN_H
#define IOMAN_H

#include "../iop.h"
#include "../iop_export.h"

#ifdef __cplusplus
extern "C" {
#endif

int ioman_open(struct iop_state* iop, int iomanx);
int ioman_close(struct iop_state* iop, int iomanx);
int ioman_read(struct iop_state* iop, int iomanx);
int ioman_write(struct iop_state* iop, int iomanx);
int ioman_lseek(struct iop_state* iop, int iomanx);
int ioman_ioctl(struct iop_state* iop, int iomanx);
int ioman_remove(struct iop_state* iop, int iomanx);
int ioman_mkdir(struct iop_state* iop, int iomanx);
int ioman_rmdir(struct iop_state* iop, int iomanx);
int ioman_dopen(struct iop_state* iop, int iomanx);
int ioman_dclose(struct iop_state* iop, int iomanx);
int ioman_dread(struct iop_state* iop, int iomanx);
int ioman_getstat(struct iop_state* iop, int iomanx);
int ioman_chstat(struct iop_state* iop, int iomanx);
int ioman_format(struct iop_state* iop, int iomanx);
int ioman_adddrv(struct iop_state* iop, int iomanx);
int ioman_deldrv(struct iop_state* iop, int iomanx);
int ioman_stdioinit(struct iop_state* iop, int iomanx);
int ioman_rename(struct iop_state* iop, int iomanx);
int ioman_chdir(struct iop_state* iop, int iomanx);
int ioman_sync(struct iop_state* iop, int iomanx);
int ioman_mount(struct iop_state* iop, int iomanx);
int ioman_umount(struct iop_state* iop, int iomanx);
int ioman_lseek64(struct iop_state* iop, int iomanx);
int ioman_devctl(struct iop_state* iop, int iomanx);
int ioman_symlink(struct iop_state* iop, int iomanx);
int ioman_readlink(struct iop_state* iop, int iomanx);
int ioman_ioctl2(struct iop_state* iop, int iomanx);

void ioman_hle_reset(void);
void ioman_hle_map_device(const char* device, const char* host_path);
void ioman_hle_unmap_device(const char* device);
void ioman_hle_clear_devices(void);

#ifdef __cplusplus
}
#endif

#endif