# COF (Composite Object File) Pipeline

> Reverse-engineered from the Diablo II 1.13c game DLLs. The addresses and call
> graphs below are 1.13c addresses, so they hold for any mod built on that
> version -- mods of this era ship Blizzard's DLLs unchanged.

> **Provenance.** The function names, addresses, and call edges in this document
> were verified against the retail 1.13c binaries in Ghidra on 2026-08-21:
> `D2Client.dll` (image base `6fab0000`, SHA-256 `dd8bc602…8836d906`),
> `D2Common.dll` (`6fd50000`), and `D2CMP.dll` (`6fe10000`), each from the
> stock 1.13c LoD install. Every function-address row in the tables below and
> all 8 string-table entries were checked; a companion audit records every
> claim, verdict, and the corrections applied —
> see [cof-pipeline-1.13c.verification.md](cof-pipeline-1.13c.verification.md).
> The "mods ship these DLLs unchanged" premise above holds for the byte-identical
> vanilla DLLs; a mod that patches a DLL at load (e.g. Project Diablo 2) is a
> separate case and is not covered by these addresses.

## Overview

COF files define how multi-layer animations are composited for characters, monsters, NPCs, and objects in Diablo 2. The COF system assembles individual sprite layers (head, torso, legs, arms, weapons, shields) into a complete animated unit.

### COF Filename Convention
```
data\global\chars\{TOKEN}\COF\{TOKEN}{MODE}{WEAPON_CLASS}.cof
```
Example: `data\global\chars\AM\COF\AMTNHTH.cof` = Amazon, Town, Hand-to-Hand

### COF String References (D2Client.dll)
| Address | String | Purpose |
|---------|--------|---------|
| `6fb84ec0` | `%s\%s\COF\%s%s%s.COF` | COF path format string |
| `6fb84ef0` | `cmncof_a6.d2` | Common COF data table (6-dir) |
| `6fb84f00` | `cmncof_a4.d2` | Common COF data table (4-dir) |
| `6fb84f10` | `cmncof_a3.d2` | Common COF data table (3-dir) |
| `6fb84f20` | `cmncof_a2.d2` | Common COF data table (2-dir) |
| `6fb84f30` | `cmncof_a1.d2` | Common COF data table (1-dir) |
| `6fb857a8` | `COF Memory->%i of %i` | COF memory budget debug string |
| `6fb85e27` | `R\Cof\palshift.dat` | COF palette shift data |

### What a COF Is, and Why It Exists

Every character in Diablo II is assembled, never drawn whole. There is no single stored picture of an Amazon standing in town. There is a head, a torso, legs mid-stride, two arms, whatever rides in each hand, and a shield if she carries one -- each authored separately, each stored as its own sprite, each stacked in a fixed order every frame the game paints. The COF file is the assembly instruction. "COF" is Composite Object File, and *composite* is the entire point: the unit on screen is built up, layer over layer, from parts that were made independently so that any one of them can change without touching the rest.

That is why the system exists. If the Amazon equips a different bow, only her hand layer changes and the torso keeps walking. If a dye recolors her armor, only a palette table swaps and the geometry is untouched -- the functions that map equipment to a layer (`GetEquipSlotAnimComponent`), rebuild the layer set when gear changes (`RebuildEquippedVisualComponents`), and recolor a layer per item (`CopyItemColorComponents`) are all in the tables below. Keep the parts separate and every combination of armor, weapon, and colour comes almost for free. Fuse them into one image and you would need a distinct animation for every possible loadout -- a combinatorial blowup no disc of the era could hold.

A COF names three things -- a token, a mode, and a weapon class -- and from them decides which layers to draw, in what order, and with which animation. The filename carries all three. `AMTNHTH.cof` is `AM` (the Amazon), `TN` (her town mode, the relaxed stance she uses in a town), and `HTH` (hand-to-hand, no weapon drawn). The eleven stages below follow that one file from the instant the game decides "this unit is now in town mode" to the instant her last pixel lands in the frame buffer.

The shape is a straight line with loops hung off it. The path is built and the file loaded (Stage 1). The animation system picks which frame of which sequence is current (Stage 2). The component system walks the COF's layer list and asks, per part, whether it is visible and which sprite it needs (Stage 3). The sprite engine loads and caches those sprites (Stage 4) and decompresses each into per-direction cels, the atomic unit of drawing (Stage 5). The renderer depth-sorts everything and blits it, layer by layer, to the screen (Stage 6). Stages 7 through 11 are the same machinery aimed at special cases: monsters, overlay effects such as auras, the mode changes that force a reload, missiles, and the palette transforms that give one sprite many colours. Read top to bottom, it is a single pass from a state change to a picture.

---

## Stage 1: COF Path Construction & File Loading

Nothing loads until there is a name to load. The pipeline's first job is to turn an abstract unit -- "this Amazon, in town, unarmed" -- into an exact string the archive can look up. That string is assembled from three tokens by `BuildCofPathString`, which fills the format `%s\%s\COF\%s%s%s.COF` (the literal lives in D2Client at `6fb84ec0`). The first two `%s` are the character token and the mode; the last three are the weapon class, split so that `GetWeaponClassToken` can supply `HTH`, `1HS`, `2HS`, and the rest depending on what the unit holds. `GetUnitWeaponAnimToken` reads the equipped weapon to decide which of those tokens applies.

For our Amazon, the tokens resolve to `AM`, `TN`, and `HTH`, and the format produces `data\global\chars\AM\COF\AMTNHTH.cof`. Monsters take a parallel route through `BuildMonsterCofFilePath`, which assembles the same kind of string for units whose tokens come from the monster tables rather than a player's equipment.

With a path in hand, `LoadCofAndSetupSkillLayers` is the front door. It pulls the file out of the MPQ archive -- `FindHashedArchiveEntry` locates the resource by hash, `LoadAndCacheArchiveGraphics` brings its bytes into memory -- and then hands off to the layer machinery: `InitializeCofLayerDataArrays` allocates one data slot per layer the COF declares, and `LoadCofLayerAnimationFrames` begins pulling the sprite frames each of those layers will need. The call graph below shows the three entry points that converge on this loading core -- skill rendering, the memory-budget initializer, and the game-state frame loader.

### D2Common.dll
| Function | Address | Purpose |
|----------|---------|---------|
| `BuildCofPathString` | `6fd93c30` | Formats the COF path using `%s\%s\COF\%s%s%s.COF` |
| `GetWeaponClassToken` | `6fd93860` | Gets the weapon class string token (HTH, 1HS, 2HS, etc.) |
| `GetUnitWeaponAnimToken` | `6fd941e0` | Gets the weapon animation token for the unit's equipped weapon |

### D2Client.dll
| Function | Address | Purpose |
|----------|---------|---------|
| `BuildMonsterCofFilePath` | `6fb19410` | Builds COF path specifically for monster units |
| `LoadCofAndSetupSkillLayers` | `6fb19cc0` | Main COF loading entry point -- loads COF file and initializes layer data for skill rendering |
| `InitializeCofLayerDataArrays` | `6fb18f40` | Allocates and initializes per-layer data arrays after COF is parsed |
| `LoadCofLayerAnimationFrames` | `6fb1bf30` | Loads sprite frames (DCC/DC6) for each layer defined in the COF |
| `FindHashedArchiveEntry` | `6fb19350` | Looks up archived resource by hash (callee of LoadCofAndSetupSkillLayers) |
| `LoadAndCacheArchiveGraphics` | `6fb19970` | Loads and caches graphics data from archive (callee of LoadCofAndSetupSkillLayers) |

