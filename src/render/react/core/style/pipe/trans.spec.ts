import assert from "node:assert/strict";
import { describe, it } from "node:test";

import "../../lv.setup.spec";
import { ANIM_PATH } from "../../lv_types";
import { TRANSITION_PROP } from "../../style_prop";
import { runPipeFull } from "../pipe.harness";
import { TransStyle } from "./trans";

describe("TransStyle pipe", () => {
  it("builds transition tuple from TRANSITION_PROP", () => {
    const { result } = runPipeFull(TransStyle, {
      "transition-property": "width, opacity",
      "transition-duration": "300ms",
      "transition-timing-function": "linear",
      "transition-delay": "100ms",
    });
    assert.deepEqual(result.transition, [
      2,
      [TRANSITION_PROP.width, TRANSITION_PROP.opacity],
      300,
      ANIM_PATH.linear,
      100,
    ]);
  });

  it("resolves transition aliases and typo hints", () => {
    const { result } = runPipeFull(TransStyle, {
      "transition-property": "padding-row, boder-post",
    });
    assert.deepEqual(result.transition?.[1], [
      TRANSITION_PROP["row-spacing"],
      TRANSITION_PROP["border-post"],
    ]);
  });

  it("parses translate and rotate into batch props", () => {
    const { map } = runPipeFull(TransStyle, {
      transform: "translate(10px, 20px) rotate(45deg)",
    });
    assert.equal(map.get("translateX"), 10);
    assert.equal(map.get("translateY"), 20);
    assert.equal(map.get("rotate"), 45);
  });

  it("pushes img-origin for Image transform-origin", () => {
    const { map } = runPipeFull(
      TransStyle,
      { "transform-origin": "10px 20px" },
      "Image",
    );
    assert.deepEqual(map.get("img-origin"), [10, 20]);
  });

  it("filters unknown transition properties", () => {
    const { result } = runPipeFull(TransStyle, {
      "transition-property": "width, not-a-prop, opacity",
    });
    assert.equal(result.transition?.[0], 2);
    assert.deepEqual(result.transition?.[1], [
      TRANSITION_PROP.width,
      TRANSITION_PROP.opacity,
    ]);
  });

  it("builds empty transition prop list when all names are unknown", () => {
    const { result } = runPipeFull(TransStyle, {
      "transition-property": "foo, bar",
    });
    assert.deepEqual(result.transition?.[0], 0);
    assert.deepEqual(result.transition?.[1], []);
  });

  it("does not set transition when transition-property is absent", () => {
    const { result } = runPipeFull(TransStyle, {
      "transition-duration": "200ms",
    });
    assert.equal(result.transition, undefined);
  });

  it("ignores transform-origin for non-Image components", () => {
    const { map } = runPipeFull(TransStyle, {
      "transform-origin": "10px 20px",
    });
    assert.equal(map.has("img-origin"), false);
  });

  describe("transform parsing", () => {
    it("parses translate-x and translate-y", () => {
      const { map } = runPipeFull(TransStyle, {
        transform: "translate-x(5px) translate-y(15px)",
      });
      assert.equal(map.get("translateX"), 5);
      assert.equal(map.get("translateY"), 15);
    });

    describe("scale and rotate routing (trans.ts scale/rotate/scaleX/scaleY branches)", () => {
      it("maps scale() to img-scale for Image", () => {
        const { map } = runPipeFull(
          TransStyle,
          { transform: "scale(1.5)" },
          "Image",
        );
        assert.equal(map.get("img-scale"), 384);
        assert.equal(map.has("scale"), false);
      });

      it("maps scale() to scale for non-Image components (View, GIF, etc.)", () => {
        for (const compName of ["View", "GIF"] as const) {
          const { map } = runPipeFull(
            TransStyle,
            { transform: "scale(2)" },
            compName,
          );
          assert.equal(map.get("scale"), 512, compName);
          assert.equal(map.has("img-scale"), false, compName);
        }
      });

      it("maps rotate() to img-rotate for Image", () => {
        const { map } = runPipeFull(
          TransStyle,
          { transform: "rotate(90deg)" },
          "Image",
        );
        assert.equal(map.get("img-rotate"), 90);
        assert.equal(map.has("rotate"), false);
      });

      it("maps rotate() to rotate for non-Image components", () => {
        const { map } = runPipeFull(
          TransStyle,
          { transform: "rotate(45deg)" },
          "View",
        );
        assert.equal(map.get("rotate"), 45);
        assert.equal(map.has("img-rotate"), false);
      });

      it("maps scale and rotate to img-* props for Image in one transform", () => {
        const { map } = runPipeFull(
          TransStyle,
          { transform: "scale(0.5) rotate(180deg)" },
          "Image",
        );
        assert.equal(map.get("img-scale"), 128);
        assert.equal(map.get("img-rotate"), 180);
        assert.equal(map.has("scale"), false);
        assert.equal(map.has("rotate"), false);
      });

      it("maps scaleX/scaleY to chart-scaleX/Y for Chart", () => {
        const { map } = runPipeFull(
          TransStyle,
          { transform: "scaleX(2) scaleY(0.5)" },
          "Chart",
        );
        assert.equal(map.get("chart-scaleX"), 512);
        assert.equal(map.get("chart-scaleY"), 128);
        assert.equal(map.has("scale"), false);
        assert.equal(map.has("img-scale"), false);
      });

      it("maps uniform scale() on Chart to scale, not chart-scale", () => {
        const { map } = runPipeFull(
          TransStyle,
          { transform: "scale(2)" },
          "Chart",
        );
        assert.equal(map.get("scale"), 512);
        assert.equal(map.has("chart-scaleX"), false);
        assert.equal(map.has("img-scale"), false);
      });

      it("maps scaleX/scaleY to img-scale for Image (last axis wins)", () => {
        const { map } = runPipeFull(
          TransStyle,
          { transform: "scaleX(2) scaleY(0.5)" },
          "Image",
        );
        assert.equal(map.get("img-scale"), 128);
        assert.equal(map.has("scale"), false);
      });

      it("maps scaleX/scaleY to scale for non-Chart non-Image (last axis wins)", () => {
        for (const compName of ["View", "GIF"] as const) {
          const { map } = runPipeFull(
            TransStyle,
            { transform: "scaleX(1.25) scaleY(3)" },
            compName,
          );
          assert.equal(map.get("scale"), 768, compName);
          assert.equal(map.has("img-scale"), false, compName);
          assert.equal(map.has("chart-scaleX"), false, compName);
          assert.equal(map.has("chart-scaleY"), false, compName);
        }
      });
    });

    it("parses transform-width and transform-height", () => {
      const { map } = runPipeFull(TransStyle, {
        transform: "transform-width(100px) transform-height(50px)",
      });
      assert.equal(map.get("transform-width"), 100);
      assert.equal(map.get("transform-height"), 50);
    });

    it("parses a chained transform with multiple supported functions", () => {
      const { map } = runPipeFull(TransStyle, {
        transform:
          "translate(1px, 2px) scale(1) rotate(0deg) transform-width(10px)",
      });
      assert.equal(map.get("translateX"), 1);
      assert.equal(map.get("translateY"), 2);
      assert.equal(map.get("scale"), 256);
      assert.equal(map.get("rotate"), 0);
      assert.equal(map.get("transform-width"), 10);
    });

    it("ignores unsupported transform functions", () => {
      const { map } = runPipeFull(TransStyle, {
        transform: "skew(10deg) matrix(1,0,0,1,0,0) translate-x(4px)",
      });
      assert.equal(map.get("translateX"), 4);
      assert.equal(map.has("skew"), false);
      assert.equal(map.has("matrix"), false);
    });

    it("ignores empty transform values", () => {
      const { map } = runPipeFull(TransStyle, {
        transform: "scale() translate-x(8px)",
      });
      assert.equal(map.get("translateX"), 8);
      assert.equal(map.has("scale"), false);
      assert.equal(map.has("img-scale"), false);
    });
  });
});
