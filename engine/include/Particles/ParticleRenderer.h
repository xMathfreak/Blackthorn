#pragma once

#include <memory>
#include <span>
#include <vector>
#include <unordered_map>

#include <glm/glm.hpp>
#include <SDL3/SDL.h>

#include "Core/Export.h"
#include "Graphics/BlendMode.h"
#include "Types/Numeric.h"
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

	struct Batch {
		std::vector<Instance> instances;
	};

	/// A contiguous, same-state span of instances kept in submission order.
	struct Run {
		std::vector<Instance> instances;
		const Graphics::Texture* texture = nullptr;
		Graphics::BlendMode blend = Graphics::BlendMode::Alpha;
	};

public:
	/**
	 * @brief Constructs the renderer and allocates GPU resources.
	 *
	 * @param cfg cfg.maxInstances is the maximum particles drawable in a
	 * single draw() call. Sizes the instance buffer once, up front; draw()
	 * calls with more particles than this are truncated (with a warning).
	 */
	explicit ParticleRenderer(const ParticleConfig& cfg = ParticleConfig{});

	/** @brief Begins a new frame of particle submissions. */
	void startBatch();

	/**
	 * @brief Queues particles for drawing, preserving submission order.
	 *
	 * @param particles Particles to draw. Copied into the renderer's runs.
	 * @param texture Texture sampled by these particles. Must outlive the flush.
	 * @param blend Blend mode for these particles. Must not be BlendMode::Unset.
	 * @param bounds World-space bounds of the emitter, used for culling.
	 * @param viewBounds Visible world-space bounds, used for culling.
	 */
	void submit(
		std::span<const Particle> particles,
		const Graphics::Texture& texture,
		Graphics::BlendMode blend,
		const SDL_FRect& bounds,
		const SDL_FRect& viewBounds
	);

	/** @brief Issues all queued draws and clears the queue. */
	void endBatch();

	/**
	 * @brief Releases the shared particle shader.
	 */
	static void cleanupShader();

private:
	Graphics::VAO vao;
	Graphics::VBO quadVbo;
	Graphics::EBO quadEbo;
	Graphics::VBO instanceVbo;

	/// Maximum queued particles before an automatic flush.
	U32 maxQueuedInstances;

	/// Particles queued since the last flush.
	U32 queuedInstances = 0;

	/// Instance buffer capacity, in particles. Set at construction.
	size_t instanceBufferCapacity = 0;

	/// Ordered runs. Only the first @c runCount entries are live this frame.
	/// Entries past runCount keep their vector capacity for reuse.
	std::vector<Run> runs;
	size_t runCount = 0;

	/** @brief Appends one instance, opening a new run if texture or blend changed. */
	void pushInstance(const Instance& inst, const Graphics::Texture& tex, Graphics::BlendMode blend);

	/** @brief Draws every live run in order, then resets the queue */
	void flush();

	/** @brief Draws one run's instances, chunked to the instance buffer capacity. */
	void drawInstances(std::span<const Instance> instances);

	/** @brief Sets GL blend state for a mode. Opaque mode disables blending */
	static void applyBlend(Graphics::BlendMode mode);

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