### Call Graph
```
ProcessSkillRecordFlags (6fb1a0c0)
  -> LoadCofAndSetupSkillLayers (6fb19cc0)
       -> BuildMonsterCofFilePath (6fb19410)
       -> FindHashedArchiveEntry (6fb19350)
       -> LoadAndCacheArchiveGraphics (6fb19970)
       -> SetupSkillDescriptionEntry (6fb1dd30)
       -> GetBitmapPixelOffset (6fb1d6d0)

InitializeMemoryBudget (6fb504d0)
  -> InitializeCofLayerDataArrays (6fb18f40)
       -> AllocateAndValidateResourceSlot (6fabc010)
       -> InitializeSpriteDataArrays (6fb1d400)

LoadAllGameStateAnimationFrames (6fb1c3b0)
  -> LoadCofLayerAnimationFrames (6fb1bf30)
       -> LoadUnitAnimationFrames (6fb1ac60)
```

---

## Stage 2: Animation Data & Frame Management

A loaded COF is a static thing. It says which layers exist and how they stack, but not which frame of which cycle is showing right now. That is this stage's work: to move the animation through time.

The clock starts with `SetUnitAnimMode`. When our Amazon steps out of a portal into town, her mode becomes `TN`, and setting that mode is what triggered the COF switch in the first place -- the D2Common side of the boundary telling the D2Client side that a new file is needed. From there the timing comes out of `AnimData.d2`, the table that says how many frames the `AMTNHTH` sequence runs and how fast: `DATATBLS_LoadAnimDataTable` is what put it in memory, and `ANIM_LookupAnimDataByPath` finds the entry for a name like `AMTNHTH` inside it -- the file's own layout, hash function, and what a frame's event byte actually drives are the subject of the [AnimData.d2 chapter](animdata-d2.md). Objects are timed from a different table -- `GetObjectsTxtRecord` hands back an Objects.txt row, whose per-mode `FrameCnt` and `FrameDelta` columns pace a chest or a door the way AnimData paces a character.

Each tick, `UpdateAllAnimationFrames` advances every active unit. `AdvanceAnimFrameWithWrap` steps the frame counter and wraps it back to zero at the end of the sequence, so the town idle loops seamlessly; `AdvanceAnimSubAccumulator` carries the fractional remainder between ticks, so an animation slower than the frame rate still moves smoothly rather than stuttering. When a frame carries an event -- a footstep sound, the instant a missile should launch, the point in a swing where damage lands -- `SetAnimEventFromFrameData` fires it. The long list of `GetAnimDataFrameInfo`, `ExtractAnimFrameData`, and `GetGraphicsMode*` entries below are the small accessors that read individual fields out of these records.

### D2Common.dll
| Function | Address | Purpose |
|----------|---------|---------|
| `SetUnitAnimMode` | `6fd83920` | Sets the unit's animation mode (walk, attack, cast, etc.) -- triggers COF switch |
| `GetObjectsTxtRecord` | `6fd8e980` | Indexes the Objects.txt table by object type id (`id * 0x1C0 + base`) -- the record whose per-mode `FrameCnt`, `FrameDelta`, and `Start` columns time an object's animation |
| `AdvanceAnimFrameWithWrap` | `6fd7f060` | Advances animation frame counter with wrap-around at sequence end |
| `AdvanceAnimSubAccumulator` | `6fd7f090` | Sub-frame accumulator for smooth animation timing |
| `UpdateAllAnimationFrames` | `6fd8bbd0` | Per-tick update of all animation frames |
| `GetAnimDataFrameInfo` | `6fd91ef0` | Reads frame info out of an AnimData.d2 record. **One function, not four** — see below |
| `ExtractAnimFrameData` | `6fd7e6e0` | Extracts four byte fields from a **6-byte-stride** frame array, indexed by an 8.8 fixed-point frame. Not AnimData.d2, whose records are `0xA0` |
| `SetAnimEventFromFrameData` | `6fd7ee10` | Sets animation event triggers from frame data (sound, missile, etc.) |

> **Three addresses removed from that row, and why it matters.** This table
> previously listed `6fd71fb0`, `6fd8c640` and `6fd9fe80` alongside `6fd91ef0`
> as four instances of the same accessor. They are not. Only `6fd91ef0` touches
> AnimData.d2; the others are unrelated functions that Ghidra labelled
> identically. Checked directly: `6fd71fb0` is a three-line bit-flag setter
> (`*p |= mask` / `*p &= ~mask`), and `6fd8c640` sorts a room's unit list and
> returns the list head at `+0x74`.
>
> The cause is worth knowing, because it produces this pattern throughout
> D2Common. Its asserts push a **blanked filename pointer at `0x6fdda728`
> shared by 567 call sites**, so anything that attributed functions by the
> nearest string labelled all 567 the same way — and `6fd8c640`'s comment was
> then copy-pasted onto `6fd71fb0`, which does something else entirely. Treat a
> repeated name in this database as a question, not a finding.
>
> `ExtractAnimFrameData` lost an address the same way. `6fd9fa30` carries that
> name and the identical comment, and is not animation code at all: it strides
> `0x23c` — the Skills table's record size — reads the unit's four base stats
> and compares them against requirement fields at `+0x176`/`+0x178`/`+0x17a`/
> `+0x17c`, returning a bool. It is a skill stat-requirement check.
| `SetAnimFieldPair` | `6fd5dce0`, `6fdce390`, `6fdd2e40` | Sets paired animation fields (3 instances) |
| `SetUnitAnimData` | `6fd84670` | Sets animation data on a unit |
| `GetObjectAnimModeRecordByte` | `6fd7ede0` | Gets object animation mode from record |
| `UnpackAnimComponentFields` | `6fdd2e80` | Unpacks packed animation component field data |
| `GetGraphicsModeField0` | `6fd7e940` | Gets graphics mode field 0 |
| `GetGraphicsModeField8` | `6fd7e900`, `6fd8c570` | Gets graphics mode field 8 (2 instances) |
| `GetGraphicsModeFrameFixed` | `6fdce290` | Gets fixed-point graphics mode frame |
| `DATATBLS_LoadAnimDataTable` | `6fd91e50` | Loads the AnimData.d2 table from resources |
| `ANIM_LookupAnimDataByPath` | `6fd91f70` | Looks up animation data by file path |
| `ANIMATE_FreeAllAnimateTables` | `6fd68f90` | Frees all loaded animation tables |
| `UNITS_CalcAnimationFrameOffset` | `6fd7e840` | Calculates animation frame offset for unit |
| `UNITS_InitializeAnimationFromGfxMode` | `6fd82820` | Initializes animation from graphics mode |
| `UNITS_UpdateAnimationSpeedByMode` | `6fd83110` | Updates animation speed based on current mode |
| `ResetEnvironmentAnimState` | `6fd88de0` | Resets environment animation state |
| `UpdatePlayerSkillAnimData8` | `6fd80670` | Updates player skill animation data (field 8) |
| `UpdatePlayerSkillAnimDataC` | `6fd806d0`, `6fd9e0a0` | Updates player skill animation data (field C, 2 instances) |
| `PATH_UpdateAnimationFrame` | `6fd87570` | Updates animation frame from path system |
| `SKILLS_GetActiveSkillNode` | `6fd80460` | Returns the active-skill node held at `pInfo+0x08` -- a `Skill*`, not animation data; callers read the skill id and flags out of it |
| `GFX_GetAnimationFlags` | `6fd85100` | Gets animation flags from graphics system |
| `ResolvePathModeAfterStep` | `6fd85210` | Resolves path mode after movement step |

