#include "graphics/view/TFT/VirtualNodeList.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

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

        row.img = lv_image_create(row.panel);
        lv_obj_set_pos(row.img, -5, 3);
        lv_obj_set_size(row.img, 32, 32);
        lv_obj_clear_flag(row.img, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_radius(row.img, 6, LV_PART_MAIN | LV_STATE_DEFAULT);

        row.btn = lv_button_create(row.panel);
        lv_obj_set_pos(row.btn, 0, 0);
        lv_obj_set_size(row.btn, lv_pct(106), lv_pct(100));
        lv_obj_set_align(row.btn, LV_ALIGN_CENTER);
        lv_obj_add_flag(row.btn, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
        lv_obj_add_event_cb(row.btn, rowClickCallback, LV_EVENT_ALL, this);

        row.lblLong = lv_label_create(row.panel);
        lv_obj_set_pos(row.lblLong, -5, 35);
        lv_obj_set_size(row.lblLong, lv_pct(80), LV_SIZE_CONTENT);
        lv_label_set_long_mode(row.lblLong, LV_LABEL_LONG_SCROLL);

        row.lblShort = lv_label_create(row.panel);
        lv_obj_set_pos(row.lblShort, 30, 2);
        lv_obj_set_size(row.lblShort, lv_pct(32), LV_SIZE_CONTENT);

        row.lblBat = lv_label_create(row.panel);
        lv_obj_set_pos(row.lblBat, 30, 18);
        lv_obj_set_size(row.lblBat, lv_pct(32), LV_SIZE_CONTENT);

        row.lblLh = lv_label_create(row.panel);
        lv_obj_set_pos(row.lblLh, -3, 3);
        lv_obj_set_size(row.lblLh, lv_pct(33), LV_SIZE_CONTENT);
        lv_obj_set_style_text_align(row.lblLh, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);

        row.lblSig = lv_label_create(row.panel);
        lv_obj_set_pos(row.lblSig, -3, 19);
        lv_obj_set_size(row.lblSig, lv_pct(33), LV_SIZE_CONTENT);
        lv_obj_set_style_text_align(row.lblSig, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);

        row.lblPos1 = lv_label_create(row.panel);
        lv_obj_set_pos(row.lblPos1, 0, 52);
        lv_obj_set_size(row.lblPos1, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_add_flag(row.lblPos1, LV_OBJ_FLAG_HIDDEN);

        row.lblPos2 = lv_label_create(row.panel);
        lv_obj_set_pos(row.lblPos2, 0, 68);
        lv_obj_set_size(row.lblPos2, lv_pct(100), LV_SIZE_CONTENT);
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

void VirtualNodeList::sync(const NodeStore &store, const VisibleNodeIndex &index, NodeId expanded)
{
    currentStore = &store;
    currentIndex = &index;
    expandedId = expanded;

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

    if (record.lastHeard > 0) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%us ago", record.lastHeard);
        lv_label_set_text(row.lblLh, buf);
    } else {
        lv_label_set_text(row.lblLh, "");
    }

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
