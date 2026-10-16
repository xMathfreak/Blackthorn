#include "Particles/ParticleRenderer.h"

#include <algorithm>

#include "Debug/Logger.h"

namespace Blackthorn::Particles {

std::shared_ptr<Graphics::Shader> ParticleRenderer::shader = nullptr;

ParticleRenderer::ParticleRenderer(const ParticleConfig& cfg)
	: maxQueuedInstances(cfg.maxInstances)
	, instanceBufferCapacity(cfg.maxInstances)
{
	if (!shader)
		initShader();

	initBuffers();
}

void ParticleRenderer::initBuffers() {
	vao.create();
	quadVbo.create();
	quadEbo.create();
	instanceVbo.create();

	vao.bind();

	const std::vector<Vertex> vertices = {
		{{-0.5f, -0.5f}, {0.0f, 0.0f}},
		{{ 0.5f, -0.5f}, {1.0f, 0.0f}},
		{{ 0.5f,  0.5f}, {1.0f, 1.0f}},
		{{-0.5f,  0.5f}, {0.0f, 1.0f}}
	};

	const std::vector<GLuint> indices = {
		0, 1, 2,
		2, 3, 0
	};

	quadVbo.bind();
	quadVbo.setData(vertices);

	quadEbo.bind();
	quadEbo.setData(indices);

	vao.enableAttrib(0, 2, GL_FLOAT, sizeof(Vertex), offsetof(Vertex, position));
	vao.enableAttrib(1, 2, GL_FLOAT, sizeof(Vertex), offsetof(Vertex, uv));

	instanceVbo.bind();
	instanceVbo.setData(nullptr, static_cast<size_t>(maxQueuedInstances) * sizeof(Instance), GL_DYNAMIC_DRAW);

	vao.enableAttrib(2, 2, GL_FLOAT, sizeof(Instance), offsetof(Instance, position), 1);
	vao.enableAttrib(3, 4, GL_FLOAT, sizeof(Instance), offsetof(Instance, color), 1);
	vao.enableAttrib(4, 1, GL_FLOAT, sizeof(Instance), offsetof(Instance, size), 1);
	vao.enableAttrib(5, 1, GL_FLOAT, sizeof(Instance), offsetof(Instance, rotation), 1);
	vao.enableAttrib(6, 4, GL_FLOAT, sizeof(Instance), offsetof(Instance, uvRect), 1);

	Graphics::VAO::unbind();
	Graphics::VBO::unbind();
}

bool ParticleRenderer::intersects(const SDL_FRect& a, const SDL_FRect& b) {
	return a.x < b.x + b.w
		&& a.x + a.w > b.x
		&& a.y < b.y + b.h
		&& a.y + a.h > b.y;
}

void ParticleRenderer::initShader() {
	if (!shader) {
		shader = std::make_shared<Graphics::Shader>("assets/shaders/particle.vert", "assets/shaders/particle.frag");

		BT_DEBUG("ParticleRenderer: Shader initialized");
	}
}

void ParticleRenderer::cleanupShader() {
	shader.reset();
}

void ParticleRenderer::startBatch() {
	queuedInstances = 0;
	runCount = 0;
}

void ParticleRenderer::pushInstance(const Instance& inst, const Graphics::Texture& texture, Graphics::BlendMode blend) {
	const bool needsNewRun = runCount == 0
		|| runs[runCount - 1].texture != &texture
		|| runs[runCount - 1].blend != blend;

	if (needsNewRun) {
		if (runCount == runs.size())
			runs.emplace_back();

		Run& run = runs[runCount++];
		run.texture = &texture;
		run.blend = blend;
		run.instances.clear();
	}

	runs[runCount - 1].instances.push_back(inst);
}

void ParticleRenderer::submit(
	std::span<const Particle> particles,
	const Graphics::Texture& texture,
	Graphics::BlendMode blend,
	const SDL_FRect& bounds,
	const SDL_FRect& viewBounds
) {
	if (blend == Graphics::BlendMode::Unset)
		return;

	if (particles.empty())
		return;

	if (!intersects(viewBounds, bounds))
		return;

	const auto width = texture.getWidth();
	const auto height = texture.getHeight();

	if (width <= 0 || height <= 0)
		return;

	const float invTexWidth = 1.0f / static_cast<float>(width);
	const float invTexHeight = 1.0f / static_cast<float>(height);

	for (const auto& p : particles) {
		if (queuedInstances >= maxQueuedInstances)
			flush();

		glm::vec4 uvRect{0.0f, 0.0f, 1.0f, 1.0f};
		if (p.sourceRect.w > 0.0f && p.sourceRect.h > 0.0f) {
			uvRect = {
				p.sourceRect.x * invTexWidth,
				p.sourceRect.y * invTexHeight,
				p.sourceRect.w * invTexWidth,
				p.sourceRect.h * invTexHeight
			};
		}

		pushInstance(
			Instance{ p.position, p.color, p.size, p.rotation, uvRect },
			texture,
			blend
		);

		++queuedInstances;
	}
}

void ParticleRenderer::endBatch() {
	flush();
}

void ParticleRenderer::drawInstances(std::span<const Instance> instances) {
	// Guards against an infinite loop when the buffer has no capacity.
	if (instanceBufferCapacity == 0)
		return;

	size_t offset = 0;

	while (offset < instances.size()) {
		const size_t count = std::min(
			instanceBufferCapacity,
			instances.size() - offset
		);

		instanceVbo.waitFence();
		instanceVbo.updateData(instances.subspan(offset, count));

		glDrawElementsInstanced(
			GL_TRIANGLES,
			6,
			GL_UNSIGNED_INT,
			nullptr,
			static_cast<GLsizei>(count)
		);

		instanceVbo.lockRange();
		offset += count;
	}
}

void ParticleRenderer::flush() {
	if (queuedInstances == 0)
		return;

	shader->bind();
	vao.bind();
	instanceVbo.bind();

	Graphics::BlendMode currentBlend = Graphics::BlendMode::Unset;
	const Graphics::Texture* currentTexture = nullptr;

	for (size_t i = 0; i < runCount; ++i) {
		const Run& run = runs[i];

		if (run.instances.empty())
			continue;

		if (run.blend != currentBlend) {
			Graphics::setBlendMode(run.blend);
			currentBlend = run.blend;
		}

		if (run.texture != currentTexture) {
			run.texture->bind(0);
			currentTexture = run.texture;
		}

		drawInstances(run.instances);
	}

	// Restore the engine default so code outside the particle system
	// sees the same blend state it did before.
	Graphics::setBlendMode(Graphics::BlendMode::Alpha);

	Graphics::VAO::unbind();
	Graphics::Shader::unbind();

	runCount = 0;
	queuedInstances = 0;
}

} // namespace Blackthorn::Particles