import { HostConfig } from "react-reconciler";
import {
  ALIGN_TYPE_MAP,
  LV_DIR_MAP,
  PART_TYPE,
  STATE_TYPE,
} from "../core/lv_types";

export { handleEvent } from "../core/event";
export { unRegistEvent, EVENTTYPE_MAP } from "../core/event";
export { setStyle } from "../core/style";
import * as React from 'react';

const components = new Map<LvgljsComponentConfig<any, any>['tagName'], LvgljsComponentConfig<any, any>>();

export const getComponentByTagName = (tagName) => {
  const config = components.get(tagName);
  if (!config) {
    throw `Unknown component ${tagName}`;
  }
  return config;
};

export function registerComponent<Props, Comp>(
  config: LvgljsComponentConfig<Props, Comp>): React.ComponentType<Props> | string {
  if (components.has(config.tagName)) {
    throw `A component with tagName: ${config.tagName} already exists. This base component will be ignored`;
  }
  components.set(config.tagName, config);
  return config.tagName;
}

export function registerComponents<Props, Comp>(configs: LvgljsComponentConfig<Props, Comp>[]) {
  configs.forEach((config) => {
    if (components.has(config.tagName)) {
      throw `A component with tagName: ${config.tagName} already exists. This base component will be ignored`;
    }
    components.set(config.tagName, config);
  });
}

export const EAlignType = ALIGN_TYPE_MAP;

export const STYLE_TYPE = {
  ...PART_TYPE,
  ...STATE_TYPE,
} as const;

export const EDropdownlistDirection = LV_DIR_MAP;

/** Matches Dropdownlist::setArrowDir enum order in dropdownlist.hpp. */
export const EDropdownListArrowDirection = {
  up: 0,
  right: 1,
  down: 2,
  left: 3,
} as const;

export const styleGetterProp: readonly (string | symbol)[] = ["height", "width", "left", "top"] as const;

export type LvgljsComponentConfig<ComponentProps, ComponentInstance> = Pick<
  HostConfig<any, ComponentProps, any, ComponentInstance, any, any, any, any, any, any, any, any>,
  | "shouldSetTextContent"
  | "createInstance"
  | "commitMount"
  | "commitUpdate"
  | "commitUpdate"
  | "insertBefore"
  | "appendInitialChild"
  | "appendChild"
  | "removeChild"
> & { tagName: string };
