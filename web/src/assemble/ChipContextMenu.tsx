export interface ChipMenu {
  id: string;
  top?: number | false;
  left?: number | false;
  right?: number | false;
  bottom?: number | false;
}

export function ChipContextMenu({
  id,
  top,
  left,
  right,
  bottom,
  canDelete,
  onInspect,
  onDelete,
  onArrange,
  onClick,
}: ChipMenu & {
  canDelete: boolean;
  onInspect: () => void;
  onDelete: () => void;
  onArrange: () => void;
  onClick: () => void;
}) {
  return (
    <div
      className="nk-ctx"
      style={{
        top: top || undefined,
        left: left || undefined,
        right: right || undefined,
        bottom: bottom || undefined,
      }}
      onClick={onClick}
    >
      <button type="button" className="nk-ctx-item" onClick={onInspect}>Inspect</button>
      {canDelete ? (
        <button type="button" className="nk-ctx-item" onClick={onDelete}>Delete</button>
      ) : null}
      <button type="button" className="nk-ctx-item" onClick={onArrange}>Arrange</button>
      <div className="nk-ctx-id">{id}</div>
    </div>
  );
}
