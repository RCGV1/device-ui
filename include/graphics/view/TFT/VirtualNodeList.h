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
    virtual void nodePositionClicked(NodeId id) {}
    virtual ~NodeListActionSink() = default;
};

struct NodeListRenderContext {
    NodeId ownNode = 0;
    bool hasOwnPosition = false;
    int32_t ownLatitude = 0;
    int32_t ownLongitude = 0;
    bool metricUnits = true;
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
    char shortText[32]{};
    char longText[40]{};
    char batteryText[32]{};
    char lastHeardText[32]{};
    char signalText[32]{};
    char positionText[48]{};
    char position2Text[32]{};
    char telemetry1Text[64]{};
    char telemetry2Text[48]{};
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

    void sync(const NodeStore &store, const VisibleNodeIndex &index, NodeId expanded = 0, uint32_t currentTime = 0,
              const NodeListRenderContext &context = {});
    void setExpanded(NodeId id);
    NodeId getExpanded() const { return expandedId; }
    void scrollTo(NodeId id, lv_anim_enable_t anim = LV_ANIM_OFF);
    void focus(NodeId id);
    size_t boundRowCount() const { return rowPool.size(); }
    uint32_t bindGenerationForTesting() const { return bindGeneration; }

    void refreshVisibleRows();

  private:
    void createRowPool();
    void updateVirtualContentHeight();
    void bindRow(ReusableRow &row, const NodeRecord &record, bool isExpanded);
    void attachGroupNavigation();
    void detachGroupNavigation();
    void handleGroupFocus(lv_group_t *group);
    void handleGroupEdge(bool forward);
    void noteFocusedButton(lv_obj_t *button);
    bool focusAdjacent(NodeId id, int direction);
    size_t poolIndexForButton(lv_obj_t *button) const;
    size_t firstVisiblePoolIndex() const;
    size_t lastVisiblePoolIndex() const;

    static void scrollEventCallback(lv_event_t *e);
    static void rowClickCallback(lv_event_t *e);
    static void rowPositionCallback(lv_event_t *e);
    static void groupFocusCallback(lv_group_t *group);
    static void groupEdgeCallback(lv_group_t *group, bool forward);

    lv_obj_t *parentPanel = nullptr;
    lv_obj_t *spacer = nullptr;
    NodeListActionSink &actionSink;

    const NodeStore *currentStore = nullptr;
    const VisibleNodeIndex *currentIndex = nullptr;
    NodeId expandedId = 0;
    uint32_t currentTime = 0;
    NodeListRenderContext renderContext{};

    std::vector<ReusableRow> rowPool;
    std::vector<int32_t> prefixHeights;
    uint32_t bindGeneration = 0;
    lv_group_t *attachedGroup = nullptr;
    lv_group_focus_cb_t previousFocusCallback = nullptr;
    lv_group_edge_cb_t previousEdgeCallback = nullptr;
    bool previousGroupWrap = true;
    bool ownsAttachedGroup = false;
    NodeId lastFocusedId = 0;
    size_t lastFocusedPoolIndex = POOL_SIZE;
    bool redirectingGroupFocus = false;
};
