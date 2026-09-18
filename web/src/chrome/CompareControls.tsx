import { useEffect, useState } from 'react';
import { getNativeFunction, hasJuceBridge } from '../bridge/juce';
import { useAstStore } from '../store/astStore';
import { useHostStore } from '../store/hostStore';
import { compareClusterClass, comparePairClass } from './toolbarChrome';

export function CompareControls() {
  const [active, setActive] = useState(0);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState('');
  const match = useHostStore(s => s.autoGain);
  const native = hasJuceBridge();
  const draft = useAstStore(s => s.script !== s.lastValidScript);
  useEffect(() => {
    let live = true;
    if (native) void getNativeFunction('compare')({action:'status'}).then(raw => {
      const state = raw as {active?:number};
      if (live && (state.active === 0 || state.active === 1)) setActive(state.active);
    }).catch(e => { if (live) setError(String(e)); });
    return () => { live = false; };
  }, [native]);
  const run = async (action: 'switch' | 'copy', slot?: number) => {
    if (busy || draft) return;
    setBusy(true); setError('');
    try {
      const result = await getNativeFunction('compare')({action, slot}) as {ok?:boolean; active?:number; error?:string};
      if (result?.ok !== true) throw new Error(result?.error || 'Comparison failed.');
      if (result.active === 0 || result.active === 1) setActive(result.active);
    } catch (e) { setError(e instanceof Error ? e.message : String(e)); }
    finally { setBusy(false); }
  };
  const toggleMatch = async () => {
    setBusy(true); setError('');
    const value = match > 0 ? 0 : 1;
    try {
      await getNativeFunction('setParam')({id:'autoGain', value:match, gesture:'begin'});
      const result = await getNativeFunction('setParam')({id:'autoGain', value, gesture:'end'});
      if (result !== true) throw new Error('Level match could not be changed.');
      useHostStore.setState({autoGain:value});
    } catch (e) { setError(e instanceof Error ? e.message : String(e)); }
    finally { setBusy(false); }
  };
  const reason = !native ? 'Available in the plugin' : draft ? 'Save or discard the Terminal draft before comparing' : 'Compare complete sound states, including IRs and macro values';
  return <div className={compareClusterClass()} role="group" aria-label="Sound comparison" title={reason}>
    <div className={comparePairClass()} role="group" aria-label="Compare slot">
      {['A','B'].map((label, slot) => <button key={label} className={active === slot ? "on" : ""} aria-label={`Compare ${label}`} aria-pressed={active===slot} disabled={!native || busy || draft} onClick={() => void run('switch',slot)}>{label}</button>)}
    </div>
    <button className="nk-clip" disabled={!native || busy || draft} title="Overwrite the other comparison slot with the current sound" onClick={() => void run('copy')}>{active===0 ? 'A→B' : 'B→A'}</button>
    <button className={`nk-clip ${match > 0 ? "on" : ""}`} disabled={!native || busy} aria-pressed={match>0} title="Match processed level toward the dry reference using the existing RMS compensation. Not LUFS normalization." onClick={() => void toggleMatch()}>MATCH</button>
    {error ? <span role="alert" className="text-[11px] text-ink" title={error}>Comparison error</span> : null}
  </div>;
}
