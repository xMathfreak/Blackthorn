#include "Graphics/Renderer.h"

#include <glm/gtc/type_ptr.hpp>

namespace Blackthorn::Graphics {

inline constexpr glm::vec4 Renderer::toGLMColor(const SDL_FColor& color) {
	return glm::vec4(color.r, color.g, color.b, color.a);
}

inline constexpr glm::vec2 Renderer::toGLMVec2(float x, float y) {
	return glm::vec2(x, y);
}

Renderer::Renderer()
	: projectionMatrix(1.0f)
	, viewMatrix(1.0f)
{
	quadBuffer = std::make_unique<Vertex2D[]>(MAX_VERTICES);

	initQuadBuffers();
	initShader();
	initWhiteTexture();

	globalUBO = std::make_unique<UBO<GlobalData>>();
	globalUBO->bind(0);

	GLuint blockIndex = glGetUniformBlockIndex(shader->id(), "GlobalData");
	if (blockIndex != GL_INVALID_INDEX)
		glUniformBlockBinding(shader->id(), blockIndex, 0);

	textureSlots.fill(nullptr);
	textureSlots[0] = whiteTexture.get();

	#ifdef BLACKTHORN_DEBUG
		SDL_Log("Renderer initialized (Max Quads: %u, Max Textures: %u)", MAX_QUADS, MAX_TEXTURE_SLOTS);
	#endif
}

Renderer::~Renderer() {

}

void Renderer::initQuadBuffers() {
	QuadVAO = std::make_unique<VAO>(true);
	QuadVBO = std::make_unique<VBO>(true);
	QuadEBO = std::make_unique<EBO>(true);

	QuadVAO->bind();
	QuadVBO->bind();

	glBufferData(GL_ARRAY_BUFFER, MAX_VERTICES * sizeof(Vertex2D), nullptr, GL_DYNAMIC_DRAW);

    QuadVAO->enableAttrib(0, 3, GL_FLOAT, sizeof(Vertex2D), offsetof(Vertex2D, position));
    QuadVAO->enableAttrib(1, 4, GL_FLOAT, sizeof(Vertex2D), offsetof(Vertex2D, color));
    QuadVAO->enableAttrib(2, 2, GL_FLOAT, sizeof(Vertex2D), offsetof(Vertex2D, texCoords));
    QuadVAO->enableAttrib(3, 1, GL_FLOAT, sizeof(Vertex2D), offsetof(Vertex2D, texIndex));

	std::vector<GLuint> indices;
	indices.reserve(MAX_INDICES);

	Uint32 offset = 0;
	for (Uint32 i = 0; i < MAX_INDICES; i += 6) {
		indices.push_back(offset + 0);
		indices.push_back(offset + 1);
		indices.push_back(offset + 2);

		indices.push_back(offset + 2);
		indices.push_back(offset + 3);
		indices.push_back(offset + 0);

		offset += 4;
	}

	QuadEBO->setData(indices);
	VAO::unbind();

	#ifdef BLACKTHORN_DEBUG
		SDL_Log("Renderer Quad buffers initialized");
	#endif
}

void Renderer::initShader() {
	shader = std::make_unique<Shader>("assets/shaders/default.vert", "assets/shaders/default.frag");
	shader->bind();

	for (Uint32 i = 0; i < MAX_TEXTURE_SLOTS; ++i) {
		shader->setInt("u_Textures[" + std::to_string(i) + "]", i);
	}

	#ifdef BLACKTHORN_DEBUG
		SDL_Log("Renderer Shader initialized");
	#endif
}

void Renderer::initWhiteTexture() {
	whiteTexture = std::make_unique<Texture>(Texture::createDefault());
}

void Renderer::startBatch() {
	quadBufferPtr = quadBuffer.get();
	quadIndexCount = 0;
	textureSlotIndex = 1;

	for (Uint32 i = 1; i < MAX_TEXTURE_SLOTS; ++i)
		textureSlots[i] = nullptr;
}

void Renderer::nextBatch() {
	flush();
	startBatch();
}

void Renderer::flush() {
	if (quadIndexCount == 0)
		return;

	Uint32 dataSize = static_cast<Uint32>(
		reinterpret_cast<Uint8*>(quadBufferPtr) - reinterpret_cast<Uint8*>(quadBuffer.get())
	);

	QuadVBO->bind();
	glBufferSubData(GL_ARRAY_BUFFER, 0, dataSize, quadBuffer.get());

	for (Uint32 i = 0; i < textureSlotIndex; ++i) {
		if (textureSlots[i])
			textureSlots[i]->bind(i);
	}

	static GLuint lastShaderID = 0;

	if (shader->id() != lastShaderID) {
		shader->bind();
		lastShaderID = shader->id();
	}

	QuadVAO->bind();

	glDrawElements(GL_TRIANGLES, quadIndexCount, GL_UNSIGNED_INT, nullptr);
}

void Renderer::beginScene() {
	startBatch();
}

void Renderer::endScene() {
	flush();
}

void Renderer::draw(const SDL_FRect& rect, float z, float rotation, const SDL_FColor& color, const Texture* texture, const SDL_FRect* srcRect) {
	if (!isVisible(rect, rotation))
		return;

	if (quadIndexCount >= MAX_INDICES)
		nextBatch();

	float texIndex = 0.0f;

	if (texture) {
		bool found = false;
		for (Uint32 i = 1; i < textureSlotIndex; ++i) {
			if (textureSlots[i] == texture) {
				texIndex = static_cast<float>(i);
				found = true;
				break;
			}
		}

		if (!found) {
			if (textureSlotIndex >= MAX_TEXTURE_SLOTS)
				nextBatch();

			texIndex = static_cast<float>(textureSlotIndex);
			textureSlots[textureSlotIndex] = texture;
			textureSlotIndex++;
		}
	}

	glm::vec4 glmColor = toGLMColor(color);

	glm::vec2 textureCoords[4];
	constexpr glm::vec2 defaultTexCoords[4] = {
		{ 0.0f, 1.0f },
		{ 1.0f, 1.0f },
		{ 1.0f, 0.0f },
		{ 0.0f, 0.0f }
	};

	if (srcRect && texture) {
		float invTexWidth = 1.0f / texture->getWidth();
		float invTexHeight = 1.0f / texture->getHeight();

		float u0 = srcRect->x * invTexWidth;
		float v0 = 1.0f - (srcRect->y * invTexHeight);
		float u1 = (srcRect->x + srcRect->w) * invTexWidth;
		float v1 = 1.0f - ((srcRect->y + srcRect->h) * invTexHeight);

		textureCoords[0] = { u0, v1 };
		textureCoords[1] = { u1, v1 };
		textureCoords[2] = { u1, v0 };
		textureCoords[3] = { u0, v0 };
	} else {
		std::memcpy(textureCoords, defaultTexCoords, sizeof(textureCoords));
	}

	if (rotation != 0.0f) {
		float centerX = rect.x + rect.w * 0.5f;
		float centerY = rect.y + rect.h * 0.5f;

		float cosR = std::cos(rotation);
		float sinR = std::sin(rotation);

		float halfW = rect.w * 0.5f;
		float halfH = rect.h * 0.5f;

		glm::vec2 corners[4] = {
			{ -halfW, -halfH },
			{  halfW, -halfH },
			{  halfW,  halfH },
			{ -halfW,  halfH }
		};

		for (int i = 0; i < 4; ++i) {
			float rotX = corners[i].x * cosR - corners[i].y * sinR;
			float rotY = corners[i].x * sinR + corners[i].y * cosR;

			quadBufferPtr->position = {centerX + rotX, centerY + rotY, z};
			quadBufferPtr->color = glmColor;
			quadBufferPtr->texCoords = textureCoords[i];
			quadBufferPtr->texIndex = texIndex;
			quadBufferPtr++;
		}
	} else {
		// Bottom-left
		quadBufferPtr->position = { rect.x, rect.y, z };
		quadBufferPtr->color = glmColor;
		quadBufferPtr->texCoords = textureCoords[0];
		quadBufferPtr->texIndex = texIndex;
		quadBufferPtr++;

		// Bottom-right
		quadBufferPtr->position = { rect.x + rect.w, rect.y, z };
		quadBufferPtr->color = glmColor;
		quadBufferPtr->texCoords = textureCoords[1];
		quadBufferPtr->texIndex = texIndex;
		quadBufferPtr++;

		// Top-right
		quadBufferPtr->position = { rect.x + rect.w, rect.y + rect.h, z };
		quadBufferPtr->color = glmColor;
		quadBufferPtr->texCoords = textureCoords[2];
		quadBufferPtr->texIndex = texIndex;
		quadBufferPtr++;

		// Top-left
		quadBufferPtr->position = { rect.x, rect.y + rect.h, z };
		quadBufferPtr->color = glmColor;
		quadBufferPtr->texCoords = textureCoords[3];
		quadBufferPtr->texIndex = texIndex;
		quadBufferPtr++;
	}

	quadIndexCount += 6;
}

void Renderer::drawQuad(const SDL_FRect& rect, float rotation, float z, const SDL_FColor& color) {
	draw(rect, z, rotation, color, nullptr, nullptr);
}

void Renderer::drawTexture(const Texture& texture, const SDL_FRect& dest, const SDL_FRect* src, float rotation, float z, const SDL_FColor& tint) {
	draw(dest, z, rotation, tint, &texture, src);
}

void Renderer::setProjection(int width, int height) {
	projectionMatrix = glm::ortho(
		0.0f, static_cast<float>(width),
		0.0f, static_cast<float>(height),
		-1.0f, 1.0f
	);

	viewBounds = { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height) };

