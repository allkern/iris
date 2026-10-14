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

#include "ps1_mcd.hpp"

namespace iris::dev::ps1_mcd {

void flush_block(Ps1Mcd* mcd, int addr) {
    fseek(mcd->file, addr, SEEK_SET);
    fwrite(&mcd->buf[addr], 1, 128, mcd->file);
}

/*
  Send Reply Comment
  81h  N/A   Memory card address
  52h  FLAG  Send Read Command (ASCII "R"), Receive FLAG Byte
  00h  5Ah   Receive Memory Card ID1
  00h  5Dh   Receive Memory Card ID2
  MSB  (00h) Send Address MSB  ;\sector number (0..3FFh)
  LSB  (pre) Send Address LSB  ;/
  00h  5Ch   Receive Command Acknowledge 1  ;<-- late /ACK after this byte-pair
  00h  5Dh   Receive Command Acknowledge 2
  00h  MSB   Receive Confirmed Address MSB
  00h  LSB   Receive Confirmed Address LSB
  00h  ...   Receive Data Sector (128 bytes)
  00h  CHK   Receive Checksum (MSB xor LSB xor Data bytes)
  00h  47h   Receive Memory End Byte (should be always 47h="G"=Good for Read)
*/
void cmd_read(sio2::Sio2* sio2, Ps1Mcd* mcd) {
    uint16_t msb = queue::at(sio2->in, 4);
    uint16_t lsb = queue::at(sio2->in, 5);

    uint16_t addr = ((msb << 8) | lsb) * 128;

    iris_debug(mcd, "cmd_read({:04x})", (msb << 8) | lsb);

    queue::push(sio2->out, 0xff);
    queue::push(sio2->out, mcd->flag);
    queue::push(sio2->out, 0x5a);
    queue::push(sio2->out, 0x5d);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, msb); // (pre)
    queue::push(sio2->out, 0x5c);
    queue::push(sio2->out, 0x5d);
    queue::push(sio2->out, msb);
    queue::push(sio2->out, lsb);

    uint8_t checksum = msb ^ lsb;

    for (int i = 0; i < 128; i++) {
        checksum ^= mcd->buf[addr + i];

        queue::push(sio2->out, mcd->buf[addr + i]);
    }

    queue::push(sio2->out, checksum);
    queue::push(sio2->out, 'G'); // 0x47
}

/*
  Send Reply Comment
  81h  N/A   Memory card address
  57h  FLAG  Send Write Command (ASCII "W"), Receive FLAG Byte
  00h  5Ah   Receive Memory Card ID1
  00h  5Dh   Receive Memory Card ID2
  MSB  (00h) Send Address MSB  ;\sector number (0..3FFh)
  LSB  (pre) Send Address LSB  ;/
  ...  (pre) Send Data Sector (128 bytes)
  CHK  (pre) Send Checksum (MSB xor LSB xor Data bytes)
  00h  5Ch   Receive Command Acknowledge 1
  00h  5Dh   Receive Command Acknowledge 2
  00h  4xh   Receive Memory End Byte (47h=Good, 4Eh=BadChecksum, FFh=BadSector)
*/
void cmd_write(sio2::Sio2* sio2, Ps1Mcd* mcd) {
    uint16_t msb = queue::at(sio2->in, 4);
    uint16_t lsb = queue::at(sio2->in, 5);

    uint16_t addr = ((msb << 8) | lsb) * 128;

    iris_debug(mcd, "cmd_write({:04x})", (msb << 8) | lsb);

    queue::push(sio2->out, 0xff);
    queue::push(sio2->out, mcd->flag);
    queue::push(sio2->out, 0x5a);
    queue::push(sio2->out, 0x5d);
    queue::push(sio2->out, 0x00);
    queue::push(sio2->out, msb); // (pre)

    for (int i = 0; i < 128; i++) {
        mcd->buf[addr+i] = queue::at(sio2->in, 6+i);

        queue::push(sio2->out, queue::at(sio2->in, 5+i)); // (pre)
    }

    flush_block(mcd, addr);

    queue::push(sio2->out, queue::at(sio2->in, 133)); // (pre)
    queue::push(sio2->out, 0x5c);
    queue::push(sio2->out, 0x5d);
    queue::push(sio2->out, 'G');

    // Reset directory read flag
    mcd->flag &= ~0x08;
}
void cmd_get_id(sio2::Sio2* sio2, Ps1Mcd* mcd) {
    iris_fatal_error(mcd, "cmd_get_id");
}
void cmd_invalid(sio2::Sio2* sio2, Ps1Mcd* mcd) {
    iris_debug(mcd, "cmd_invalid({:02x})", queue::size(sio2->in));

    queue::push(sio2->out, 0xff);
    queue::push(sio2->out, mcd->flag);

    for (int i = 2; i < queue::size(sio2->in); i++)
        queue::push(sio2->out, 0xff);
}
void cmd_detect_pocketstation(sio2::Sio2* sio2, Ps1Mcd* mcd) {
    iris_debug(mcd, "cmd_detect_pocketstation");

    queue::push(sio2->out, 0xff);
    queue::push(sio2->out, mcd->flag);
    queue::push(sio2->out, 0xff);
    queue::push(sio2->out, 0xff);
    queue::push(sio2->out, 0xff);
    queue::push(sio2->out, 0xff);
}

void handle_command(sio2::Sio2* sio2, void* udata, int cmd) {
    Ps1Mcd* mcd = (Ps1Mcd*)udata;

    switch (cmd) {
        case 0x52: cmd_read(sio2, mcd); return;
        case 0x53: cmd_get_id(sio2, mcd); return;
        case 0x57: cmd_write(sio2, mcd); return;
        case 0x58: {
            if (mcd->type == 0)
                break;

            cmd_detect_pocketstation(sio2, mcd); return;
        }
    }

    sio2->recv1 |= 0x2000;

    cmd_invalid(sio2, mcd);
}

void set_type(Ps1Mcd* mcd, int type) {
    mcd->type = type;
}

Ps1Mcd* attach(logger::Logger* logger, sio2::Sio2* sio2, int port, const char* path) {
    FILE* file = fopen(path, "r+b");

    if (!file)
        return nullptr;

    Ps1Mcd* mcd = new Ps1Mcd();

    mcd->logger = logger;
    mcd->logger_id = logger::register_source(logger, "ps1_mcd");
    sio2::Device dev;

    mcd->file = file;
    mcd->flag = 0x08;

    fread(mcd->buf, 1, SIZE, file);

    iris_debug(mcd, "Memory card at \'{}\' initialized.", path);

    dev.detach = detach;
    dev.reset = reset;
    dev.handle_command = handle_command;
    dev.udata = mcd;
    dev.id = sio2::DEV_MCD;

    sio2::attach_device(sio2, dev, port);

    return mcd;
}

void reset(void* udata) {
    Ps1Mcd* mcd = (Ps1Mcd*)udata;

    mcd->flag = 0x08;
}

void detach(void* udata) {
    Ps1Mcd* mcd = (Ps1Mcd*)udata;

    // Flush buffer back to file
    fseek(mcd->file, 0, SEEK_SET);
    fwrite(mcd->buf, 1, SIZE, mcd->file);

    fclose(mcd->file);
    delete mcd;
}

}
