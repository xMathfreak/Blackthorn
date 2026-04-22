#pragma once

namespace Blackthorn::Graphics {

/**
 * @brief Named Z-depth anchors for draw call ordering.
 *
 * These are well-known reference points, not enforced categories. You can
 * pass any float z-value to renderer draw calls - these constants exist so
 * common cases have a consistent, readable name instead of magic numbers.
 *
 * Layers are spaced 20 units apart, leaving room for up to 19 distinct
 * sub-layers between any two anchors, e.g.:
 *
 *   RenderLayers::UI         - panel background
 *   RenderLayers::UI + 1.0f  - panel border / decorations
 *   RenderLayers::UI + 2.0f  - text / icons drawn on top of the panel
 *
 * Layer overview:
 *
 *   Background  (-80)  Skies, tilemaps, static backdrops.
 *   World       (-60)  ECS entities, sprites, world-space objects.
 *   Effects     (-40)  Particles, projectile FX, world-space VFX.
 *   UI          ( 20)  Base UI layer - panels, buttons, labels.
 *   UIOverlay   ( 40)  Tooltips, dropdowns, modals, anything above base UI.
 *   Debug       ( 60)  Debug overlays, hitboxes, dev tools. Always on top.
 */
namespace RenderLayers {

	constexpr float NearPlane  = -100.0f;
	constexpr float FarPlane   = 100.0f;

	constexpr float Background = -80.0f;
	constexpr float World      = -60.0f;
	constexpr float Effects    = -40.0f;
	constexpr float UI         = 20.0f;
	constexpr float UIOverlay  = 40.0f;
	constexpr float Debug      = 60.0f;

} // namespace RenderLayers

} // namespace Blackthorn::Graphics