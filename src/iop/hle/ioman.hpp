#pragma once

#include "../iop.hpp"
#include "../iop_export.hpp"
#include "logger.hpp"

namespace iris::iop::hle::ioman {

int open(iop::Iop* iop, int iomanx);
int close(iop::Iop* iop, int iomanx);
int read(iop::Iop* iop, int iomanx);
int write(iop::Iop* iop, int iomanx);
int lseek(iop::Iop* iop, int iomanx);
int ioctl(iop::Iop* iop, int iomanx);
int remove(iop::Iop* iop, int iomanx);
int mkdir(iop::Iop* iop, int iomanx);
int rmdir(iop::Iop* iop, int iomanx);
int dopen(iop::Iop* iop, int iomanx);
int dclose(iop::Iop* iop, int iomanx);
int dread(iop::Iop* iop, int iomanx);
int getstat(iop::Iop* iop, int iomanx);
int chstat(iop::Iop* iop, int iomanx);
int format(iop::Iop* iop, int iomanx);
int adddrv(iop::Iop* iop, int iomanx);
int deldrv(iop::Iop* iop, int iomanx);
int stdioinit(iop::Iop* iop, int iomanx);
int rename(iop::Iop* iop, int iomanx);
int chdir(iop::Iop* iop, int iomanx);
int sync(iop::Iop* iop, int iomanx);
int mount(iop::Iop* iop, int iomanx);
int umount(iop::Iop* iop, int iomanx);
int lseek64(iop::Iop* iop, int iomanx);
int devctl(iop::Iop* iop, int iomanx);
int symlink(iop::Iop* iop, int iomanx);
int readlink(iop::Iop* iop, int iomanx);
int ioctl2(iop::Iop* iop, int iomanx);

void reset();
void map_device(const char* device, const char* host_path);
void unmap_device(const char* device);
void clear_devices();

}
