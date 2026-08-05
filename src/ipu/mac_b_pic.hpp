#pragma once
#include "vlc_table.hpp"

namespace iris::ipu {

class Macroblock_BPic : public VLC_Table
{
    private:
        static VLC_Entry table[];
        static unsigned int index_table[];

        constexpr static int SIZE = 11;
    public:
        Macroblock_BPic();
};

}