### D2Client.dll
| Function | Address | Purpose |
|----------|---------|---------|
| `FindAnimationFrameByOrdinal` | `6fb19910` | Looks up a specific animation frame by ordinal index |
| `GetAnimationFrameIndex` | `6fb58710` | Gets current animation frame index |
| `SetUnitAnimationRate` | `6fb58be0` | Sets playback rate for unit animation |
| `SetUnitAnimationProperties` | `6fb77c80` | Sets animation properties (rate, frame, flags) |
| `MarkUnitAnimationDirty` | `6fb77c30` | Marks unit animation as needing refresh/rebuild |
| `SetViewSpriteAnimState` | `6fac4520` | Sets animation state for view sprite |
| `AnimateLoadingProgressBar` | `6fac4d30` | Animates the loading screen progress bar |
| `InitializeStarFieldAnimation` | `6fb3ae00` | Initializes star field background animation |

---

## Stage 3: Component/Layer System

Here is where "composite" earns its name. The COF's layer list is the heart of the format: a set of components -- `HD`, `TR`, `LG`, `RA`, `LA`, `RH`, `LH`, `SH`, `S1` through `S8` -- each a body part or a piece of gear, drawn in a defined order. `HD` is the head, `TR` the torso, `LG` the legs; `RA` and `LA` are the right and left arms; `RH` and `LH` are the right and left hands, which is where a weapon rides; `SH` is the shield; `S1` through `S8` are special layers for effects a particular animation needs. Stacked in order, they are a character.

For the Amazon in town, hand-to-hand, the visible set is her body -- head, torso, legs, arms -- with empty hands. `RenderUnitComponentLayers` is the function that walks this list, and for each entry it asks two questions. `IsComponentVisibleForUnitType` decides whether the layer should be drawn at all: an unarmed unit skips the weapon-hand layers, and a unit type that never uses a given component skips it too. `GetEquipSlotAnimComponent` maps a piece of equipment back to the component it feeds, which is how putting on a breastplate changes the `TR` layer and nothing else.

The rest of the stage is the plumbing behind those two questions. `GetComponentListMode`, `GetComponentListType`, and `GetComponentListColorIndex` read the mode, type, and palette index out of a component-list entry -- the last of which is how a single torso sprite can be drawn in a dozen armor colours. `RebuildEquippedVisualComponents`, over on the D2Common side, is what fires when gear changes: it recomputes the component set so the next frame draws the new loadout. And because a component can carry sound as well as pixels, `GetComponentSoundIds` and `PlayComponentAnimationSound` tie a footstep or a weapon-whoosh to the exact frame of the exact layer that should make it.

### D2Common.dll
| Function | Address | Purpose |
|----------|---------|---------|
| `GetEquipSlotAnimComponent` | `6fd717f0`, `6fd82220` | Maps equipment slot to animation component (HD/TR/LG/RA/LA...) (2 instances) |
| `GetMonsterComponentRecord` | `6fdd4c70` | Gets component record for a specific monster type |
| `GetMonsterMaxComponentVisualTier` | `6fd73540`, `6fd738b0` | Gets maximum visual tier for monster components (2 instances) |
| `RebuildEquippedVisualComponents` | `6fd71420` | Rebuilds visual component data when equipment changes |
| `UnpackAnimComponentFields` | `6fdd2e80` | Unpacks packed animation component data |

### D2Client.dll
| Function | Address | Purpose |
|----------|---------|---------|
| `GetComponentAnimationFrame` | `6fad21c0` | Gets the current animation frame for a specific component layer |
| `GetComponentListColorIndex` | `6fb1d690` | Gets palette transform/colour index for a component |
| `GetComponentListMode` | `6fb1d700` | Gets the mode field from a component-list entry |
| `GetComponentListType` | `6fb1d740` | Gets the type field from a component-list entry |
| `GetComponentListProperty` | `6fb1d780` | Gets a property from a component-list entry |
| `GetComponentSoundIds` | `6fad27e0` | Gets sound IDs associated with a component |
| `IsComponentVisibleForUnitType` | `6fb1da40` | Checks if a component layer should be drawn for this unit type |
| `PlayComponentAnimationSound` | `6fad2d60` | Plays sound associated with a component's current animation frame |
| `InvokeComponentHandlers` | `6fad12f0` | Invokes component handler callbacks |
| `InvokeComponentCreateHandlers` | `6fad13e0` | Invokes component creation handler callbacks |
| `InvokeComponentHandlersType2` | `6fad13d0` | Invokes type 2 component handlers |
| `InvokeComponentHandlersType3` | `6fad13c0` | Invokes type 3 component handlers |
| `GetMonsterSkillComponentIndex` | `6fb779c0` | Gets component index for monster skill effects |
| `CopyItemColorComponents` | `6fb1d840` | Copies item colour/palette data to component |
| `GetRecordSpriteData` | `6fb1d8c0` | Gets sprite data from a component record |
| `InitializeSpriteDataArrays` | `6fb1d400` | Initializes sprite data arrays for components |
| `GetGraphicsEffectComponentByIndex` | `6fab1a60` | Gets graphics effect component by index |
| `RenderUnitComponentLayers` | `6fb1c490` | Main function: iterates all COF layers and renders each component |

### RenderUnitComponentLayers Call Graph
```
RenderUnitByType (6fb1cc00)
  -> RenderUnitComponentLayers (6fb1c490)
       -> DispatchGraphicsLayerRender (6fb1a2f0)
       -> DrawGraphicsInterface (6fabd120)
       -> GetComponentListColorIndex (6fb1d690)
       -> GetComponentListProperty (6fb1d780)
       -> GetUnitDrawInfo (6fb560d0)
       -> GetUnitGlowType (6fb1dfe0)
       -> GetUnitPaletteTransform (6fb1dee0)
       -> RenderUnitOverlayEffects (6fb1aee0)
       -> RenderUnitWithVisibilityCheck (6fb166d0)
       -> ValidateSpriteWithinScreen (6fb1a3d0)
       -> ProcessUnitGraphicsEffect (6fb1e2c0)
       -> ProcessSkillWithLevelCheck (6fb1dca0)
       -> GetSkillPaletteData (6fb03a30)
       -> CompareSelectedMissileUnit (6fb56fc0)
```

---

## Stage 4: Sprite System (D2CMP.dll)

Every visible component the last stage kept needs actual pixels, and those pixels live in sprite files -- `DCC` for the compact, delta-compressed character animations, `DC6` for simpler frames. This is the layer of the pipeline that turns a component's identity into loaded, decoded image data, and it lives entirely in D2CMP.dll, the compression module. What is actually in those two file formats, byte by byte, is the [sprite formats chapter](sprite-formats-dcc-dc6.md)'s subject; this stage only needs to know that a load produces cached, decoded frames.

The Amazon's torso layer, having survived the visibility check, now needs its sprite. `BuildSpritePath` constructs the file path; `BuildSpriteCacheKey` turns that path into a lookup key; and `LoadOrCreateSpriteEntry` checks the cache before it touches the disk. That ordering is the stage's whole economy: a town full of Amazons sharing a body sprite loads it once and reads it from the cache every time after. Only on a miss does `LoadSpriteDefinition` read the header and `CacheAndInitializeSpriteByType` branch on whether the file is DCC or DC6 and set it up accordingly.

