#include "sifcmd.hpp"

#include "../iop_def.hpp"
#include "../iop_export.hpp"

namespace iris::iop::hle::sifcmd {

inline constexpr auto EXPORT_TABLE = 0x14;
inline constexpr auto ORDINAL_REGISTER_RPC = 17;

static const char* get_service_name(uint32_t id) {
    switch (id) {
        case 0x76500001: return "ACMEME";
        case 0x76500002: return "ACCDVDE";
        case 0x76500003: return "ACJV";
        case 0x76500004: return "ACNSWE";
        case 0x00006502: return "UARTMAN";
        case 0x0000ffff: return "ACRTC";
        case 0x00001000: return "SSM";
        case 0x80000001: return "FILEIO";
        case 0x80000003: return "FILEIO heap";
        case 0x80000006: return "LOADFILE";
        case 0x80000100: return "PADMAN";
        case 0x80000101: return "PADMAN extension";
        case 0x80000400: return "MCSERV";
        case 0x80000592: return "CDVDFSV init";
        case 0x80000593: return "CDVDFSV S commands";
        case 0x80000595: return "CDVDFSV N commands";
        case 0x80000597: return "CDVDFSV SearchFile";
        case 0x8000059a: return "CDVDFSV disc ready";
        case 0x80000701: return "SDRDRV";
        case 0x80000901: return "MTAPMAN port open";
        case 0x80000902: return "MTAPMAN port close";
        case 0x80000903: return "MTAPMAN get connection";
        case 0x80000904: return "MTAPMAN";
        case 0x80000905: return "MTAPMAN";
        case 0x80001400: return "EYETOY";
    }

    return "unknown";
}

void register_exports(iop::Iop* iop, uint32_t library) {
    iop->sifcmd_register_rpc = iop::read32(iop, library + EXPORT_TABLE + (ORDINAL_REGISTER_RPC * 4));
}

int hook_for_target(iop::Iop* iop, uint32_t target) {
    if (!target || target != iop->sifcmd_register_rpc)
        return HOOK_NONE;

    return HOOK_REGISTER_RPC;
}

int run_hook(iop::Iop* iop, int hook) {
    if (hook != HOOK_REGISTER_RPC)
        return 0;

    uint32_t id = iop->r[5];

    iris_debug(iop, "sceSifRegisterRpc {:08x} ({})", id, get_service_name(id));

    return 0;
}

}
