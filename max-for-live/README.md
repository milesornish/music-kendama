# Max for Live devices

OSC → Max for Live → Live, per architecture §"Software Stack" (`udpreceive 9000` → route by
address → Live). The compiled `bridge/` publishes sensor OSC to `127.0.0.1:9000`; these devices
receive it. No MIDI anywhere in the path.

## `kendama-osc-tone` — first motion → sound proof
Self-contained audible test: receives `/kendama/ken/accel` and generates a tone whose **pitch
follows ax-tilt (150–850 Hz)** and **volume follows ay-tilt**. Lets you hear the instrument react
with nothing else patched.

Signal flow: `udpreceive 9000` → `route /kendama/ken/accel` → `unpack 0 0. 0. 0.`
(the leading `seq` int — test telemetry — is skipped; ax/ay/az are the floats) →
`* / +` scaling → `cycle~` × amplitude → `plugout~`.

### Load it (the GUI steps must be done by hand — Ableton can't be scripted)
1. Make sure the **bridge is running** (`bridge/music-kendama-bridge <portA> [portB]`) and an
   instrument is streaming — `/kendama/ken/accel` should be live on `:9000`.
2. In Live: create an **Audio track**, set its monitor to **In**, raise the track volume.
3. Drag a **Max Audio Effect** onto the track, click its **edit (✎)** button — the Max editor opens.
4. In Max: **File → Open** → `max-for-live/kendama-osc-tone.maxpat` (opens in a new window).
5. **Edit → Select All**, **Copy**. Switch to the device-editor window, delete its default
   contents, **Paste**, then **Save** (`.amxd`, e.g. into Live's User Library).
6. You should hear a drone. **Move the ken** → pitch and volume change.

> Authored without a Max install on hand, so it's **untested in Max** — `plugout~` only resolves
> inside the M4L device (expected to error in the standalone window until pasted in). If an object
> errors or a wire needs nudging, say what you see and I'll fix the patch.

## `kendama-osc-av` — sound + light from one gesture
Extends the tone device with an **LED branch**: the same accel is mapped to R/G/B
(`ax→R, ay→G, az→B`, clipped 0–255), packed, and sent as `/kendama/ken/led/rgb` via
`udpsend 127.0.0.1 9001` → the bridge down-path → RX_DOWN → the ken's DotStar. So moving the
ken drives **both** the `cycle~` tone **and** the strip from one motion. Requires the bridge to be
running with a **Port B** (down-path) argument, and the ken streaming.

Load it the same way (steps above) — paste `kendama-osc-av.maxpat` over the device's contents and
re-save. Only `plugout~` is M4L-only (errors in the standalone window until pasted in); `pack`,
`prepend`, `udpsend`, `clip` are stock Max and should resolve everywhere.

## Next: `live.remote~` (control real instruments)
The spec's intended continuous mapping (§"Software Stack"): replace the `cycle~` tail with
`live.remote~`, click **Map**, and pick any Live parameter — tilt then drives a synth/filter/etc.
We'll build that once the tone device confirms the OSC path into Live.
