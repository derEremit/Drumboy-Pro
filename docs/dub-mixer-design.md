# Dub mixer: design

Design for a mixer page in the fashion of a dub desk: a strip per layer with sends to the echo and the reverb, a filter per channel, and the big filter knob. Asked by the owner on 2026-11-13. Nothing here is built. A mockup at the device's real size is in the app's Screen ideas tab ("Dub mixer"). The cost and memory figures come from [`ux-and-signal-path-analysis.md`](ux-and-signal-path-analysis.md), section 3, and are estimates until measured (backlog 37).

## 1. What makes a mixer a dub mixer

A dub desk is played like an instrument. The design is built around the five gestures that make that possible:

| Gesture | On a desk | On the Drumboy |
|---|---|---|
| **Drop** | Mute a channel; what it already sent to the echo keeps ringing | MUTE + pad, as today. Mute stops new hits, so tails ring on |
| **Throw** | Open a channel's echo send for one hit, then close it | Hold EFFECT 1 + pad (echo) or REVERB + pad (reverb): the send is fully open while held |
| **Ride the feedback** | Turn the echo's feedback up until it builds, pull it back before it takes over | FEEDBACK is always under the same knob, and goes past 100 % |
| **Sweep the big knob** | A stepped high-pass over a group of channels | DUB FILTER: one knob, in steps, on the bus the strips feed |
| **Send a return on** | The echo return goes into the reverb | ECHO > VERB level |

## 2. Signal path in DUB mode

