#include <ApplicationServices/ApplicationServices.h>
#include "../../src/view.h"

ANGRY_TEST(identifiesLeftChildInBinaryTreeHierarchy) {
    struct window_node parent = {0};
    struct window_node child = { .parent = &parent };
    parent.left = &child;
    bool left = window_node_is_left_child(&child);
    ANGRY_ASSERT(left, "window_node_is_left_child returned false for left child");
}

ANGRY_TEST(equalizesBspNodeRatioAcrossSpecifiedSplitAxis) {
    struct window_node node = { .split = SPLIT_Y, .ratio = 0.8f };
    g_space_manager.split_ratio = 0.5f;
    window_node_equalize(&node, SPLIT_Y);
    ANGRY_ASSERT(node.ratio == 0.5f, "window_node_equalize failed to reset node ratio");
}

ANGRY_TEST(locatesIndexOfWindowInWindowNodeList) {
    struct window_node node = { .window_count = 3, .window_list = { 101, 202, 303 } };
    int idx = window_node_index_of_window(&node, 202);
    ANGRY_ASSERT(idx == 1, "window_node_index_of_window returned incorrect index");
}

ANGRY_TEST(clearsZoomNodeRecursivelyFromBspSubtree) {
    struct window_node zoom_n = {0};
    struct window_node node = { .zoom = &zoom_n };
    window_node_clear_zoom(&node);
    ANGRY_ASSERT(node.zoom == NULL, "window_node_clear_zoom failed to clear zoom pointer");
}

ANGRY_TEST(locatesLastLeafNodeInDeepBspTree) {
    struct window_node left = {0}, right = {0};
    struct window_node root = { .left = &left, .right = &right };
    struct window_node *last = window_node_find_last_leaf(&root);
    ANGRY_ASSERT(last == &right, "window_node_find_last_leaf failed to return rightmost leaf");
}

ANGRY_TEST(locatesPreviousLeafNodeInBspTree) {
    struct window_node left = {0}, right = {0};
    struct window_node root = { .left = &left, .right = &right };
    left.parent = &root; right.parent = &root;
    struct window_node *prev = window_node_find_prev_leaf(&right);
    ANGRY_ASSERT(prev == &left, "window_node_find_prev_leaf failed to return left sibling");
}

ANGRY_TEST(returnsNullWhenSearchingDirectionalNodeOnInvalidSpace) {
    struct view v = { .sid = 0 };
    struct window_node src = {0};
    struct window_node *target = view_find_window_node_in_direction(&v, &src, DIR_EAST);
    ANGRY_ASSERT(target == NULL, "view_find_window_node_in_direction should return NULL for invalid space");
}

ANGRY_TEST(allocatesAndPopulatesWindowListFromLeafNodes) {
    struct window_node node = { .window_count = 2, .window_list = { 401, 402 } };
    struct view v = { .root = &node };
    int count = 0;
    uint32_t *list = view_find_window_list(&v, &count);
    ANGRY_ASSERT_ALL("view_find_window_list failed to populate window list",
                     count == 2,
                     list[0] == 401,
                     list[1] == 402);
}

ANGRY_TEST(evaluatesViewIsInvalidWhenValidFlagIsUnset) {
    struct view v = { .flags = 0 };
    bool inv = view_is_invalid(&v);
    ANGRY_ASSERT(inv == true, "view_is_invalid must return true when VIEW_IS_VALID flag is missing");
}

ANGRY_TEST(marksViewDirtyWhenFlushingInvisibleSpace) {
    struct view v = { .sid = 0 };
    view_flush(&v);
    bool dirty = view_is_dirty(&v);
    ANGRY_ASSERT(dirty, "view_flush failed to mark invisible space view as dirty");
}

ANGRY_TEST(serializesBspViewPropertiesToJsonStream) {
    struct view v = { .sid = 501, .layout = VIEW_BSP, .uuid = CFSTR("view-test-uuid") };
    char buf[2048] = {0};
    FILE *f = fmemopen(buf, sizeof(buf), "w");
    view_serialize(f, &v, 0);
    fclose(f);
    ANGRY_ASSERT_ALL("view_serialize failed to emit complete JSON",
                     strstr(buf, "\"id\":501"),
                     strstr(buf, "\"type\":\"bsp\""));
}

ANGRY_TEST(clearsViewRootTreeAndResetsNodeDimensions) {
    struct window_node root = {0};
    struct view v = { .sid = space_manager_active_space(), .root = &root };
    view_clear(&v);
    ANGRY_ASSERT(v.root->window_count == 0, "view_clear failed to reset root node");
}

