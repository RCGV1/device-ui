#pragma once
#include "graphics/driver/DisplayDriverConfig.h"

class DeviceGUI;
class DisplayDriver;

class ViewFactory
{
  public:
    static DeviceGUI *create(void);
    static DeviceGUI *create(const DisplayDriverConfig &cfg);
#ifdef UNIT_TEST
    static DeviceGUI *createForTesting(const DisplayDriverConfig &cfg, DisplayDriver *driver);
#endif

  protected:
    ViewFactory(void);
};
