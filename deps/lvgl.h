#pragma once

#ifndef LVGL_H_WRAPPER
#define LVGL_H_WRAPPER

#include "lvgl/lvgl.h"
#include "lv_drv_conf.h"

#ifdef __cplusplus
extern "C" {
#endif

/* For migrating from lvgl 8.2 to lvgl 9.5 */
static inline void lv_gif_pause(lv_obj_t * obj)
{
    lv_gif_t *gifobj = (lv_gif_t *)obj;
    if (gifobj->gif != NULL && gifobj->timer != NULL) {
        lv_timer_pause(gifobj->timer);
    }
}

static inline void lv_gif_resume(lv_obj_t * obj)
{
    lv_gif_t *gifobj = (lv_gif_t *)obj;
    if (gifobj->gif != NULL && gifobj->timer != NULL) {
        lv_timer_resume(gifobj->timer);
    }
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LVGL_H_WRAPPER */
