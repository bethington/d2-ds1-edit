# COF (Composite Object File) Pipeline

> Reverse-engineered from the Diablo II 1.13c game DLLs. The addresses and call
> graphs below are 1.13c addresses, so they hold for any mod built on that
> version -- mods of this era ship Blizzard's DLLs unchanged.

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

---

## Stage 1: COF Path Construction & File Loading

These functions build the COF file path from unit token + animation mode + weapon class, then load and parse the COF file from the MPQ archive.

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

These functions manage animation state: which mode the unit is in, frame advancement, speed calculation, and AnimData table lookups.

### D2Common.dll
| Function | Address | Purpose |
|----------|---------|---------|
| `SetUnitAnimMode` | `6fd83920` | Sets the unit's animation mode (walk, attack, cast, etc.) -- triggers COF switch |
| `GetAnimSequenceRecord` | `6fd8e980` | Gets the AnimData sequence record for a token+mode combination |
| `AdvanceAnimFrameWithWrap` | `6fd7f060` | Advances animation frame counter with wrap-around at sequence end |
| `AdvanceAnimSubAccumulator` | `6fd7f090` | Sub-frame accumulator for smooth animation timing |
| `UpdateAllAnimationFrames` | `6fd8bbd0` | Per-tick update of all animation frames |
| `GetAnimDataFrameInfo` | `6fd71fb0`, `6fd8c640`, `6fd91ef0`, `6fd9fe80` | Gets frame info from AnimData.d2 tables (4 instances) |
| `ExtractAnimFrameData` | `6fd7e6e0`, `6fd9fa30` | Extracts frame data from AnimData records (2 instances) |
| `SetAnimEventFromFrameData` | `6fd7ee10` | Sets animation event triggers from frame data (sound, missile, etc.) |
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
| `InitializeAnimFromGraphicsMode` | `6fd95d30` | Initializes animation from graphics mode |
| `ResetEnvironmentAnimState` | `6fd88de0` | Resets environment animation state |
| `UpdatePlayerSkillAnimData8` | `6fd80670` | Updates player skill animation data (field 8) |
| `UpdatePlayerSkillAnimDataC` | `6fd806d0`, `6fd9e0a0` | Updates player skill animation data (field C, 2 instances) |
| `PATH_UpdateAnimationFrame` | `6fd87570` | Updates animation frame from path system |
| `SKILLS_GetActiveSkillAnimData` | `6fd80460` | Gets animation data for active skill |
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

The core of what makes COF "composite" -- these functions manage the individual body part layers (HD, TR, LG, RA, LA, RH, LH, SH, S1-S8) that are assembled into a complete character.

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
| `GetComponentListColorIndex` | `6fb1d690` | Gets palette transform/color index for a component |
| `GetComponentListProperty` | `6fb1d700`, `6fb1d740`, `6fb1d780` | Gets properties from component list entry (3 instances) |
| `GetComponentSoundIds` | `6fad27e0` | Gets sound IDs associated with a component |
| `IsComponentVisibleForUnitType` | `6fb1da40` | Checks if a component layer should be drawn for this unit type |
| `PlayComponentAnimationSound` | `6fad2d60` | Plays sound associated with a component's current animation frame |
| `InvokeComponentHandlers` | `6fad12f0` | Invokes component handler callbacks |
| `InvokeComponentCreateHandlers` | `6fad13e0` | Invokes component creation handler callbacks |
| `InvokeComponentHandlersType2` | `6fad13d0` | Invokes type 2 component handlers |
| `InvokeComponentHandlersType3` | `6fad13c0` | Invokes type 3 component handlers |
| `GetMonsterSkillComponentIndex` | `6fb779c0` | Gets component index for monster skill effects |
| `CopyItemColorComponents` | `6fb1d840` | Copies item color/palette data to component |
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

