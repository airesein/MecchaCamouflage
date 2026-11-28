# Original Paint-Stroke Recording and Exact Reuse

## What it records

The native bridge can capture the local game body's real
`RuntimePaintableComponent::PaintAtUVWithBrush` calls while the user paints.
Each entry stores the exact value parameters the game receives:

- `Uv`
- `ChannelData` (albedo RGBA, metallic, roughness, height, emissive, apply mode)
- `BrushSettings` (radius, hardness, opacity, spacing, falloff, blend mode,
  rotation, and the brush-tip texture identity)
- target `Channel`

The values are stored as raw floating-point stroke data. No sRGB conversion,
texture resampling, palette quantization, or compression is applied on either
the recording or replay path. The app's own normal paint calls are also
recorded; the app's Reuse Paint replay calls are explicitly suppressed so a
replay can never feed itself back into the recording.

## Recording must be on while painting

A stroke is captured only while recording is active. Strokes painted before
**Record** is pressed, or after **Stop Recording** is pressed, are not in the
stroke list. The typical flow is:

1. Start the game and connect the app.
2. Open the Image tab and press **Record**. The status line shows the live
   stroke count while the game is painting.
3. Paint in-game with the real brush.
4. Press **Stop Recording**.
5. Press **Export Paint**. The resulting `.mcpaint` archive contains the
   recorded strokes together with the exported albedo/PBR texture.
6. Later, select that archive and press **Reuse Paint → Start**. The host sends
   the recorded strokes to the game, which applies them through
   `PaintAtUVWithBrush`.

## `.mcpaint` compatibility

A stroke-aware archive contains:

- `manifest.json` (texture metadata plus `stroke_count`, `stroke_source`, and
  `strokes_file`)
- `albedo.rgba` / `albedo.png`
- `pbr.rgba` / `pbr.png`
- `strokes.json` (raw recorded stroke list)

Older `.mcpaint` files have only the texture. They remain valid: Reuse Paint
detects that no stroke file is present and uses the existing texture replay
strategies (Adaptive, Contour, Base + Detail). Apply Exact / Preview also keep
using the exported texture.

Exact 1:1 reproduction requires recorded strokes. A texture-only archive can
only approximate the original brush path through the texture replay sampler.

## Native implementation and lifecycle

The game does not expose a readable full stroke history: the observable
component/manager API exposes counts and pending queue pressure
(`GetRecordedStrokeCount`, `GetQueuedStrokeCount`,
`GetReplicationPressure`), while the manager's arrays are drained batches, not
the user's complete paint history. The bridge therefore records the calls
themselves.

At **Record** time the bridge resolves the local `RuntimePaintableComponent` and
its `PaintAtUVWithBrush` UFunction and installs its existing ProcessEvent detour
on that component's vtable (the same safe detour mechanism used by the native
Present/paint paths). The hook appends only calls whose receiver is the local
component and whose UFunction is the resolved `PaintAtUVWithBrush`. The detour
always forwards the original event; it does not break normal paint or hook
chaining.

The recorded vector is a process-lifetime full history for the current
recording take. **Record** starts a fresh take, **Stop Recording** freezes it,
**Clear Recording** empties it, and bridge shutdown disarms recording. The
brush-tip texture pointer is process-local; on export its object path is stored,
and on replay the path is resolved again. If it cannot be resolved (for example
after a game restart changed the object layout), the current local brush tip is
used while the recorded radius/material/brush-shape values remain exact.

### Bridge commands

- `start_stroke_recording` (`restart` optional)
- `stop_stroke_recording`
- `clear_stroke_recording`
- `export_strokes` (`output_path` optional)
- `replay_recorded_strokes` (`strokes_path` required)
- `stroke_recording_status`

`export_paint_texture` also writes the stroke file when a recording exists and
reports `stroke_count`, `stroke_source`, and `strokes_written` in its metadata.

## Limitations

- Recording depends on the game delivering `PaintAtUVWithBrush` through
  `UObject::ProcessEvent`; a future game build that bypasses reflection would
  not be observed by this detour.
- Recording is bound to the local component resolved when Record was pressed.
  Respawning or switching to a different body can require starting a new
  recording.
- Brush-tip textures are process-local objects. Cross-session replay resolves
  them by path when possible and otherwise falls back to the currently selected
  in-game brush tip.
- The archive's `strokes.json` uses a versioned schema
  (`meccha_paint_strokes_v1`). Unknown schemas are rejected and fall back to
  texture replay through the host.
