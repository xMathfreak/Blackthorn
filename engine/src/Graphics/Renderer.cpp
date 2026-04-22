#include "Graphics/Renderer.h"
#include "Graphics/RenderLayers.h"

#include <cstring>

#include <glm/gtc/type_ptr.hpp>

#include "Debug/Logger.h"

namespace Blackthorn::Graphics {

Renderer::Renderer(Uint32 maxQuads)
	: MAX_QUADS(maxQuads)
	, MAX_VERTICES(maxQuads * 4)
	, MAX_INDICES(maxQuads * 6)
	, projectionMatrix(1.0f)
	, viewMatrix(1.0f)
{
	quadBuffer = std::make_unique<Vertex[]>(MAX_VERTICES);

	initQuadBuffers();
	initShader();
	initScreenPass();

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);

	initWhiteTexture();

	globalUBO = std::make_unique<UBO<GlobalData>>();
	globalUBO->bind(0);

	GLuint blockIndex = glGetUniformBlockIndex(shader->id(), "GlobalData");
	if (blockIndex != GL_INVALID_INDEX)
		glUniformBlockBinding(shader->id(), blockIndex, 0);

	textureSlots.fill(nullptr);
	textureSlots[0] = whiteTexture.get();

	BT_DEBUG("Renderer: Initialized (Max Quads: {}, Max Textures: {})", MAX_QUADS, MAX_TEXTURE_SLOTS);
}

Renderer::~Renderer() {}

void Renderer::initQuadBuffers() {
	QuadVAO = std::make_unique<VAO>(true);
	QuadVBO = std::make_unique<VBO>(true);
	QuadEBO = std::make_unique<EBO>(true);

	QuadVAO->bind();
	QuadVBO->bind();

	glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(MAX_VERTICES) * sizeof(Vertex), nullptr, GL_DYNAMIC_DRAW);

	QuadVAO->enableAttrib(0, 3, GL_FLOAT, sizeof(Vertex), offsetof(Vertex, position));
	QuadVAO->enableAttrib(1, 4, GL_FLOAT, sizeof(Vertex), offsetof(Vertex, color));
	QuadVAO->enableAttrib(2, 2, GL_FLOAT, sizeof(Vertex), offsetof(Vertex, texCoords));
	QuadVAO->enableAttrib(3, 1, GL_FLOAT, sizeof(Vertex), offsetof(Vertex, texIndex));

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

	BT_DEBUG("Renderer: Quad buffers initialized");
}

void Renderer::initShader() {
	shader = std::make_unique<Shader>("assets/shaders/default.vert", "assets/shaders/default.frag");
	shader->bind();

	for (Uint32 i = 0; i < MAX_TEXTURE_SLOTS; ++i) {
		shader->setInt("u_Textures[" + std::to_string(i) + "]", i);
	}

	BT_DEBUG("Renderer: Shader initialized");
}

void Renderer::initScreenPass() {
	screenVAO = std::make_unique<VAO>(true);
}

void Renderer::initWhiteTexture() {
	whiteTexture = std::make_unique<Texture>(Texture::createDefault());

	screenShader = std::make_unique<Shader>(
		"assets/shaders/screen.vert",
		"assets/shaders/screen.frag"
	);

	screenShader->bind();
	screenShader->setInt("u_ScreenTexture", 0);

	activeScreenShader = screenShader.get();

	BT_DEBUG("Renderer: Screen pass initialized");
}

void Renderer::startBatch() {
	quadBufferPtr = quadBuffer.get();
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

	const GLsizeiptr dataSize = reinterpret_cast<const Uint8*>(quadBufferPtr) - reinterpret_cast<const Uint8*>(quadBuffer.get());
	QuadVBO->bind();
	glBufferSubData(GL_ARRAY_BUFFER, 0, dataSize, quadBuffer.get());

	for (Uint32 i = 0; i < textureSlotIndex; ++i) {
		if (textureSlots[i])
			textureSlots[i]->bind(i);
	}

	shader->bind();
	QuadVAO->bind();

	glDrawElements(GL_TRIANGLES, quadIndexCount, GL_UNSIGNED_INT, nullptr);
}

void Renderer::beginScene() {
	if (fbo)
		fbo->bind();

	glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	startBatch();
}

void Renderer::endScene() {
	flush();
	presentToScreen();
}

