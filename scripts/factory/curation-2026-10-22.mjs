/** Reviewed factory decisions. Shared by generation and the one-shot migration. */
import { inferTags } from "../factoryTags.mjs";
const k=(name,min,max,def)=>({name,min,max,default:def});
const make=(name,category,description,body,knobs={})=>{
  const defs=Object.entries(knobs).map(([a,p])=>`param ${a} = ${p.name} [${p.min}, ${p.max}]`).join("\n");
  const script=`# ${name}\n# ${description}\n${defs}\n${body.trim()}\n`;
  const preset={name,category,description,inputGain:0,outputGain:0,mix:1,script,tags:inferTags(script,name,description,category)};
  for(const [a,p] of Object.entries(knobs))preset["param"+a.toUpperCase()]=p;
  return preset;
};

export const retired = {
  "Radio Voice": ["Radio Band Vocal","Overlapping crushed bandpass voice; the retained version exposes both bit depth and drive."],
  "AM Radio Narrow": ["Radio Band Vocal","Same narrow-radio job, consolidated into the retained bandpass/crush controls."],
  "Dialogue Phone": ["Telephone Cup","Same highpass/lowpass/diode topology and overlapping cutoff ranges."],
  "Telephone EQ": ["Telephone Cup","Phone coloration consolidated into one configurable low/high/Q/drive preset."],
  "Loudness Safe": ["Streaming Ceiling","80 Hz highpass and hard clipping are not a transparent mastering safety tool."],
  "Sidechain Pump": ["Sidechain Pad EDM","The retired script was LFO tremolo with no sidechain input. Use real ducking or the new patterned pump."],
  "Vocal Opto Soft": ["Optical Leveler Comp","Low-ratio leveler overlaps the retained optical-style compressor, whose ratio range now includes 2:1."],
  "Vocal Comp": ["Opto Leveler","Generic one-compressor preset overlaps the retained controllable vocal leveler."],
  "Hard Clip Club": ["Transient Clip Bus","Overlaps Drum Clip Bus; replaced by the explicit clipper with threshold/knee/trim."],
  "Drum Clip Bus": ["Transient Clip Bus","Same insert clipping job as Hard Clip Club, consolidated in the typed clipper."],
  "Mixbus Satin": ["Transformer Bus Cushion","Near-identical default response to Loudness Clip Master; replaced by a dedicated transformer-style saturation job."],
  "Dual Path Regen": ["Drum Bus Regen","The retired script contains no parallel paths despite its name, and overlaps the feedback coloration."],
};
export const revisions = new Map();
const revise=(name,category,description,body,knobs,reason)=>revisions.set(name,{preset:make(name,category,description,body,knobs),reason});

revise("Chopper","Modulation","Eight-step accented gate at host-synced sixteenths. Gate controls note length; Floor preserves the quiet steps.",
`stepseq1: steps = 8; sync = 1/16; values = 1,0,0.7,0,1,0.35,0,0.7; gate = a; glide = 0.01
stage1: y = x * (c + (1-c) * (1-b+b*stepseq1))`,
{a:k("Gate",.2,1,.7),b:k("Depth",.25,1,.8),c:k("Floor",.03,.5,.12)},
"Use the sequencer for actual editable accents instead of a fixed square LFO.");

revise("Tape Wobble","Lo-Fi","Wet short-delay wow and flutter with seeded slow drift. No control oscillator is added to the audio.",
`random1: rate = 0.7; min = -1; max = 1; smooth = 0.9; seed = 149
delay1: time = 22 + random1 * b; feedback = 0; mix = 1; wow = a; rate = c; flutter = 0.12; damp = 9000
saturate1: mode = tape; drive = 1.5; dynamics = 0.3; character = 0.25; tone = 0.55; mix = 0.25
filter1: type = lowpass; cutoff = 10000; resonance = 0.3`,
{a:k("Wow",0,1,.35),b:k("Drift",0,3,1.1),c:k("Rate",.1,3,.6)},
"Replace additive sub-audio LFO/feedback with actual time variation and tape saturation.");

