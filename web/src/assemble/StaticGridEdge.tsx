export function staticRoute(data: unknown): string {
  const r = (data as { route?: string } | undefined)?.route;
  return typeof r === "string" ? r : "";
}