A song is either CHAIN (today's patch, set on the ROUTING page) or DUB. Old songs load as CHAIN and sound the same.

```
layer 1-10 ─► channel filter ─► level ─┬─ BUS: FILT ─► EQ ─► FILTER 1 = DUB FILTER ──────────────┐
line-in ────►                          ├─ BUS: DIR ──────────────────────────────────────────────┤
                                       ├─ ECHO send ─► FILTER 2 ─► EFFECT 1 (delay) ─┬─ ECHO RTN ─┤
                                       │                                             └─ > VERB ─┐ │
                                       ├─ VERB send ────────────────────────────────────────────┴─► REVERB ─ VERB RTN ─┤
                                       └─ FX send ─► EFFECT 2 ─────────────────────────────────────────────────────────┤
                                                                                              MASTER ─► limiter ─► out
```

- **It uses the modules that exist,** in the order the engine already runs them (EQ, Filter 1, Filter 2, Effect 1, Effect 2, Reverb), so the patch is loop-free and adds no latency. Their own pages keep working for the detailed settings.
- **Filter 1 is the dub filter.** Each strip chooses its bus: FILT (through the EQ and the dub filter) or DIR (straight to the master). A kick on DIR keeps its weight while the sweep thins everything else.
- **Filter 2 sits before the echo.** A high-pass there keeps the low end out of the repeats. It is off until set.
- **Effect 1 is the echo.** In DUB mode it is the delay, its DRY is off and its WET is the return level.
- **Effect 2 is a third send** (phaser, flanger, chorus or a distortion), on the second page.
- **Line-in gets a strip,** so audio from Loopy can be dubbed with the same sends and throws.
- **Sends are after the fader.** A throw opens the send fully regardless of the fader and of mute.

### What this path cannot do

- **Reverb into the echo.** The reverb runs last, so only echo into reverb is possible.
- **The dub filter over the returns.** Filter 1 runs before the echo and the reverb. On a real desk the big knob also sat on a group, not on the whole mix. A filter over everything needs the master stage of backlog 48.
- **Stereo on the FILT bus.** The EQ and the dub filter are mono, as today, so a strip on FILT stays centred. Pan there needs both to run twice (phase D5).

### Stereo: what the design adds (owner, 2026-11-13: yes, if the processing room allows)

Today every layer and module is mono; only the reverb's SURROUND makes a right channel. Two cheap additions give the desk width without making the whole path stereo:

- **Pan on DIR strips.** A strip that goes straight to the master gets a left and a right gain. Cost: one more multiply and add per sample per strip, under 0.5 % of the audio block for ten strips.
- **Ping-pong echo.** The repeats alternate left and right: the first on the left, the second on the right, and so on. It needs no second delay line: the delay reads its buffer at two points (one echo time back for the left, two for the right) and feeds back from the second. Cost: one more memory read per sample, about 0.1–0.3 %. ECHO WIDTH sets how far apart the repeats sit (0 = centre, as today).
  - The two read points must fit the delay's buffer (2.18 s), so ping-pong works while two echo times are shorter than that: at 1 beat down to 55 BPM, at 1 1/2 beats down to 83 BPM. Below that the echo falls back to the centre.
  - The level into the reverb takes both sides added together.

**Processing room.** Together these add under 1 % to an estimated 40 % for the whole dub mixer, against the 80 % gate. These are estimates from counting operations; the measurement of backlog 37 confirms or corrects them before D2 ships.

## 3. The page

Reached with MIXER: MIXER → DUB → ROUTING → MIXER. While the page is open, the lane area shows the desk (the mechanism built for the Euclid rings); leaving the page brings the lanes back.

### 3.1 Screen

- **Eleven strips of 60 px:** layers 1–10 and LINE IN. From the top: number, state (MUTE, THRW), name, the channel filter (FL: off in the centre, low-pass to the left, high-pass to the right), echo send (EC), reverb send (RV), the fader with a meter beside it, and the bus (FILT, or DIR with its pan).
- **ECHO return:** time, centre or ping-pong, FEEDBACK (magenta past 100 %), TONE, the level into the reverb, the return fader and meter.
- **VERB return:** decay, size, the return fader and meter.
- **DUB FILTER and master:** the big knob as a dial with its steps and the current frequency, master meters L and R, limiter gain reduction.
- **Colours:** the selected strip is cyan, a muted strip is dimmed, a thrown send is yellow.
- **Drawing cost:** opening the page draws the area once. After that only the bar, fader or meter that changed is redrawn.

### 3.2 Knobs

The top row edits the selected strip. The bottom row is the desk and never changes, so the hands learn it.

| | 1 | 2 | 3 | 4 |
|---|---|---|---|---|
| **Top: selected strip** | LEVEL | FILTER | ECHO | VERB |
| **Bottom: the desk** | FEEDBACK | ECHO RTN | VERB RTN | DUB FILTER |

- **FILTER** is one knob: centre is off, left closes a low-pass, right opens a high-pass.
- **FEEDBACK** runs 0–110 %. Past 100 % the echo grows; a soft clipper in the feedback path keeps it bounded, as tape does.
- **DUB FILTER** moves in steps: OFF, then a high-pass at 70, 100, 150, 250, 500 Hz, 1, 2, 3, 5 and 7.5 kHz (the steps usually quoted for King Tubby's filter). FILTER 1's own page still sets type, slope and resonance, so it can also be a low-pass.
- **ADD + turn** changes every strip, as on the other pages.

Second page (the next step in the MIXER cycle):

| | 1 | 2 | 3 | 4 |
|---|---|---|---|---|
| **Top: selected strip** | BUS (FILT / DIR) | PAN | DRIVE | FX SEND |
| **Bottom: the echo** | ECHO TIME | ECHO TONE | ECHO WIDTH | ECHO > VERB |

- **PAN** acts on a strip whose BUS is DIR. On a FILT strip it shows CENTRE until the bus is stereo (D5).
- **ECHO TIME** is a fraction of a beat today (1/8, 1/6, 1/4, 1/3, 1/2, 1/1). It gains the dotted values dub uses most: 3/4 of a beat (a dotted eighth) and 1 1/2 beats (a dotted quarter).
- **ECHO WIDTH** 0–100 %: 0 is the centred echo of today, 100 is full ping-pong.
- **The channel filter's resonance is fixed** at a moderate value, as on a one-knob DJ filter, to keep the page at eight fields. Effect 2's return level stays on its own page (WET).

### 3.3 Keys and pads

| Keys | Action |
|---|---|
| Pad | Plays the layer and selects its strip |
| MUTE + pad | Drop: mute or unmute the strip (as today) |
| S (left) + pad | Solo (as today) |
| **EFFECT 1 + pad** | **Echo throw:** the strip's echo send is fully open while both are held. The pad does not play the sound |
| **REVERB + pad** | **Reverb throw,** the same way |
| EFFECT 1 or REVERB tapped alone | Opens that effect's page, as today |
| CLEAR | Resets the selected strip's sends and filter |

- **A throw on a muted strip** plays its hits into the send only: the sound is heard as echo without the dry hit.
- **Several pads at once** throw several strips.

## 4. What is new in the engine

| Part | Size | Notes |
|---|---|---|
| Send levels per source (bus, echo, reverb, FX), following the knob with a short ramp | M | Backlog 46. The router already sends one source to several nodes with a gain per sample |
| A level on one module-to-module route (ECHO > VERB) | S | Module routes are on/off today |
| DUB mode per song and its fixed patch | S | The router is table-driven |
| Throws: a momentary send that ignores fader and mute | S | The destination is latched when a note starts, which suits a throw |
| Channel filter per layer, one knob | M | Backlog 47. State in the unused DTCM RAM |
| Echo: feedback to 110 % with a soft clipper, dotted times | S | In the delay |
| Ping-pong echo: a second read point, a stereo output into the master | S | The delay's output stage and one more 32-sample buffer; no second delay line |
| Pan on DIR strips: a left and a right gain into the master | S | Backlog 36 A |
| Meters: a peak per strip, return and master from the audio engine | M | Backlog 41 |
| File block for the strip settings | S | After the song's `CHN` and `BTX` blocks; older firmware ignores it |

**Cost (estimates):** sends 1–2 % of the audio block, channel filters 1.6–3.2 %, pan and ping-pong under 1 % together, the rest under 1 %. With the 34 % measured at V1.04 that lands around 40 %, under the 80 % gate. Measure first (backlog 37).

## 5. Phases

| Phase | Content | You can then |
|---|---|---|
| D1 | DUB mode, send levels, returns, the page with strips and faders (no meters), file block | Give each layer its own amount of echo and reverb, and ride FEEDBACK and the dub filter |
| D2 | Throws, feedback past 100 %, dotted echo times, ECHO > VERB, ping-pong echo (ECHO WIDTH) | Play the desk: drops, throws, runaway echo bouncing left and right |
| D3 | Channel filter and drive per strip, pan on DIR strips, page 2 | Filter one layer without touching the others; place DIR strips left and right |
| D4 | Meters, line-in strip | See levels; dub external audio |
| D5 | A stereo FILT bus (EQ and dub filter run twice); a filter over the whole mix (backlog 36, 48) | Pan every strip, also through the dub filter |

Each phase is a release that can be tested on its own. D1 and D2 need no new DSP beyond the echo's clipper and second read point.

## 6. Decisions (owner, 2026-11-13: "yours on all of them", so the recommendations stand)

1. **Filters:** a one-knob filter per strip plus a BUS switch, FILT (through the EQ and the dub filter) or DIR (straight to the master). A BUS level (a strip partly through the dub filter) can replace the switch later without changing the rest.
2. **Throw keys:** EFFECT 1 + pad throws into the echo, REVERB + pad into the reverb. Tapping either key alone still opens its page.
3. **A throw pad does not play the sound:** it only opens the send for the sequenced hits. Throwing a live hit can be added later as a setting.
4. **The big knob acts on the FILT bus only.** Filtering the returns as well needs the master stage of backlog 48 and stays in the last phase (D5).
5. **Effect 2 is a third send** (phaser, flanger, chorus or distortion), reached from strip page 2 (FX SEND) with its return on its own page.
6. **Page order:** MIXER, then DUB, then ROUTING.
