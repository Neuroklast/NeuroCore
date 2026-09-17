/** Source spans retain original whitespace/comments; offsets always address the original text. */
export type SourceBlock = { id: string; start: number; end: number; body: number };
export function codeMask(text: string): string {
  return text.replace(/#[^\r\n]*|\/\/[^\r\n]*/g, m => ' '.repeat(m.length));
}
export function sourceBlocks(text: string): SourceBlock[] {
  const mask = codeMask(text), blocks: SourceBlock[] = [];
  let depth = 0, start = 0, current: SourceBlock | undefined;
  for (let end = 0; end <= mask.length; ++end) {
    if (end !== mask.length && mask[end] !== '\n') continue;
    const line = mask.slice(start, end);
    const head = depth === 0 ? line.match(/^\s*([a-z_][a-z_0-9]*|bus\s+[a-z_][a-z_0-9]*)\s*:/i) : null;
    if (head) {
      current = { id: head[1].toLowerCase(), start, end: end < mask.length ? end + 1 : end, body: start + head[0].length };
      blocks.push(current);
    } else if (current && (depth > 0 || /^\s*;/.test(line))) current.end = end < mask.length ? end + 1 : end;
    for (const c of line) {
      if (c === '(' || c === '[') ++depth;
      if (c === ')' || c === ']') depth = Math.max(0, depth - 1);
    }
    start = end + 1;
  }
  return blocks;
}
export function sourceBlock(text: string, id: string): SourceBlock | undefined {
  return sourceBlocks(text).find(b => b.id === id.toLowerCase());
}
export function replaceSourceArg(text: string, id: string, key: string, value: string): string {
  const b = sourceBlock(text, id);
  if (!b || !/^[a-z_][a-z_0-9]*$/i.test(key)) return text;
  const mask = codeMask(text);
  const args = mask.slice(b.body, b.end);
  let depth = 0, begin = 0;
  for (let i = 0; i <= args.length; ++i) {
    const c = args[i];
    if (c === '(' || c === '[') ++depth;
    if (c === ')' || c === ']') --depth;
    if (i !== args.length && (c !== ';' || depth !== 0)) continue;
    const segment = args.slice(begin, i);
    const match = segment.match(/^\s*([a-z_][a-z_0-9]*)\s*=\s*/i);
    if (match?.[1].toLowerCase() === key.toLowerCase()) {
      const from = b.body + begin + match[0].length;
      const to = b.body + begin + segment.trimEnd().length;
      const comments = text.slice(from, to).match(/#[^\r\n]*|\/\/[^\r\n]*/g) ?? [];
      const retained = comments.length ? '\n' + comments.join('\n') + '\n' : '';
      return text.slice(0, from) + value + retained + text.slice(to);
    }
    begin = i + 1;
  }
  const at = b.body + args.trimEnd().length;
  return text.slice(0, at) + `; ${key} = ${value}` + text.slice(at);
}
export function renameSourceToken(text: string, from: string, to: string): string {
  if (!/^[a-z_][a-z_0-9]*$/i.test(to) || !sourceBlock(text, from) || sourceBlock(text, to)) return text;
  const mask = codeMask(text);
  const edits = [...mask.matchAll(/\b[a-z_][a-z_0-9]*\b/gi)].filter(m => m[0].toLowerCase() === from.toLowerCase());
  for (const m of edits.reverse()) text = text.slice(0, m.index) + to + text.slice(m.index! + m[0].length);
  return text;
}

export function logicalSourceLines(text: string): string[] {
  const mask = codeMask(text);
  const blocks = sourceBlocks(text);
  let folded = text;
  for (const b of blocks.reverse()) {
    const original = text.slice(b.start, b.end).trimEnd();
    if (!original.includes('\n')) continue;
    const part = mask.slice(b.start, b.end).trimEnd();
    const comment = original.split('\n')[0].match(/#[^\r\n]*|\/\/[^\r\n]*/)?.[0];
    folded = folded.slice(0, b.start) + part.replace(/\r?\n/g, ' ') + (comment ? ' ' + comment : '') + '\n' + folded.slice(b.end);
  }
  return folded.split(/\r?\n/);
}
