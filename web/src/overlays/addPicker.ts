import { ADD_CATEGORIES, ADDABLE_BLOCKS, blocksInCategory, type AddableBlock } from "../assemble/addBlock";

export const ADD_ITEM_H = 28;
export const ADD_HEAD_H = 22;
export const ADD_PAD = 8;
export const ADD_CAT_W = 96;
export const ADD_LIST_W = 148;

/** One split panel: categories stay put, only the block list may scroll. */
export function addPickerSize(category = ADD_CATEGORIES[0] as string): { w: number; h: number } {
  const cats = ADD_CATEGORIES.length;
  const blocks = Math.max(blocksInCategory(category).length, 1);
  return {
    w: ADD_CAT_W + ADD_LIST_W + ADD_PAD * 2,
    h: ADD_HEAD_H + ADD_PAD * 2 + Math.max(cats, blocks) * ADD_ITEM_H,
  };
}

export function addPickerBlocks(category: string): AddableBlock[] {
  return blocksInCategory(category);
}

/** Spotlight filter. Empty query lists the category (or the whole catalog). */
export function addPickerSearch(query: string, category?: string): AddableBlock[] {
  const q = query.trim().toLowerCase();
  const pool = category ? blocksInCategory(category) : ADDABLE_BLOCKS;
  if (! q) {
    return pool;
  }
  return pool.filter((b) => (
    b.label.toLowerCase().includes(q)
    || b.type.toLowerCase().includes(q)
    || b.category.toLowerCase().includes(q)
  ));
}

export function addPickerListsEveryBlock(): string[] {
  return ADDABLE_BLOCKS.map((b) => b.label);
}

/** Root context chrome never scrolls. Only the block column may. */
export function addMenuOverflow(part: "root" | "list"): "visible" | "auto" {
  return part === "list" ? "auto" : "visible";
}