void Renderer::presentToScreen() {
	if (!fbo)
		return;

	FBO::unbind();

	if (!postProcessingEnabled) {
		glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo->getID());
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

		glBlitFramebuffer(
			0, 0, fbo->getWidth(), fbo->getHeight(),
			0, 0, fbo->getWidth(), fbo->getHeight(),
			GL_COLOR_BUFFER_BIT, GL_NEAREST
		);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		return;
	}

	glDisable(GL_DEPTH_TEST);
	activeScreenShader->bind();
	fbo->getTexture().bind(0);
	screenVAO->bind();
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glEnable(GL_DEPTH_TEST);
}

void Renderer::setPostProcessingEnabled(bool enabled) {
	postProcessingEnabled = enabled;
}

void Renderer::setScreenShader(Shader* customShader) {
	if (customShader) {
		customShader->bind();
		customShader->setInt("u_ScreenTexture", 0);
		activeScreenShader = customShader;
	} else {
		activeScreenShader = screenShader.get();
	}
}


void Renderer::draw(const SDL_FRect& rect, float z, float rotation, const Math::Color& color, const Texture* texture, const SDL_FRect* srcRect) {
	if (!isVisible(rect, rotation))
		return;

	if (quadIndexCount >= MAX_INDICES)
		nextBatch();

	float texIndex = 0.0f;

	if (texture)
		texIndex = static_cast<float>(findOrAddTexture(texture));

	glm::vec2 textureCoords[4];
	constexpr glm::vec2 defaultTexCoords[4] = {
		{ 0.0f, 0.0f },
		{ 1.0f, 0.0f },
		{ 1.0f, 1.0f },
		{ 0.0f, 1.0f }
	};

	if (srcRect && texture) {
		float invTexWidth = 1.0f / texture->getWidth();
		float invTexHeight = 1.0f / texture->getHeight();

		float u0 = srcRect->x * invTexWidth;
		float v0 = srcRect->y * invTexHeight;
		float u1 = (srcRect->x + srcRect->w) * invTexWidth;
		float v1 = (srcRect->y + srcRect->h) * invTexHeight;

		textureCoords[0] = { u0, v0 };
		textureCoords[1] = { u1, v0 };
		textureCoords[2] = { u1, v1 };
		textureCoords[3] = { u0, v1 };
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
			{ -halfW,  halfH },
		};

		for (int i = 0; i < 4; ++i) {
			float rotX = corners[i].x * cosR - corners[i].y * sinR;
			float rotY = corners[i].x * sinR + corners[i].y * cosR;

			quadBufferPtr->position = {centerX + rotX, centerY + rotY, z};
			quadBufferPtr->color = color;
			quadBufferPtr->texCoords = textureCoords[i];
			quadBufferPtr->texIndex = texIndex;
			quadBufferPtr++;
		}
	} else {
		// Top-left
		quadBufferPtr->position = { rect.x, rect.y, z };
		quadBufferPtr->color = color;
		quadBufferPtr->texCoords = textureCoords[0];
		quadBufferPtr->texIndex = texIndex;
		quadBufferPtr++;

		// Top-right
		quadBufferPtr->position = { rect.x + rect.w, rect.y, z };
		quadBufferPtr->color = color;
		quadBufferPtr->texCoords = textureCoords[1];
		quadBufferPtr->texIndex = texIndex;
		quadBufferPtr++;

		// Bottom-right
		quadBufferPtr->position = { rect.x + rect.w, rect.y + rect.h, z };
		quadBufferPtr->color = color;
		quadBufferPtr->texCoords = textureCoords[2];
		quadBufferPtr->texIndex = texIndex;
		quadBufferPtr++;

		// Bottom-left
		quadBufferPtr->position = { rect.x, rect.y + rect.h, z };
		quadBufferPtr->color = color;
		quadBufferPtr->texCoords = textureCoords[3];
		quadBufferPtr->texIndex = texIndex;
		quadBufferPtr++;
	}

	quadIndexCount += 6;
}

void Renderer::drawQuad(const SDL_FRect& rect, float rotation, float z, const Math::Color& color) {
	draw(rect, z, rotation, color, nullptr, nullptr);
}

