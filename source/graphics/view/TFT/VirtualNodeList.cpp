#include "graphics/view/TFT/VirtualNodeList.h"

#include "fonts.h"
#include "images.h"
#include "styles.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

namespace
{
VirtualNodeList *activeGroupNavigationList = nullptr;

template <size_t Size> void setRowText(lv_obj_t *label, char (&storage)[Size], const char *text)
{
    std::snprintf(storage, Size, "%s", text ? text : "");
    lv_label_set_text_static(label, storage);
}

void formatLastHeard(uint32_t lastHeard, uint32_t currentTime, char *buffer, size_t bufferSize)
{
    if (lastHeard == 0) {
        buffer[0] = '\0';
        return;
    }

    const uint32_t age = currentTime > lastHeard ? currentTime - lastHeard : 0;
    if (age < 60) {
        std::snprintf(buffer, bufferSize, "now");
    } else if (age < 3600) {
        std::snprintf(buffer, bufferSize, "%u min", age / 60);
    } else if (age < 86400) {
        std::snprintf(buffer, bufferSize, "%u h", age / 3600);
    } else if (age < 86400 * 60) {
        std::snprintf(buffer, bufferSize, "%u d", age / 86400);
    } else {
        buffer[0] = '\0';
    }
}

void setHidden(lv_obj_t *obj, bool hidden)
{
    if (hidden) {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
}

std::tuple<uint32_t, uint32_t> nodeColor(NodeId nodeNum)
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

void setRoleImage(const NodeRecord &record, lv_obj_t *img)
{
    uint32_t bgColor = 0;
    uint32_t fgColor = 0;
    std::tie(bgColor, fgColor) = nodeColor(record.id);

    if (record.unmessagable) {
        lv_image_set_src(img, &img_unmessagable_image);
        lv_obj_set_style_border_color(img, lv_color_hex(bgColor), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(img, lv_color_hex(0x202020), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_image_recolor(img, lv_color_hex(0xff5555), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_image_recolor_opa(img, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        return;
    }

    lv_obj_remove_local_style_prop(img, LV_STYLE_IMAGE_RECOLOR, LV_PART_MAIN | LV_STATE_DEFAULT);

    switch (record.user.role) {
    case meshtastic_Config_DeviceConfig_Role_ROUTER:
    case meshtastic_Config_DeviceConfig_Role_REPEATER:
    case meshtastic_Config_DeviceConfig_Role_ROUTER_LATE:
        lv_image_set_src(img, &img_node_router_image);
        break;
    case meshtastic_Config_DeviceConfig_Role_ROUTER_CLIENT:
        lv_image_set_src(img, &img_top_nodes_image);
        break;
    case meshtastic_Config_DeviceConfig_Role_TRACKER:
    case meshtastic_Config_DeviceConfig_Role_SENSOR:
    case meshtastic_Config_DeviceConfig_Role_LOST_AND_FOUND:
    case meshtastic_Config_DeviceConfig_Role_TAK_TRACKER:
        lv_image_set_src(img, &img_node_sensor_image);
        break;
    case meshtastic_Config_DeviceConfig_Role_CLIENT:
    case meshtastic_Config_DeviceConfig_Role_CLIENT_MUTE:
    case meshtastic_Config_DeviceConfig_Role_CLIENT_HIDDEN:
    case meshtastic_Config_DeviceConfig_Role_TAK:
        lv_image_set_src(img, &img_node_client_image);
        break;
    default:
        lv_image_set_src(img, record.hasUser ? &img_node_client_image : &img_circle_question_image);
        break;
    }
    lv_obj_set_style_image_recolor_opa(img, fgColor ? 0 : 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(img, lv_color_hex(bgColor), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(img, lv_color_hex(bgColor), LV_PART_MAIN | LV_STATE_DEFAULT);
}

void formatLegacyShortDisplay(char *dest, size_t destSize, const char *shortName, NodeId nodeId)
{
    if (destSize == 0) {
        return;
    }

    if (!shortName || lv_txt_get_width(shortName, std::strlen(shortName), &ui_font_montserrat_14, 0) <= 4) {
        std::snprintf(dest, destSize, "%04x", static_cast<unsigned int>(nodeId & 0xffff));
    } else {
        std::snprintf(dest, destSize, "%s", shortName);
    }
}

void formatShortName(const NodeRecord &record, const NodeListRenderContext &context, char *buffer, size_t bufferSize)
{
    formatLegacyShortDisplay(buffer, bufferSize, record.user.short_name, record.id);

    if (!context.hasOwnPosition || !record.position.known || record.id == context.ownNode ||
        (record.position.latitude == 0 && record.position.longitude == 0) || bufferSize < 6) {
        return;
    }

    for (size_t i = 0; i < 4 && i + 1 < bufferSize; ++i) {
        if (buffer[i] == '\0') {
            buffer[i] = ' ';
        }
    }

    const float dx = 71.5f * 1e-7f * static_cast<float>(context.ownLongitude - record.position.longitude);
    const float dy = 111.3f * 1e-7f * static_cast<float>(context.ownLatitude - record.position.latitude);
    const float dist = std::sqrt(dx * dx + dy * dy);

    buffer[4] = '\n';
    if (context.metricUnits) {
        if (dist > 1.0f) {
            std::snprintf(&buffer[5], bufferSize - 5, "%.1f km ", dist);
        } else {
            std::snprintf(&buffer[5], bufferSize - 5, "%u m ", static_cast<unsigned int>(std::round(dist * 1000.0f)));
        }
    } else {
        if (dist > 0.1f) {
            std::snprintf(&buffer[5], bufferSize - 5, "%.1f mi ", std::round(dist * 0.621371f));
        } else {
            std::snprintf(&buffer[5], bufferSize - 5, "%u ft ", static_cast<unsigned int>(dist * 3280.84f));
        }
    }
}
} // namespace

VirtualNodeList::VirtualNodeList(lv_obj_t *parent, NodeListActionSink &sink) : parentPanel(parent), actionSink(sink)
{
    if (parentPanel) {
        lv_obj_add_event_cb(parentPanel, scrollEventCallback, LV_EVENT_SCROLL, this);
        createRowPool();
        attachGroupNavigation();
    }
}

VirtualNodeList::~VirtualNodeList()
{
    detachGroupNavigation();
    if (parentPanel) {
        lv_obj_remove_event_cb(parentPanel, scrollEventCallback);
    }
    if (spacer) {
        lv_obj_delete(spacer);
        spacer = nullptr;
    }
    for (auto &row : rowPool) {
        if (row.panel) {
            lv_obj_delete(row.panel);
            row.panel = nullptr;
        }
    }
    rowPool.clear();
}

void VirtualNodeList::createRowPool()
{
    if (!parentPanel) {
        return;
    }

    // Disable flex layout on parent so rows use absolute virtual coordinates
    lv_obj_set_style_layout(parentPanel, LV_LAYOUT_NONE, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Create spacer for virtual scroll height
    spacer = lv_obj_create(parentPanel);
    lv_obj_set_size(spacer, 1, 1);
    lv_obj_set_pos(spacer, 0, 0);
    lv_obj_remove_flag(spacer, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_set_style_opa(spacer, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(spacer, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(spacer, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    rowPool.resize(POOL_SIZE);

    for (size_t i = 0; i < POOL_SIZE; ++i) {
        auto &row = rowPool[i];

        row.panel = lv_obj_create(parentPanel);
        lv_obj_set_pos(row.panel, 0, 0);
        lv_obj_set_size(row.panel, lv_pct(100), COLLAPSED_ROW_HEIGHT);
        lv_obj_set_align(row.panel, LV_ALIGN_TOP_MID);
        lv_obj_set_style_pad_top(row.panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_bottom(row.panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_remove_flag(row.panel, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_GESTURE_BUBBLE));
        add_style_node_panel_style(row.panel);
        lv_obj_set_style_radius(row.panel, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(row.panel, lv_color_hex(0xfffff4), LV_PART_MAIN | LV_STATE_PRESSED);

        row.img = lv_image_create(row.panel);
        lv_obj_set_pos(row.img, -5, 3);
        lv_obj_set_size(row.img, 32, 32);
        lv_image_set_src(row.img, &img_node_client_image);
        lv_image_set_pivot(row.img, 0, 0);
        lv_obj_clear_flag(row.img, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_align(row.img, LV_ALIGN_TOP_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(row.img, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_image_recolor_opa(row.img, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(row.img, lv_color_hex(0x5d9388), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(row.img, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(row.img, lv_color_hex(0xff5555), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(row.img, 3, LV_PART_MAIN | LV_STATE_DEFAULT);

        row.btn = lv_button_create(row.panel);
        lv_obj_set_pos(row.btn, 0, 0);
        lv_obj_set_size(row.btn, lv_pct(106), lv_pct(100));
        lv_obj_set_align(row.btn, LV_ALIGN_CENTER);
        add_style_node_button_style(row.btn);
        lv_obj_set_style_shadow_width(row.btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_max_height(row.btn, 132, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_min_height(row.btn, 50, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_add_flag(row.btn, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
        lv_obj_add_event_cb(row.btn, rowClickCallback, LV_EVENT_ALL, this);

        row.lblLong = lv_label_create(row.panel);
        lv_obj_set_pos(row.lblLong, -5, 35);
        lv_obj_set_size(row.lblLong, lv_pct(80), LV_SIZE_CONTENT);
        lv_label_set_long_mode(row.lblLong, LV_LABEL_LONG_SCROLL);
        lv_obj_set_style_align(row.lblLong, LV_ALIGN_TOP_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);

        row.lblShort = lv_label_create(row.panel);
        lv_obj_set_pos(row.lblShort, 30, 10);
        lv_obj_set_size(row.lblShort, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_remove_flag(row.lblShort, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_align(row.lblShort, LV_ALIGN_TOP_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(row.lblShort, &ui_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

        row.lblBat = lv_label_create(row.panel);
        lv_obj_set_pos(row.lblBat, 8, 17);
        lv_obj_set_size(row.lblBat, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_label_set_long_mode(row.lblBat, LV_LABEL_LONG_CLIP);
        lv_obj_remove_flag(row.lblBat, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_align(row.lblBat, LV_ALIGN_TOP_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_align(row.lblBat, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);

        row.lblLh = lv_label_create(row.panel);
        lv_obj_set_pos(row.lblLh, 8, 33);
        lv_obj_set_size(row.lblLh, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_label_set_long_mode(row.lblLh, LV_LABEL_LONG_CLIP);
        lv_obj_remove_flag(row.lblLh, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_align(row.lblLh, LV_ALIGN_TOP_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_align(row.lblLh, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);

        row.lblSig = lv_label_create(row.panel);
        lv_obj_set_pos(row.lblSig, 8, 1);
        lv_obj_set_size(row.lblSig, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_label_set_long_mode(row.lblSig, LV_LABEL_LONG_CLIP);
        lv_obj_remove_flag(row.lblSig, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_align(row.lblSig, LV_ALIGN_TOP_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_align(row.lblSig, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);

        row.lblPos1 = lv_label_create(row.panel);
        lv_obj_set_pos(row.lblPos1, -5, 49);
        lv_obj_set_size(row.lblPos1, 120, LV_SIZE_CONTENT);
        lv_label_set_long_mode(row.lblPos1, LV_LABEL_LONG_CLIP);
        lv_obj_remove_flag(row.lblPos1, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_align(row.lblPos1, LV_ALIGN_TOP_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(row.lblPos1, lv_color_hex(0x05f6cb), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_add_flag(row.lblPos1, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(row.lblPos1, rowPositionCallback, LV_EVENT_CLICKED, this);
        lv_obj_add_flag(row.lblPos1, LV_OBJ_FLAG_HIDDEN);

        row.lblPos2 = lv_label_create(row.panel);
        lv_obj_set_pos(row.lblPos2, -5, 63);
        lv_obj_set_size(row.lblPos2, 108, LV_SIZE_CONTENT);
        lv_label_set_long_mode(row.lblPos2, LV_LABEL_LONG_SCROLL);
        lv_obj_remove_flag(row.lblPos2, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_align(row.lblPos2, LV_ALIGN_TOP_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_add_flag(row.lblPos2, LV_OBJ_FLAG_HIDDEN);

        row.lblTm1 = lv_label_create(row.panel);
        lv_obj_set_pos(row.lblTm1, 8, 49);
        lv_obj_set_size(row.lblTm1, 130, LV_SIZE_CONTENT);
        lv_label_set_long_mode(row.lblTm1, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_align(row.lblTm1, LV_ALIGN_TOP_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_align(row.lblTm1, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_add_flag(row.lblTm1, LV_OBJ_FLAG_HIDDEN);

        row.lblTm2 = lv_label_create(row.panel);
        lv_obj_set_pos(row.lblTm2, 8, 63);
        lv_obj_set_size(row.lblTm2, 130, LV_SIZE_CONTENT);
        lv_label_set_long_mode(row.lblTm2, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_align(row.lblTm2, LV_ALIGN_TOP_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_align(row.lblTm2, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_add_flag(row.lblTm2, LV_OBJ_FLAG_HIDDEN);

        lv_label_set_text_static(row.lblShort, row.shortText);
        lv_label_set_text_static(row.lblLong, row.longText);
        lv_label_set_text_static(row.lblBat, row.batteryText);
        lv_label_set_text_static(row.lblLh, row.lastHeardText);
        lv_label_set_text_static(row.lblSig, row.signalText);
        lv_label_set_text_static(row.lblPos1, row.positionText);
        lv_label_set_text_static(row.lblPos2, row.position2Text);
        lv_label_set_text_static(row.lblTm1, row.telemetry1Text);
        lv_label_set_text_static(row.lblTm2, row.telemetry2Text);

        lv_obj_add_flag(row.panel, LV_OBJ_FLAG_HIDDEN);
    }
}

void VirtualNodeList::attachGroupNavigation()
{
    attachedGroup = lv_group_create();
    if (!attachedGroup) {
        return;
    }

    for (auto &row : rowPool) {
        if (row.btn) {
            lv_group_add_obj(attachedGroup, row.btn);
        }
    }
    activeGroupNavigationList = this;
    lv_group_set_wrap(attachedGroup, false);
    lv_group_set_edge_cb(attachedGroup, groupEdgeCallback);
}

void VirtualNodeList::detachGroupNavigation()
{
    if (attachedGroup && activeGroupNavigationList == this) {
        activeGroupNavigationList = nullptr;
    }
    if (attachedGroup) {
        for (auto &row : rowPool) {
            if (row.btn) {
                lv_group_remove_obj(row.btn);
            }
        }
        lv_group_delete(attachedGroup);
    }
    attachedGroup = nullptr;
}

void VirtualNodeList::updateVirtualContentHeight()
{
    if (!currentIndex) {
        prefixHeights.clear();
        return;
    }

    const auto &ids = currentIndex->ids();
    prefixHeights.resize(ids.size() + 1);
    prefixHeights[0] = 0;

    int32_t currentY = 0;
    for (size_t i = 0; i < ids.size(); ++i) {
        prefixHeights[i] = currentY;
        const int32_t h = (ids[i] == expandedId) ? EXPANDED_ROW_HEIGHT : COLLAPSED_ROW_HEIGHT;
        currentY += h;
    }
    prefixHeights[ids.size()] = currentY;

    if (spacer) {
        lv_obj_set_size(spacer, 1, currentY > 0 ? currentY : 1);
        lv_obj_set_pos(spacer, 0, 0);
    }
}

void VirtualNodeList::sync(const NodeStore &store, const VisibleNodeIndex &index, NodeId expanded, uint32_t now,
                           const NodeListRenderContext &context)
{
    currentStore = &store;
    currentIndex = &index;
    expandedId = expanded;
    currentTime = now != 0 ? now : static_cast<uint32_t>(std::time(nullptr));
    renderContext = context;

    updateVirtualContentHeight();
    refreshVisibleRows();
}

void VirtualNodeList::setExpanded(NodeId id)
{
    if (expandedId != id) {
        expandedId = id;
        updateVirtualContentHeight();
        refreshVisibleRows();
    }
}

void VirtualNodeList::scrollTo(NodeId id, lv_anim_enable_t anim)
{
    if (!currentIndex || !parentPanel) {
        return;
    }
    const auto optIdx = currentIndex->indexOf(id);
    if (!optIdx.has_value()) {
        return;
    }

    const size_t idx = optIdx.value();
    if (idx < prefixHeights.size()) {
        const int32_t targetY = prefixHeights[idx];
        lv_obj_scroll_to_y(parentPanel, targetY, anim);
        refreshVisibleRows();
    }
}

void VirtualNodeList::focus(NodeId id)
{
    scrollTo(id, LV_ANIM_OFF);
    for (auto &row : rowPool) {
        if (row.boundId == id && !lv_obj_has_flag(row.panel, LV_OBJ_FLAG_HIDDEN)) {
            redirectingGroupFocus = true;
            lv_group_focus_obj(row.btn);
            redirectingGroupFocus = false;
            noteFocusedButton(row.btn);
            break;
        }
    }
}

size_t VirtualNodeList::poolIndexForButton(lv_obj_t *button) const
{
    for (size_t i = 0; i < rowPool.size(); ++i) {
        if (rowPool[i].btn == button) {
            return i;
        }
    }
    return POOL_SIZE;
}

void VirtualNodeList::noteFocusedButton(lv_obj_t *button)
{
    const size_t poolIndex = poolIndexForButton(button);
    if (poolIndex >= rowPool.size()) {
        return;
    }

    lastFocusedId = rowPool[poolIndex].boundId;
}

bool VirtualNodeList::focusAdjacent(NodeId id, int direction)
{
    if (!currentIndex || direction == 0) {
        return false;
    }
    const auto current = currentIndex->indexOf(id);
    if (!current.has_value()) {
        return false;
    }

    const auto &ids = currentIndex->ids();
    const auto target = static_cast<int64_t>(current.value()) + direction;
    if (target < 0 || target >= static_cast<int64_t>(ids.size())) {
        return false;
    }

    focus(ids[static_cast<size_t>(target)]);
    return true;
}

void VirtualNodeList::handleGroupEdge(bool forward)
{
    if (lastFocusedId == 0) {
        return;
    }

    redirectingGroupFocus = true;
    focusAdjacent(lastFocusedId, forward ? 1 : -1);
    redirectingGroupFocus = false;
}

void VirtualNodeList::bindRow(ReusableRow &row, const NodeRecord &record, bool isExpanded)
{
    bindGeneration++;
    row.boundId = record.id;
    lv_obj_set_user_data(row.panel, reinterpret_cast<void *>(static_cast<uintptr_t>(record.id)));
    lv_obj_set_user_data(row.btn, reinterpret_cast<void *>(static_cast<uintptr_t>(record.id)));
    lv_obj_set_user_data(row.lblPos1, reinterpret_cast<void *>(static_cast<uintptr_t>(record.id)));

    setRoleImage(record, row.img);
    if (!record.hasKey) {
        lv_obj_set_style_border_color(row.img, lv_color_hex(0xff5555), LV_PART_MAIN | LV_STATE_DEFAULT);
    } else if (!record.unmessagable) {
        lv_obj_set_style_border_color(row.img, lv_obj_get_style_bg_color(row.img, LV_PART_MAIN), LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    formatShortName(record, renderContext, row.shortText, sizeof(row.shortText));
    lv_label_set_text_static(row.lblShort, row.shortText);
    lv_obj_set_pos(row.lblShort, 30, std::strchr(row.shortText, '\n') ? -1 : 10);
    setRowText(row.lblLong, row.longText, record.user.long_name);

    if (record.hasDeviceMetrics) {
        std::snprintf(row.batteryText, sizeof(row.batteryText), "%u%% %0.2fV",
                      static_cast<unsigned int>(record.deviceMetrics.battery_level), record.deviceMetrics.voltage);
    } else {
        row.batteryText[0] = '\0';
    }
    lv_label_set_text_static(row.lblBat, row.batteryText);

    formatLastHeard(record.lastHeard, currentTime, row.lastHeardText, sizeof(row.lastHeardText));
    lv_label_set_text_static(row.lblLh, row.lastHeardText);

    if (record.signalDisplay == NodeSignalDisplayKind::Hops && record.hopsAway >= 0) {
        std::snprintf(row.signalText, sizeof(row.signalText), "hops: %d", static_cast<int>(record.hopsAway));
    } else if (record.signalDisplay == NodeSignalDisplayKind::Rssi && (record.rssi != 0 || record.snr != 0.0f)) {
        std::snprintf(row.signalText, sizeof(row.signalText), "rssi: %d snr: %.1f", static_cast<int>(record.rssi), record.snr);
    } else {
        row.signalText[0] = '\0';
    }
    lv_label_set_text_static(row.lblSig, row.signalText);

    const int32_t height = isExpanded ? EXPANDED_ROW_HEIGHT : COLLAPSED_ROW_HEIGHT;
    lv_obj_set_size(row.panel, lv_pct(100), height);
    lv_obj_update_layout(row.panel);

    row.positionText[0] = '\0';
    row.position2Text[0] = '\0';
    row.telemetry1Text[0] = '\0';
    row.telemetry2Text[0] = '\0';

    const bool showPosition =
        isExpanded && record.position.known && (record.position.latitude != 0 || record.position.longitude != 0);
    if (showPosition) {
        const int32_t altitude = std::abs(record.position.altitude) < 10000 ? record.position.altitude : 0;
        std::snprintf(row.positionText, sizeof(row.positionText), "%.5f %.5f", record.position.latitude * 1e-7,
                      record.position.longitude * 1e-7);
        std::snprintf(row.position2Text, sizeof(row.position2Text), "%dm MSL", static_cast<int>(altitude));
    }
    lv_label_set_text_static(row.lblPos1, row.positionText);
    lv_label_set_text_static(row.lblPos2, row.position2Text);
    setHidden(row.lblPos1, !showPosition);
    setHidden(row.lblPos2, !showPosition);

    const bool showTelemetry1 = isExpanded && record.hasEnvironmentMetrics;
    if (showTelemetry1) {
        const auto &metrics = record.environmentMetrics;
        if (static_cast<int>(metrics.relative_humidity) > 0) {
            std::snprintf(row.telemetry1Text, sizeof(row.telemetry1Text), "%2.1f°C %d%% %3.1fhPa", metrics.temperature,
                          static_cast<int>(metrics.relative_humidity), metrics.barometric_pressure);
        } else {
            std::snprintf(row.telemetry1Text, sizeof(row.telemetry1Text), "%2.1f°C %3.1fhPa", metrics.temperature,
                          metrics.barometric_pressure);
        }
    }
    lv_label_set_text_static(row.lblTm1, row.telemetry1Text);
    setHidden(row.lblTm1, !showTelemetry1);

    const bool showTelemetry2 =
        isExpanded && record.hasEnvironmentMetrics && record.environmentMetrics.iaq > 0 && record.environmentMetrics.iaq < 1000;
    if (showTelemetry2) {
        const auto &metrics = record.environmentMetrics;
        std::snprintf(row.telemetry2Text, sizeof(row.telemetry2Text), "IAQ: %d %.1fV %.1fmA", static_cast<int>(metrics.iaq),
                      metrics.voltage, metrics.current);
    }
    lv_label_set_text_static(row.lblTm2, row.telemetry2Text);
    setHidden(row.lblTm2, !showTelemetry2);

    if (!isExpanded) {
        setHidden(row.lblPos1, true);
        setHidden(row.lblPos2, true);
        setHidden(row.lblTm1, true);
        setHidden(row.lblTm2, true);
    }
}

void VirtualNodeList::refreshVisibleRows()
{
    if (!parentPanel || !currentIndex || !currentStore) {
        return;
    }

    const auto &ids = currentIndex->ids();
    if (ids.empty()) {
        for (auto &row : rowPool) {
            lv_obj_add_flag(row.panel, LV_OBJ_FLAG_HIDDEN);
            row.boundId = 0;
        }
        return;
    }

    int32_t scrollY = lv_obj_get_scroll_y(parentPanel);
    if (scrollY < 0) {
        scrollY = 0;
    }

    // Binary search for first visible row in prefixHeights
    auto it = std::upper_bound(prefixHeights.begin(), prefixHeights.end(), scrollY);
    size_t firstIdx = (it != prefixHeights.begin()) ? std::distance(prefixHeights.begin(), it) - 1 : 0;

    // Buffer 1 row of overscan above if possible
    if (firstIdx > 0) {
        firstIdx--;
    }
    if (!ids.empty() && firstIdx >= ids.size()) {
        firstIdx = ids.size() - 1;
    }

    for (size_t p = 0; p < POOL_SIZE; ++p) {
        const size_t nodeIdx = firstIdx + p;
        auto &row = rowPool[p];

        if (nodeIdx < ids.size()) {
            const NodeId id = ids[nodeIdx];
            const auto *rec = currentStore->find(id);
            if (rec) {
                bindRow(row, *rec, id == expandedId);
                lv_obj_set_y(row.panel, prefixHeights[nodeIdx]);
                lv_obj_remove_flag(row.panel, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(row.panel, LV_OBJ_FLAG_HIDDEN);
                row.boundId = 0;
            }
        } else {
            lv_obj_add_flag(row.panel, LV_OBJ_FLAG_HIDDEN);
            row.boundId = 0;
        }
    }
}

void VirtualNodeList::scrollEventCallback(lv_event_t *e)
{
    auto *self = static_cast<VirtualNodeList *>(lv_event_get_user_data(e));
    if (self) {
        self->refreshVisibleRows();
    }
}

void VirtualNodeList::rowClickCallback(lv_event_t *e)
{
    auto *self = static_cast<VirtualNodeList *>(lv_event_get_user_data(e));
    auto *btn = static_cast<lv_obj_t *>(lv_event_get_target(e));
    if (!self || !btn) {
        return;
    }

    const auto id = static_cast<NodeId>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(btn)));
    if (id == 0) {
        return;
    }

    const lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        self->actionSink.nodeClicked(id);
    } else if (code == LV_EVENT_LONG_PRESSED) {
        self->actionSink.nodeLongPressed(id);
    } else if (code == LV_EVENT_FOCUSED) {
        self->noteFocusedButton(btn);
        self->actionSink.nodeFocused(id);
    }
}

void VirtualNodeList::rowPositionCallback(lv_event_t *e)
{
    auto *self = static_cast<VirtualNodeList *>(lv_event_get_user_data(e));
    auto *label = static_cast<lv_obj_t *>(lv_event_get_target(e));
    if (!self || !label) {
        return;
    }

    const auto id = static_cast<NodeId>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(label)));
    if (id != 0 && lv_event_get_code(e) == LV_EVENT_CLICKED) {
        self->actionSink.nodePositionClicked(id);
    }
}

void VirtualNodeList::groupEdgeCallback(lv_group_t *group, bool forward)
{
    (void)group;
    if (activeGroupNavigationList) {
        activeGroupNavigationList->handleGroupEdge(forward);
    }
}
