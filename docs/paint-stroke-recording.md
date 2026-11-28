# Original Paint-Stroke Recording and Exact Reuse

## What it records

The native bridge can capture the game's own first-person brush calls while the
user paints. It does not assume the game uses the same function or object as the
app's direct transport. Recording installs a verified MinHook detour on the
game's `UObject::ProcessEvent` entry point(s), resolves every paint-related
UFunction by reflection, and captures:

- `PaintAtUVWithBrush` / `PaintAtUV` value parameters.
- `PaintAtScreenPosition` / `PaintAtWorldPosition` return UV/world hits with the
  component's `CurrentBrushSettings` at the time of the call.
- `PaintStroke`, `PaintStrokeBatch`, server/relay/send stroke batches, and other
  reflected paint functions whose stroke payload is recognized.
- As a fallback, the game's `QueuedLocalPaintBatches` and `StrokeChunkBuffer`
  arrays if they still retain strokes when recording stops.

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
2. Open the Image tab and press **Record**. The first 30 seconds are also a
   discovery window. The status line shows the live stroke count while the game
   is painting; its tooltip lists per-source counts and the observed paint
   UFunctions.
3. Paint in-game with the real brush.
4. Press **Stop Recording**. A zero-stroke stop reports `zero_count_reason`
   together with `paint_events_observed`, ignored-receiver, layout, server/batch,
   and recorded-queue counts.
5. Press **Export Paint**. The resulting `.mcpaint` archive contains the
   recorded strokes together with the exported albedo/PBR texture.
6. Later, select that archive and press **Reuse Paint → Start**. The host sends
   the recorded strokes to the game, which applies them through
   `PaintAtUVWithBrush`.
7. The status line under the path shows the recorded-stroke count for the
   selected archive, so an archive can be checked before applying anything.

## Discovery and zero-count diagnostics

Recording start also starts a short diagnostic window (`discovery_seconds`,
default 30, override in the start command). During that window the global
ProcessEvent detour records every paint/stroke/brush-related UFunction with its
receiver class and object name, whether the receiver is local, and its reflected
parameter size. The status response exposes:

- `discovered_paint_functions` (function, name, receiver, receiver class/name,
  kind, local receiver, call count)
- `paint_events_observed`, `paint_events_captured`,
  `paint_events_ignored_receiver`, `paint_events_layout_unavailable`
- `captured_paint_at_uv_with_brush`, `captured_paint_at_screen_position`,
  `captured_paint_at_world_position`, `captured_paint_at_uv`,
  `captured_stroke_batches`, `captured_server_stroke_batches`,
  `captured_native_strokes`, `captured_recorded_queue_strokes`
- `global_process_event_hook` and `zero_count_reason`

`zero_count_reason` distinguishes `no_paint_ufunctions_resolved`,
`no_paint_process_event_calls_observed`,
`paint_calls_observed_on_nonlocal_receivers`,
`paint_calls_observed_without_capture_layout`, and
`paint_calls_observed_without_reflected_spec`. That makes a `count=0` log
actionable instead of repeating the same counters.

## `.mcpaint` compatibility

A stroke-aware archive contains:

- `manifest.json` (texture metadata plus `stroke_count`, `stroke_source`, and
  `strokes_file`)
- `albedo.rgba` / `albedo.png`
- `pbr.rgba` / `pbr.png`
- `strokes.json` (raw recorded stroke list)

Reuse Paint uses recorded strokes only. There is no Adaptive/Contour/Base +
Detail strategy selector. Older `.mcpaint` files with no `strokes.json` remain
valid for **Preview**, which imports the exported albedo/PBR texture locally
and restores the previous snapshot when preview is stopped. Pressing **Start**
on a texture-only archive shows a clear message directing the user to Preview
or to export again after recording strokes. Nothing is applied to other
players by Preview.

## Native implementation and lifecycle

The game does not expose a readable full stroke history: the observable
component/manager API exposes counts and pending queue pressure
(`GetRecordedStrokeCount`, `GetQueuedStrokeCount`,
`GetReplicationPressure`), while the manager's arrays are drained batches, not
the user's complete paint history. The bridge therefore records the calls
themselves.

At **Record** time the bridge resolves the local player/controller/body and all
live `RuntimePaintableComponent` instances, then installs a real MinHook detour
on each verified `UObject::ProcessEvent` entry point found from live vtables. It
also keeps the per-component vtable detour as a fallback when MinHook cannot
attach. Every paint-related UFunction is resolved by name and reflected layout
(`Uv`, `ScreenPosition`, `WorldPosition`, `ChannelData`, `BrushSettings`,
`Channel`, `ReturnValue`, `Strokes`, `StrokeBatch`, `PaintStroke`). Calls are
accepted from the local component identity, the local pawn/controller/relay,
their outer or owner chain, or the resolved local component set. The detour
always forwards the original event; it does not break normal paint, ESP, or
other hook chaining, and capture is skipped while Reuse Paint replay is active.

Per-stroke brush values come from the call's `BrushSettings` when present;
otherwise the component's `CurrentBrushSettings` is read at the call so a user
who changes brush size mid-painting keeps each stroke's real size. Stroke-batch
payloads retain each `PaintStroke::BrushSettings` independently.

The recorded vector is a process-lifetime full history for the current
recording take. **Record** starts a fresh take, **Stop Recording** freezes it,
**Clear Recording** empties it, and bridge shutdown disarms recording. The
brush-tip texture pointer is process-local; on export its object path is stored,
and on replay the path is resolved again. If it cannot be resolved (for example
after a game restart changed the object layout), the current local brush tip is
used while the recorded radius/material/brush-shape values remain exact.

### Bridge commands

- `start_stroke_recording` (`restart` and `discovery_seconds` optional)
- `stop_stroke_recording`
- `clear_stroke_recording`
- `export_strokes` (`output_path` optional)
- `replay_recorded_strokes` (`strokes_path` required)
- `stroke_recording_status`

`export_paint_texture` also writes the stroke file when a recording exists and
reports `stroke_count`, `stroke_source`, and `strokes_written` in its metadata.

## Limitations

- ProcessEvent capture depends on the game delivering the brush call through
  reflection. If a future build calls a native paint method directly, discovery
  reports `no_paint_process_event_calls_observed` and the recorded-queue fallback
  is the only capture source.
- Compact stroke batches (`CompactPaintStrokeBatch`) are detected and listed by
  discovery but are only captured when the reflected payload is a full
  `FPaintStroke` array or when the game's recorded queue still retains the
  strokes.
- Recording is bound to the local body/controller resolved when Record was
  pressed. Respawning or switching to a different body can require starting a
  new recording.
- Brush-tip textures are process-local objects. Cross-session replay resolves
  them by path when possible and otherwise falls back to the currently selected
  in-game brush tip.
- The archive's `strokes.json` uses a versioned schema
  (`meccha_paint_strokes_v1`). Unknown or missing stroke files are not replayed;
  the archive remains usable through the exact texture **Preview**.