void Renderer::drawTexture(const Texture& texture, const SDL_FRect& dest, const SDL_FRect* src, float rotation, float z, const Math::Color& tint) {
	draw(dest, z, rotation, tint, &texture, src);
}

void Renderer::drawNineSlice(const Texture& texture, const SDL_FRect& dest, const SliceMargins& sliceMargins, float z, const Math::Color& tint) {
	if (!isVisible(dest))
		return;

	if (quadIndexCount + 54 > MAX_INDICES)
		nextBatch();

	float texIndex = static_cast<float>(findOrAddTexture(&texture));

	float texW = static_cast<float>(texture.getWidth());
	float texH = static_cast<float>(texture.getHeight());

	const float L = sliceMargins.left;
	const float R = sliceMargins.right;
	const float T = sliceMargins.top;
	const float B = sliceMargins.bottom;

	// if (dest.w < L + R || dest.h < T + B)
	// 	return;

	float dx[4] = { dest.x, dest.x + L, dest.x + dest.w - R, dest.x + dest.w };
	float dy[4] = { dest.y, dest.y + T, dest.y + dest.h - B, dest.y + dest.h };

	float ux[4] = { 0.0f, L / texW, (texW - R) / texW, 1.0f };
	float uy[4] = { 0.0f, T / texH, (texH - B) / texH, 1.0f };

	for (int row = 0; row < 3; ++row) {
		for (int col = 0; col < 3; ++col) {
			quadBufferPtr->position  = { dx[col], dy[row], z };
			quadBufferPtr->color = tint;
			quadBufferPtr->texCoords = {ux[col], uy[row] };
			quadBufferPtr->texIndex = texIndex;
			quadBufferPtr++;

			quadBufferPtr->position  = { dx[col + 1], dy[row], z };
			quadBufferPtr->color = tint;
			quadBufferPtr->texCoords = { ux[col + 1], uy[row] };
			quadBufferPtr->texIndex = texIndex;
			quadBufferPtr++;

			quadBufferPtr->position  = { dx[col + 1], dy[row + 1], z };
			quadBufferPtr->color = tint;
			quadBufferPtr->texCoords = { ux[col + 1], uy[row + 1] };
			quadBufferPtr->texIndex = texIndex;
			quadBufferPtr++;

			quadBufferPtr->position  = { dx[col], dy[row + 1], z };
			quadBufferPtr->color = tint;
			quadBufferPtr->texCoords = { ux[col], uy[row + 1] };
			quadBufferPtr->texIndex = texIndex;
			quadBufferPtr++;


			quadIndexCount += 6;
		}
	}
}

void Renderer::setProjection(int width, int height) {
	if (fbo) {
		fbo->resize(width, height);
	} else {
		fbo = std::make_unique<FBO>(width, height);
	}

	projectionMatrix = glm::ortho(
		0.0f, static_cast<float>(width),
		static_cast<float>(height), 0.0f,
		RenderLayers::NearPlane, RenderLayers::FarPlane
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

inline bool Renderer::isVisible(const SDL_FRect& rect, float rotation) const {
	if (!cullingEnabled)
		return true;

	if (rotation == 0.0f)
		return SDL_HasRectIntersectionFloat(&rect, &viewBounds);

	float halfW = rect.w * 0.5f;
	float halfH = rect.h * 0.5f;

	float abscos = std::abs(std::cos(rotation));
	float abssin = std::abs(std::sin(rotation));

	float aabbHalfW = halfW * abscos + halfH * abssin;
	float aabbHalfH = halfW * abssin + halfH * abscos;

	float cx = rect.x + halfW;
	float cy = rect.y + halfH;

	return (
		cx + aabbHalfW >= viewBounds.x &&
		cx - aabbHalfW <= viewBounds.x + viewBounds.w &&
		cy + aabbHalfH >= viewBounds.y &&
		cy - aabbHalfH <= viewBounds.y + viewBounds.h
	);
}

Uint32 Renderer::findOrAddTexture(const Texture* texture){
	for (Uint32 i = 1; i < textureSlotIndex; ++i) {
		if (textureSlots[i] == texture)
			return i;
	}

	if (textureSlotIndex >= MAX_TEXTURE_SLOTS)
		nextBatch();

	const Uint32 slot = textureSlotIndex;
	textureSlots[textureSlotIndex++] = texture;
	return slot;
}

} // namespace Blackthorn::Graphics