revise("Vocal Double","Vocals","Dry vocal plus a delayed micro-pitch bus. Detune is in semitones; no DC offset or feedback-only fake detune.",
`stage1: y = x
bus double:
 send: in = 1
 pitch1: semitones = a; mix = 1; formant = 1
 delay1: time = b; feedback = 0; mix = 1; damp = 8500
 filter1: type = highpass; cutoff = 140; resonance = 0.4
out: main = 1; double = c`,
{a:k("Detune",-.18,.18,.07),b:k("Delay",12,45,24),c:k("Blend",.08,.5,.22)},
"Replace DC-injecting formula with real micro-pitch plus a separate delayed bus.");

revise("Arp Shimmer","Synth","Parallel octave-up delay with a highpassed return. Keep the dry arpeggio intact; Sparkle sets the return highpass.",
`stage1: y = x
bus shimmer:
 send: in = 1
 pitch1: semitones = 12; mix = 1; formant = 1
 delay1: time = 187.5; feedback = 0.28; mix = 1; damp = 11000
 filter1: type = highpass; cutoff = a; resonance = 0.35
out: main = 1; shimmer = b`,
{a:k("Sparkle",1500,7000,3200),b:k("Shimmer",.08,.55,.25)},
"Remove inaudible additive LFO and preserve the dry signal around an actual octave return.");

revise("Broken Grid Chaos","Creative","Seeded sample-and-hold steps drive cutoff and crush depth. A second smooth random lane moves the delay feedback.",
`filter1: type = highpass; cutoff = 85; resonance = 0.4
random1: rate = 100; min = 0; max = 1; seed = 735
samplehold1: ina = random1; rate = a; slew = 0.02
random2: rate = d; min = 0; max = 1; smooth = 0.8; seed = 832
filter2: type = lowpass; cutoff = 400 + samplehold1 * 3000; resonance = 0.85
stage1: y = bitcrush(x,b+samplehold1*3)
delay1: time = 80; feedback = 0.4 + random2 * 0.22; mix = 0.35; damp = 5000
stage2: y = fold(x,-0.2*c,0.6*c)
filter3: type = lowpass; cutoff = 6800; resonance = 0.32
gate1: threshold = e; hyst = 6; hold = 0.06; range = -18; attack = 0.001; release = 0.06
stage3: y = x * f`,
{a:k("Chaos Rate",4,18,9),b:k("Crush Depth",4,10,7),c:k("Fold Asym",.3,.8,.5),d:k("Delay Chaos",.2,1.5,.6),e:k("Gate Thresh",-42,-30,-38),f:k("Mix",.5,1,.8)},
"Replace periodic square/triangle approximations with actual seeded Random and SampleHold sources.");

revise("Glitch Hold Gate","Creative","A seeded held control value sets each sixteenth's amplitude. The threshold gate keeps background noise out.",
`random1: rate = 200; min = 0; max = 1; seed = 412
samplehold1: ina = random1; sync = 1/16; slew = 0.01
gate1: threshold = b; hyst = 3; hold = 0.01; range = -70
stage1: y = lerp(x,x*(1-a+a*samplehold1),c)`,
{a:k("Depth",.5,1,.85),b:k("Open",-30,-8,-18),c:k("Mix",.3,1,.8)},
"Replace freq=1/16 slow oscillator with genuine host-synced held steps.");

revise("Acoustic Body","Guitar","Low-mid body shelf and gentle transient definition for thin piezo recordings. The full frequency range remains present.",
`filter1: type = highpass; cutoff = 45; resonance = 0.4
eq1: type = peak; freq = a; q = 0.75; gain = b
transient1: attack = c; sustain = 0; mix = 0.4
filter2: type = lowpass; cutoff = 15000; resonance = 0.3`,
{a:k("Body",120,600,280),b:k("Body Gain",0,6,2.5),c:k("Pick",0,1,.2)},
"Replace full-band removal by bandpass with a body EQ that retains acoustic transients.");

