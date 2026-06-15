#include "./chart.hpp"

#include "native/core/event/event.hpp"

namespace {

uint32_t maxLinePointCount(const std::vector<axis_data>& data) {
    uint32_t max_count = 0;
    for (const auto& item : data) {
        if (item.data.size() > max_count) {
            max_count = static_cast<uint32_t>(item.data.size());
        }
    }
    return max_count;
}

uint32_t maxScatterPointCount(const std::vector<axis_data>& data) {
    uint32_t max_count = 0;
    for (const auto& item : data) {
        uint32_t pair_count = static_cast<uint32_t>(item.data.size() / 2);
        if (pair_count > max_count) {
            max_count = pair_count;
        }
    }
    return max_count;
}

void ensurePointCount(lv_obj_t* chart, uint32_t count) {
    if (count == 0) {
        return;
    }
    if (lv_chart_get_point_count(chart) != count) {
        lv_chart_set_point_count(chart, count);
    }
}

}  // namespace

Chart::Chart(std::string uid, lv_obj_t* parent): BasicComponent(uid) {
    this->type = COMP_TYPE_CHART;

    this->uid = uid;
    lv_obj_t* parent_obj = parent != nullptr ? parent : GetWindowInstance();

    this->instance = lv_obj_create(parent_obj);
    LV_ASSERT_NULL(this->instance);
    lv_obj_remove_style_all(this->instance);
    lv_obj_set_flex_flow(this->instance, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_user_data(this->instance, this);

    this->chart_obj = lv_chart_create(this->instance);
    LV_ASSERT_NULL(this->chart_obj);
    lv_obj_set_width(this->chart_obj, lv_pct(100));
    lv_obj_set_flex_grow(this->chart_obj, 1);

    lv_group_add_obj(lv_group_get_default(), this->instance);

    lv_obj_add_flag(this->chart_obj, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_flag(this->chart_obj, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_clear_flag(this->chart_obj, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_set_user_data(this->chart_obj, this);

    this->initStyle(LV_PART_MAIN);
};

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
    return this->styleTargetMain();
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

void Chart::setScaleX (int32_t value) {
    lv_obj_t* wrapper = this->styleTargetMain();
    lv_obj_t* chart = this->styleTargetChart();

    // value is chart-scaleX from NormalizeScale (256 = 1.0, e.g. scaleX(3) -> 768).
    float factor = value / 256.0f;

    if (factor > 1.0f) {
        lv_obj_add_flag(wrapper, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scroll_dir(wrapper, (lv_dir_t)(lv_obj_get_scroll_dir(wrapper) | LV_DIR_HOR));
        lv_obj_set_flex_grow(chart, 1);
        lv_obj_set_width(chart, lv_pct((int32_t)(factor * 100.0f)));
    } else {
        lv_obj_set_width(chart, lv_pct(100));
        lv_obj_set_flex_grow(chart, 1);
        lv_dir_t dir = (lv_dir_t)(lv_obj_get_scroll_dir(wrapper) & ~LV_DIR_HOR);
        lv_obj_set_scroll_dir(wrapper, dir);
        if (dir == LV_DIR_NONE) {
            lv_obj_clear_flag(wrapper, LV_OBJ_FLAG_SCROLLABLE);
        }
    }
}

void Chart::setScaleY (int32_t value) {
    lv_obj_t* wrapper = this->styleTargetMain();
    lv_obj_t* chart = this->styleTargetChart();

    // value is chart-scaleY from NormalizeScale (256 = 1.0, e.g. scaleY(2) -> 512).
    float factor = value / 256.0f;

    if (factor > 1.0f) {
        lv_obj_add_flag(wrapper, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scroll_dir(wrapper, (lv_dir_t)(lv_obj_get_scroll_dir(wrapper) | LV_DIR_VER));
        lv_obj_set_flex_grow(chart, 1);
        lv_obj_set_height(chart, lv_pct((int32_t)(factor * 100.0f)));
    } else {
        lv_obj_set_height(chart, lv_pct(100));
        lv_obj_set_flex_grow(chart, 1);
        lv_dir_t dir = (lv_dir_t)(lv_obj_get_scroll_dir(wrapper) & ~LV_DIR_VER);
        lv_obj_set_scroll_dir(wrapper, dir);
        if (dir == LV_DIR_NONE) {
            lv_obj_clear_flag(wrapper, LV_OBJ_FLAG_SCROLLABLE);
        }
    }
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
    (void)major_len;
    (void)minor_len;
    (void)major_num;
    (void)minor_num;
    (void)draw_size;
};

void Chart::setRightAxisOption (
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
    (void)major_len;
    (void)minor_len;
    (void)major_num;
    (void)minor_num;
    (void)draw_size;
};

void Chart::setLeftAxisData (std::vector<axis_data>& data) {
    lv_obj_t* chart = this->styleTargetChart();

    ensurePointCount(chart, maxLinePointCount(data));
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

    ensurePointCount(chart, maxLinePointCount(data));
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
};

void Chart::setRightAxisLabels (std::vector<std::string>& labels) {
    this->right_axis_labels = labels;
};

void Chart::setTopAxisLabels (std::vector<std::string>& labels) {
    this->top_axis_labels = labels;
};

void Chart::setBottomAxisLabels (std::vector<std::string>& labels) {
    this->bottom_axis_labels = labels;
};

void Chart::setLeftAxisRange (int32_t min, int32_t max) {
    lv_chart_set_axis_range(this->styleTargetChart(), LV_CHART_AXIS_PRIMARY_Y, min, max);
};

void Chart::setRightAxisRange (int32_t min, int32_t max) {
    lv_chart_set_axis_range(this->styleTargetChart(), LV_CHART_AXIS_SECONDARY_Y, min, max);
};

void Chart::setTopAxisRange (int32_t min, int32_t max) {
    lv_chart_set_axis_range(this->styleTargetChart(), LV_CHART_AXIS_SECONDARY_X, min, max);
};

void Chart::setBottomAxisRange (int32_t min, int32_t max) {
    lv_chart_set_axis_range(this->styleTargetChart(), LV_CHART_AXIS_PRIMARY_X, min, max);
};

void Chart::setScatterData (std::vector<axis_data>& data) {
    lv_obj_t* chart = this->styleTargetChart();

    ensurePointCount(chart, maxScatterPointCount(data));
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
