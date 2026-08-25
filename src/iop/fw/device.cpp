#include "device.hpp"

namespace iris::fw::device {

int read(Device* dev, uint64_t offset, uint8_t* buf, int len) {
    if (!dev->connected || !dev->ops || !dev->ops->read)
        return RESP_ADDRESS_ERROR;

    return dev->ops->read(dev, offset, buf, len);
}

int write(Device* dev, uint64_t offset, const uint8_t* buf, int len) {
    if (!dev->connected || !dev->ops || !dev->ops->write)
        return RESP_ADDRESS_ERROR;

    return dev->ops->write(dev, offset, buf, len);
}

void reset(Device* dev) {
    if (dev->ops && dev->ops->reset)
        dev->ops->reset(dev);
}

void free(Device* dev) {
    if (dev->ops && dev->ops->free)
        dev->ops->free(dev);

    dev->ops = NULL;
    dev->priv = NULL;
    dev->connected = 0;
    dev->node_id = 0;
    dev->guid = 0;
}

}