Sprites do not all finish loading at once. `GetSpriteProcessingStatus`, `FindAsyncSpriteByParams`, and `CountCompletedAsyncSprites` exist because some loads are asynchronous -- tracked and drained over several frames rather than blocking the game on a stall. Underneath sits an LRU cache (`InitializeSpriteCache`, `AllocateSpriteCacheSlot`) that evicts the least-recently-used sprite when memory runs short, which is the mechanism the debug string `COF Memory->%i of %i` was built to watch. The long blitting table at the end of the stage -- the family of `BlitSpriteRLE*` functions -- is the eventual payoff: each is a specialized inner loop for one combination of clipped-or-full and plain-palette-or-blended-or-chained-table drawing, and `BlitSpriteDispatcher` picks the right one for a given sprite. We do not reach those until Stage 6, but they are staged here because the sprite is where a blit's pixels come from.

### Sprite Loading & Cache
| Function | Address | Purpose |
|----------|---------|---------|
| `BuildSpritePath` | `6fe1c2d0` | Builds the DCC/DC6 sprite file path |
| `BuildSpriteCacheKey` | `6fe1c670` | Generates a cache key for sprite lookup |
| `LoadOrCreateSpriteEntry` | `6fe1def0` | Main sprite load entry point (checks cache first, loads if missing) |
| `LoadSpriteFrameWithCache` | `6fe1e1d0` | Loads a specific sprite frame using LRU cache |
| `LoadSpriteDefinition` | `6fe1d350` | Loads sprite definition/header from file |
| `CacheAndInitializeSpriteByType` | `6fe1ddc0` | Caches and initializes sprite by type (DCC vs DC6) |
| `CacheAndInitializeSpriteFromSource` | `6fe1dc80` | Caches and initializes sprite from source data |
| `InitializeSpriteSystem` | `6fe1d680` | Initializes the entire sprite subsystem |
| `InitializeSpriteCache` | `6fe1c0b0` | Sets up the LRU sprite cache |
| `InitializeSpriteEntity` | `6fe1d7f0` | Initializes a sprite entity structure |
| `InitializeSpriteEntityByType` | `6fe1d9f0` | Initializes sprite entity by type (DCC/DC6) |
| `ShutdownSpriteSystem` | `6fe1d560` | Shuts down the sprite subsystem |

### Sprite Data Access
| Function | Address | Purpose |
|----------|---------|---------|
| `GetSpriteFramePointer` | `6fe1d140` | Gets pointer to a decoded sprite frame |
| `GetValidatedSpriteFrame` | `6fe1d310` | Gets validated sprite frame with error checking |
| `GetSpriteProcessingStatus` | `6fe1c280` | Gets async processing status of a sprite |
| `DecodeSpriteFrameData` | `6fe1d76e` | Decodes compressed sprite frame data |
| `FreeSpriteEntity` | `6fe1d600` | Frees a sprite entity and its resources |
| `FreeSpriteFrameData` | `6fe1bc00` | Frees sprite frame pixel data |
| `ResetSpriteFrameFields` | `6fe1bc70` | Resets sprite frame fields to default |
| `ReleaseSpriteDefinition` | `6fe1d5e0` | Releases a loaded sprite definition |
| `FormatSpriteErrorMessage` | `6fe1d1f0` | Formats an error message for sprite loading failures |

### Sprite Cache Management
| Function | Address | Purpose |
|----------|---------|---------|
| `AllocateSpriteCacheSlot` | `6fe1cc40` | Allocates a slot in the sprite cache |
| `AllocateSpriteFrameInfoArray` | `6fe1cfc0` | Allocates frame info array for sprite |
| `DestroySpriteCache` | `6fe1ce40` | Destroys the sprite cache |
| `FindAsyncSpriteByParams` | `6fe1be10` | Finds an async sprite load by parameters |
| `CountCompletedAsyncSprites` | `6fe1bea0` | Counts completed async sprite loads |

### Sprite Iterators
| Function | Address | Purpose |
|----------|---------|---------|
| `InitializeSpriteFrameIterator` | `6fe23b10` | Initializes an iterator for walking sprite frames |
| `AdvanceSpriteFrameIterator` | `6fe23300` | Advances the sprite frame iterator to next frame |

### Sprite Blitting (Rendering to Screen)
| Function | Address | Purpose |
|----------|---------|---------|
| `BlitSpriteDispatcher` | `6fe20270` | Main blit dispatcher -- selects appropriate blit function |
| `BlitSpriteRLECopyClipped` | `6fe1faa0` | Blits RLE sprite with clipping |
| `BlitSpriteRLECopyFull` | `6fe1fb50` | Blits RLE sprite without clipping (full frame) |
| `BlitSpriteRLEWithBlendClipped` | `6fe1e480` | Blits RLE sprite with alpha blend + clipping |
| `BlitSpriteRLEWithBlendFull` | `6fe1e6b0` | Blits RLE sprite with alpha blend, no clipping |
| `BlitSpriteRLEWithChainedTableClipped` | `6fe1f2c0` | Blits with chained palette table + clipping |
| `BlitSpriteRLEWithChainedTableFull` | `6fe1f4b0` | Blits with chained palette table, no clipping |
| `BlitSpriteRLEWithDualTableClipped` | `6fe1eb50` | Blits with two chained palette tables + clipping |
| `BlitSpriteRLEWithPaletteBlendClipped` | `6fe1f620` | Blits with palette blend + clipping |
| `BlitSpriteRLEWithDualTableFull` | `6fe1edd0` | Blits with two chained palette tables, no clipping |
| `BlitSpriteRLEWithPaletteBlendFull` | `6fe1f8a0` | Blits with palette blend, no clipping |
| `BlitSpriteRLEWithPaletteClipped` | `6fe1e860` | Blits with palette + clipping |
| `BlitSpriteRLEWithPaletteClippedAlt` | `6fe1efd0` | Blits with palette + clipping (alternate dispatch path) |
| `BlitSpriteRLEWithPaletteFull` | `6fe1ea20` | Blits with palette, no clipping |
| `BlitSpriteRLEWithPaletteFullAlt` | `6fe1f190` | Blits with palette, no clipping (alternate dispatch path) |
| `ConvertRGBSpriteToRLE` | `6fe20710` | Converts RGB pixel data to RLE encoding |
| `CompressFramesToRLEWithDirections` | `6fe208f0` | Compresses multi-direction frames to RLE |
| `CalculateDirectionScaleFactor` | `6fe1e090` | Calculates scale factor for direction rendering |

---

## Stage 5: Cel/Frame Primitives (D2CMP.dll)

A sprite is a container; a cel is what it contains. Every DCC or DC6 holds a grid of cels -- one image per direction, per frame -- and the cel is the smallest thing the engine draws. When Stage 4 loads the Amazon's torso sprite, it is really loading a set of cels: her town idle, in every facing, one cel each.

Two facts about D2's format make this stage necessary rather than trivial. First, the frames are compressed: `DecompressDCCDirection` unpacks one direction's worth of DCC data at a time, and `CalculateDCCCellCount` works out how many cells that direction decodes into. Second, the frames are stored per-direction, so `PopulateDirectionFramePointers` builds the table that lets the renderer jump straight to a given facing and frame without walking the whole sprite. `LoadCelDataCached` fronts all of this with its own cache, and `LoadCelFromHashTable` resolves a cel by hash -- the same load-once discipline as the sprite layer, one level down.

The accessors read the format's fields directly. `GetCelFramesPerDirection` and `GetCelDirectionBitmask` answer "how many frames, in which directions"; `GetCelFramePixelData` reaches the raw pixels; `GetCelFrameTableEntry` indexes a specific frame. When our Amazon turns to face a new direction, these are the functions that produce the exact cel to hand upward to the blitter. Everything else in the stage -- the cel entities, the clone and free and validate routines, the tile-direction chain -- is the bookkeeping that keeps those cels alive in memory for exactly as long as something on screen still needs them.

