// The VU core reaches the GIF on XGKICK. The differential test never connects a
// GIF, and a GS write is not architectural state we compare, so swallow it.

#include "u128.h"

namespace iris::gif {

struct Gif;

void fifo_write(Gif*, uint128_t, int) {}

}
