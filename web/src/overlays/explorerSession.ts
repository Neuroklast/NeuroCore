export type ExplorerScope = "all" | "factory" | "user";

export type ExplorerSession = {
  q: string;
  scope: ExplorerScope;
  cat: string;
  selectedName: string;
  folderScroll: number;
  listScroll: number;
};

export const explorerSession: ExplorerSession = {
  q: "",
  scope: "all",
  cat: "",
  selectedName: "",
  folderScroll: 0,
  listScroll: 0,
};

export function patchExplorer(partial: Partial<ExplorerSession>): ExplorerSession {
  Object.assign(explorerSession, partial);
  return explorerSession;
}
