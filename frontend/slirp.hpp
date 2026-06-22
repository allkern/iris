#pragma once

struct ps2_smap;

namespace iris::slirp {

bool start(struct ps2_smap* smap);
void stop();
void pump(struct ps2_smap* smap);

}