ANGRY_TEST(displaysAndDestroysInsertFeedbackWindow) {
    struct window_node node = { .area = {{ 100, 100 }, { 300, 300 }}, .window_order = { 1 }, .insert_dir = STACK };
    insert_feedback_show(&node);
    bool showed = node.feedback_window.id != 0;
    insert_feedback_destroy(&node);
    bool destroyed = node.feedback_window.id == 0;
    ANGRY_ASSERT_ALL("insert_feedback failed to create and destroy feedback window",
                     showed,
                     destroyed);
}

ANGRY_TEST(movesInsertFeedbackEntryToNewFrontWindowOfStack) {
    struct window_node node = { .area = {{ 100, 100 }, { 300, 300 }}, .window_count = 1, .window_list = { 7101 }, .window_order = { 7101 }, .insert_dir = STACK };
    insert_feedback_show(&node);
    struct window w = { .id = 7102 };
    view_stack_window_node(&node, &w);
    uint32_t old_front = 7101, new_front = 7102;
    bool moved = table_find(&g_window_manager.insert_feedback, &new_front) == &node;
    bool cleared = table_find(&g_window_manager.insert_feedback, &old_front) == NULL;
    insert_feedback_destroy(&node);
    bool removed = table_find(&g_window_manager.insert_feedback, &new_front) == NULL;
    ANGRY_ASSERT_ALL("insert_feedback entry did not follow the new front window of the stack",
                     moved,
                     cleared,
                     removed);
}

ANGRY_TEST(handsInsertFeedbackEntryToParentWhenSiblingIsRemoved) {
    struct view *view = view_create(998);
    view->layout = VIEW_BSP;
    struct window w1 = { .id = 7201 };
    struct window w2 = { .id = 7202 };
    view_add_window_node(view, &w1);
    view_add_window_node(view, &w2);
    struct window_node *node = view_find_window_node(view, w2.id);
    node->insert_dir = STACK;
    insert_feedback_show(node);
    view_remove_window_node(view, &w1);
    bool handed = table_find(&g_window_manager.insert_feedback, &w2.id) == view->root;
    insert_feedback_destroy(view->root);
    bool removed = table_find(&g_window_manager.insert_feedback, &w2.id) == NULL;
    view_destroy(view);
    ANGRY_ASSERT_ALL("insert_feedback entry still pointed at the freed sibling node",
                     handed,
                     removed);
}

//
// NOTE: a node keeps its windows in arrays of NODE_MAX_WINDOW_COUNT entries.
// A space with a stack layout is a single node, so the window after the last
// one that fits used to be written past the end of those arrays.
//
ANGRY_TEST(leavesFullStackLayoutUntouchedWhenOneMoreWindowArrives)
{
    struct view *view = view_create(999);
    view->layout = VIEW_STACK;
    struct window windows[NODE_MAX_WINDOW_COUNT + 1];
    for (int i = 0; i <= NODE_MAX_WINDOW_COUNT; ++i) windows[i] = (struct window) { .id = 5000 + i };
    for (int i = 0; i < NODE_MAX_WINDOW_COUNT; ++i) view_add_window_node(view, &windows[i]);
    struct window *extra = &windows[NODE_MAX_WINDOW_COUNT];
    struct window_node *node = view_add_window_node(view, extra);
    bool kept = ANGRY_ALL((node == NULL),
                          (view->root->window_count == NODE_MAX_WINDOW_COUNT),
                          (!window_node_contains_window(view->root, extra->id)),
                          (view->root->window_order[0] == windows[NODE_MAX_WINDOW_COUNT - 1].id));
    view_destroy(view);
    ANGRY_ASSERT(kept, "a window past the capacity of a stack layout must not be added to its node");
}

ANGRY_TEST(tilesWindowBesideFullStackInsteadOfStackingOntoIt)
{
    struct view *view = view_create(999);
    view->layout = VIEW_BSP;
    struct window windows[NODE_MAX_WINDOW_COUNT];
    for (int i = 0; i < NODE_MAX_WINDOW_COUNT; ++i) windows[i] = (struct window) { .id = 6000 + i };
    view_add_window_node(view, &windows[0]);
    for (int i = 1; i < NODE_MAX_WINDOW_COUNT; ++i) view_stack_window_node(view->root, &windows[i]);
    view->root->insert_dir = STACK;
    view->insertion_point = windows[0].id;
    struct window extra = { .id = 6100 };
    view_add_window_node(view, &extra);
    struct window_node *stack = view_find_window_node(view, windows[0].id);
    struct window_node *tile = view_find_window_node(view, extra.id);
    bool beside = ANGRY_ALL((stack != NULL),
                            (tile != NULL),
                            (stack != tile),
                            (stack->window_count == NODE_MAX_WINDOW_COUNT),
                            (tile->window_count == 1));
    view_destroy(view);
    ANGRY_ASSERT(beside, "a window meant for a full stack must get a tile of its own next to it");
}

