"""Missing WebView2 must not leave a blank Component."""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
holder = (root / 'src/bridge/WebViewHolder.cpp').read_text(encoding='utf-8')
assert 'makeWebViewUnavailableSurface' in holder, holder
assert 'std::make_unique<juce::Component>()' not in holder
policy = (root / 'src/bridge/WebEditorPolicy.h').read_text(encoding='utf-8')
assert 'webViewUnavailableMessage' in policy
assert 'go.microsoft.com/fwlink/p/?LinkId=2124703' in policy
print('WebView unavailable surface is a repair diagnostic: PASS')
