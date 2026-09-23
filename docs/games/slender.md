# Slender-style forest game

Status: **idea**. Nothing game-specific has been built yet. The feature list below was checked
against the engine on 2026-09-22.

## The game

The player spawns at night in a dense forest, carrying a flashlight. Eight pages are hidden
around the map, and the goal is to find them all. A tall, faceless figure hunts the player the
whole time: it never walks, it appears somewhere nearby, a little closer each time, and gets more
aggressive with every page collected. Looking at it fills the screen with static, and looking for
too long, or letting it get too close, ends the game.

It is the most basic of the planned games: one level, a first-person walker, no combat and no
animation. That makes it a good first test of how Pine handles a large outdoor scene.

### Core loop

- Walk through the forest with the flashlight and look for pages.
- Pick up a page with a raycast from the camera. The page counter goes up, and so does the
  threat.
- The figure teleports to a spot near the player, preferably out of sight. Whether the player
  can see it is a view-angle test plus a line-of-sight raycast.
- While the figure is in view, static and audio build up. At full static the game is over.
- Collect all eight pages to win. Either way, the game ends on a game-over or win screen with a
  restart option.

## Engine features

`Engine/src/Pine/` paths are shortened to start below it.

### Already in the engine

- [x] Terrain with sculpting, layer painting and heightfield collision (`Assets/Terrain/`,
      [physics.md](../physics.md#terrain-collision)).
- [x] Cutout, two-sided materials for leaves and grass (`MaterialRenderingMode::Discard`,
      `MaterialRenderFace::Both`, see [rendering.md](../rendering.md#materials--transparency)).
- [x] First-person movement: the `CharacterController` component, with
      `data/projects/gm/assets/PlayerController.cs` as a working example and
      `InputManager.SetCursorMode` for mouse lock.
- [x] Flashlight: a spot light with shadows, parented to the camera. Every object has two
      spot-light slots, so the flashlight and one world spot light can reach the same surface.
- [x] Fog and ambient light, set on the `Level` asset.
- [x] Positional 3D audio and music, with `AudioSource` and `AudioListener` both available in C#
      (see [audio.md](../audio.md)).
- [x] Scripting building blocks: `Physics3D.RayCast`, `Blueprint.SpawnEntity` for random page
      placement, `Entity.Destroy`, and `Level.Load` for restarting.
- [x] Standalone runner: GameHost, driven by `data/game/game.json`.

### Needed

- [ ] **In-game UI.** For the page counter, a "press E" prompt, menus and the win/game-over
      screens. `Renderer2D::AddText` and the `Font` asset exist, but `Renderer2D` is only driven
      by `Pipeline2D`, so nothing draws 2D on top of a 3D view, and C# has no UI API.
- [ ] **Foliage/scatter system.** `EngineConfiguration::m_MaxObjectCount` (default 4096) limits
      both the entity count and each component type's count, and every tree is an entity with a
      `ModelRenderer` and a `Collider`. A few thousand trees fit, but grass and undergrowth need
      instanced placement on the terrain that doesn't use entities, painted in the editor. The
      terrain brush (`Editor/src/Other/TerrainSculpting/`) is a natural base for the painting tool.
- [ ] **Post-processing that scripts can control.** For the static effect: grain and vignette
      strengths are hardcoded locals in `Rendering/Features/PostProcessing/PostProcessing.cpp`
      (0.08 and 0.5), and C# cannot change them.
- [ ] **Level fog from C#.** For fog that thickens as the game goes on. `Level`'s fog fields
      are editor-only today.
- [ ] **Quitting the game from C#.** There is no binding for closing the application.

### Could come later

- [ ] **Model LOD or impostors** for distant trees. Only terrain has LOD today. Heavy fog hides
      the distance, so the first version may not need it.
- [ ] **Wind sway** for foliage.
- [ ] **Terrain height from C#.** `TerrainRenderer` is not bound to scripts. Until it is, a
      raycast down onto the terrain collider finds the ground for placing the figure.
- [ ] **Trigger callbacks** (`OnTriggerEnter`). A distance check is fine for eight pages.
- [ ] **Audio voice priority.** The pool has 32 voices, and a sound requested while all of them
      are busy is not heard. Listed in `Audio/TODO.md`.

### Not needed for this game

- **Pathfinding / navmesh.** The figure teleports instead of walking.
- **Skeletal animation.** The figure only stands still.

A version where the figure chases the player would need both.

## Open questions

- **Forest performance is unmeasured.** How many trees, and how much undergrowth, a real GPU
  keeps at frame rate with the flashlight's shadow redrawn every frame. Measure this on real
  hardware (`GET /stats` on the debug server) before deciding how much of the foliage system and
  LOD the first version needs.