### Cel Loading & Cache
| Function | Address | Purpose |
|----------|---------|---------|
| `LoadCelDataCached` | `6fe1bc90` | Loads cel data with LRU caching |
| `LoadCelFromHashTable` | `6fe22580` | Retrieves cel from hash-based lookup table |
| `InitializeCelCache` | `6fe1bbb0` | Initializes the cel cache system |
| `ShutdownCelCache` | `6fe1bd70` | Shuts down the cel cache |
| `FlushCelCache` | `6fe1bc60` | Flushes all entries from cel cache |

### Cel Data Operations
| Function | Address | Purpose |
|----------|---------|---------|
| `ConvertRawCelToLoaded` | `6fe21d00` | Converts raw cel bytes to renderable format |
| `CreateCelEntity` | `6fe228b0` | Creates a new cel entity |
| `CreateCelFromRawData` | `6fe21ee0` | Creates cel from raw byte data |
| `DestroyCelCacheEntry` | `6fe22670` | Destroys a cel cache entry |
| `CloneCelData` | `6fe21b90` | Clones cel data (deep copy) |
| `FreeCelData` | `6fe26600` | Frees cel data memory |
| `InitializeCelFrameData` | `6fe21ac0` | Initializes cel frame data structures |
| `ValidateCelAndInitModule` | `6fe21a80` | Validates cel data and initializes module |
| `ValidateCelDataPtr` | `6fe21f80` | Validates a cel data pointer |

### Cel Accessors
| Function | Address | Purpose |
|----------|---------|---------|
| `GetCelDirectionBitmask` | `6fe21640` | Gets bitmask of which directions exist in this cel |
| `GetCelFramesPerDirection` | `6fe21610` | Gets number of frames per direction |
| `GetCelRenderDataPtr` | `6fe216a0` | Gets pointer to cel render data |
| `GetCelRenderState` | `6fe1ba70` | Gets current render state of a cel |
| `GetCelDataSize` | `6fe21700` | Gets total data size of cel |
| `GetCelFlags` | `6fe21730` | Gets cel flags |
| `GetCelNextPtr` | `6fe216d0` | Gets pointer to next cel in chain |
| `GetCelDescriptorFramePtr` | `6fe214f0` | Gets frame pointer from cel descriptor |
| `GetCelEmbeddedDataPtr` | `6fe21670` | Gets embedded data pointer |
| `GetCelPixelSizeMapping` | `6fe221c0` | Gets pixel size mapping table |
| `GetCelFramePixelData` | `6fe222e0` | Gets raw pixel data for a cel frame |
| `GetCelFrameTableEntry` | `6fe22240` | Gets frame table entry by index |
| `SerializeCelDataToBuffer` | `6fe25e20` | Serializes cel data to output buffer |

### Cel Blitting
| Function | Address | Purpose |
|----------|---------|---------|
| `BlitAllCelFrames` | `6fe219c0` | Blits all frames of a cel |
| `BlitCelFrameClipped` | `6fe21780` | Blits a single cel frame with clipping |
| `BlitCelFrameHorizontalClipped` | `6fe21890` | Blits cel frame with horizontal clipping |
| `BlitCelFrameWithSolidColor` | `6fe22480` | Blits cel frame filled with solid colour |
| `AddCelFrameToQueue` | `6fe22790` | Adds cel frame to render queue |
| `CopyCelFramePixelData` | `6fe22380` | Copies cel frame pixel data |
| `EnumerateCelPixels` | `6fe25b00` | Enumerates all pixels in a cel |
| `IterateAllCelFramePixels` | `6fe22400` | Iterates over all pixels in all frames |
| `FixupCelDataLayout` | `6fe25d10` | Fixes up cel data memory layout |
| `FlushCelTextures` | `6fe21a30` | Flushes cel texture cache |
| `InvalidateCelFrameTextures` | `6fe21520` | Invalidates cached textures for cel frames |
| `BuildTileCelDescriptor` | `6fe26750` | Builds a cel descriptor for tile rendering |

### DCC Decompression
| Function | Address | Purpose |
|----------|---------|---------|
| `DecompressDCCDirection` | `6fe240a0` | Decompresses a single DCC direction |
| `CalculateDCCCellCount` | `6fe24020` | Calculates cell count for DCC frame |
| `PopulateDirectionFramePointers` | `6fe276d0` | Populates frame pointers for all directions |

### Direction/Tile Management
| Function | Address | Purpose |
|----------|---------|---------|
| `AllocateTileDirection` | `6fe25a10` | Allocates a tile direction entry |
| `CloneTileDirection` | `6fe25fd0` | Clones a tile direction |
| `CreateTileDirectionEntry` | `6fe263b0` | Creates a new tile direction entry |
| `EncodeTileDirectionsToPixelData` | `6fe26a10` | Encodes tile direction data to pixels |
| `FreeTileDirection` | `6fe25a60` | Frees a tile direction entry |
| `FreeTileDirectionChain` | `6fe269b0` | Frees entire tile direction chain |
| `GetTileDirectionFrameOffset` | `6fe21fb0` | Gets byte offset for a given direction |
| `GetTileDirectionRenderData` | `6fe220a0` | Gets render data for a tile direction |
| `InitializeSpriteDecompressionLock` | `6fe22ff0` | Initializes decompression thread lock |
| `CleanupSpriteDecompressionLock` | `6fe22fc0` | Cleans up decompression thread lock |
| `LoadTileProject` | `6fe19720` | Loads a tile project |
| `LoadTileResourceData` | `6fe23010` | Loads tile resource data |

### Cell List Management
| Function | Address | Purpose |
|----------|---------|---------|
| `AllocateAndInitializeCellList` | `6fe11a50` | Allocates and initializes a cell list |
| `ProcessMemberDestructors` | `6fe11ad0` | Runs the container's member destructors (a distinct function, not a second `CleanupAllCellListItems` instance) |
| `CleanupAllCellListItems` | `6fe11f80` | Cleans up all cell list items |
| `InitializeCellListStructure` | `6fe11ba0` | Initializes cell list structure |
| `FreeAndUnwindCellListArray` | `6fe11c10` | Frees and unwinds cell list array |
| `UnlinkAndFreeCellList` | `6fe11fc0` | Unlinks and frees a cell list |

---

## Stage 6: Rendering Pipeline (D2Client.dll)

This is where the Amazon becomes pixels. Everything so far has been preparation -- a path, a mode, a layer list, loaded sprites, decoded cels. Rendering is where they are stacked and drawn: in the right order, at the right place, behind the right walls.

The frame does not draw units in the order it finds them; it draws them in depth order, so a character standing behind a fence is occluded by it. `AddUnitToRenderQueue` inserts each unit into a depth-sorted queue, `CompareDrawItemSortOrder` is the comparison that keeps that queue ordered, and separate entries handle the special cases -- `AddShadowToRenderQueue` for the blob of shadow beneath her, `AddGroundUnitToRenderQueue` for items and corpses that sit on the floor, `AddWallUnitToRenderQueueWithFade` for the walls that fade out when she steps behind them.

When the queue drains, `RenderUnitByType` dispatches each unit by what it is -- player, monster, object, or missile -- and for our Amazon that leads to `RenderUnitComponentLayers`, the same layer-walking function from Stage 3, now on the drawing side. It iterates her components in stacking order and, for each, calls down through `DispatchGraphicsLayerRender` into `BlitSpriteDispatcher` in D2CMP -- the dispatcher from Stage 4, finally invoked. The blitter copies the chosen cel's pixels into the frame buffer, applying whatever palette transform `GetUnitPaletteTransform` and `GetUnitRenderColorByMode` selected for her current mode. Layer by layer -- legs, torso, arms, head -- she is composited back together on the screen, in the same order the COF declared her apart. The head goes down last, on top, and the Amazon is drawn.