ANGRY_TEST(leavesWindowOutOfLayoutWhenTilingOnFullStackSpace)
{
    uint64_t sid = 0xBE5C0001;
    struct view *view = space_manager_find_view(&g_space_manager, sid);
    view->layout = VIEW_STACK;
    struct application app = { .pid = 4400, .name = "stack_app" };
    struct window windows[NODE_MAX_WINDOW_COUNT + 1];
    for (int i = 0; i <= NODE_MAX_WINDOW_COUNT; ++i) windows[i] = (struct window) { .id = 7000 + i, .application = &app };
    for (int i = 0; i < NODE_MAX_WINDOW_COUNT; ++i) space_manager_tile_window_on_space(&g_space_manager, &windows[i], sid);
    struct window *extra = &windows[NODE_MAX_WINDOW_COUNT];
    struct view *result = space_manager_tile_window_on_space(&g_space_manager, extra, sid);
    bool left_out = ANGRY_ALL((result == view),
                              (view_find_window_node(view, extra->id) == NULL),
                              (view->root->window_count == NODE_MAX_WINDOW_COUNT));
    for (int i = NODE_MAX_WINDOW_COUNT - 1; i >= 0; --i) space_manager_untile_window(view, &windows[i]);
    ANGRY_ASSERT(left_out, "tiling a window on a full stack space must leave it out of the layout instead of failing");
}

//
// NOTE: three windows in a view whose tree is built by hand, so the scroll and
// accordion layout math runs without a display, a space or the real windows of
// the session. The column layout is [(first second) third] over an area of
// 1000x600 with a gap of 10, which gives columns of 500 and a step of 510.
//
struct angry_three_columns
{
    struct window_node root, branch, first, second, third;
    struct view view;
};

static void angry_three_columns_begin(struct angry_three_columns *cols, enum view_type layout)
{
    cols->first  = (struct window_node) { .parent = &cols->branch, .window_count = 1 };
    cols->second = (struct window_node) { .parent = &cols->branch, .window_count = 1 };
    cols->branch = (struct window_node) { .parent = &cols->root, .left = &cols->first, .right = &cols->second };
    cols->third  = (struct window_node) { .parent = &cols->root, .window_count = 1 };
    cols->root   = (struct window_node) { .area = { 0, 0, 1000, 600 }, .left = &cols->branch, .right = &cols->third };

    cols->first.window_list[0]  = cols->first.window_order[0]  = 8101;
    cols->second.window_list[0] = cols->second.window_order[0] = 8102;
    cols->third.window_list[0]  = cols->third.window_order[0]  = 8103;

    cols->view = (struct view) { .layout = layout, .root = &cols->root, .window_gap = 10, .flags = VIEW_ENABLE_GAP };
}

ANGRY_TEST(tilesScrollColumnsWithConfiguredWidthAndGap)
{
    struct angry_three_columns cols;
    angry_three_columns_begin(&cols, VIEW_SCROLL);

    uint32_t saved_focus = g_window_manager.focused_window_id;
    float saved_width    = g_window_manager.scroll_column_width;
    bool saved_reversed  = g_window_manager.scroll_reversed;
    g_window_manager.focused_window_id = 8101;
    g_window_manager.scroll_column_width = 0.5f;
    g_window_manager.scroll_reversed = false;

    window_node_update(&cols.view, &cols.root);

    float first_x = cols.first.area.x, second_x = cols.second.area.x, third_x = cols.third.area.x;
    float width = cols.first.area.w, height = cols.first.area.h;

    g_window_manager.focused_window_id = saved_focus;
    g_window_manager.scroll_column_width = saved_width;
    g_window_manager.scroll_reversed = saved_reversed;

    ANGRY_ASSERT_ALL("scroll layout did not place the columns of half a root width with one gap between them",
                     first_x == 0.0f,
                     second_x == 510.0f,
                     third_x == 1020.0f,
                     width == 500.0f,
                     height == 600.0f);
}

