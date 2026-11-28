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
- `RuntimePaintableComponent.MulticastPackedPaintBatchToOthers` /
  `MulticastPackedPaintBatch` (`TArray<uint8> PackedData`, `int32 StrokeCount`),
  the route the shipping first-person brush actually uses. The game decodes the
  packed bytes and expands each `CompactPaintStroke` into a full `PaintStroke`
  through its own native converter; the bridge captures that converted result.
- As a fallback, the game's `GetRecordedStrokeBatch` backing array (the
  component's `TArray<FPaintStroke>` recorded queue), plus the
  `QueuedLocalPaintBatches` / `QueuedOutgoingBatches` / `StrokeChunkBuffer`
  manager arrays if they still retain strokes when recording stops.

### Packed paint batch layout (build 2026-11-16)

The UFunction is not a full `FPaintStroke` array. It receives an opaque byte
buffer plus a stroke count:

| offset | size | meaning |
| --- | --- | --- |
| `0x00` | 1 | mode: `1..3` delta stroke stream, `4` grouped stream |
| `0x01` | 16 | `FGuid ReplicationSourceId` shared by the batch |
| `0x11` | 4 | `uint32` declared stroke count; matches the RPC `StrokeCount` |
| `0x15` | 2 | `uint16` group count (mode `4` only) |
| `0x17` | ... | grouped payload (mode `4`) |
| `0x15` | ... | delta payload (modes `1..3`) |

Decoded elements are `CompactPaintStroke` (`0x40` bytes): skeletal triangle
index, six barycentric bytes, `Radius` at `0x0C`, BGRA albedo/metallic/
roughness/emissive, target channel, effective subdivision fields, and the shared
replication GUID at `0x30`. The game expands these through
`CompactPaintStroke -> PaintStroke`; that converter reconstructs the UV from the
skeletal triangle anchor and preserves each stroke's `Radius`. The bridge hooks
that converter and records the expanded `FPaintStroke`, including the
per-stroke brush size, instead of re-quantising the packed bytes. If the
converter cannot be located, the component's recorded queue is read at each
batch and at Stop; both paths use a content hash for de-duplication.

Packed batches are parsed and reported with `packed_batch_mode`,
`packed_batch_bytes`, `packed_batch_declared_count`,
`packed_batch_group_count`, `packed_batch_count_mismatch`, and
`decoded_packed_strokes`.

### Discovered native route (build 2026-11-16)

These are image-relative RVAs for the shipping build; dynamic discovery runs
first and these are only a fallback/validation anchor:

- `UFunction::Func` native thunk at `UFunction + 0xD8`.
- `MulticastPackedPaintBatchToOthers` thunk RVA `0x6549230`, which calls the
  receiver vtable slot `+0x538`.
- Multicast implementation RVA `0x657ED40`.
- Packed decoder RVA `0x6581790` (`bool DecodePacked(TArray<uint8>, int32,
  TArray<CompactPaintStroke>&)`).
- Compact apply RVA `0x6580470`.
- `CompactPaintStroke -> PaintStroke` converter RVA `0x6580180`
  (`PaintStroke* Converter(component, out, const CompactPaintStroke*)`).
- `GetRecordedStrokeBatch` reads component `+0x230` (Data), `+0x238` (Num),
  and `+0x2C4` (retained high-water) directly.

Each entry stores the exact value parameters the game receives:

- `Uv`
- `ChannelData` (albedo RGBA, metallic, roughness, height, emissive, apply mode)
- `BrushSettings` (radius, hardness, opacity, spacing, falloff, blend mode,
  rotation, and the brush-tip texture identity)
- target `Channel`

`strokes.json` also preserves the full converter geometry so replay never has to
guess where a stroke belongs: `has_uv`, world/local position and their flags, the
skeletal triangle anchor flag/index and barycentric weights, the effective
world-radius/subdivision/template fields, and the replication source GUID. Older
archives that only contain `u`/`v` still load unchanged (their UV is treated as
present).

The values are stored as raw floating-point stroke data. No sRGB conversion,
texture resampling, palette quantization, or compression is applied on either
the recording or replay path. The app's own normal paint calls are also
recorded; the app's Reuse Paint replay calls are explicitly suppressed so a
replay can never feed itself back into the recording.

