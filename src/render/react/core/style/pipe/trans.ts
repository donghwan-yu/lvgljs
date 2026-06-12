import { ANIM_PATH } from "../../lv_types";
import {
  type LvStylePropId,
  TRANSITION_PROP,
  TRANSITION_PROP_ALIAS,
  TRANSITION_TYPO_HINTS,
} from "../../style_prop";
import { STYLE_PROP, StyleTransformResult } from "../batch";
import type { TransStyleType } from "../type";
import {
  NormalizeDeg,
  NormalizePx,
  NormalizeScale,
  NormalizeTime,
} from "../util";

const transformSupportKeys = [
  "translate",
  "translate-x",
  "translate-y",
  "scale",
  "scaleX",
  "scaleY",
  "rotate",
  "transform-width",
  "transform-height",
];

function resolveTransitionProp(name: string): LvStylePropId | undefined {
  const typo =
    TRANSITION_TYPO_HINTS[name as keyof typeof TRANSITION_TYPO_HINTS];
  if (typo) name = typo;
  const alias =
    TRANSITION_PROP_ALIAS[name as keyof typeof TRANSITION_PROP_ALIAS];
  if (alias) name = alias;
  return TRANSITION_PROP[name as keyof typeof TRANSITION_PROP];
}

export function TransStyle(
  style: TransStyleType,
  result: StyleTransformResult,
  compName?: string,
) {
  const batch = result.batch;

  if (style["transition-property"]) {
    const properties = style["transition-property"]
      .split(",")
      .map((item) => item.replace(/\s/g, ""))
      .map((item) => resolveTransitionProp(item))
      .filter((item): item is LvStylePropId => item !== undefined);

    const funcKey = style["transition-timing-function"] || "linear";
    result.transition = [
      properties.length,
      properties,
      NormalizeTime(style["transition-duration"]) ?? 0,
      ANIM_PATH[funcKey] ?? ANIM_PATH.linear,
      NormalizeTime(style["transition-delay"]) ?? 0,
    ];
  }

  // No early return here: transform-origin below must apply even when
  // `transform` is absent or unparseable.
  if (style["transform"]) {
    const value = style["transform"];
    const matches = value.match(/[a-zA-Z\-]+\([^\(]+\)/g) ?? [];
    const reg = /^([a-zA-Z\-]+)\((.+)\)$/;
    for (const item of matches) {
      const matched = item.match(reg);
      if (!matched) continue;
      const prop = matched[1];
      const val = matched[2];
      if (!transformSupportKeys.includes(prop) || !val) continue;

      if (prop === "translate") {
        const parts = val.split(",");
        batch.push(STYLE_PROP.translateX, NormalizePx(parts[0]));
        batch.push(STYLE_PROP.translateY, NormalizePx(parts[1]));
      } else if (prop === "translate-x" || prop === "translate-y") {
        batch.push(
          prop === "translate-x"
            ? STYLE_PROP.translateX
            : STYLE_PROP.translateY,
          NormalizePx(val),
        );
      } else if (prop === "scale") {
        batch.push(
          compName === "Image" ? STYLE_PROP["img-scale"] : STYLE_PROP.scale,
          NormalizeScale(val),
        );
      } else if (prop === "rotate") {
        batch.push(
          compName === "Image" ? STYLE_PROP["img-rotate"] : STYLE_PROP.rotate,
          NormalizeDeg(val),
        );
      } else if (prop === "scaleX" || prop === "scaleY") {
        if (compName === "Chart") {
          batch.push(STYLE_PROP[`chart-${prop}`], NormalizeScale(val));
        } else if (compName === "Image") {
          // LVGL only has uniform zoom outside Chart; Image maps either axis to
          // img-scale, other components don't support per-axis scaling at all.
          batch.push(STYLE_PROP["img-scale"], NormalizeScale(val));
        } else {
          // Fallback to uniform scale for other components as they don't support
          // per-axis scaling yet.
          batch.push(STYLE_PROP["scale"], NormalizeScale(val));
        }
      } else if (prop === "transform-width" || prop === "transform-height") {
        batch.push(STYLE_PROP[prop], NormalizePx(val));
      }
    }
  }

  if (style["transform-origin"] && compName === "Image") {
    const [x, y] = style["transform-origin"].trim()?.split(" ");
    batch.push(STYLE_PROP["img-origin"], [NormalizePx(x), NormalizePx(y)]);
  }
}