ANGRY_TEST(scrollsFocusedColumnIntoView)
{
    struct angry_three_columns cols;
    angry_three_columns_begin(&cols, VIEW_SCROLL);

    uint32_t saved_focus = g_window_manager.focused_window_id;
    g_window_manager.focused_window_id = 8103;

    window_node_update(&cols.view, &cols.root);

    float offset = cols.view.scroll_offset;
    float third_x = cols.third.area.x;
    float first_x = cols.first.area.x;

    g_window_manager.focused_window_id = saved_focus;

    ANGRY_ASSERT_ALL("scroll layout did not scroll the last focused column to the right edge",
                     offset == 520.0f,
                     third_x == 500.0f,
                     first_x == -520.0f);
}

ANGRY_TEST(clampsScrollOffsetPastFirstAndLastColumn)
{
    struct angry_three_columns cols;
    angry_three_columns_begin(&cols, VIEW_SCROLL);
    cols.view.scroll_offset = 0;

    uint32_t saved_focus = g_window_manager.focused_window_id;
    g_window_manager.focused_window_id = 8101;

    view_scroll_by(&cols.view, 10);
    float right_offset = cols.view.scroll_offset;
    float third_x = cols.third.area.x;

    view_scroll_by(&cols.view, -10);
    float left_offset = cols.view.scroll_offset;
    float first_x = cols.first.area.x;

    g_window_manager.focused_window_id = saved_focus;

    ANGRY_ASSERT_ALL("scrolling past either end of the columns must stop at the end",
                     right_offset == 520.0f,
                     third_x == 500.0f,
                     left_offset == 0.0f,
                     first_x == 0.0f);
}

ANGRY_TEST(reversesScrollColumnOrderWhenScrollReversed)
{
    struct angry_three_columns cols;
    angry_three_columns_begin(&cols, VIEW_SCROLL);

    uint32_t saved_focus = g_window_manager.focused_window_id;
    bool saved_reversed  = g_window_manager.scroll_reversed;
    g_window_manager.focused_window_id = 8101;
    g_window_manager.scroll_reversed = true;

    window_node_update(&cols.view, &cols.root);

    float first_x = cols.first.area.x;
    float third_x = cols.third.area.x;

    g_window_manager.focused_window_id = saved_focus;
    g_window_manager.scroll_reversed = saved_reversed;

    ANGRY_ASSERT_ALL("reversed scroll columns must place the first window rightmost",
                     first_x == 500.0f,
                     third_x == -520.0f);
}

ANGRY_TEST(stacksAccordionRibbonsAroundFocusedColumn)
{
    struct angry_three_columns cols;
    angry_three_columns_begin(&cols, VIEW_ACCORDION);

    uint32_t saved_focus = g_window_manager.focused_window_id;
    int saved_ribbon     = g_window_manager.accordion_ribbon_width;
    g_window_manager.focused_window_id = 8102;
    g_window_manager.accordion_ribbon_width = 100;

    window_node_update(&cols.view, &cols.root);

    struct area focused = cols.second.area, first = cols.first.area, third = cols.third.area;

    g_window_manager.focused_window_id = saved_focus;
    g_window_manager.accordion_ribbon_width = saved_ribbon;

    ANGRY_ASSERT_ALL("accordion layout did not stack ribbons above and below the focused window",
                     first.y == 0.0f,
                     first.h == 100.0f,
                     focused.y == 100.0f,
                     focused.h == 400.0f,
                     focused.w == 1000.0f,
                     third.y == 500.0f,
                     third.h == 100.0f);
}

//
// NOTE: a BSP view whose split algorithm is fixed, over an area of 1000x600 so
// that the longer side is horizontal and the shorter side is vertical.
//
static struct view *angry_split_view(uint64_t sid, enum split_algorithm algorithm)
{
    struct view *view = view_create(sid);
    view->layout = VIEW_BSP;
    view->auto_balance = SPLIT_NONE;
    view->split_type = SPLIT_AUTO;
    view->split_algorithm = algorithm;
    view_set_flag(view, VIEW_SPLIT_TYPE);
    view_set_flag(view, VIEW_SPLIT_ALGO);
    view->root->area = (struct area) { 0, 0, 1000, 600 };
    return view;
}