revise("Radio Band Vocal","Vocals","Crushed radio band with adjustable drive, bit depth, center and width. Insert coloration for speech and sung vocals.",
`filter1: type = bandpass; center = b; width = c
stage1: y = bitcrush(softclip(x,d),a)
filter2: type = lowpass; cutoff = 4500; resonance = 0.3`,
{a:k("Bits",5,12,8),b:k("Center",700,2300,1400),c:k("Width",350,2000,1000),d:k("Drive",1,4,1.4)},
"Consolidate the overlapping crushed-radio presets into one controllable voicing.");

revise("Telephone Cup","Vocals","Adjustable telephone band with diode grit and resonant cutoff edges. One phone coloration for dialogue, vocals and instruments.",
`filter1: type = highpass; cutoff = b; resonance = d
filter2: type = lowpass; cutoff = c; resonance = d
stage1: y = diode(x,a)`,
{a:k("Grit",.5,3.5,1.3),b:k("Low",120,700,380),c:k("High",2200,5000,3200),d:k("Resonance",.25,1.2,.4)},
"Consolidate dialogue/telephone variants with the full useful cutoff and resonance range.");

revise("Bitcrush Glitch","Sound Design","Clock-jittered decimation with a fixed sixteenth gate pattern. The effect stays silent without input.",
`decimate1: rate = b; bits = a; jitter = 0.3; hold = 0; recon = 0.6; mix = 0.85
stepseq1: steps = 8; sync = 1/16; values = 1,0.1,0.7,0,1,0.3,0,0.9; gate = 0.7; glide = 0.02
stage1: y = x*(0.25+0.75*stepseq1)`,
{a:k("Bits",2,12,7),b:k("Clock",.05,.75,.35)},
"Remove the audio-rate oscillator injected into silent input; use actual clock reduction and a sequenced gain pattern.");

revise("Dark Atmosphere","Ambient","Low noise generator layered under the source, with seeded slow filter drift. Fog sets the noise level, including when input is silent.",
`random1: rate = 0.18; min = 0; max = 1; smooth = 0.9; seed = 279
stage1: y = x*(1-b)+noise(t*sr)*b
filter1: type = highpass; cutoff = 20; resonance = 0.35
filter2: type = lowpass; cutoff = a*4*(1+c*(random1-0.5)); resonance = 0.85`,
{a:k("Dark",40,300,110),b:k("Fog",0,.3,.08),c:k("Drift",0,1,.3)},
"Make the intentional noise-generator role explicit and controllable; replace uncontrolled feedback with seeded filter drift.");

export const dcCleanupNames=["Germanium Fuzz","Blues Break OD","Frequency Assassin","Boom Tail","Tension Bed","Midtempo Pressure Pump","Broken Grid Chaos","Preamp Regen Stack","Cascade Loop Dirt","Synth Feedback Lead"];