	globalUBO->getData().viewProjection = getViewProjectionMatrix();
	globalUBO->uploadField(&GlobalData::viewProjection);
}

void Renderer::setProjection(const glm::mat4& projection) {
	projectionMatrix = projection;
	glm::vec4 topRight = glm::inverse(projection) * glm::vec4(1.0f, 1.0f, 0.0f, 1.0f);
	viewBounds = { 0.0f, 0.0f, topRight.x, topRight.y };

	globalUBO->getData().viewProjection = getViewProjectionMatrix();
	globalUBO->uploadField(&GlobalData::viewProjection);
}

void Renderer::setView(const glm::mat4& view) {
	viewMatrix = view;
	
	globalUBO->getData().viewProjection = getViewProjectionMatrix();
	globalUBO->uploadField(&GlobalData::viewProjection);
}

inline bool Renderer::isVisible(const SDL_FRect& rect, float rotation) const  {
	if (!cullingEnabled)
		return true;

	if (rotation == 0.0f)
		return SDL_HasRectIntersectionFloat(&rect, &viewBounds);

	float cx = rect.x + rect.w * 0.5f;
	float cy = rect.y + rect.h * 0.5f;

	float radius = std::sqrt(rect.w * rect.w + rect.h * rect.h) * 0.5f;

	return (
		cx + radius >= viewBounds.x &&
		cx - radius <= viewBounds.x + viewBounds.w &&
		cy + radius >= viewBounds.y &&
		cy - radius <= viewBounds.y + viewBounds.h
	);
}

} // namespace Blackthorn::Graphics