ANGRY_TEST(splitsAlongLongestSideWhenConfigured)
{
    struct view *view = angry_split_view(994, SPLIT_ALGO_LONGEST_SIDE);
    struct window first = { .id = 8111 }, second = { .id = 8112 };

    view_add_window_node(view, &first);
    view_add_window_node(view, &second);

    enum window_node_split split = view->root->split;
    float left_w = view->root->left->area.w;
    float right_x = view->root->right->area.x;
    view_destroy(view);

    ANGRY_ASSERT_ALL("longest_side must split the longer horizontal side into equal vertical halves",
                     split == SPLIT_Y,
                     left_w == 500.0f,
                     right_x == 500.0f);
}

ANGRY_TEST(splitsAcrossShortestSideWhenConfigured)
{
    struct view *view = angry_split_view(995, SPLIT_ALGO_SHORTEST_SIDE);
    struct window first = { .id = 8121 }, second = { .id = 8122 };

    view_add_window_node(view, &first);
    view_add_window_node(view, &second);

    enum window_node_split split = view->root->split;
    float left_h = view->root->left->area.h;
    view_destroy(view);

    ANGRY_ASSERT_ALL("shortest_side must split the shorter vertical side into equal horizontal halves",
                     split == SPLIT_X,
                     left_h == 300.0f);
}

ANGRY_TEST(usesGoldenRatioForGoldenAlgorithm)
{
    struct view *view = angry_split_view(996, SPLIT_ALGO_GOLDEN);
    struct window first = { .id = 8131 }, second = { .id = 8132 };

    view_add_window_node(view, &first);
    view_add_window_node(view, &second);

    enum window_node_split split = view->root->split;
    float ratio = view->root->ratio;
    float left_w = view->root->left->area.w;
    view_destroy(view);

    ANGRY_ASSERT_ALL("golden must split the longer side by the golden ratio",
                     split == SPLIT_Y,
                     ratio == 0.618f,
                     left_w == 618.0f);
}

ANGRY_TEST(alternatesSplitAxisAcrossDepthForAlternateAlgorithm)
{
    struct view *view = angry_split_view(992, SPLIT_ALGO_ALTERNATE);
    view->root->area = (struct area) { 0, 0, 3000, 1000 };
    view->root->ratio = 0.5f;
    struct window first = { .id = 8141 }, second = { .id = 8142 }, third = { .id = 8143 };

    view_add_window_node(view, &first);
    view_add_window_node(view, &second);
    view_add_window_node_with_insertion_point(view, &third, first.id);

    enum window_node_split root_split = view->root->split;
    enum window_node_split child_split = view_find_window_node(view, first.id)->parent->split;
    view_destroy(view);

    ANGRY_ASSERT_ALL("alternate_axis must split the child across the axis of its parent, not along the longer side",
                     root_split == SPLIT_Y,
                     child_split == SPLIT_X);
}

ANGRY_TEST(alternatesSplitAxisByDepthForSpiralAlgorithm)
{
    struct view *view = angry_split_view(993, SPLIT_ALGO_SPIRAL);
    view->root->area = (struct area) { 0, 0, 3000, 1000 };
    view->root->ratio = 0.5f;
    struct window first = { .id = 8151 }, second = { .id = 8152 }, third = { .id = 8153 };

    view_add_window_node(view, &first);
    view_add_window_node(view, &second);
    view->root->split = SPLIT_X;
    view_add_window_node_with_insertion_point(view, &third, first.id);

    enum window_node_split child_split = view_find_window_node(view, first.id)->parent->split;
    view_destroy(view);

    ANGRY_ASSERT(child_split == SPLIT_X, "spiral must split by tree depth even when the parent was split on the other axis");
}

ANGRY_TEST(keepsWholeWindowListOfFullStackOutOfNextTemporaryAllocation) {
    struct view *view = view_create(997);
    view->layout = VIEW_STACK;
    struct window windows[NODE_MAX_WINDOW_COUNT];
    for (int i = 0; i < NODE_MAX_WINDOW_COUNT; ++i) {
        windows[i] = (struct window) { .id = 7300 + i };
        view_add_window_node(view, &windows[i]);
    }
    int count = 0;
    uint32_t *list = view_find_window_list(view, &count);
    uint32_t *next = ts_alloc_list(uint32_t, NODE_MAX_WINDOW_COUNT);
    memset(next, 0xff, sizeof(uint32_t) * NODE_MAX_WINDOW_COUNT);
    bool intact = count == NODE_MAX_WINDOW_COUNT;
    for (int i = 0; intact && i < count; ++i) intact = list[i] != 0xffffffff;
    view_destroy(view);
    ANGRY_ASSERT(intact, "the next temporary allocation overwrote the window list of a full stack");
}
