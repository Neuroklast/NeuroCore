import type { FaceProps } from "./faceKind";
import { routingFaceModel, routingMapPath, routingStems } from "./routingMap";

export function RoutingFace({ type, args }: FaceProps) {
  const model = routingFaceModel(type, args);
  return (
    <div
      className="nk-node-face"
      data-glyph="map"
      data-axis="map"
      data-mode={model.mode}
      data-mix={String(model.mix)}
      data-width={String(model.width)}
      data-name={model.name}
      data-kanal={model.kanal}
      aria-label="calculated routing map"
    >
      <div className="nk-face-plot" data-axis="map">
        <svg viewBox="0 0 100 40" preserveAspectRatio="none" aria-hidden>
          {routingStems(type, args).map((stem, i) => (
            <line
              key={`${stem.y1.toFixed(2)}-${i}`}
              data-tap="route"
              x1={stem.x1}
              y1={stem.y1}
              x2={stem.x2}
              y2={stem.y2}
              opacity={Math.max(0.25, stem.amp)}
            />
          ))}
          <path data-curve="map" d={routingMapPath(type, args)} />
        </svg>
      </div>
    </div>
  );
}
