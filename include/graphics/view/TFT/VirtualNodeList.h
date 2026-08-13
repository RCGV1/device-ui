#pragma once

#include "graphics/common/NodeStore.h"
#include "graphics/common/VisibleNodeIndex.h"
#include "lvgl.h"

#include <cstddef>
#include <cstdint>
#include <vector>

class NodeListActionSink
{
  public:
    virtual void nodeClicked(NodeId id) = 0;
    virtual void nodeLongPressed(NodeId id) = 0;
    virtual void nodeFocused(NodeId id) = 0;
    virtual ~NodeListActionSink() = default;
};

struct ReusableRow {
    lv_obj_t *panel = nullptr;
    lv_obj_t *img = nullptr;
    lv_obj_t *btn = nullptr;
    lv_obj_t *lblLong = nullptr;
    lv_obj_t *lblShort = nullptr;
    lv_obj_t *lblBat = nullptr;
    lv_obj_t *lblLh = nullptr;
    lv_obj_t *lblSig = nullptr;
    lv_obj_t *lblPos1 = nullptr;
    lv_obj_t *lblPos2 = nullptr;
    lv_obj_t *lblTm1 = nullptr;
    lv_obj_t *lblTm2 = nullptr;
    NodeId boundId = 0;
};

class VirtualNodeList
{
  public:
    static constexpr size_t POOL_SIZE = 7;
    static constexpr int32_t COLLAPSED_ROW_HEIGHT = 53;
    static constexpr int32_t EXPANDED_ROW_HEIGHT = 136;

    VirtualNodeList(lv_obj_t *parent, NodeListActionSink &sink);
    ~VirtualNodeList();

    void sync(const NodeStore &store, const VisibleNodeIndex &index, NodeId expanded = 0);
    void setExpanded(NodeId id);
    NodeId getExpanded() const { return expandedId; }
    void scrollTo(NodeId id, lv_anim_enable_t anim = LV_ANIM_OFF);
    void focus(NodeId id);
    size_t boundRowCount() const { return rowPool.size(); }

    void refreshVisibleRows();

  private:
    void createRowPool();
    void updateVirtualContentHeight();
    void bindRow(ReusableRow &row, const NodeRecord &record, bool isExpanded);

    static void scrollEventCallback(lv_event_t *e);
    static void rowClickCallback(lv_event_t *e);

    lv_obj_t *parentPanel = nullptr;
    lv_obj_t *spacer = nullptr;
    NodeListActionSink &actionSink;

    const NodeStore *currentStore = nullptr;
    const VisibleNodeIndex *currentIndex = nullptr;
    NodeId expandedId = 0;

    std::vector<ReusableRow> rowPool;
    std::vector<int32_t> prefixHeights;
};
