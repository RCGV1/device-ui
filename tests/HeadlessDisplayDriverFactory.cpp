#include "graphics/driver/DisplayDriverFactory.h"

#ifdef DEVICE_UI_HEADLESS_TEST
DisplayDriverFactory::DisplayDriverFactory() {}

DisplayDriver *DisplayDriverFactory::create(uint16_t, uint16_t)
{
    return nullptr;
}

DisplayDriver *DisplayDriverFactory::create(const DisplayDriverConfig &)
{
    return nullptr;
}
#endif
