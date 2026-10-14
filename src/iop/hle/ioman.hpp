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
