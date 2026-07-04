# Niagara VFX Templates

Base Niagara Systems for the AI-driven VFX workflow. These templates are meant
to be duplicated and parameterized by `duplicate_niagara_system` / `set_niagara_parameter`
instead of being authored from scratch.

## Quick start

1. Make sure the **Niagara** and **Python Editor Scripting** plugins are enabled.
2. Run `Content/Python/create_niagara_templates.py` inside the Unreal Editor.
3. Open each created system in the Niagara Editor and wire the `User.*` parameters
   listed below.
4. Save all assets.

## Parameter naming convention

All templates expose parameters in the **User** namespace with a `User.` prefix.
This convention must stay consistent so that the MCP tooling can set values at
runtime.

| Type | Niagara type | Example default |
|---|---|---|
| `User.Color` | Linear Color | `(1.0, 0.6, 0.2, 1.0)` |
| `User.Size` | Float | `1.0` |
| `User.Rate` | Float | `50.0` |
| `User.Lifetime` | Float | `1.0` |
| `User.Velocity` | Vector3 | `(1.0, 1.0, 1.0)` |
| `User.Width` | Float | `0.15` |
| `User.Speed` | Float | `1.0` |
| `User.Density` | Float | `20.0` |
| `User.DecalSize` | Float | `0.5` |

## Templates

### NS_BurstBase

**Purpose:** One-shot burst / explosion / spark effect.

**Exposed parameters:**
- `User.Color` — tint / emissive color
- `User.Size` — global particle size multiplier
- `User.Rate` — spawn burst count / spawn rate
- `User.Lifetime` — particle lifetime
- `User.Velocity` — initial velocity scale

**Suggested wiring:**
1. Add a **Sprite** emitter.
2. Set **Spawn** to `Spawn Burst Instantaneous` with count bound to `User.Rate`.
3. In **Initialize Particle**:
   - `Lifetime` → `User.Lifetime`
   - `Color` → `User.Color`
   - `Sprite Size Mode` → `Random Uniform` or `Non-Uniform`, multiplied by `User.Size`
   - `Velocity` → random sphere / cone scaled by `User.Velocity`
4. Add a **Sprite Renderer**.
   - Material: translucent additive unlit particle material (e.g. `M_Generated_Unlit_Additive` or a simple circular sprite).
5. Optional: add a **Gravity** or **Drag** module for physical falloff.

---

### NS_TrailBase

**Purpose:** Ribbon or particle trail that follows a moving source.

**Exposed parameters:**
- `User.Color` — trail tint
- `User.Width` — ribbon width or particle size
- `User.Lifetime` — trail segment lifetime
- `User.Speed` — emission / fade speed scale

**Suggested wiring:**
1. Add a **Ribbon** emitter, or a **Sprite** emitter with spawn per-unit.
2. Bind **Spawn Rate** to a constant value (artistic default ~30); expose through `User.Speed` if dynamic emission is desired.
3. In **Initialize Particle**:
   - `Lifetime` → `User.Lifetime`
   - `Color` → `User.Color`
   - `Ribbon Width` / `Sprite Size` → `User.Width`
4. Add a **Ribbon Renderer** or **Sprite Renderer** with additive/blend material.
5. Set **Source** to `From Actor / Component` so the trail follows a socket or actor.

---

### NS_AmbientBase

**Purpose:** Looping ambient dust, magic motes, or environmental floating particles.

**Exposed parameters:**
- `User.Color` — particle tint
- `User.Density` — spawn rate / particle count
- `User.Size` — particle size multiplier
- `User.Speed` — drift / orbit speed

**Suggested wiring:**
1. Add a **Sprite** emitter with **Spawn Rate** bound to `User.Density`.
2. Set **Box** or **Sphere** location with artist-chosen bounds.
3. In **Initialize Particle**:
   - `Lifetime` → random range (e.g. 2–5 s)
   - `Color` → `User.Color`
   - `Sprite Size` → random range multiplied by `User.Size`
4. Add gentle **Velocity** / **Noise** modules scaled by `User.Speed`.
5. Add a **Sprite Renderer** with soft translucent circular material.

---

### NS_ImpactBase

**Purpose:** Hit flash, sparks, and optional ground decal for impacts.

**Exposed parameters:**
- `User.Color` — flash / spark color
- `User.Size` — overall effect scale
- `User.DecalSize` — decal projection size
- `User.Lifetime` — flash and spark lifetime

**Suggested wiring:**
1. Add a **Sprite** emitter for the flash:
   - `Spawn Burst Instantaneous`, count ~1–3
   - `Lifetime` → `User.Lifetime`
   - `Color` → `User.Color`
   - `Sprite Size` → `User.Size`
2. Add a second **Sprite** emitter for sparks:
   - Burst count bound to `User.Size * 20`
   - Cone velocity scaled by `User.Size`
   - Gravity module for physical falloff
3. Optional: add a **Decal** renderer:
   - Size → `User.DecalSize`
   - Lifetime → `User.Lifetime * 2`
   - Material: simple scorch / impact decal material
4. Use additive unlit materials for flash and sparks.

## Renderer / material guidance

- All templates should use a **Sprite Renderer** by default.
- Use a simple circular/soft particle texture (`T_SoftCircle` or engine default particle texture).
- For colored glowing effects, prefer an **Unlit Additive** translucent material.
- If the PBR master materials from `Content/Materials/Generated/` are available,
  `M_Generated_Unlit_Additive` is the recommended parent for the sprite material.

## Verification checklist

- [ ] `NS_BurstBase` exists at `/Game/VFX/Templates/NS_BurstBase`
- [ ] `NS_TrailBase` exists at `/Game/VFX/Templates/NS_TrailBase`
- [ ] `NS_AmbientBase` exists at `/Game/VFX/Templates/NS_AmbientBase`
- [ ] `NS_ImpactBase` exists at `/Game/VFX/Templates/NS_ImpactBase`
- [ ] Each system exposes the `User.*` parameters listed above
- [ ] Each system has at least one Sprite Renderer
- [ ] Templates can be duplicated and spawned via `duplicate_niagara_system`
