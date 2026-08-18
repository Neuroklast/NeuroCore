export type ExplorerScope = "all" | "factory" | "user";

export type ExplorerSession = {
  q: string;
  scope: ExplorerScope;
  cat: string;
  sel: number;
  sortKey: "name" | "category";
  folderScroll: number;
  listScroll: number;
};

export const explorerSession: ExplorerSession = {
  q: "",
  scope: "all",
  cat: "",
  sel: 0,
  sortKey: "name",
  folderScroll: 0,
  listScroll: 0,
};

export function patchExplorer(partial: Partial<ExplorerSession>): ExplorerSession {
  Object.assign(explorerSession, partial);
  return explorerSession;
}
