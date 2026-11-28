# Runtime Paint Replication Research

## Production contract

Production paint has one route: the bridge invokes the game's reflected
`PaintAtUVWithBrush` once for each planned stroke. The game owns its recorded
stroke queue, rendering, and replication. The bridge must not construct or
send a custom multiplayer paint payload.

Preview and Unpreview may use texture export/import to preserve a local material
snapshot. Normal Paint must never use texture import as a fallback.

The current material-properties target is AMRE: Metallic, Roughness, and
Emissive are written atomically through paint channel 7. Explicit Emissive
writes prevent a previous glowing value from persisting in a newly painted
surface.

## Reuse Paint

Reuse Paint has separate Start, Stop, Preview, and Stop Preview controls.
Start extracts the `.mcpaint` archive to a temporary directory and sends
`replay_paint_texture` with the extracted `albedo.rgba` and `pbr.rgba` paths.
The bridge reads those raw files synchronously before queueing the async job, so
the temporary directory can be deleted as soon as the command returns. Raw
texture replay never performs a local import and never displays the final
texture up front: only the gradually submitted strokes appear. When no raw
paths are supplied, the bridge retains the current-game-texture fallback for
backward compatibility and internal use.

Replay uses the same runtime mesh surface sample/plan generation as Image Paint:
the live mesh profile, the RuntimePaintable triangle cache, and the Image Paint
sample density (the Image brush size) produce the mesh samples. It does not use
the 4-face image or the canonical atlas. Each mesh sample's UV indexes the saved
raw albedo/PBR texture directly (no `v` flip) with edge-clamped bilinear
sampling. Replay uses the raw albedo and PBR values also used by Preview and
stamps them with a solid brush through `PaintAtUVWithBrush` on channel 7.
It can merge neighboring strokes when an enlarged brush covers their original
footprints and the surrounding albedo and PBR texels have identical values.
The response reports the number of saved submissions in `replay_strokes_merged`.

Preview locally imports the archive and preserves the pre-preview material
snapshot through the existing Preview/Unpreview mechanism. Stop Preview restores
that snapshot. The local import is never the production replication transport.
Reuse Paint requires the same body and mesh/UV layout that produced the export,
and its duration grows with the number of mesh surface samples.

## Queue telemetry and completion

The visible dotted frontier was caused by the bridge submitting strokes much
faster than the game drained its own recorded-paint queue. Submission success
is therefore not completion.

For every async job, the bridge samples the game-owned recorded/component queue
when available and reports:

- `local_strokes_submitted`
- `local_strokes_synced` (confirmed rendered strokes)
- `native_queue_*` observations, target, peak, and wait count
- `paint_eta_ms` calculated from confirmed progress

The direct scheduler preserves a one-millisecond wake-up floor and a bounded
CPU slice. It may hold a small queue window, but must wait for the queue to
drain before reporting terminal completion. Do not bypass this with immediate
reposts, queue-memory writes, or zero-delay loops.

## Multiplayer validation

Validate host-painter and joining-client-painter separately. For each run,
record:

1. `PaintAtUVWithBrush` activity and direct-route metadata.
2. Submitted versus confirmed strokes and native queue depth over time.
3. Painter completion time and joining-client visible completion time.
4. Cancellation behavior: no additional submission after cancel, followed by a
   bounded natural drain of already recorded work.
5. A changed-pixel/material export from the selected painter component and, if
   possible, the joining receiver component.

The test fails if the game crashes, the queue grows without bounded progress,
terminal completion occurs while the queue remains nonzero, or a joining client
remains visibly behind after the queue reaches zero.

## Game-update investigation

Build a small numeric loop before editing the runtime route:

1. Use one region and a small stroke limit.
2. Confirm reflection resolves `PaintAtUVWithBrush`, the paint component, and
   `FPaintChannelData` including Emissive.
3. Use manual sentinel material values and compare exported M/R/E values.
4. Repeat with Auto Detect on; compare the selected dominant material pattern,
   not the manual values.
5. Run the same controlled job in host and joining-client roles.

Never carry old RVAs, payload layouts, queue offsets, or alternate transports
into a new game version. Missing or ambiguous direct reflection is an explicit
failure that needs investigation.
