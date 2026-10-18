import { useHostStore } from "../../store/hostStore";
import type { FaceProps } from "./faceKind";
import { cabFaceModel, cabMixPath, cabStems } from "./cabMix";

export function CabFace({ args, id, host }: FaceProps) {
  const model = cabFaceModel(args, host);
  const slot = (id ?? "").trim().toLowerCase();
  const ir = useHostStore((s) => s.irSlots.find((row) => row.slot.trim().toLowerCase() === slot));
  return (
    <div
      className="nk-node-face"
      data-glyph="mix"
      data-axis="mix"
      data-mix={String(model.mix)}
      data-gain={String(model.gain)}
      data-slot={slot}
      data-ir={ir?.name ?? ""}
      data-loaded={ir?.loaded ? "on" : "off"}
      aria-label="calculated cabinet mix"
    >
      <div className="nk-face-plot" data-axis="mix">
        <svg viewBox="0 0 100 40" preserveAspectRatio="none" aria-hidden>
          {cabStems(args, host).map((stem) => (
            <line key={stem.x.toFixed(2)} data-tap="mix" x1={stem.x} y1={40} x2={stem.x} y2={stem.y1} />
          ))}
          <path data-curve="mix" d={cabMixPath(args, host)} />
        </svg>
      </div>
    </div>
  );
}
