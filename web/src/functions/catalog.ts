import catalog from "./functions_en.json";
import { categoryForName } from "./plotModel";

export interface FunctionEntry {
  name: string;
  description: string;
  soundCharacter: string;
  example: string;
  category: string;
  useCases: string[];
}

type Raw = {
  functions?: Array<{
    name?: string;
    description?: string;
    soundCharacter?: string;
    example?: string;
    useCases?: string[];
  }>;
};

export function loadFunctions(): FunctionEntry[] {
  const raw = catalog as Raw;
  const list = Array.isArray(raw.functions) ? raw.functions : [];
  return list.map((f) => ({
    name: String(f.name ?? ""),
    description: String(f.description ?? ""),
    soundCharacter: String(f.soundCharacter ?? ""),
    example: String(f.example ?? ""),
    category: categoryForName(String(f.name ?? "")),
    useCases: Array.isArray(f.useCases) ? f.useCases.map(String) : [],
  })).filter((f) => f.name.length > 0);
}

export function categoriesOf(list: FunctionEntry[]): Array<{ name: string; count: number }> {
  const counts = new Map<string, number>();
  for (const f of list) {
    counts.set(f.category, (counts.get(f.category) ?? 0) + 1);
  }
  return [
    { name: "All", count: list.length },
    ...["Core", "Drive", "Crush", "Blocks"]
      .filter((n) => (counts.get(n) ?? 0) > 0)
      .map((n) => ({ name: n, count: counts.get(n) ?? 0 })),
  ];
}
