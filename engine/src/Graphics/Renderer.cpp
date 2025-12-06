#include "Graphics/Renderer.h"

#include <glm/gtc/type_ptr.hpp>

namespace Blackthorn::Graphics {

glm::vec4 Renderer::toGLMColor(const SDL_FColor& color) {
	return glm::vec4(color.r, color.g, color.b, color.a);
}

glm::vec2 Renderer::toGLMVec2(float x, float y) {
	return glm::vec2(x, y);
}

Renderer::Renderer()
	: viewProjectionMatrix(1.0f)
{
	quadBuffer = new Vertex2D[MAX_VERTICES];

	initQuadBuffers();
	initShader();
	initWhiteTexture();

	globalUBO = std::make_unique<GlobalUBO>();
	globalUBO->bind(0);

	shader->bind();
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
	delete[] quadBuffer;
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
	quadBufferPtr = quadBuffer;
	quadIndexCount = 0;
	textureSlotIndex = 1;
}

void Renderer::nextBatch() {
	flush();
	startBatch();
}

void Renderer::flush() {
	if (quadIndexCount == 0)
		return;

	Uint32 dataSize = static_cast<Uint32>(
		reinterpret_cast<Uint8*>(quadBufferPtr) - reinterpret_cast<Uint8*>(quadBuffer)
	);

	QuadVBO->bind();
	glBufferSubData(GL_ARRAY_BUFFER, 0, dataSize, quadBuffer);

	for (Uint32 i = 0; i < textureSlotIndex; ++i) {
		if (textureSlots[i])
			textureSlots[i]->bind(i);
	}

	shader->bind();
	QuadVAO->bind();

	glDrawElements(GL_TRIANGLES, quadIndexCount, GL_UNSIGNED_INT, nullptr);
}

void Renderer::beginScene(const glm::mat4& projectionMatrix) {
	viewProjectionMatrix = projectionMatrix;
	globalUBO->updateViewProjection(projectionMatrix);

	startBatch();
}

void Renderer::endScene() {
	flush();
}

void Renderer::draw(const SDL_FRect& rect, float z, float rotation, const SDL_FColor& color, const Texture* texture, const SDL_FRect* srcRect) {
	if (!isVisible(rect))
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

	if (srcRect && texture) {
		float texWidth = static_cast<float>(texture->getWidth());
		float texHeight = static_cast<float>(texture->getHeight());

		float u0 = srcRect->x / texWidth;
		float v0 = 1.0f - (srcRect->y / texHeight);
		float u1 = (srcRect->x + srcRect->w) / texWidth;
		float v1 = 1.0f - ((srcRect->y + srcRect->h) / texHeight);

		textureCoords[0] = { u0, v1 };
		textureCoords[1] = { u1, v1 };
		textureCoords[2] = { u1, v0 };
		textureCoords[3] = { u0, v0 };
	} else {
		textureCoords[0] = { 0.0f, 1.0f };
		textureCoords[1] = { 1.0f, 1.0f };
		textureCoords[2] = { 1.0f, 0.0f };
		textureCoords[3] = { 0.0f, 0.0f };
	}

	if (rotation != 0.0f) {
		float centerX = rect.x + rect.w * 0.5f;
		float centerY = rect.y + rect.h * 0.5f;

		float cosR = std::cos(rotation);
		float sinR = std::sin(rotation);

		glm::vec2 corners[4] = {
			{ -rect.w * 0.5f, -rect.h * 0.5f },
			{ rect.w * 0.5f, -rect.h * 0.5f },
			{ rect.w * 0.5f, rect.h * 0.5f },
			{ -rect.w * 0.5f, rect.h * 0.5f }
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

} // namespace Blackthorn::Graphics