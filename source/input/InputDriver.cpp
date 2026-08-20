#include "input/InputDriver.h"

InputDriver *InputDriver::driver = nullptr;
lv_indev_t *InputDriver::keyboard = nullptr;
lv_indev_t *InputDriver::pointer = nullptr;
lv_indev_t *InputDriver::encoder = nullptr;
lv_indev_t *InputDriver::button = nullptr;
lv_group_t *InputDriver::inputGroup = nullptr;

InputDriver *InputDriver::instance(void)
{
    if (!driver)
        driver = new InputDriver;
    return driver;
}

InputDriver::~InputDriver(void)
{
    if (keyboard)
        releaseKeyboardDevice();
    if (pointer)
        releasePointerDevice();
}

#ifdef DEVICE_UI_HEADLESS_TEST
void InputDriver::configureDevicesForTesting(bool withKeyboard, bool withEncoder, bool withPointer)
{
    if (keyboard) {
        lv_indev_delete(keyboard);
        keyboard = nullptr;
    }
    if (encoder) {
        lv_indev_delete(encoder);
        encoder = nullptr;
    }
    if (pointer) {
        lv_indev_delete(pointer);
        pointer = nullptr;
    }

    if (withKeyboard) {
        keyboard = lv_indev_create();
        lv_indev_set_type(keyboard, LV_INDEV_TYPE_KEYPAD);
    }
    if (withEncoder) {
        encoder = lv_indev_create();
        lv_indev_set_type(encoder, LV_INDEV_TYPE_ENCODER);
    }
    if (withPointer) {
        pointer = lv_indev_create();
        lv_indev_set_type(pointer, LV_INDEV_TYPE_POINTER);
    }
}
#endif