export const additions = [
make("Curved Sync Duck","Modulation","Continuous curved quarter-note ducking from a looping MSEG. No external sidechain; the recovery curve differs from stepped gates.",
`mseg1: points = 0:0:0,0.08:0.05:-1,0.7:1:1.5,1:1:0; sync = 1/4; mode = loop; source = free
stage1: y = x*(1-a+a*mseg1)
filter1: type = highpass; cutoff = 25; resonance = 0.35`,
{a:k("Duck",.2,.95,.78)}),
make("MIDI Bowed Resonator","Ambient","MIDI-gated curved bow into a resonant body and room. Hold sustains the second point; release plays the remaining curved tail. Route MIDI notes to the insert.",
`mseg1: points = 0:0:1.5,0.35:0.8:-0.5,0.65:0.6:1,1:0:0; duration = a; source = midi; sustain = 2; retrigger = on
stage1: y = x*(0.08+0.92*mseg1)
resonator1: freq = b; res = 2.8; gain = -5; mix = 0.28
reverb1: size = 0.65; decay = c; damp = 0.7; mix = 0.3`,
{a:k("Bow Time",.3,4,1.8),b:k("Body",140,1800,420),c:k("Space",.4,3,1.3)}),
make("Bipolar Formant Arc","Filter","Asymmetric bipolar MSEG sweeps a bandpass through a repeatable curved arc. Range moves the formant while the dry floor keeps the original body.",
`mseg1: points = 0:-1:-1,0.2:0.7:2,0.65:-0.3:-2,0.85:1:1,1:-1:0; duration = a; source = free; mode = loop
bus formant:
 filter1: type = bandpass; center = 400+(0.5+0.5*mseg1)*b; width = 900; resonance = 1.1
out: main = 1-c; formant = c`,
{a:k("Arc Time",.4,8,2.6),b:k("Range",800,7000,3000),c:k("Formant",.2,.9,.55)}),

make("Sixteen Step Filter","Modulation","Sixteen cutoff accents at host-synced sixteenths. Depth opens the filter; Glide rounds the steps.",
`stepseq1: steps = 16; sync = 1/16; values = 0.1,0.4,0.2,0.8,0.1,0.6,0.3,1,0.2,0.45,0.1,0.7,0.3,0.9,0.2,0.6; gate = 1; glide = c
filter1: type = lowpass; cutoff = a + stepseq1*b; resonance = 0.8`,
{a:k("Base",180,2000,500),b:k("Depth",300,6000,2800),c:k("Glide",0,.5,.05)}),
make("Probability Break Chops","Drums","Seeded chance-based sixteenth gating with a short open window. Floor preserves some drum sustain.",
`stepseq1: steps = 8; sync = 1/16; values = 1,0.4,0,1,0.2,0.7,1,0; gate = a; probability = b; seed = 1987; glide = 0.01
stage1: y = x*(c+(1-c)*stepseq1)`,
{a:k("Gate",.1,.9,.4),b:k("Chance",.25,1,.7),c:k("Floor",.03,.6,.15)}),
make("MIDI Pluck Filter","Synth","MIDI-triggered ADSR opens a resonant lowpass. Route MIDI notes to the insert; audio stays audible between notes.",
`adsr1: gate = midi; attack = 0.003; decay = a; sustain = 0.1; release = 0.12; velocity = 0.7
filter1: type = lowpass; cutoff = b + adsr1*c; resonance = 0.95`,
{a:k("Decay",.03,.8,.2),b:k("Base",250,2500,700),c:k("Envelope",800,9000,4000)}),
make("MIDI Swell Return","Ambient","MIDI-gated ADSR swells the reverb input while the low dry floor remains. Requires MIDI notes routed to the insert.",
`adsr1: gate = midi; attack = a; decay = 0.1; sustain = 0.8; release = b
stage1: y = x*(0.15+0.85*adsr1)
reverb1: size = 0.72; decay = 0.7; damp = 0.5; mix = c; width = 0.9`,
{a:k("Swell",.1,2,.6),b:k("Release",.1,3,1.2),c:k("Space",.2,.85,.55)}),
make("Held Envelope Cutoff","Filter","The input envelope is sampled at eighth notes to make held cutoff levels rather than continuous auto-wah.",
`env1: type = peak; attack = 0.002; release = 0.08
samplehold1: ina = env1; sync = 1/8; slew = c
filter1: type = lowpass; cutoff = a + samplehold1*b; resonance = 0.75`,
{a:k("Base",200,1600,600),b:k("Sensitivity",1000,14000,6500),c:k("Slew",0,.5,.06)}),
make("Seeded Drift Filter","Sound Design","Smooth deterministic random cutoff motion. Seed is fixed so patch reload restores the same starting sequence.",
`random1: rate = a; min = 0; max = 1; smooth = 0.85; shape = gaussian; seed = 119
filter1: type = bandpass; center = 350 + random1*b; width = c`,
{a:k("Drift",.1,4,.8),b:k("Range",700,6000,2600),c:k("Width",400,3000,1200)}),
make("Transient Accent Fold","Industrial","Detected onsets briefly increase folding. Threshold selects attacks and Hold controls the accent window.",
`envtrigger1: thresholddb = a; sensitivity = 0; hysteresis = 7; holdoff = 45; hold = b; level = 1
stage1: y = lerp(x,fold(x*(1+envtrigger1*c),-0.7,0.7),0.55)
filter1: type = lowpass; cutoff = 10000; resonance = 0.3`,
{a:k("Threshold",-42,-8,-23),b:k("Hold",10,160,45),c:k("Accent",.5,8,3)}),
make("Control Crossfade Filter","Modulation","A stepped pattern and smooth random drift are mixed before driving one filter. A/B balance changes the motion itself.",
`stepseq1: steps = 4; sync = 1/8; values = 0.1,0.9,0.35,0.65; glide = 0.06
random1: rate = 1.1; min = 0; max = 1; smooth = 0.85; seed = 731
modmix1: ina = stepseq1; inb = random1; levela = a; levelb = b; mix = 0.5
filter1: type = lowpass; cutoff = 350 + modmix1*c; resonance = 0.7`,
{a:k("Pattern",0,2,1.2),b:k("Drift",0,2,.8),c:k("Range",800,8000,3600)}),
make("Rectified Motion","Modulation","Absolute-value control math doubles the LFO's positive envelope without rectifying the audio.",
`osc1: shape = sine; freq = a; depth = 1
modmath1: ina = osc1; mode = abs; amount = 1; mix = 1
filter1: type = lowpass; cutoff = b + modmath1*c; resonance = 0.65`,
{a:k("Rate",.1,8,1.1),b:k("Base",250,2500,700),c:k("Depth",600,7000,2400)}),
make("Quantized Random Steps","Creative","Random control is quantized into discrete levels, then used as an amplitude pattern. No audio bit reduction.",
`random1: rate = a; min = -1; max = 1; seed = 441
modmath1: ina = random1; mode = quantize; quantize = b; amount = 1; mix = 1
stage1: y = x*(c+(1-c)*(0.5+0.5*modmath1))`,
{a:k("Rate",1,24,7),b:k("Coarseness",.5,1,.94),c:k("Floor",.05,.6,.12)}),
make("Transient Clip Bus","Drums","Hard peak clipping with a real threshold and knee. Use Drive for the hit density and Trim for level matching.",
`filter1: type = highpass; cutoff = 25; resonance = 0.35
clip1: mode = hard; drive = a; threshold = b; knee = c; bias = 0; outputtrim = d; mix = 1; autogain = off
filter2: type = lowpass; cutoff = 15000; resonance = 0.3`,
{a:k("Drive",0,18,5),b:k("Threshold",-12,0,-2),c:k("Knee",0,.8,.08),d:k("Trim",-18,0,-2)}),
make("Transformer Bus Cushion","Mastering","Transformer-style saturation adds level-dependent weight before a conservative ceiling. Match the output before judging color.",
`saturate1: mode = transformer; drive = a; character = b; dynamics = 0.4; tone = 0.55; mix = c; autogain = on
limit1: ceiling = -0.7; release = 0.12`,
{a:k("Drive",0,12,3),b:k("Character",0,1,.4),c:k("Color",.05,.6,.2)}),
make("Folded Parallel Texture","Sound Design","Foldback waveshaping on a parallel bus keeps a dry foundation. Tone removes harsh fold images.",
`stage1: y = x
bus folds:
 send: in = 1
 shaper1: mode = fold; drive = a; outputtrim = -9; mix = 1
 filter1: type = lowpass; cutoff = b; resonance = 0.4
out: main = 1-c; folds = c`,
{a:k("Drive",3,24,12),b:k("Tone",1500,10000,4800),c:k("Fold Mix",.1,.8,.4)}),
make("Jittered Drum Sampler","Lo-Fi","Rate reduction with clock jitter and a reconstruction filter. The dry blend keeps drum transients readable.",
`decimate1: rate = a; bits = b; jitter = c; hold = 0.15; recon = 0.65; outputtrim = 0; mix = d`,
{a:k("Reduction",.05,.75,.3),b:k("Bits",4,14,9),c:k("Jitter",0,.8,.3),d:k("Blend",.15,.85,.55)}),
make("Transparent Gain Trim","Mixing","Gain in dB with no saturation or modulation. Use to match bypass loudness and feed subsequent processors.",
`amp1: gain = a; outputtrim = 0; mix = 1`,
{a:k("Gain",-18,18,0)}),
make("Metallic Resonant Body","Sound Design","A narrow resonant body blended under the source, followed by a short room. Tune selects the ringing frequency.",
`resonator1: freq = a; res = b; gain = -3; mix = c
reverb1: size = 0.24; decay = 0.28; damp = 0.65; mix = 0.12`,
{a:k("Tune",180,2400,670),b:k("Resonance",1,8,3.5),c:k("Body",.1,.8,.4)}),
make("Frequency Shift Halo","Creative","Small single-sideband frequency shift mixed around the dry signal. Slow beating differs from pitch-shift doubling.",
`freqshift1: hz = a; mix = b
filter1: type = highpass; cutoff = 35; resonance = 0.35`,
{a:k("Shift",-35,35,6),b:k("Halo",.1,.75,.45)}),
make("Fold Threshold Bass","Bass","Dedicated wavefolder with independent drive and fold threshold, followed by a lowpass recovery filter.",
`filter1: type = highpass; cutoff = 28; resonance = 0.3
wavefold1: drive = a; fold = b; mix = 0.7
filter2: type = lowpass; cutoff = c; resonance = 0.4`,
{a:k("Drive",1,8,2.5),b:k("Fold",.15,1.2,.6),c:k("Tone",900,6500,3200)}),
make("Nonlinear Lowpass Drive","Filter","Saturation before the lowpass changes the material entering the cutoff. Parallel Mix keeps the original attack.",
`nlfilter1: cutoff = a; drive = b; mix = c`,
{a:k("Cutoff",250,12000,2200),b:k("Drive",1,8,2),c:k("Mix",.15,1,.7)}),
make("Tape Brake Transition","Creative","One-shot slowing delay for a transition. Reload or reset the patch to restart the brake; it is not a transport stop command.",
`tapestop1: stop = a; depth = b; mix = c`,
{a:k("Stop",.15,3,.7),b:k("Depth",.2,.95,.75),c:k("Blend",.1,.85,.5)}),
make("Reverse Grain Return","Delay","Filtered reverse-delay return blended with the dry input. Time sets the reverse window and Repeat controls regeneration.",
`reversedelay1: time = a; feedback = b; mix = c
filter1: type = lowpass; cutoff = 9500; resonance = 0.35`,
{a:k("Window",60,750,280),b:k("Repeat",0,.6,.22),c:k("Return",.1,.7,.35)}),
make("Ring Sideband Percussion","Sound Design","Audio-rate ring modulation gives percussion inharmonic sidebands. Carrier frequency changes the metallic spacing.",
`ringmod1: freq = a; mix = b
filter1: type = lowpass; cutoff = c; resonance = 0.4`,
{a:k("Carrier",35,1600,190),b:k("Ring",.15,.9,.55),c:k("Tone",2000,14000,9000)}),
];
export function curateFactory(input) {
  const addedNames=new Set(additions.map(p=>p.name));
  const result=input.filter(p=>!retired[p.name]&&!addedNames.has(p.name)).map(p=>{
    if(p.name==="Mesa High Gain")return p;
    if(revisions.has(p.name))return revisions.get(p.name).preset;
    if(p.name==="Midtempo Pressure Pump"){
      const description="Tempo-synced moving filter into tube and asymmetric fold distortion, followed by compressor pumping. Push adds pre-filter gain; no external sidechain is required.";
      return {...p,description,paramE:{...p.paramE,name:"Push"},script:p.script.replace(/^#.*\n#.*\n/,"# Midtempo Pressure Pump\n# "+description+"\n").replace("param e = Noise","param e = Push")};
    }
    if(p.name==="Optical Leveler Comp")return {...p,description:"Slow optical-style level control with a fixed 10 ms attack and 480 ms release, followed by tube saturation. Ratio reaches down to 2:1.",script:p.script.replace("Limit [3.0, 12.0]","Limit [2.0, 12.0]"),paramC:{...p.paramC,min:2}};
    return p;
  });
  return [...result,...additions].map(p=>{
    if(!dcCleanupNames.includes(p.name)||p.script.includes("filter999:"))return p;
    return {...p,script:p.script.trimEnd()+"\nfilter999: type = highpass; cutoff = 20; resonance = 0.35\n"};
  });
}
