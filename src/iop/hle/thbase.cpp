#include "thbase.hpp"

#include "../iop_def.hpp"
#include "../iop_export.hpp"

namespace iris::iop::hle::thbase {

inline constexpr auto EXPORT_TABLE = 0x14;
inline constexpr auto ORDINAL_CREATE_THREAD = 4;
inline constexpr auto ORDINAL_START_THREAD = 6;

// Note: The arcade security daemon watches the card from the lowest priority the
//       kernel allows, contending with MCMAN for the card lock often enough that the
//       game's dongle reads fail. Real hardware schedules finely enough to hide it.
inline constexpr auto SUPPRESSED_PRIORITY = 126;
inline constexpr auto PRIORITY_OFFSET = 16;

void register_exports(iop::Iop* iop, uint32_t library) {
    uint32_t create = iop::read32(iop, library + EXPORT_TABLE + (ORDINAL_CREATE_THREAD * 4));
    uint32_t start = iop::read32(iop, library + EXPORT_TABLE + (ORDINAL_START_THREAD * 4));

    iop->thbase_create_thread = create;
    iop->thbase_start_thread = start;
}

static void create_thread(iop::Iop* iop) {
    uint32_t priority = iop::read32(iop, iop->r[4] + PRIORITY_OFFSET);

    if (priority >= SUPPRESSED_PRIORITY) {
        iop->suppress_next_thread_start = 1;
    }
}

static int start_thread(iop::Iop* iop) {
    if (!iop->suppress_next_thread_start)
        return 0;

    iop->suppress_next_thread_start = 0;

    iop::set_return(iop, 0);

    return 1;
}

int hook_for_target(iop::Iop* iop, uint32_t target) {
    if (!iop->suppress_daemon || !target)
        return HOOK_NONE;

    if (target == iop->thbase_create_thread)
        return HOOK_CREATE_THREAD;

    if (target == iop->thbase_start_thread)
        return HOOK_START_THREAD;

    return HOOK_NONE;
}

int run_hook(iop::Iop* iop, int hook) {
    if (!iop->suppress_daemon)
        return 0;

    if (hook == HOOK_CREATE_THREAD) {
        create_thread(iop);

        return 0;
    }

    return start_thread(iop);
}

}
