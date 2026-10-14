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
#include "logger.hpp"

namespace iris::iop::rpc {

struct __attribute__((packed)) sif_cmd_header {
    unsigned int psize : 8;
    unsigned int dsize : 24;
    unsigned int dest;
    unsigned int cid;
    unsigned int opt;
};

struct sif_rpc_packet_header {
    struct sif_cmd_header sif_cmd;
    unsigned int rec_id;
    unsigned int pkt_addr;
    unsigned int id;
};

struct sif_rpc_client_data {
    struct sif_rpc_packet_header hdr;
    unsigned int command;
    unsigned int buff, *cbuff;
    unsigned int end_function;
    unsigned int end_param;
    unsigned int server;
};

struct sif_rpc_server_data {
    unsigned int sid;
    unsigned int func;
    unsigned int buff;
    unsigned int size;
    unsigned int cfunc;
    unsigned int cbuff;
    unsigned int size2;
    unsigned int client;
    unsigned int pkt_addr;
    unsigned int number;
    unsigned int recv;
    unsigned int rsize;
    unsigned int rmode;
    unsigned int rid;
    unsigned int link;
    unsigned int next;
    unsigned int base;
};

struct sif_rpc_data_queue {
    unsigned int thread_id, active;
    unsigned int link, start, end;
    unsigned int next;
};

struct sif_dma_transfer {
    unsigned int src, dest;
    unsigned int size;
    unsigned int attr;
};

struct sif_saddr_pkt {
    struct sif_cmd_header header;
    unsigned int buf;
};

struct sif_cmd_set_sreg_pkt {
    struct sif_cmd_header header;
    unsigned int index;
    unsigned int value;
};

struct sif_init_pkt {
    struct sif_cmd_header header;
    unsigned int buff;
};

struct sif_iop_reset_pkt {
    struct sif_cmd_header header;
    unsigned int arglen;
    unsigned int mode;
    char arg[80];
};

struct sif_rpc_rend_pkt {
    struct sif_cmd_header sifcmd;
    unsigned int rec_id;
    unsigned int pkt_addr;
    unsigned int id;
    unsigned int client;
    unsigned int cid;
    unsigned int server;
    unsigned int buff;
    unsigned int cbuff;
};

struct sif_rpc_bind_pkt {
    struct sif_cmd_header sifcmd;
    unsigned int rec_id;
    unsigned int pkt_addr;
    unsigned int id;
    unsigned int client;
    unsigned int sid;
};

struct sif_rpc_call_pkt {
    struct sif_cmd_header sifcmd;
    unsigned int rec_id; 
    unsigned int pkt_addr;
    unsigned int id;
    unsigned int client;
    unsigned int number;
    unsigned int send_size;
    unsigned int receive;
    unsigned int recv_size;
    unsigned int rmode;
    unsigned int server;
};

struct sif_rpc_other_data_pkt {
    struct sif_cmd_header sifcmd;
    unsigned int rec_id;
    unsigned int pkt_addr;
    unsigned int id;
    unsigned int receive;
    unsigned int src;
    unsigned int dest;
    unsigned int size;
};

char* decode_packet(logger::Logger* logger, size_t logger_id, iop::Iop* iop, char* buf, uint32_t* data);

}
