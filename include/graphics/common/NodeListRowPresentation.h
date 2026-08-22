#pragma once

#include "lvgl.h"

#include <cstdint>
#include <tuple>

namespace NodeListRowPresentation
{
inline std::tuple<uint32_t, uint32_t> nodeColors(uint32_t nodeNum)
{
    uint32_t red = (nodeNum & 0xff0000) >> 16;
    uint32_t green = (nodeNum & 0xff00) >> 8;
    uint32_t blue = nodeNum & 0xff;
    while (red + green + blue < 0xF0) {
        red += red / 3 + 10;
        green += green / 3 + 10;
        blue += blue / 3 + 10;
    }
    return std::make_tuple((red << 16) | (green << 8) | blue, (2 * red + 2 * green + blue) > 600 ? 0x000000 : 0xFFFFFF);
}

inline void applyNodeImage(lv_obj_t *img, uint32_t nodeNum, const void *source, bool unmessagable, bool resetRecolor)
{
    uint32_t bgColor = 0;
    uint32_t fgColor = 0;
    std::tie(bgColor, fgColor) = nodeColors(nodeNum);
    lv_image_set_src(img, source);
    if (unmessagable) {
        lv_obj_set_style_border_color(img, lv_color_hex(bgColor), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(img, lv_color_hex(0x202020), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_image_recolor(img, lv_color_hex(0xff5555), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_image_recolor_opa(img, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        return;
    }

    if (resetRecolor) {
        lv_obj_remove_local_style_prop(img, LV_STYLE_IMAGE_RECOLOR, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    lv_obj_set_style_bg_color(img, lv_color_hex(bgColor), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(img, lv_color_hex(bgColor), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_image_recolor_opa(img, fgColor ? 0 : 255, LV_PART_MAIN | LV_STATE_DEFAULT);
}
} // namespace NodeListRowPresentation
