#include "Particles/ParticleRenderer.h"

#include "Debug/Logger.h"

namespace Blackthorn::Particles {

std::shared_ptr<Graphics::Shader> ParticleRenderer::shader = nullptr;

ParticleRenderer::ParticleRenderer(const ParticleConfig& cfg)
	: maxInstances(cfg.maxInstances)
{
	if (!shader)
		initShader();

	instanceScratch.reserve(maxInstances);

	initBuffers();
}

void ParticleRenderer::initBuffers() {
	vao.create();
	quadVbo.create();
	quadEbo.create();
	instanceVbo.create();

	vao.bind();
	quadVbo.bind();

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
	instanceVbo.setData(nullptr, static_cast<size_t>(maxInstances) * sizeof(Instance), GL_DYNAMIC_DRAW);

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

void ParticleRenderer::draw(
	std::span<const Particle> particles,
	const Graphics::Texture& texture,
	const SDL_FRect& bounds,
	const SDL_FRect& viewBounds
) {
	if (particles.empty())
		return;

	if (!intersects(bounds, viewBounds))
		return;

	U32 instanceCount = static_cast<U32>(particles.size());
	if (instanceCount > maxInstances) {
		BT_WARN("ParticleRenderer: particle count ({}) exceeds renderer capacity ({}), truncating", instanceCount, maxInstances);
		instanceCount = maxInstances;
	}

	const float invTexWidth = 1.0f / static_cast<float>(texture.getWidth());
	const float invTexHeight = 1.0f / static_cast<float>(texture.getHeight());

	instanceScratch.clear();
	for (U32 i = 0; i < instanceCount; ++i) {
		const Particle& p = particles[i];

		// UVs normalized here, rather than being stored on Particle
		glm::vec4 uvRect{0.0f, 0.0f, 1.0f, 1.0f};
		if (p.sourceRect.w > 0.0f && p.sourceRect.h > 0.0f) {
			uvRect = {
				p.sourceRect.x * invTexWidth,
				p.sourceRect.y * invTexHeight,
				p.sourceRect.w * invTexWidth,
				p.sourceRect.h * invTexHeight
			};
		}

		instanceScratch.push_back(Instance{ p.position, p.color, p.size, p.rotation, uvRect });
	}

	instanceVbo.bind();

	instanceVbo.waitFence();
	instanceVbo.updateData(instanceScratch);

	shader->bind();
	texture.bind(0);
	shader->setInt("u_Texture", 0);

	vao.bind();

	glDrawElementsInstanced(
		GL_TRIANGLES,
		6,
		GL_UNSIGNED_INT,
		nullptr,
		static_cast<GLsizei>(instanceCount)
	);

	instanceVbo.lockRange();
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

} // namespace Blackthorn::Particles
