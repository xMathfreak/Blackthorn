#pragma once

#include <memory>
#include <span>
#include <vector>

#include <glm/glm.hpp>
#include <SDL3/SDL.h>

#include "Core/Export.h"
#include "Core/Types/Numeric.h"
#include "Graphics/EBO.h"
#include "Graphics/Shader.h"
#include "Graphics/VAO.h"
#include "Graphics/VBO.h"
#include "Graphics/Texture.h"
#include "Math/Color.h"
#include "Particles/Particle.h"
#include "Particles/ParticleConfig.h"

namespace Blackthorn::Particles {

/**
 * @brief Turns particle data into GPU instanced draw calls.
 *
 * Intentionally decoupled from particle simulation
 * and @c Graphics::Renderer, handling only texture based instanced rendering and
 * bounds culling so particles from multiple effects can be batched together
 * when grouped by texture.
 *
 * @note Requires a valid OpenGL context to be current on the calling thread.
 */
class BLACKTHORN_API ParticleRenderer {
private:
	/// Per-vertex data for the shared unit quad.
	struct Vertex {
		glm::vec2 position;
		glm::vec2 uv;
	};

	/// Per-instance data uploaded to instanceVbo, one entry per particle.
	struct Instance {
		glm::vec2 position;
		Math::Color color;
		float size;
		float rotation;

		/// Normalized (0..1) UV sub-rect: xy = origin, zw = size. Default
		/// {0,0,1,1} samples the full texture. Computed in draw() from each
		/// Particle::sourceRect (pixel space) divided by the bound texture's
		/// dimensions.
		glm::vec4 uvRect;
	};

public:
	/**
	 * @brief Constructs the renderer and allocates GPU resources.
	 * @param cfg cfg.maxInstances is the maximum particles drawable in a
	 * single draw() call. Sizes the instance buffer once, up front; draw()
	 * calls with more particles than this are truncated (with a warning).
	 */
	explicit ParticleRenderer(const ParticleConfig& cfg = ParticleConfig{});

	/**
	 * @brief Draws camera-facing textured particle quads, culling by the
	 * supplied world-space bounds and converting each pixel space sourceRect
	 * to normalized UVs using the bound texture's dimensions.
	 *
	 * @param particles Particles to draw, typically one emitter's live
	 * range (or several same-texture emitters' ranges batched by the
	 * caller).
	 * @param texture Texture bound for every instance in this call.
	 * Also supplies the dimensions used to normalize each particle's
	 * pixel-space sourceRect into a UV sub-rect.
	 * @param bounds World-space AABB the particles are contained within,
	 * used for the cull test above.
	 * @param viewBounds Current camera view bounds, e.g. from
	 * Graphics::Renderer::getViewBounds().
	 */
	void draw(
		std::span<const Particle> particles,
		const Graphics::Texture& texture,
		const SDL_FRect& bounds,
		const SDL_FRect& viewBounds
	);

	/**
	 * @brief Releases the shared particle shader.
	 */
	static void cleanupShader();

private:
	Graphics::VAO vao;
	Graphics::VBO quadVbo;
	Graphics::EBO quadEbo;
	Graphics::VBO instanceVbo;

	/// Instance buffer capacity, in particles. Set at construction.
	U32 maxInstances;

	/// CPU-side scratch buffer instance data to avoid per-frame allocation.
	std::vector<Instance> instanceScratch;

	static std::shared_ptr<Graphics::Shader> shader;

	static void initShader();

	void initBuffers();

	/**
	 * @brief Axis-aligned rectangle overlap test used for the per-emitter
	 * cull check in draw().
	 */
	[[nodiscard]] static bool intersects(const SDL_FRect& a, const SDL_FRect& b);
};

} // namespace Blackthorn::Particles
