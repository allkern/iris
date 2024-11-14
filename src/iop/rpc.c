#include <stdint.h>
#include <stdio.h>

#include "rpc.h"

const char* rpc_get_server(uint32_t id) {
    switch (id) {
        case 0x80000001: return "FILEIO";
        case 0x80000003: return "IOP heap alloc (FILEIO)";
        case 0x80000006: return "LOADFILE";
        case 0x80000100: return "PADMAN";
        case 0x80000101: return "PADMAN ext";
        case 0x80000400: return "MCSERV";
        case 0x80000592: return "CDVD Init (CDVDFSV)";
        case 0x80000593: return "CDVD S commands (CDVDFSV)";
        case 0x80000595: return "CDVD N commands (CDVDFSV)";
        case 0x80000597: return "CDVD SearchFile (CDVDFSV)";
        case 0x8000059A: return "CDVD Disk Ready (CDVDFSV)";
        case 0x80000701: return "LIBSD Remote (SDRDRV)";
        case 0x80000901: return "MTAP Port Open (MTAPMAN)";
        case 0x80000902: return "MTAP Port Close (MTAPMAN)";
        case 0x80000903: return "MTAP Get Connection (MTAPMAN)";
        case 0x80000904: return "MTAP Unknown (MTAPMAN)";
        case 0x80000905: return "MTAP Unknown (MTAPMAN)";
        case 0x80001400: return "EYETOY";
    }

    return "<unknown>";
}

char* rpc_decode_packet(char* buf, uint32_t* data) {
    char* ptr = buf;

    struct sif_cmd_header* hdr = (struct sif_cmd_header*)data;

    ptr += sprintf(ptr, "rpc: ");

    switch (hdr->cid) {
        case 0x80000000: ptr += sprintf(ptr, "ChangeSaddr: "); break;
        case 0x80000001: ptr += sprintf(ptr, "SetSreg: "); break;
        case 0x80000002: {
            struct sif_init_pkt* init = (struct sif_init_pkt*)data;

            ptr += sprintf(ptr, "Init (opt=%d)", init->header.opt);
        } break;
        case 0x80000003: ptr += sprintf(ptr, "Reboot: "); break;
        case 0x80000008: ptr += sprintf(ptr, "RequestEnd: "); break;
        case 0x80000009: {
            struct sif_rpc_bind_pkt* bind = (struct sif_rpc_bind_pkt*)data;

            const char* server = rpc_get_server(bind->sid);

            ptr += sprintf(ptr, "Bind (%s)", server); break;
        } break;
        case 0x8000000A: ptr += sprintf(ptr, "Call: "); break;
        case 0x8000000C: ptr += sprintf(ptr, "GetOtherData: "); break;    
    }

    *ptr++ = '\0';

    return buf;
}

// 80000000h Change SADDR
// 80000001h Set SREG
// 80000002h SIFCMD Init
// 80000003h Reboot IOP
// 80000008h Request End
// 80000009h Bind
// 8000000Ah Call
// 8000000Ch Get other data

// 0x80000001 "FILEIO"
// 0x80000003 "IOP heap alloc (FILEIO)"
// 0x80000006 "LOADFILE"
// 0x80000100 "PADMAN"
// 0x80000101 "PADMAN ext"
// 0x80000400 "MCSERV"
// 0x80000592 "CDVD Init (CDVDFSV)"
// 0x80000593 "CDVD S commands (CDVDFSV)"
// 0x80000595 "CDVD N commands (CDVDFSV)"
// 0x80000597 "CDVD SearchFile (CDVDFSV)"
// 0x8000059A "CDVD Disk Ready (CDVDFSV)"
// 0x80000701 "LIBSD Remote (SDRDRV)"
// 0x80000901 "MTAP Port Open (MTAPMAN)"
// 0x80000902 "MTAP Port Close (MTAPMAN)"
// 0x80000903 "MTAP Get Connection (MTAPMAN)"
// 0x80000904 "MTAP Unknown (MTAPMAN)"
// 0x80000905 "MTAP Unknown (MTAPMAN)"
// 0x80001400 "EYETOY"