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

#include <new>
#include "nand.hpp"

#include <cctype>

namespace iris::s14x::nand {

Nand* create(logger::Logger* logger) {
    Nand* nand = new Nand();

    nand->logger = logger;
    nand->logger_id = logger::register_source(logger, "nand");

    nand->buf = new uint8_t[PAGE_SIZE_ECC]();
    nand->state = STATE_READ_BYTE0;

    return nand;
}

int load(Nand* nand, const char* path) {
    if (nand->file) {
        fclose(nand->file);
    }

    nand->file = fopen(path, "rb");

    if (!nand->file) {
        return 0;
    }

    return 1;
}

void handle_offset_write(Nand* nand, uint8_t data) {
    switch (nand->state) {
        case STATE_READ_BYTE0: {
            nand->byte_offset = (nand->byte_offset & 0xff00) | data;
            nand->state++;
        } break;
        case STATE_READ_BYTE1: {
            nand->byte_offset = (nand->byte_offset & 0x00ff) | (data << 8);
            nand->state++;
        } break;
        case STATE_READ_PAGE0: {
            nand->page_offset = (nand->page_offset & 0xffff00) | data;
            nand->state++;
        } break;
        case STATE_READ_PAGE1: {
            nand->page_offset = (nand->page_offset & 0xff00ff) | (data << 8);
            nand->state++;
        } break;
        case STATE_READ_PAGE2: {
            nand->page_offset = (nand->page_offset & 0x00ffff) | ((uint32_t)data << 16);
            nand->state = STATE_READ_BYTE0;
        } break;
    }
}

void handle_cmd_read(Nand* nand) {
    nand->size = PAGE_SIZE_ECC;
    nand->index = nand->byte_offset;

    if (!nand->file) {
        memset(nand->buf, 0, PAGE_SIZE_ECC);

        return;
    }

    fseek(nand->file, nand->page_offset * PAGE_SIZE_ECC, SEEK_SET);
    fread(nand->buf, 1, PAGE_SIZE_ECC, nand->file);
}

uint64_t read8(Nand* nand, uint32_t addr) {
    addr -= 0x14000000;

    switch (addr) {
        case REG_OUTBYTE: {
            int index = nand->index++ % nand->size;

            return nand->buf[index];
        } break;

        default: {
            // iris_debug(nand, "Nand: Unhandled register read {:02x}", addr);
        } break;
    }

    return 0;
}

void write8(Nand* nand, uint32_t addr, uint64_t data) {
    addr -= 0x14000000;

    switch (addr) {
        case REG_CMD: {
            nand->cmd = data;

            switch (nand->cmd) {
                case 0: {
                    // NOP
                } break;

                case CMD_READ: {
                    handle_cmd_read(nand);
                } break;

                default: {
                    iris_fatal_error(nand, "Nand: Unhandled command {:02x}", nand->cmd);
                } break;
            }
        } break;

        case REG_OFFSET: {
            handle_offset_write(nand, data);
        } break;

        default: {
            // iris_debug(nand, "Nand: Unhandled register write {:02x} = {:02x}", addr, (uint8_t)data);
        } break;
    }
}

void destroy(Nand* nand) {
    if (nand->file) {
        fclose(nand->file);
    }

    delete[] nand->buf;

    delete nand;
}

}
