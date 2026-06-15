#pragma once
#include <stdlib.h>
#include <vector>

#include "native/components/component.hpp"
#include "native/core/basic/comp.hpp"

typedef struct axis_data {
  int32_t color;
  std::vector<int32_t> data;
} axis_data;

class Chart final : public BasicComponent {
 public:
  Chart(std::string uid, lv_obj_t* parent = nullptr);

  lv_obj_t* chart_obj = nullptr;

  lv_obj_t* styleTargetMain() const {
    LV_ASSERT_NULL(this->instance);
    return this->instance;
  }
  lv_obj_t* styleTargetChart() const {
    LV_ASSERT_NULL(this->chart_obj);
    return this->chart_obj;
  }

  std::vector<lv_chart_series_t*> left_axis;
  std::vector<lv_chart_series_t*> bottom_axis;
  std::vector<lv_chart_series_t*> right_axis;
  std::vector<lv_chart_series_t*> top_axis;

  std::vector<std::string> left_axis_labels;
  std::vector<std::string> right_axis_labels;
  std::vector<std::string> top_axis_labels;
  std::vector<std::string> bottom_axis_labels;

  void setLeftAxisRange (int32_t min, int32_t max);
  void setRightAxisRange (int32_t min, int32_t max);
  void setTopAxisRange (int32_t min, int32_t max);
  void setBottomAxisRange (int32_t min, int32_t max);

  void setLeftAxisLabels (std::vector<std::string>& labels);
  void setRightAxisLabels (std::vector<std::string>& labels);
  void setBottomAxisLabels (std::vector<std::string>& labels);
  void setTopAxisLabels (std::vector<std::string>& labels);

  void setType (int32_t type);

  void setLeftAxisOption (
    int32_t major_len,
    int32_t minor_len,
    int32_t major_num,
    int32_t minor_num,
    int32_t draw_size
  );

  void setRightAxisOption (
    int32_t major_len,
    int32_t minor_len,
    int32_t major_num,
    int32_t minor_num,
    int32_t draw_size
  );

  void setTopAxisOption (
    int32_t major_len,
    int32_t minor_len,
    int32_t major_num,
    int32_t minor_num,
    int32_t draw_size
  );

  void setBottomAxisOption (
    int32_t major_len,
    int32_t minor_len,
    int32_t major_num,
    int32_t minor_num,
    int32_t draw_size
  );

  void setLeftAxisData (std::vector<axis_data>& data);
  void setRightAxisData (std::vector<axis_data>& data);
  void setBottomAxisData (std::vector<axis_data>& data);
  void setTopAxisData (std::vector<axis_data>& data);

  void setScatterData (std::vector<axis_data>& data);

  void setPointNum (int32_t num);

  void setDivLineCount (int32_t hdiv, int32_t vdiv);

  void setScaleX (int32_t value);
  void setScaleY (int32_t value);

  lv_obj_t* styleTarget(int32_t type) override;
};
