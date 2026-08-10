#pragma once

namespace iris {

struct Instance;

namespace settings {
    bool init(Instance* iris);
    void save(Instance* iris);
    void close(Instance* iris);
    void apply_device_maps(Instance* iris);
}

}
