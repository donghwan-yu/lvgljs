import { StyleProps } from "./index";
import { isValidUrl } from "../../utils/helpers";
import { fetchAssetBinary, loadLocalAsset } from "../../utils/assets";
import { BUILT_IN_SYMBOL } from "./symbol";

export function PostProcessStyle({ comp, styleSheet, styleType }: { comp: any; styleSheet: StyleProps; styleType: any }) {
  if (styleSheet["background-image"] !== void 0) {
    let url = styleSheet["background-image"];

    if (BUILT_IN_SYMBOL[url]) {
      comp.setBackgroundImage(null, styleType, BUILT_IN_SYMBOL[url]);
      return;
    }

    if (url === null) {
      comp.setBackgroundImage(null, styleType);
    } else if (!isValidUrl(url)) {
      loadLocalAsset(url, "setBackground error", (buffer) =>
        comp.setBackgroundImage(buffer, styleType));
    } else {
      fetchAssetBinary(url, {})
        .then((buffer) => comp.setBackgroundImage(buffer, styleType))
        .catch(console.warn);
    }
  }

  if (styleSheet["arc-image"] !== void 0) {
    let url = styleSheet["arc-image"];

    if (BUILT_IN_SYMBOL[url]) {
      comp.setArcImage(null, styleType, BUILT_IN_SYMBOL[url]);
      return;
    }

    if (url === null) {
      comp.setArcImage(null, styleType);
    } else if (!isValidUrl(url)) {
      loadLocalAsset(url, "setArcImage error", (buffer) =>
        comp.setArcImage(buffer, styleType));
    } else {
      fetchAssetBinary(url, {})
        .then((buffer) => comp.setArcImage(buffer, styleType))
        .catch(console.warn);
    }
  }
}