That is the full round trip: `AMTNHTH.cof` named her, the animation system timed her, the component system chose her parts, the sprite and cel systems supplied her pixels, and the render queue placed her in the world. Stages 7 through 11 are variations on this exact path.

### Render Queue
| Function | Address | Purpose |
|----------|---------|---------|
| `AddUnitToRenderQueue` | `6fb29d30` | Adds a unit to the depth-sorted render queue |
| `AddUnitToRenderQueueWithFlags` | `6fb29e20` | Adds unit to render queue with rendering flags |
| `AddGroundUnitToRenderQueue` | `6fb2a500` | Adds ground-level unit (corpses, items) to queue |
| `AddShadowToRenderQueue` | `6fb2a180` | Adds unit shadow to render queue |
| `AddWallUnitToRenderQueueWithFade` | `6fb2a340` | Adds wall unit with fade transparency |
| `AllocateWallRenderGrid` | `6fb2a0d0` | Allocates grid for wall rendering |
| `InitializeWallRenderingPass` | `6fb2a9f0` | Initializes a wall rendering pass |
| `ProcessTilesForWallRendering` | `6fb2a5d0` | Processes tiles for wall rendering |
| `ProcessViewportWallRendering` | `6fb3b0c0` | Processes viewport wall rendering |

### Unit Rendering
| Function | Address | Purpose |
|----------|---------|---------|
| `RenderUnitByType` | `6fb1cc00` | Top-level: dispatches rendering by unit type (player/monster/object/missile) |
| `RenderUnitComponentLayers` | `6fb1c490` | Iterates all COF layers and renders each component |
| `DispatchGraphicsLayerRender` | `6fb1a2f0` | Dispatches rendering for each graphics layer |
| `RenderUnitWithVisibilityCheck` | `6fb166d0` | Renders unit with visibility/frustum check |
| `RenderUnitSpriteWithEffects` | `6fb1a860` | Renders unit sprite with visual effects applied |
| `RenderUnitWithPerspective` | `6fb1b700` | Renders unit with perspective transform |
| `RenderUnitWithShadowEffect` | `6fb1ba90` | Renders unit with shadow effect |
| `RenderUnitWithAnimationOffset` | `6fb75e50` | Renders unit at animation offset position |
| `ValidateSpriteWithinScreen` | `6fb1a3d0` | Validates sprite is within screen bounds before rendering |
| `RenderUnitOverlayEffects` | `6fb1aee0` | Renders overlay effects (auras, enchants, states) |
| `DrawOverlaySpritesForLayer` | `6fb1b190` | Draws overlay sprites for a specific layer |
| `DrawAtUnitOffset` | `6fb39f60` | Draws a sprite at a unit's screen position |

### Unit Draw State
| Function | Address | Purpose |
|----------|---------|---------|
| `ApplyUnitDrawInfo` | `6fb74960` | Applies draw info (position, colour, flags) to unit |
| `GetUnitDrawInfo` | `6fb560d0` | Gets current draw parameters for a unit |
| `GetUnitRenderColorByMode` | `6fb02eb0` | Gets palette shift/colour based on animation mode |
| `SetUnitDrawFlagHigh` | `6fb607e0` | Sets high draw flag on unit |
| `DrawGraphicsInterface` | `6fabd120` | Low-level graphics interface draw call |
| `GfxRenderCommand` | `6fabd1c8` | Executes a graphics render command |
| `GfxDrawAndUpdateFrameIndex` | `6fabd0f6` | Draws and updates frame index |
| `DrawRectangleWithOffset` | `6fb18970` | Draws rectangle at offset position |
| `CompareDrawItemSortOrder` | `6fb37a90` | Comparison function for depth-sorting draw items |

### Unit Animation Loading
| Function | Address | Purpose |
|----------|---------|---------|
| `LoadUnitAnimationFrames` | `6fb1ac60` | Loads all animation frames for a unit |
| `LoadUnitOverlayAnimationFrames` | `6fb1c020` | Loads overlay animation frames (auras, enchants) |
| `LoadAllGameStateAnimationFrames` | `6fb1c3b0` | Bulk loads animation frames for all game state overlays |
| `AllocateUnitGraphicsNode` | `6fb19b40` | Allocates the graphics data structure for a unit |
| `AllocateUnitGraphicsWithValidation` | `6fb19f20` | Allocates graphics with validation checks |
| `CleanupUnitAnimationChain` | `6fb19700` | Frees animation chain when unit is destroyed |
| `ProcessUnitGraphicsModeUpdate` | `6fb1a180` | Processes graphics mode update for unit |
| `ProcessUnitGraphicsEffect` | `6fb1e2c0` | Processes graphics effects on unit |
| `ProcessSkillRecordFlags` | `6fb1a0c0` | Processes skill record flags (caller of LoadCofAndSetupSkillLayers) |

### Graphics Pipeline Support
| Function | Address | Purpose |
|----------|---------|---------|
| `HandleUnitModeVisualEffects` | `6fad0c40` | Applies visual effects based on current mode |
| `GetUnitGlowType` | `6fb1dfe0` | Gets glow type for unit rendering |
| `GetUnitPaletteTransform` | `6fb1dee0` | Gets palette transform for unit |
| `GetSkillPaletteData` | `6fb03a30` | Gets palette data for skill effects |
| `ProcessSkillWithLevelCheck` | `6fb1dca0` | Processes skill with level check for rendering |
| `ApplyMissileRenderEffect` | `6fb50650` | Applies render effect to missile |
| `CheckUnitGraphicsBoundsHit` | `6fb1a550` | Checks if point hits unit graphics bounds |
| `GetUnitGraphicsOutputCoords` | `6fb1a6b0` | Gets unit's output screen coordinates |
| `ProcessUnitSelectionGraphics` | `6fb1e670` | Processes selection highlight graphics |

---

## Stage 7: Monster-Specific Animation (D2Client.dll)

Monsters run the same pipeline as the Amazon, with one addition: a monster is a state machine. A Fallen idles, notices you, charges, attacks, flinches when hit, and dies -- and each of those is a mode with its own COF, the same mode-to-COF mechanism from Stage 2, but driven by AI rather than by input. `ProcessMonsterModeAnimation` is that state machine, and `ProcessMonsterAnimationFrame` does the per-frame work inside whichever mode is current.

The rest of the stage exists because not every monster is drawn from the standard tables. `CheckMonsterSpecialGraphicsMode` and `CheckMonsterHasSpecialGraphics` test for the overrides that super uniques and bosses carry -- the reason a unique monster can be tinted or resized away from its base type -- while `GetMonsterModeGraphicsData` fetches the graphics for a monster in a given mode and `GetMonsterAnimationRate` sets how fast they play. `HandleMonsterModeStateEvent` handles the events fired as one mode gives way to the next, the monster-side echo of the frame events from Stage 2.

| Function | Address | Purpose |
|----------|---------|---------|
| `ProcessMonsterModeAnimation` | `6fafda60` | Handles monster mode transitions and animation state machine |
| `ProcessMonsterAnimationFrame` | `6faff2c0` | Per-frame processing for monster animations |
| `GetMonsterAnimationRate` | `6fadfcd0` | Gets animation speed for monster type |
| `GetMonsterModeGraphicsData` | `6fafbff0` | Gets graphics data for a monster in a specific mode |
| `CheckMonsterSpecialGraphicsMode` | `6fb56070` | Checks for special/unique monster graphics (super uniques, etc.) |
| `CheckMonsterHasSpecialGraphics` | `6fb556c0` | Checks if monster has special graphics override |
| `HandleMonsterModeStateEvent` | `6fafee40` | Handles state events during monster mode transitions |
| `GetMonsterBaseId` | `6fadfcb0` | Gets base monster ID |
| `GetMonsterBaseSound` | `6fadfc90` | Gets base sound for monster |

