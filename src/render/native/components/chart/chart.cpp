#include "./chart.hpp"

#include "component.hpp"
#include "native/core/event/event.hpp"
#include <lvgl.h>

namespace {

struct PlotLayout {
    lv_coord_t rel_x = 0;
    lv_coord_t rel_y = 0;
    lv_coord_t w = 0;
    lv_coord_t h = 0;
};

PlotLayout plotLayoutFromWrapperAndChart (lv_obj_t* wrapper, lv_obj_t* chart) {
    PlotLayout plot;

    lv_obj_update_layout(wrapper);
    lv_obj_update_layout(chart);

    lv_area_t wrapper_box;
    lv_obj_get_coords(wrapper, &wrapper_box);

    lv_area_t chart_box;
    lv_obj_get_coords(chart, &chart_box);

    const ChartPlotInsets ins = Chart::getPlotInsets(chart);

    plot.rel_x = static_cast<lv_coord_t>(chart_box.x1 - wrapper_box.x1 + ins.left);
    plot.rel_y = static_cast<lv_coord_t>(chart_box.y1 - wrapper_box.y1 + ins.top);
    plot.w = ins.plot_w;
    plot.h = ins.plot_h;
    return plot;
}

/** Place zoomed scale in axis viewport: plot origin tracks wrapper scroll via rel_x/rel_y. */
void placeScaleInAxisViewport (lv_obj_t* scale, lv_obj_t* viewport, lv_coord_t x, lv_coord_t y) {
    lv_obj_set_pos(scale, x, y);
    lv_obj_scroll_to(viewport, 0, 0, LV_ANIM_OFF);
}

} // namespace

