#include "graphics/view/TFT/VirtualNodeList.h"

#include "fonts.h"
#include "images.h"
#include "styles.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace
{
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
} // namespace

VirtualNodeList::VirtualNodeList(lv_obj_t *parent, NodeListActionSink &sink) : parentPanel(parent), actionSink(sink)
{
    if (parentPanel) {
        lv_obj_add_event_cb(parentPanel, scrollEventCallback, LV_EVENT_SCROLL, this);
        createRowPool();
    }
}

VirtualNodeList::~VirtualNodeList()
{
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
        lv_obj_set_style_image_recolor(row.img, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
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
        lv_obj_add_flag(row.lblPos1, LV_OBJ_FLAG_HIDDEN);

        row.lblPos2 = lv_label_create(row.panel);
        lv_obj_set_pos(row.lblPos2, -5, 63);
        lv_obj_set_size(row.lblPos2, 108, LV_SIZE_CONTENT);
        lv_label_set_long_mode(row.lblPos2, LV_LABEL_LONG_SCROLL);
        lv_obj_remove_flag(row.lblPos2, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_align(row.lblPos2, LV_ALIGN_TOP_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_add_flag(row.lblPos2, LV_OBJ_FLAG_HIDDEN);

        row.lblTm1 = lv_label_create(row.panel);
        lv_obj_set_pos(row.lblTm1, 0, 84);
        lv_obj_set_size(row.lblTm1, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_add_flag(row.lblTm1, LV_OBJ_FLAG_HIDDEN);

        row.lblTm2 = lv_label_create(row.panel);
        lv_obj_set_pos(row.lblTm2, 0, 100);
        lv_obj_set_size(row.lblTm2, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_add_flag(row.lblTm2, LV_OBJ_FLAG_HIDDEN);

        lv_obj_add_flag(row.panel, LV_OBJ_FLAG_HIDDEN);
    }
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

void VirtualNodeList::sync(const NodeStore &store, const VisibleNodeIndex &index, NodeId expanded, uint32_t now)
{
    currentStore = &store;
    currentIndex = &index;
    expandedId = expanded;
    currentTime = now != 0 ? now : static_cast<uint32_t>(std::time(nullptr));

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
            lv_group_focus_obj(row.btn);
            break;
        }
    }
}

void VirtualNodeList::bindRow(ReusableRow &row, const NodeRecord &record, bool isExpanded)
{
    row.boundId = record.id;
    lv_obj_set_user_data(row.panel, reinterpret_cast<void *>(static_cast<uintptr_t>(record.id)));
    lv_obj_set_user_data(row.btn, reinterpret_cast<void *>(static_cast<uintptr_t>(record.id)));

    lv_label_set_text(row.lblShort, record.user.short_name);
    lv_label_set_text(row.lblLong, record.user.long_name);

    if (record.hasDeviceMetrics) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%u%% %0.2fV", static_cast<unsigned int>(record.deviceMetrics.battery_level),
                      record.deviceMetrics.voltage);
        lv_label_set_text(row.lblBat, buf);
    } else {
        lv_label_set_text(row.lblBat, "");
    }

    char lastHeard[32];
    formatLastHeard(record.lastHeard, currentTime, lastHeard, sizeof(lastHeard));
    lv_label_set_text(row.lblLh, lastHeard);

    if (record.hopsAway >= 0) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "hops: %d", static_cast<int>(record.hopsAway));
        lv_label_set_text(row.lblSig, buf);
    } else if (record.rssi != 0 || record.snr != 0.0f) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "rssi: %d", static_cast<int>(record.rssi));
        lv_label_set_text(row.lblSig, buf);
    } else {
        lv_label_set_text(row.lblSig, "");
    }

    const int32_t height = isExpanded ? EXPANDED_ROW_HEIGHT : COLLAPSED_ROW_HEIGHT;
    lv_obj_set_height(row.panel, height);

    if (isExpanded && record.position.known) {
        char buf[48];
        std::snprintf(buf, sizeof(buf), "%.5f %.5f", record.position.latitude * 1e-7, record.position.longitude * 1e-7);
        lv_label_set_text(row.lblPos1, buf);
        lv_obj_remove_flag(row.lblPos1, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(row.lblPos1, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(row.lblPos2, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(row.lblTm1, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(row.lblTm2, LV_OBJ_FLAG_HIDDEN);
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
        self->actionSink.nodeFocused(id);
    }
}