---

## Stage 8: Overlay System (D2Client.dll)

An overlay is a layer that does not belong to the unit's own body. A Paladin's aura, the frost on a chilled enemy, the shimmer of a stat buff, rain and bubbling water -- all of these are drawn on or around a unit, and all of them ride the same COF-and-sprite machinery as the body layers. The difference is only where they attach.

Creation is split by cause. `CreateOverlayEffect` is the general path; `CreateOverlayByUnitModeFlags` spawns overlays that a unit's current mode implies; `CreateObjectSpecialOverlay` and `CreateRandomEventOverlay` cover objects and world events. Once created, `ApplyOverlayEffectToUnit` binds the overlay to its host, and `RenderUnitOverlayEffects` -- which also appears in the Stage 6 render call graph, because that is where overlays actually draw -- walks the attached effects and blits each one. `ProcessOverlayEffectTimers` is what makes overlays temporary: a buff that lasts a set time is an overlay whose timer expires and whose resources `CleanupAllOverlayResources` then reclaims. The weather group at the end (`InitializeWeatherOverlays`, `UpdateBubbleOverlayAnimations`) is the same idea scaled up from a single unit to the whole screen.

### Overlay Creation & Management
| Function | Address | Purpose |
|----------|---------|---------|
| `CreateOverlayEffect` | `6fb1b300` | Creates a new overlay visual effect |
| `CreateOverlayByUnitModeFlags` | `6fada590` | Creates overlay based on unit mode flags |
| `CreateObjectSpecialOverlay` | `6fad5b70` | Creates special overlay for objects |
| `CreateRandomEventOverlay` | `6fb22c80` | Creates overlay for random events |
| `ApplyOverlayEffectToUnit` | `6fb23760` | Applies an overlay effect to a unit |
| `ApplyOverlayWithSound` | `6fb56f60` | Applies overlay with associated sound |
| `ApplyOverlayStatToUnit` | `6fb32210` | Applies stat-based overlay to unit |
| `HasUnitOverlayEffect` | `6fb19130` | Checks if unit has a specific overlay |
| `NormalizeOverlayMode` | `6fb1d2f0` | Normalizes overlay mode value |
| `GetOverlayRecordFlag` | `6fb1d7c0` | Gets flag from overlay record |
| `SetOverlayInstanceValue30` | `6fb18c80` | Sets overlay instance value at offset 0x30 |
| `SetOverlayListItemValue` | `6fb190c0` | Sets value in overlay list |
| `SetOverlayRecordPointer` | `6fb1d800` | Sets overlay record pointer |

### Overlay Rendering
| Function | Address | Purpose |
|----------|---------|---------|
| `RenderUnitOverlayEffects` | `6fb1aee0` | Main overlay rendering for a unit |
| `DrawOverlaySpritesForLayer` | `6fb1b190` | Draws overlay sprites for a specific layer |
| `RenderScreenOverlay` | `6fb21220` | Renders screen-wide overlay |
| `RenderScreenFadeOverlay` | `6fac0ec0` | Renders screen fade transition overlay |

### Overlay Processing
| Function | Address | Purpose |
|----------|---------|---------|
| `ProcessOverlayEffectCallback` | `6fb22530` | Processes overlay effect callbacks |
| `ProcessOverlayEffectTimers` | `6fb1c130` | Processes overlay timer expirations |
| `ProcessSkillOverlayEffects` | `6fb1c3f0` | Processes skill overlay effects |
| `ProcessUnitOverlayLayers` | `6fb1ea50` | Processes all overlay layers for a unit |
| `ProcessUnitModeWithOverlay` | `6fb23560` | Processes unit mode with overlay consideration |
| `ProcessObjectWithOverlayEffect` | `6fad59f0` | Processes object overlay effect |
| `ProcessUnitOverlayRanges` | `6fb22700` | Processes overlay ranges for unit |
| `UpdateUnitOverlayState` | `6fada040` | Updates overlay state for unit |

### Overlay Cleanup
| Function | Address | Purpose |
|----------|---------|---------|
| `CleanupAllOverlayResources` | `6fb50490` | Cleans up all overlay resources |
| `CleanupOverlayBufferState` | `6fb18be0`, `6fb18c30` | Cleans up overlay buffer state (2 instances) |
| `CleanupUnitOverlaySystem` | `6fb18f00` | Cleans up unit overlay system |
| `ClearPlayerOverlaySlots` | `6fb1cb30` | Clears player overlay slots |

### Weather & Environment Overlays
| Function | Address | Purpose |
|----------|---------|---------|
| `InitializeWeatherOverlays` | `6fb31750` | Initializes weather overlay effects |
| `UpdateBubbleOverlayAnimations` | `6fb300d0`, `6fb30160` | Updates bubble overlay animations (2 instances) |
| `ResetSplashAndBubbleOverlays` | `6fb31720` | Resets splash/bubble water effects |

---

## Stage 9: Mode Transitions (D2Client.dll)

Stage 2 showed a mode change setting a new COF in motion; this stage is the guard rail around that change. Not every transition is legal -- a dead unit does not start walking, and an attack in progress should not be interrupted arbitrarily -- so `CheckModeTransitionValid` vets each one before it is allowed to reload a COF.

When a transition is accepted, the old animation state has to be torn down cleanly so the new mode starts fresh. `ClearUnitGraphicsAndResetMode` drops the current graphics and returns the unit to a default mode; `ClearPlayerAnimationState` and the two `ClearUnitAnimationState` instances zero the per-unit animation fields; `ClearUnitAnimationDelay` clears the timing carried over from the previous mode. `HandleSkillModeTransition` is the specialized path for casting, where a skill drives the mode change. And `IsUnitInDeathMode` is the check that matters most often: death is the one transition that must not be reversed, and much of the rest of the engine asks this function before it touches a unit's animation.

| Function | Address | Purpose |
|----------|---------|---------|
| `CheckModeTransitionValid` | `6fb2aef0` | Validates that a mode transition is allowed |
| `ClearUnitGraphicsAndResetMode` | `6fb234d0` | Clears graphics and resets to default mode |
| `HandleSkillModeTransition` | `6fb07860` | Handles mode transition for skill activation |
| `DispatchStatModeHandler` | `6fb19a90` | Dispatches handler based on stat mode |
| `ClearPlayerAnimationState` | `6fab16a0` | Clears player animation state |
| `ClearUnitAnimationDelay` | `6fb06780` | Clears unit animation delay |
| `ClearUnitAnimationState` | `6fb06700`, `6fb06740` | Clears unit animation state (2 instances) |
| `IsUnitInDeathMode` | `6fb56180` | Checks if unit is in death animation mode |

---

## Stage 10: Missile Animation (D2Client.dll)

A missile -- an arrow, a fireball, a frozen orb -- is a unit thin enough to be almost pure animation. It has no equipment and no body layers to composite, but it still has a COF, a mode, and frames, and it flows through the same loading and blitting path as the Amazon who fired it.

