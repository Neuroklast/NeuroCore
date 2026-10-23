export type ExplorerScope = "all" | "factory" | "user";

export type ExplorerSession = {
  q: string;
  scope: ExplorerScope;
  cat: string;
  sources: string[];
  types: string[];
  styles: string[];
  selectedName: string;
  folderScroll: number;
  listScroll: number;
};

export const explorerSession: ExplorerSession = {
  q: "",
  scope: "all",
  cat: "",
  sources: [],
  types: [],
  styles: [],
  selectedName: "",
  folderScroll: 0,
  listScroll: 0,
};

export function patchExplorer(partial: Partial<ExplorerSession>): ExplorerSession {
  Object.assign(explorerSession, partial);
  return explorerSession;
}