Chart::Chart(std::string uid, lv_obj_t* parent): BasicComponent(uid) {
    this->type = COMP_TYPE_CHART;

    this->uid = uid;
    lv_obj_t* parent_obj = parent != nullptr ? parent : GetWindowInstance();

    this->instance = lv_obj_create(parent_obj);
    LV_ASSERT_NULL(this->instance);
    // main_cont: outer axis shell only; no theme frame (border/bg/radius on virtual_box).
    lv_obj_set_style_pad_all(this->instance, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(this->instance, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(this->instance, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(this->instance, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(this->instance, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(this->instance, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_set_style_clip_corner(this->instance, false, LV_PART_MAIN);
    lv_obj_set_user_data(this->instance, this);

    this->virtual_box = lv_obj_create(this->instance);
    LV_ASSERT_NULL(this->virtual_box);
    // Theme card (border, bg, radius) applies on create; pad 0 so lv_pct(100) fills plot area.
    lv_obj_set_style_pad_all(this->virtual_box, 0, LV_PART_MAIN);
    lv_obj_set_width(this->virtual_box, lv_pct(100));
    lv_obj_set_height(this->virtual_box, lv_pct(100));
    lv_obj_clear_flag(this->virtual_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_user_data(this->virtual_box, this);
    lv_obj_add_event_cb(this->virtual_box, &Chart::LayoutEventCallback, LV_EVENT_SCROLL, this);

    this->chart_obj = lv_chart_create(this->virtual_box);
    LV_ASSERT_NULL(this->chart_obj);
    lv_obj_set_style_radius(this->chart_obj, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(this->chart_obj, 0, LV_PART_MAIN);
    // Plot inset: theme pad_small on chart_obj; bar spacing in syncChartBarSpacing().
    lv_obj_add_event_cb(this->instance, &Chart::LayoutEventCallback, LV_EVENT_SIZE_CHANGED, this);
    lv_obj_add_event_cb(this->instance, &Chart::LayoutEventCallback, LV_EVENT_STYLE_CHANGED, this);

    lv_group_add_obj(lv_group_get_default(), this->instance);

    lv_obj_add_flag(this->chart_obj, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_flag(this->chart_obj, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_clear_flag(this->chart_obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(this->chart_obj, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_set_user_data(this->chart_obj, this);

    this->initStyle(LV_PART_MAIN);
    this->syncScrollZoom();
};

void Chart::initStyle (int32_t type) {
    bool is_new = this->ensureStyle(type);
    lv_style_t* style = this->style_map.at(type);
    lv_style_init(style);
    lv_style_reset(style);

    this->initCompStyle(type);

    if (is_new) {
        lv_obj_add_style(this->styleTargetVirtual(), style, type);
    }
}

void Chart::setStyle (JSContext* ctx, JSValue& nativeStyle, int32_t type, bool isinit) {
    BasicComponent::setStyle(ctx, nativeStyle, type, isinit);
    if (type == static_cast<int32_t>(LV_PART_MAIN)) {
        this->applyMainLayoutFromStyle(type);
        this->syncPlotFrameClip();
        this->syncScrollZoom();
    }
}

lv_obj_t* Chart::styleTarget(int32_t type) {
    lv_style_selector_t selector = static_cast<lv_style_selector_t>(type);
    lv_part_t part = lv_obj_style_get_selector_part(selector);
    lv_state_t state = lv_obj_style_get_selector_state(selector);

    if (part == LV_PART_ITEMS || part == LV_PART_INDICATOR) {
        return this->styleTargetChart();
    }
    if (state & LV_STATE_PRESSED) {
        return this->styleTargetChart();
    }
    return this->styleTargetVirtual();
}

void Chart::ChartEventCallback(lv_event_t* event) {
    Chart* chart = static_cast<Chart*>(lv_event_get_user_data(event));
    if (chart == nullptr || lv_event_get_code(event) != LV_EVENT_VALUE_CHANGED) {
        return;
    }

    lv_obj_t* chart_obj = chart->styleTargetChart();
    if (!chart->isEventRegist(LV_EVENT_PRESSED)) {
        return;
    }

    if (lv_chart_get_pressed_point(chart_obj) == LV_CHART_POINT_NONE) {
        return;
    }

    FireEventToJS(event, chart->uid, LV_EVENT_PRESSED);
}

void Chart::syncChartObjEventListener() {
    lv_obj_t* chart_obj = this->styleTargetChart();

    bool need_chart_cb = this->isEventRegist(LV_EVENT_PRESSED);

    if (need_chart_cb && !this->chart_obj_events_attached) {
        lv_obj_add_event_cb(chart_obj, &Chart::ChartEventCallback, LV_EVENT_VALUE_CHANGED, this);
        this->chart_obj_events_attached = true;
    } else if (!need_chart_cb && this->chart_obj_events_attached) {
        lv_obj_remove_event_cb(chart_obj, &Chart::ChartEventCallback);
        this->chart_obj_events_attached = false;
    }
}

void Chart::addEventListener(int eventType) {
    BasicComponent::addEventListener(eventType);
    this->syncChartObjEventListener();
}

void Chart::removeEventListener(int eventType) {
    BasicComponent::removeEventListener(eventType);
    this->syncChartObjEventListener();
}

void Chart::LayoutEventCallback(lv_event_t* event) {
    Chart* chart = static_cast<Chart*>(lv_event_get_user_data(event));
    if (chart == nullptr) {
        return;
    }

    const lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_SCROLL) {
        chart->syncAxisScaleLayout();
        return;
    }

    chart->syncScrollZoom();
}

void Chart::applyMainLayoutFromStyle (int32_t type) {
    if (type != static_cast<int32_t>(LV_PART_MAIN)) {
        return;
    }

    const auto it = this->style_map.find(type);
    if (it == this->style_map.end()) {
        return;
    }

    lv_style_t* style = it->second;
    lv_obj_t* main = this->styleTargetMain();
    lv_obj_t* frame = this->styleTargetVirtual();
    const lv_part_t part = LV_PART_MAIN;
    const lv_style_selector_t selector = static_cast<lv_style_selector_t>(type);

    static const lv_style_prop_t layout_props[] = {
        LV_STYLE_WIDTH,
        LV_STYLE_HEIGHT,
        LV_STYLE_MIN_WIDTH,
        LV_STYLE_MAX_WIDTH,
        LV_STYLE_MIN_HEIGHT,
        LV_STYLE_MAX_HEIGHT,
        LV_STYLE_MARGIN_TOP,
        LV_STYLE_MARGIN_BOTTOM,
        LV_STYLE_MARGIN_LEFT,
        LV_STYLE_MARGIN_RIGHT,
    };

    for (lv_style_prop_t prop : layout_props) {
        lv_style_value_t v;
        if (lv_style_get_prop(style, prop, &v) != LV_STYLE_RES_FOUND) {
            continue;
        }
        lv_obj_set_local_style_prop(main, prop, v, selector);
        lv_style_remove_prop(style, prop);
    }

    lv_obj_refresh_style(main, part, LV_STYLE_PROP_ANY);
    lv_obj_refresh_style(frame, part, LV_STYLE_PROP_ANY);
}

void Chart::syncPlotFrameClip() {
    lv_obj_t* frame = this->styleTargetVirtual();
    const lv_coord_t radius = lv_obj_get_style_radius(frame, LV_PART_MAIN);
    lv_obj_set_style_clip_corner(frame, radius > 0, LV_PART_MAIN);
    lv_obj_set_style_clip_corner(this->styleTargetMain(), false, LV_PART_MAIN);
}

void Chart::syncScrollZoom() {
    if (this->syncing_scroll_zoom) {
        return;
    }
    this->syncing_scroll_zoom = true;

    this->applyMainLayoutFromStyle(static_cast<int32_t>(LV_PART_MAIN));
    this->syncPlotFrameClip();

    lv_obj_t* main = this->styleTargetMain();
    lv_obj_t* wrapper = this->virtual_box;
    lv_obj_t* chart = this->styleTargetChart();

    lv_obj_update_layout(main);

    const lv_coord_t gutter_w = static_cast<lv_coord_t>(this->draw_left + this->draw_right);
    const lv_coord_t gutter_h = static_cast<lv_coord_t>(this->draw_bottom);

    // Styled width/height is the plot viewport; outer targetMain grows by axis draw_size (8.2).
    const lv_coord_t cur_w = lv_obj_get_width(main);
    const lv_coord_t cur_h = lv_obj_get_height(main);
    const lv_coord_t expected_outer_w =
        static_cast<lv_coord_t>(this->plot_frame_w + this->inflated_gutter_w);
    const lv_coord_t expected_outer_h =
        static_cast<lv_coord_t>(this->plot_frame_h + this->inflated_gutter_h);

    if (cur_w == expected_outer_w && cur_h == expected_outer_h) {
        // Already inflated; keep plot_frame.
    } else if (cur_w == this->plot_frame_w && cur_h == this->plot_frame_h) {
        // Style re-applied plot frame size without gutters.
    } else {
        this->plot_frame_w = static_cast<lv_coord_t>(cur_w - gutter_w);
        this->plot_frame_h = static_cast<lv_coord_t>(cur_h - gutter_h);
        if (this->plot_frame_w < 1) {
            this->plot_frame_w = cur_w;
        }
        if (this->plot_frame_h < 1) {
            this->plot_frame_h = cur_h;
        }
    }

    const lv_coord_t outer_w = static_cast<lv_coord_t>(this->plot_frame_w + gutter_w);
    const lv_coord_t outer_h = static_cast<lv_coord_t>(this->plot_frame_h + gutter_h);
    bool outer_changed = false;
    if (lv_obj_get_width(main) != outer_w || lv_obj_get_height(main) != outer_h) {
        lv_obj_set_size(main, outer_w, outer_h);
        outer_changed = true;
    }
    this->inflated_gutter_w = gutter_w;
    this->inflated_gutter_h = gutter_h;
    if (outer_changed) {
        lv_obj_update_layout(main);
    }

    // 8.2 draw_size equivalent: axis gutters shrink the virtual_box viewport via targetMain pad.
    bool pad_changed = false;
    if (lv_obj_get_style_pad_left(main, LV_PART_MAIN) != this->draw_left) {
        lv_obj_set_style_pad_left(main, this->draw_left, LV_PART_MAIN);
        pad_changed = true;
    }
    if (lv_obj_get_style_pad_right(main, LV_PART_MAIN) != this->draw_right) {
        lv_obj_set_style_pad_right(main, this->draw_right, LV_PART_MAIN);
        pad_changed = true;
    }
    if (lv_obj_get_style_pad_bottom(main, LV_PART_MAIN) != this->draw_bottom) {
        lv_obj_set_style_pad_bottom(main, this->draw_bottom, LV_PART_MAIN);
        pad_changed = true;
    }
    if (pad_changed) {
        lv_obj_update_layout(main);
    }

    lv_obj_set_width(wrapper, lv_pct(100));
    lv_obj_set_height(wrapper, lv_pct(100));
    lv_obj_update_layout(main);

    lv_coord_t viewport_w = lv_obj_get_content_width(wrapper);
    lv_coord_t viewport_h = lv_obj_get_content_height(wrapper);
    if (viewport_w <= 0 || viewport_h <= 0) {
        this->syncing_scroll_zoom = false;
        return;
    }

    const bool zoom_x = this->scale_x_value > 256;
    const bool zoom_y = this->scale_y_value > 256;

    lv_obj_clear_flag(main, LV_OBJ_FLAG_SCROLLABLE);
    if (zoom_x || zoom_y) {
        lv_obj_add_flag(wrapper, LV_OBJ_FLAG_SCROLLABLE);
    } else {
        lv_obj_clear_flag(wrapper, LV_OBJ_FLAG_SCROLLABLE);
    }

    lv_coord_t content_w = viewport_w;
    lv_coord_t content_h = viewport_h;
    const ChartPlotInsets ins = Chart::getPlotInsets(chart);
    if (zoom_x) {
        // 8.2: virtual plot w = (lv_obj_get_content_width(chart) * zoom_x) >> 8
        const lv_coord_t inset_w = static_cast<lv_coord_t>(ins.left + ins.right);
        const lv_coord_t base_cw = static_cast<lv_coord_t>(viewport_w - inset_w);
        content_w = static_cast<lv_coord_t>(
            (((int32_t)base_cw * this->scale_x_value) >> 8) + inset_w);
    }
    if (zoom_y) {
        // 8.2: virtual plot h = (lv_obj_get_content_height(chart) * zoom_y) >> 8
        const lv_coord_t inset_h = static_cast<lv_coord_t>(ins.top + ins.bottom);
        const lv_coord_t base_ch = static_cast<lv_coord_t>(viewport_h - inset_h);
        content_h = static_cast<lv_coord_t>(
            (((int32_t)base_ch * this->scale_y_value) >> 8) + inset_h);
    }

    // virtual_box fills instance; chart is the zoomed scroll content inside it.
    lv_obj_set_size(chart, content_w, content_h);
    this->syncChartBarSpacing();

    lv_obj_update_layout(main);
    this->layoutScales();
    this->syncing_scroll_zoom = false;
}

void Chart::syncChartBarSpacing() {
    lv_obj_t* chart = this->styleTargetChart();
    // 8.2 theme: chart_bg MAIN pad_column=10, chart_series ITEMS pad_column=2 (DPX).
    const lv_coord_t base_block_gap = lv_dpx(10);
    const lv_coord_t base_ser_gap = lv_dpx(2);
    lv_coord_t block_gap = base_block_gap;
    lv_coord_t ser_gap = base_ser_gap;
    // 8.2 scaled gaps in draw: (pad_column * zoom_x) >> 8. Chart width is already zoomed here.
    if (this->scale_x_value > 256) {
        block_gap = static_cast<lv_coord_t>(((int32_t)base_block_gap * this->scale_x_value) >> 8);
        ser_gap = static_cast<lv_coord_t>(((int32_t)base_ser_gap * this->scale_x_value) >> 8);
    }
    lv_obj_set_style_pad_column(chart, block_gap, LV_PART_MAIN);
    lv_obj_set_style_pad_column(chart, ser_gap, LV_PART_ITEMS);
}

lv_obj_t* Chart::scaleDrawParent() const {
    return this->styleTargetMain();
}

lv_obj_t* Chart::scaleAnchor() const {
    return this->virtual_box;
}

void Chart::syncAxisScaleLayout() {
    this->layoutScales();
}

lv_obj_t* Chart::ensureScaleViewport (lv_obj_t** viewport) {
    if (*viewport != nullptr) {
        return *viewport;
    }

    lv_obj_t* parent = this->scaleDrawParent();
    *viewport = lv_obj_create(parent);
    LV_ASSERT_NULL(*viewport);
    lv_obj_set_style_pad_all(*viewport, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(*viewport, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(*viewport, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(*viewport, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(*viewport, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_scrollbar_mode(*viewport, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(*viewport, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_user_data(*viewport, this);
    return *viewport;
}

void Chart::layoutScale (lv_obj_t* scale, lv_obj_t* viewport, lv_scale_mode_t mode) {
    if (scale == nullptr || viewport == nullptr) {
        return;
    }

    lv_obj_t* wrapper = this->scaleAnchor();
    lv_obj_t* chart = this->styleTargetChart();
    lv_obj_t* draw_parent = this->scaleDrawParent();

    if (lv_obj_get_parent(viewport) != draw_parent) {
        lv_obj_set_parent(viewport, draw_parent);
    }
    if (lv_obj_get_parent(scale) != viewport) {
        lv_obj_set_parent(scale, viewport);
    }

    const PlotLayout plot = plotLayoutFromWrapperAndChart(wrapper, chart);
    const lv_coord_t viewport_w = lv_obj_get_content_width(wrapper);
    const lv_coord_t viewport_h = lv_obj_get_content_height(wrapper);
    const bool zoom_x = this->scale_x_value > 256;
    const bool zoom_y = this->scale_y_value > 256;

    switch (mode) {
        case LV_SCALE_MODE_VERTICAL_LEFT:
            lv_obj_set_size(viewport, this->draw_left, viewport_h);
            lv_obj_align_to(viewport, wrapper, LV_ALIGN_OUT_LEFT_TOP, 0, 0);
            if (zoom_y) {
                lv_obj_add_flag(viewport, LV_OBJ_FLAG_SCROLLABLE);
            } else {
                lv_obj_clear_flag(viewport, LV_OBJ_FLAG_SCROLLABLE);
            }
            lv_obj_set_width(scale, this->draw_left);
            lv_obj_set_height(scale, plot.h);
            placeScaleInAxisViewport(scale, viewport, 0, plot.rel_y);
            break;
        case LV_SCALE_MODE_VERTICAL_RIGHT:
            lv_obj_set_size(viewport, this->draw_right, viewport_h);
            lv_obj_align_to(viewport, wrapper, LV_ALIGN_OUT_RIGHT_TOP, 0, 0);
            if (zoom_y) {
                lv_obj_add_flag(viewport, LV_OBJ_FLAG_SCROLLABLE);
            } else {
                lv_obj_clear_flag(viewport, LV_OBJ_FLAG_SCROLLABLE);
            }
            lv_obj_set_width(scale, this->draw_right);
            lv_obj_set_height(scale, plot.h);
            placeScaleInAxisViewport(scale, viewport, 0, plot.rel_y);
            break;
        case LV_SCALE_MODE_HORIZONTAL_BOTTOM:
            lv_obj_set_size(viewport, viewport_w, this->draw_bottom);
            lv_obj_align_to(viewport, wrapper, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);
            if (zoom_x) {
                lv_obj_add_flag(viewport, LV_OBJ_FLAG_SCROLLABLE);
            } else {
                lv_obj_clear_flag(viewport, LV_OBJ_FLAG_SCROLLABLE);
            }
            lv_obj_set_width(scale, plot.w);
            lv_obj_set_height(scale, this->draw_bottom);
            placeScaleInAxisViewport(scale, viewport, plot.rel_x, 0);
            break;
        case LV_SCALE_MODE_HORIZONTAL_TOP:
            lv_obj_set_size(viewport, viewport_w, this->draw_bottom);
            lv_obj_align_to(viewport, wrapper, LV_ALIGN_OUT_TOP_LEFT, 0, 0);
            if (zoom_x) {
                lv_obj_add_flag(viewport, LV_OBJ_FLAG_SCROLLABLE);
            } else {
                lv_obj_clear_flag(viewport, LV_OBJ_FLAG_SCROLLABLE);
            }
            lv_obj_set_width(scale, plot.w);
            lv_obj_set_height(scale, this->draw_bottom);
            placeScaleInAxisViewport(scale, viewport, plot.rel_x, 0);
            break;
        default:
            break;
    }

    lv_obj_move_foreground(viewport);
    lv_obj_move_foreground(scale);
}

void Chart::layoutScales() {
    lv_obj_t* main = this->styleTargetMain();

    this->layoutScale(this->scale_left, this->virtual_box_scale_left, LV_SCALE_MODE_VERTICAL_LEFT);
    this->layoutScale(this->scale_right, this->virtual_box_scale_right, LV_SCALE_MODE_VERTICAL_RIGHT);
    this->layoutScale(this->scale_bottom, this->virtual_box_scale_bottom, LV_SCALE_MODE_HORIZONTAL_BOTTOM);

    lv_obj_refresh_ext_draw_size(main);
}

lv_obj_t* Chart::ensureScale (lv_obj_t** scale, lv_obj_t** viewport, lv_scale_mode_t mode) {
    if (*scale != nullptr) {
        return *scale;
    }

    this->ensureScaleViewport(viewport);
    *scale = lv_scale_create(*viewport);
    lv_obj_clear_flag(*scale, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(*scale, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_scale_set_mode(*scale, mode);
    this->layoutScales();
    return *scale;
}

void Chart::configureNumericScale (
    lv_obj_t* scale,
    lv_scale_mode_t mode,
    int32_t major_len,
    int32_t minor_len,
    int32_t major_num,
    int32_t minor_num,
    int32_t draw_size
) {
    if (scale == nullptr) {
        return;
    }

    const uint32_t every = static_cast<uint32_t>(minor_num > 0 ? minor_num + 1 : 1);
    const uint32_t total = major_num > 1
        ? static_cast<uint32_t>((major_num - 1) * static_cast<int32_t>(every) + 1)
        : 1U;

    lv_scale_set_mode(scale, mode);
    lv_scale_set_total_tick_count(scale, total);
    lv_scale_set_major_tick_every(scale, every);
    lv_scale_set_label_show(scale, true);
    lv_obj_set_style_length(scale, major_len, LV_PART_INDICATOR);
    lv_obj_set_style_length(scale, minor_len, LV_PART_ITEMS);

    if (draw_size > 0) {
        switch (mode) {
            case LV_SCALE_MODE_VERTICAL_LEFT:
                this->draw_left = draw_size;
                lv_obj_set_width(scale, draw_size);
                break;
            case LV_SCALE_MODE_VERTICAL_RIGHT:
                this->draw_right = draw_size;
                lv_obj_set_width(scale, draw_size);
                break;
            case LV_SCALE_MODE_HORIZONTAL_BOTTOM:
                this->draw_bottom = draw_size;
                lv_obj_set_height(scale, draw_size);
                break;
            default:
                break;
        }
    }

    this->syncScrollZoom();
}

void Chart::configureCategoryScale (
    lv_obj_t* scale,
    lv_scale_mode_t mode,
    int32_t major_len,
    int32_t minor_len,
    int32_t draw_size,
    uint32_t label_count
) {
    if (scale == nullptr || label_count == 0) {
        return;
    }

    lv_scale_set_mode(scale, mode);
    lv_scale_set_total_tick_count(scale, label_count);
    lv_scale_set_major_tick_every(scale, 1);
    lv_scale_set_label_show(scale, true);
    lv_obj_set_style_length(scale, major_len, LV_PART_INDICATOR);
    lv_obj_set_style_length(scale, minor_len, LV_PART_ITEMS);

    if (draw_size > 0) {
        switch (mode) {
            case LV_SCALE_MODE_VERTICAL_LEFT:
                this->draw_left = draw_size;
                lv_obj_set_width(scale, draw_size);
                break;
            case LV_SCALE_MODE_VERTICAL_RIGHT:
                this->draw_right = draw_size;
                lv_obj_set_width(scale, draw_size);
                break;
            case LV_SCALE_MODE_HORIZONTAL_BOTTOM:
                this->draw_bottom = draw_size;
                lv_obj_set_height(scale, draw_size);
                break;
            default:
                break;
        }
    }

    this->syncScrollZoom();
}

void Chart::applyScaleLabels (
    lv_obj_t* scale,
    std::vector<std::string>& labels,
    std::vector<const char*>& ptrs
) {
    if (scale == nullptr || labels.empty()) {
        return;
    }

    ptrs.clear();
    ptrs.reserve(labels.size() + 1);
    for (const auto& label : labels) {
        ptrs.push_back(label.c_str());
    }
    ptrs.push_back(nullptr);
    lv_scale_set_text_src(scale, ptrs.data());
    lv_obj_invalidate(scale);
}

ChartPlotInsets Chart::getPlotInsets (lv_obj_t* chart) {
    ChartPlotInsets ins;

    ins.left = static_cast<lv_coord_t>(lv_obj_get_style_space_left(chart, LV_PART_MAIN));
    ins.right = static_cast<lv_coord_t>(lv_obj_get_style_space_right(chart, LV_PART_MAIN));
    ins.top = static_cast<lv_coord_t>(lv_obj_get_style_space_top(chart, LV_PART_MAIN));
    ins.bottom = static_cast<lv_coord_t>(lv_obj_get_style_space_bottom(chart, LV_PART_MAIN));
    ins.plot_w = lv_obj_get_content_width(chart);
    ins.plot_h = lv_obj_get_content_height(chart);
    return ins;
}

void Chart::setScaleX (int32_t value) {
    // value is chart-scaleX from NormalizeScale (256 = 1.0, e.g. scaleX(3) -> 768).
    this->scale_x_value = value;
    this->syncScrollZoom();
}

void Chart::setScaleY (int32_t value) {
    // value is chart-scaleY from NormalizeScale (256 = 1.0, e.g. scaleY(2) -> 512).
    this->scale_y_value = value;
    this->syncScrollZoom();
}

void Chart::setType (int32_t type) {
    lv_chart_set_type(this->styleTargetChart(), static_cast<lv_chart_type_t>(type));
};

void Chart::setDivLineCount (int32_t hdiv, int32_t vdiv) {
    lv_chart_set_div_line_count(this->styleTargetChart(), hdiv, vdiv);
};

void Chart::setLeftAxisOption (
    int32_t major_len,
    int32_t minor_len,
    int32_t major_num,
    int32_t minor_num,
    int32_t draw_size
  ) {
    lv_obj_t* scale = this->ensureScale(&this->scale_left, &this->virtual_box_scale_left, LV_SCALE_MODE_VERTICAL_LEFT);
    if (!this->left_axis_labels.empty()) {
        this->configureCategoryScale(
            scale, LV_SCALE_MODE_VERTICAL_LEFT, major_len, minor_len, draw_size,
            static_cast<uint32_t>(this->left_axis_labels.size()));
        this->applyScaleLabels(scale, this->left_axis_labels, this->left_label_ptrs);
    } else {
        this->configureNumericScale(
            scale, LV_SCALE_MODE_VERTICAL_LEFT, major_len, minor_len, major_num, minor_num, draw_size);
        if (this->left_range_set) {
            lv_scale_set_range(scale, this->left_range_min, this->left_range_max);
        }
    }
};

void Chart::setRightAxisOption (
    int32_t major_len,
    int32_t minor_len,
    int32_t major_num,
    int32_t minor_num,
    int32_t draw_size
  ) {
    lv_obj_t* scale = this->ensureScale(&this->scale_right, &this->virtual_box_scale_right, LV_SCALE_MODE_VERTICAL_RIGHT);
    if (!this->right_axis_labels.empty()) {
        this->configureCategoryScale(
            scale, LV_SCALE_MODE_VERTICAL_RIGHT, major_len, minor_len, draw_size,
            static_cast<uint32_t>(this->right_axis_labels.size()));
        this->applyScaleLabels(scale, this->right_axis_labels, this->right_label_ptrs);
    } else {
        this->configureNumericScale(
            scale, LV_SCALE_MODE_VERTICAL_RIGHT, major_len, minor_len, major_num, minor_num, draw_size);
        if (this->right_range_set) {
            lv_scale_set_range(scale, this->right_range_min, this->right_range_max);
        }
    }
};

void Chart::setTopAxisOption (
    int32_t major_len,
    int32_t minor_len,
    int32_t major_num,
    int32_t minor_num,
    int32_t draw_size
  ) {
    (void)major_len;
    (void)minor_len;
    (void)major_num;
    (void)minor_num;
    (void)draw_size;
};

void Chart::setBottomAxisOption (
    int32_t major_len,
    int32_t minor_len,
    int32_t major_num,
    int32_t minor_num,
    int32_t draw_size
  ) {
    lv_obj_t* scale = this->ensureScale(&this->scale_bottom, &this->virtual_box_scale_bottom, LV_SCALE_MODE_HORIZONTAL_BOTTOM);
    if (!this->bottom_axis_labels.empty()) {
        this->configureCategoryScale(
            scale, LV_SCALE_MODE_HORIZONTAL_BOTTOM, major_len, minor_len, draw_size,
            static_cast<uint32_t>(this->bottom_axis_labels.size()));
        this->applyScaleLabels(scale, this->bottom_axis_labels, this->bottom_label_ptrs);
    } else {
        this->configureNumericScale(
            scale, LV_SCALE_MODE_HORIZONTAL_BOTTOM, major_len, minor_len, major_num, minor_num, draw_size);
        if (this->bottom_range_set) {
            lv_scale_set_range(scale, this->bottom_range_min, this->bottom_range_max);
        }
    }
};

void Chart::setLeftAxisData (std::vector<axis_data>& data) {
    lv_obj_t* chart = this->styleTargetChart();

    uint32_t point_count = lv_chart_get_point_count(chart);

    for (size_t i = 0; i < this->left_axis.size(); i++) {
        lv_chart_remove_series(chart, this->left_axis[i]);
    }
    this->left_axis.clear();

    for (size_t i = 0; i < data.size(); i++) {
        int32_t color = data[i].color;
        if (color == -1) {
            this->left_axis.push_back(lv_chart_add_series(chart, lv_theme_get_color_primary(chart), LV_CHART_AXIS_PRIMARY_Y));
        } else {
            this->left_axis.push_back(lv_chart_add_series(chart, lv_color_hex(color), LV_CHART_AXIS_PRIMARY_Y));
        }
    }

    for (size_t i = 0; i < data.size(); i++) {
        const axis_data& item = data[i];
        int32_t* y_points = lv_chart_get_series_y_array(chart, this->left_axis[i]);
        uint32_t n = static_cast<uint32_t>(item.data.size());
        if (n > point_count) {
            n = point_count;
        }
        for (uint32_t j = 0; j < n; j++) {
            y_points[j] = item.data[j];
        }
    }

    lv_chart_refresh(chart);
};

void Chart::setRightAxisData (std::vector<axis_data>& data) {
    lv_obj_t* chart = this->styleTargetChart();

    uint32_t point_count = lv_chart_get_point_count(chart);

    for (size_t i = 0; i < this->right_axis.size(); i++) {
        lv_chart_remove_series(chart, this->right_axis[i]);
    }
    this->right_axis.clear();

    for (size_t i = 0; i < data.size(); i++) {
        int32_t color = data[i].color;
        if (color == -1) {
            this->right_axis.push_back(lv_chart_add_series(chart, lv_theme_get_color_primary(chart), LV_CHART_AXIS_SECONDARY_Y));
        } else {
            this->right_axis.push_back(lv_chart_add_series(chart, lv_color_hex(color), LV_CHART_AXIS_SECONDARY_Y));
        }
    }

    for (size_t i = 0; i < data.size(); i++) {
        const axis_data& item = data[i];
        int32_t* y_points = lv_chart_get_series_y_array(chart, this->right_axis[i]);
        uint32_t n = static_cast<uint32_t>(item.data.size());
        if (n > point_count) {
            n = point_count;
        }
        for (uint32_t j = 0; j < n; j++) {
            y_points[j] = item.data[j];
        }
    }

    lv_chart_refresh(chart);
};

// void Chart::setTopAxisData (std::vector<axis_data>& data) {
//     if (this->top_axis == nullptr) {
//         this->top_axis = lv_chart_add_series(this->instance, lv_theme_get_color_primary(this->instance), LV_CHART_AXIS_SECONDARY_X);
//     }
//     int32_t i;
//     for (i=0; i<data.size(); i++) {
//         this->top_axis->y_points[i] = (lv_coord_t)data[i];
//     }
// };

// void Chart::setBottomAxisData (std::vector<axis_data>& data) {
//     int32_t i;
//     if (this->bottom_axis == nullptr) {
//         this->bottom_axis = lv_chart_add_series(this->instance, lv_theme_get_color_primary(this->instance), LV_CHART_AXIS_PRIMARY_X);
//     }
//     for (i=0; i<data.size(); i++) {
//         this->bottom_axis->y_points[i] = (lv_coord_t)data[i];
//     }
// };

void Chart::setPointNum (int32_t num) {
    if (num < 0) {
        return;
    }
    lv_chart_set_point_count(this->styleTargetChart(), static_cast<uint32_t>(num));
};

void Chart::setLeftAxisLabels (std::vector<std::string>& labels) {
    this->left_axis_labels = labels;
    if (this->scale_left != nullptr && !this->left_axis_labels.empty()) {
        this->applyScaleLabels(this->scale_left, this->left_axis_labels, this->left_label_ptrs);
    }
};

void Chart::setRightAxisLabels (std::vector<std::string>& labels) {
    this->right_axis_labels = labels;
    if (this->scale_right != nullptr && !this->right_axis_labels.empty()) {
        this->applyScaleLabels(this->scale_right, this->right_axis_labels, this->right_label_ptrs);
    }
};

void Chart::setTopAxisLabels (std::vector<std::string>& labels) {
    this->top_axis_labels = labels;
};

void Chart::setBottomAxisLabels (std::vector<std::string>& labels) {
    this->bottom_axis_labels = labels;
    if (this->scale_bottom != nullptr && !this->bottom_axis_labels.empty()) {
        this->applyScaleLabels(this->scale_bottom, this->bottom_axis_labels, this->bottom_label_ptrs);
    }
};

void Chart::setLeftAxisRange (int32_t min, int32_t max) {
    lv_chart_set_axis_range(this->styleTargetChart(), LV_CHART_AXIS_PRIMARY_Y, min, max);
    this->left_range_min = min;
    this->left_range_max = max;
    this->left_range_set = true;
    if (this->scale_left != nullptr && this->left_axis_labels.empty()) {
        lv_scale_set_range(this->scale_left, min, max);
    }
};

void Chart::setRightAxisRange (int32_t min, int32_t max) {
    lv_chart_set_axis_range(this->styleTargetChart(), LV_CHART_AXIS_SECONDARY_Y, min, max);
    this->right_range_min = min;
    this->right_range_max = max;
    this->right_range_set = true;
    if (this->scale_right != nullptr && this->right_axis_labels.empty()) {
        lv_scale_set_range(this->scale_right, min, max);
    }
};

void Chart::setTopAxisRange (int32_t min, int32_t max) {
    lv_chart_set_axis_range(this->styleTargetChart(), LV_CHART_AXIS_SECONDARY_X, min, max);
};

void Chart::setBottomAxisRange (int32_t min, int32_t max) {
    lv_chart_set_axis_range(this->styleTargetChart(), LV_CHART_AXIS_PRIMARY_X, min, max);
    this->bottom_range_min = min;
    this->bottom_range_max = max;
    this->bottom_range_set = true;
    if (this->scale_bottom != nullptr && this->bottom_axis_labels.empty()) {
        lv_scale_set_range(this->scale_bottom, min, max);
    }
};

void Chart::setScatterData (std::vector<axis_data>& data) {
    lv_obj_t* chart = this->styleTargetChart();

    uint32_t point_count = lv_chart_get_point_count(chart);

    for (size_t i = 0; i < this->scatter_series.size(); i++) {
        lv_chart_remove_series(chart, this->scatter_series[i]);
    }
    this->scatter_series.clear();

    for (size_t i = 0; i < data.size(); i++) {
        int32_t color = data[i].color;
        if (color == -1) {
            this->scatter_series.push_back(lv_chart_add_series(chart, lv_theme_get_color_primary(chart), LV_CHART_AXIS_PRIMARY_Y));
        } else {
            this->scatter_series.push_back(lv_chart_add_series(chart, lv_color_hex(color), LV_CHART_AXIS_PRIMARY_Y));
        }
    }

    for (size_t i = 0; i < data.size(); i++) {
        const axis_data& item = data[i];
        int32_t* x_points = lv_chart_get_series_x_array(chart, this->scatter_series[i]);
        int32_t* y_points = lv_chart_get_series_y_array(chart, this->scatter_series[i]);
        uint32_t pair_count = static_cast<uint32_t>(item.data.size() / 2);
        if (pair_count > point_count) {
            pair_count = point_count;
        }
        for (uint32_t p = 0; p < pair_count; p++) {
            x_points[p] = item.data[p * 2];
            y_points[p] = item.data[p * 2 + 1];
        }
    }

    lv_chart_refresh(chart);
};