What is distinctive here is motion and variation. `HandleMissileVelocityMode` ties the animation to the missile's velocity, and `SyncMissileAnimationWithTarget` keeps a tracking missile's frames aligned with where it is heading. Because a volley of identical arrows drawn in perfect lockstep looks artificial, `InitializeMissileAnimationRng` and `UpdateMissileAnimationWithRandom` seed each missile with a little randomness so a group animates with natural variation. The cluster of `CreateMissile*` functions are the spawn points -- one for scatter missiles, one for multiple simultaneous casts, one that adds a height offset, one that delays the mode change -- each wiring a new missile into this animation path as it is born.

| Function | Address | Purpose |
|----------|---------|---------|
| `HandleMissileAnimationMode` | `6fb60da0` | Handles missile animation mode |
| `HandleMissileVelocityMode` | `6fb624a0` | Handles missile velocity-based animation |
| `ProcessMissileFrameAnimation` | `6fb62f90` | Per-frame missile animation processing |
| `UpdateMissileAnimationWithRandom` | `6fb60df0` | Updates missile animation with randomization |
| `SyncMissileAnimationWithTarget` | `6fb60f40` | Syncs missile animation with target position |
| `InitializeMissileAnimationRng` | `6fb23b60` | Initializes RNG for missile animation variation |
| `InitializeUnitAnimationSeed` | `6fb60290` | Initializes animation seed for unit |
| `InitializeUnitAnimationWithSeed` | `6faf0130` | Initializes unit animation with specific seed |
| `CastMissileWithAnimation` | `6faf1290` | Casts a missile with animation |
| `CastMultipleMissilesAnimated` | `6fb24ac0` | Casts multiple missiles with animation |
| `CreateMissileWithAnimationFlags` | `6fb65a10` | Creates missile with specific animation flags |
| `CreateMissileWithAnimationHeight` | `6fb75d20` | Creates missile with animation height offset |
| `CreateScatterMissileWithAnimation` | `6fb60b70` | Creates scatter missile with animation |
| `CreateMissileWithDrawInfo` | `6fb60450` | Creates missile with draw info |
| `CreateMissileWithDelayedMode` | `6fb677a0` | Creates missile with delayed mode transition |

---

## Stage 11: Palette & Colour (D2CMP.dll)

Return, at the very end, to the promise the compositing system made at the start: that one sprite can wear many colours. This is the stage that keeps it. A palette transform is a small table that remaps a sprite's colour indices to different ones as it is blitted -- which is how a single torso cel becomes blue armor here and red armor there, and how a unique monster is tinted apart from its base, all without a second copy of a single pixel. This section is the address map only; the [palettes and colour chapter](palettes-and-colour.md) is where the `.dat`/`.pl2` byte layouts, the nine generator functions, and the item- and monster-colour mechanisms are traced and verified byte for byte.

`LoadPaletteFile` and `LoadPCXPaletteFromFile` bring these tables in from disk; `LoadItemPaletteFile` and `LoadAllItemPaletteTransforms` load the item-specific transforms that give equipment its colour variants. The blitters back in Stage 4 are the consumers -- the `WithPalette`, `WithChainedTable`, and `WithDualTable` variants each expect one of these tables to be resolved and ready. And the colour index chosen per component back in Stage 3, by `GetComponentListColorIndex`, is the number that selects among them. The loop closes here: the layer separation Stage 3 established is what makes per-layer recoloring possible, and this stage is where that recoloring is finally supplied.

| Function | Address | Purpose |
|----------|---------|---------|
| `LoadPaletteFile` | `6fe1a2e0` | Loads a palette (.dat) file |
| `LoadPCXPaletteFromFile` | `6fe19b00` | Loads palette from PCX file |
| `LoadItemPaletteFile` | `6fe24f20` | Loads item-specific palette |
| `LoadAllItemPaletteTransforms` | `6fe250d0` | Loads all item palette transform tables |
| `InitializeLoadedResourceHeader` | `6fe25660` | Initializes a loaded palette resource header |

---

## Pipeline Flow Summary

```
Unit Mode Change
  |
  v
SetUnitAnimMode (D2Common)
  |
  v
ProcessSkillRecordFlags (D2Client)
  |
  v
LoadCofAndSetupSkillLayers (D2Client)
  |-- BuildMonsterCofFilePath / BuildCofPathString
  |-- FindHashedArchiveEntry -> LoadAndCacheArchiveGraphics
  |
  v
InitializeCofLayerDataArrays (D2Client)
  |-- AllocateAndValidateResourceSlot
  |-- InitializeSpriteDataArrays
  |
  v
LoadCofLayerAnimationFrames (D2Client)
  |-- LoadUnitAnimationFrames
  |       |-- DispatchGraphicsLayerRender
  |       |       |-- FindAnimationFrameByOrdinal
  |       |       |-- ProcessUnitGraphicsModeUpdate
  |       |
  |       v
  |   BuildSpritePath (D2CMP)
  |       |-- LoadOrCreateSpriteEntry
  |       |       |-- LoadSpriteDefinition
  |       |       |-- DecodeSpriteFrameData
  |       |       |-- CacheAndInitializeSpriteByType
  |       |
  |       v
  |   LoadCelDataCached (D2CMP)
  |       |-- LoadCelFromHashTable
  |       |-- ConvertRawCelToLoaded
  |
  v
Per-Frame Update Loop:
  UpdateAllAnimationFrames (D2Common)
  |-- AdvanceAnimFrameWithWrap
  |-- SetAnimEventFromFrameData
  |
  v
Render Phase:
  AddUnitToRenderQueue (D2Client)
  |
  v
  RenderUnitByType
  |-- RenderUnitComponentLayers
  |       |-- per-layer: DispatchGraphicsLayerRender
  |       |       |-- BlitSpriteDispatcher (D2CMP)
  |       |-- RenderUnitOverlayEffects
  |       |-- DrawOverlaySpritesForLayer
  |
  v
  Screen Output
```

---

## Function Count Summary

Counted from the tables in this document, not from an independent survey of the
DLLs: "rows" is the number of table lines, "addresses" the number of distinct
function addresses those lines name.

| DLL | Table rows | Distinct addresses | Coverage |
|-----|-----------|--------------------|----------|
| **D2CMP.dll** | 111 | 111 | all named in Ghidra; none still `FUN_*` |
| **D2Common.dll** | 36 | 45 | all named in Ghidra; none still `FUN_*` |
| **D2Client.dll** | 144 | 144 | all named in Ghidra; none still `FUN_*` |
| **Total** | **291** | **300** | 300 of 300 named |

The two columns differ because a single row can carry several addresses when one
routine was compiled into more than one instance (`GetAnimDataFrameInfo` alone
accounts for four), while a function that appears in two stages --
`RenderUnitComponentLayers` in Stages 3 and 6, `RenderUnitOverlayEffects` in
Stages 6 and 8 -- is counted as a row twice but as one address. The "none still
`FUN_*`" claim was checked by listing every `FUN_*` function in each DLL:
D2Client and D2CMP have none at all, and the 21 that remain in D2Common are all
outside this document.

---

## Version differences

Every address in this chapter was verified against 1.13c alone. Unlike most of
this book's other chapters, each of which cross-checks 1.09d or a second
version somewhere in its body, this chapter's evidence does not extend to any
other patch. The provenance block's claim --
that mods of this era ship Blizzard's DLLs unchanged, so these addresses hold
for any 1.13c-based mod -- is a claim about *mods*, not about other game
*versions*: whether any address here matches 1.09d or another patch was not
checked, and no comparison is asserted.

---

## Companion report

Every function-address row, string-table entry, and call-graph edge in this
chapter, the corrections applied, and what remains open:
[cof-pipeline-1.13c.verification.md](cof-pipeline-1.13c.verification.md).
