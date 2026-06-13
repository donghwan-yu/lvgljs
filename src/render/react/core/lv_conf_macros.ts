import { LV_COORD_MAX } from "./lv_conf";

/** deps/lvgl/src/misc/lv_area.h - LV_COORD_MAX === (1 << shift) - 1, so LV_COORD_MAX + 1 === 1 << shift */
export const _LV_COORD_TYPE_SHIFT = 31 - Math.clz32(LV_COORD_MAX + 1);

/** deps/lvgl/src/misc/lv_area.h - derived from _LV_COORD_TYPE_SHIFT */
export const LV_COORD_TYPE_MASK = 3 << _LV_COORD_TYPE_SHIFT;
export const LV_COORD_TYPE_SPEC = 1 << _LV_COORD_TYPE_SHIFT;

/** deps/lvgl/src/misc/lv_area.h */
export { LV_COORD_MAX };
export const LV_COORD_MIN = -LV_COORD_MAX;

/** deps/lvgl/src/extra/layouts/grid/lv_grid.h - #define LV_GRID_CONTENT (LV_COORD_MAX - 101) */
export const LV_GRID_CONTENT = LV_COORD_MAX - 101;

/**
 * Match LVGL LV_COORD_SET_SPEC(x) in deps/lvgl/src/misc/lv_area.h.
 * ORs the plain value with _LV_COORD_TYPE_SPEC (1 << _LV_COORD_TYPE_SHIFT).
 */
export function LV_COORD_SET_SPEC(x: number): number {
  return x | LV_COORD_TYPE_SPEC;
}

/** deps/lvgl/src/misc/lv_area.h - LV_COORD_SET_SPEC(2001) */
export const LV_SIZE_CONTENT = LV_COORD_SET_SPEC(2001);

/**
 * Match LVGL LV_GRID_FR(x) in deps/lvgl/src/extra/layouts/grid/lv_grid.h.
 * Encodes a fractional grid track (CSS `Nfr`); x is the weight vs other FR tracks.
 */
export function LV_GRID_FR(x: number): number {
  return LV_COORD_MAX - 100 + x;
}

/**
 * Match LVGL LV_PCT(x) in deps/lvgl/src/misc/lv_area.h.
 * Pass the result to native setters (e.g. width) instead of using separate *_pct keys.
 */
export function lv_pct(x: number): number {
  const plain = x < 0 ? 1000 - x : x;
  return LV_COORD_SET_SPEC(plain);
}
