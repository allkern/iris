#pragma once
#include <cstdint>
#include <queue>
#include "ipu_fifo.hpp"

namespace iris::ipu {

struct VLC_Entry
{
    uint32_t key;
    uint32_t value;
    uint8_t bits;
};

class VLC_Table
{
    private:
        VLC_Entry* table;
        int table_size, max_bits;
        unsigned int* index_table;
    protected:
        VLC_Table(VLC_Entry* table, int table_size, int max_bits, unsigned int* index_table);
    public:
        bool peek_symbol(IPU_FIFO& FIFO, VLC_Entry& entry, bool* invalid = nullptr);
        bool get_symbol(IPU_FIFO& FIFO, uint32_t& result, bool* invalid = nullptr);
};

}