### Replay coverage and missing UVs

The game's compact `CompactPaintStroke -> PaintStroke` converter can leave `Uv`
at `(0,0)` when it only expands the skeletal triangle anchor. Replaying such a
stroke through `PaintAtUVWithBrush` would paint every dab at one corner, which
looks like large blank areas.

Replay therefore resolves each stroke before dispatch:

1. If the archive has a usable `Uv`, use it exactly.
2. Otherwise, if the stroke has a skeletal triangle anchor, interpolate the UV
   from the verified mesh profile's per-vertex UVs using the stored barycentric
   weights.
3. If neither is available, omit that stroke and report it instead of painting
   it at `(0,0)`.

Replay reports the per-stroke brush radius, channel histogram, apply-mode
histogram, stroke order, anchor counts, UV range, out-of-range count, unique
quantized UV count, a 32x32 UV coverage grid, and the counts reconstructed from
anchors or skipped. The host writes them to the runtime log as
`Reuse Paint coverage: ...`; a low unique-UV count, a low coverage fraction
(`replay_coverage_low`), or a nonzero skipped count identifies why an archive
replays with blanks even though Preview is complete. Per-stroke brush/radius,
channel, apply mode, colour data, and order are always kept.

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
- `global_process_event_hook`, `global_process_event_hook_attempted`,
  `global_process_event_hook_targets`,
  `global_process_event_hook_installed_targets`,
  `global_process_event_hook_failure`, `packed_converter_hook`,
  `packed_converter_route`, `packed_converter_failure`, `hook_route`, and
  `zero_count_reason`

`zero_count_reason` distinguishes `no_paint_ufunctions_resolved`,
`no_paint_process_event_calls_observed`,
`paint_calls_observed_on_nonlocal_receivers`,
`paint_calls_observed_without_capture_layout`,
`paint_calls_observed_without_reflected_spec`,
`packed_batch_decode_hook_unavailable`, and
`packed_batch_observed_but_no_decoded_strokes`. That makes a `count=0` log
actionable instead of repeating the same counters.

`global_process_event_hook` is the state that was active during the take, not
the post-uninstall state. Stop used to uninstall the hook before building its
metadata, so a working global MinHook still logged `false`. The status now keeps
a per-take snapshot (`global_process_event_hook_active`), reports the MinHook
attempt/target/status/failure, and the per-component vtable detour remains the
verified route if MinHook cannot attach to the game's ProcessEvent entry point
(for example because UE4SS already owns that trampoline).

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
- Compact/packed stroke batches are decoded by the game's own native converter.
  The bridge does not re-implement the delta/grouped payload compression; it
  discovers the converter through the multicast UFunction thunk (with a
  build-2026-11-16 RVA fallback) and captures the expanded `FPaintStroke`. If
  the converter hook cannot be installed, the component's `GetRecordedStrokeBatch`
  backing array is the exact fallback. The packed bytes themselves carry the
  per-stroke radius but not the original hardness/opacity/spacing/falloff/
  blend/rotation/tip values; the converter supplies the current brush values for
  those fields.
- If the converter leaves `Uv` absent, replay reconstructs it from the skeletal
  triangle anchor and the verified mesh profile. That requires the archive's
  triangle/barycentric anchor to match the live body's mesh profile; a body
  switch, a different mesh layout, or a missing profile reports
  `replay_strokes_skipped_invalid_geometry` and leaves those strokes unpainted
  instead of covering the mesh with one-corner dabs. Compact apply at the known
  RVA is not called by Reuse Paint: its exact argument layout has not been
  verified, and calling an unverified native apply could crash the game, so the
  replay keeps the verified `PaintAtUVWithBrush` transport with reconstructed
  UVs.
- Recording is bound to the local body/controller resolved when Record was
  pressed. Respawning or switching to a different body can require starting a
  new recording.
- Brush-tip textures are process-local objects. Cross-session replay resolves
  them by path when possible and otherwise falls back to the currently selected
  in-game brush tip.
- The archive's `strokes.json` uses a versioned schema
  (`meccha_paint_strokes_v1`). Unknown or missing stroke files are not replayed;
  the archive remains usable through the exact texture **Preview**.
