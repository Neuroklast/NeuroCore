/** Bind the packaged monaco. CDN vs-path is dead in the plugin WebView. */
export function bundledMonacoConfig<T>(monaco: T): { monaco: T } {
  return { monaco };
}
