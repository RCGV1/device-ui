#if defined(DEVICE_UI_HEADLESS_TEST) || defined(DEVICE_UI_X11_SIMULATOR)

#include "linux/LinuxHardwareI2C.h"

namespace arduino
{
LinuxHardwareI2C Wire;

void LinuxHardwareI2C::begin() {}

void LinuxHardwareI2C::begin(const char *) {}

void LinuxHardwareI2C::end() {}

void LinuxHardwareI2C::beginTransmission(uint8_t) {}

uint8_t LinuxHardwareI2C::endTransmission(bool)
{
    return I2cAddrNAK;
}

int LinuxHardwareI2C::writeQuick(uint8_t)
{
    return -1;
}

size_t LinuxHardwareI2C::write(uint8_t)
{
    return 0;
}

size_t LinuxHardwareI2C::write(const uint8_t *, size_t)
{
    return 0;
}

uint8_t LinuxHardwareI2C::requestFrom(uint8_t, size_t, bool)
{
    return 0;
}

int LinuxHardwareI2C::available()
{
    return 0;
}

int LinuxHardwareI2C::read()
{
    return -1;
}

size_t LinuxHardwareI2C::readBytes(char *, size_t)
{
    return 0;
}
} // namespace arduino

#endif
