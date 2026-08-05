#include "device.hpp"

namespace iris::usb::device {

int transfer(Device* dev, int pid, int ep, uint8_t* buf, int len) {
    return dev->ops->transfer(dev, pid, ep, buf, len);
}

void reset(Device* dev) {
    dev->address = 0;
    dev->pending_address = 0;
    dev->configuration = 0;

    if (dev->ops && dev->ops->reset)
        dev->ops->reset(dev);
}

void free(Device* dev) {
    if (dev->ops && dev->ops->free)
        dev->ops->free(dev);

    dev->ops = NULL;
    dev->priv = NULL;
    dev->connected = 0;
}

}