The sprite engine handles loading, caching, decompressing, and blitting DCC/DC6 sprite data. COF layers reference individual sprite files that are managed by this system.

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
| `BlitSpriteRLEWithPaletteBlendClipped` | `6fe1eb50`, `6fe1f620` | Blits with palette blend + clipping (2 variants) |
| `BlitSpriteRLEWithPaletteBlendFull` | `6fe1edd0`, `6fe1f8a0` | Blits with palette blend, no clipping (2 variants) |
| `BlitSpriteRLEWithPaletteClippedAlt` | `6fe1e860`, `6fe1efd0` | Blits with palette + clipping alt path (2 variants) |
| `BlitSpriteRLEWithPaletteFullAlt` | `6fe1ea20`, `6fe1f190` | Blits with palette, no clipping alt path (2 variants) |
| `ConvertRGBSpriteToRLE` | `6fe20710` | Converts RGB pixel data to RLE encoding |
| `CompressFramesToRLEWithDirections` | `6fe208f0` | Compresses multi-direction frames to RLE |
| `CalculateDirectionScaleFactor` | `6fe1e090` | Calculates scale factor for direction rendering |

---

## Stage 5: Cel/Frame Primitives (D2CMP.dll)

Low-level cel (cell) operations for individual animation frames. Cels are the atomic rendering unit.

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
| `BlitCelFrameWithSolidColor` | `6fe22480` | Blits cel frame filled with solid color |
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
| `CleanupAllCellListItems` | `6fe11ad0`, `6fe11f80` | Cleans up all cell list items (2 instances) |
| `InitializeCellListStructure` | `6fe11ba0` | Initializes cell list structure |
| `FreeAndUnwindCellListArray` | `6fe11c10` | Frees and unwinds cell list array |
| `UnlinkAndFreeCellList` | `6fe11fc0` | Unlinks and frees a cell list |

---

## Stage 6: Rendering Pipeline (D2Client.dll)

Functions that take assembled COF/sprite data and render it to the screen, including depth sorting, shadow rendering, and visual effects.

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
| `ApplyUnitDrawInfo` | `6fb74960` | Applies draw info (position, color, flags) to unit |
| `GetUnitDrawInfo` | `6fb560d0` | Gets current draw parameters for a unit |
| `GetUnitRenderColorByMode` | `6fb02eb0` | Gets palette shift/color based on animation mode |
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

Functions specifically for monster animation processing, including mode state machines and special monster graphics.

| Function | Address | Purpose |
|----------|---------|---------|
| `ProcessMonsterModeAnimation` | `6fafda60` | Handles monster mode transitions and animation state machine |
| `ProcessMonsterAnimationFrame` | `6fafbef0` | Per-frame processing for monster animations |
| `GetMonsterAnimationRate` | `6fadfcd0` | Gets animation speed for monster type |
| `GetMonsterModeGraphicsData` | `6fafbff0` | Gets graphics data for a monster in a specific mode |
| `CheckMonsterSpecialGraphicsMode` | `6fb56070` | Checks for special/unique monster graphics (super uniques, etc.) |
| `CheckMonsterHasSpecialGraphics` | `6fb556c0` | Checks if monster has special graphics override |
| `HandleMonsterModeStateEvent` | `6fafee40` | Handles state events during monster mode transitions |
| `GetMonsterBaseId` | `6fadfcb0` | Gets base monster ID |
| `GetMonsterBaseSound` | `6fadfc90` | Gets base sound for monster |

---

## Stage 8: Overlay System (D2Client.dll)

Overlays are additional visual effects layered on top of units -- auras, enchants, states, weather effects. They follow the same COF/sprite pipeline.

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

Mode transitions trigger COF file changes -- when a unit switches from walking to attacking, a new COF is loaded for the new mode.

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

Missiles have their own animation pipeline that uses the same COF/sprite system.

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

## Stage 11: Palette & Color (D2CMP.dll)

Palette transforms applied to COF layers for color variation (different armor colors, unique monster tints, etc.).

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

| DLL | COF-Related Functions | Coverage |
|-----|----------------------|----------|
| **D2CMP.dll** | 95 functions | Fully named |
| **D2Common.dll** | 38 functions | Fully named |
| **D2Client.dll** | 120+ functions | Mostly named, ~12 still FUN_* |
| **Total** | **253+ functions** | ~95% named |
