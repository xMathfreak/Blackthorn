#include "Graphics/VBO.h"
#include "Debug/Logger.h"

#include <cstring>

namespace Blackthorn::Graphics {

bool VBO::bufferStorageSupported() {
	static const bool supported = GLAD_GL_ARB_buffer_storage;
	return supported;
}

VBO::VBO(BufferType type, bool createNow)
	: bufferType(type)
{
	if (createNow)
		create();
}

VBO::VBO(bool createNow)
	: VBO(BufferType::Streaming, createNow)
{}

VBO::~VBO() {
	destroy();
}

VBO::VBO(VBO&& other) noexcept
	: id(other.id)
	, bufferSize(other.bufferSize)
	, bufferType(other.bufferType)
	, persistentPtr(other.persistentPtr)
	, stagingBuffer(std::move(other.stagingBuffer))
	, fence(other.fence)
{
	other.id = 0;
	other.bufferSize = 0;
	other.persistentPtr = nullptr;
	other.fence = nullptr;
}

VBO& VBO::operator=(VBO&& other) noexcept {
	if (this != &other) {
		destroy();

		id = other.id;
		bufferSize = other.bufferSize;
		bufferType = other.bufferType;
		persistentPtr = other.persistentPtr;
		stagingBuffer = std::move(other.stagingBuffer);
		fence = other.fence;

		other.id = 0;
		other.bufferSize = 0;
		other.persistentPtr = nullptr;
		other.fence = nullptr;
	}
	return *this;
}

void VBO::create() {
	if (id != 0)
		return;

	glGenBuffers(1, &id);
	BT_DEBUG("VBO {}: Created ({})", id,
		bufferType == BufferType::Static ? "Static" : "Streaming");
}

void VBO::destroy() {
	if (id == 0)
		return;

	if (fence) {
		glDeleteSync(fence);
		fence = nullptr;
	}

	persistentPtr = nullptr;
	stagingBuffer.clear();

	glDeleteBuffers(1, &id);
	id = 0;
	bufferSize = 0;
}

void VBO::bind() const {
	glBindBuffer(GL_ARRAY_BUFFER, id);
}

void VBO::unbind() {
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void VBO::allocatePersistent(size_t sizeInBytes) {
	if (persistentPtr) {
		glBindBuffer(GL_ARRAY_BUFFER, id);
		glUnmapBuffer(GL_ARRAY_BUFFER);
		persistentPtr = nullptr;
	}

	const GLbitfield storageFlags =
		GL_MAP_WRITE_BIT |
		GL_MAP_PERSISTENT_BIT |
		GL_MAP_COHERENT_BIT;

	const GLbitfield mapFlags =
		GL_MAP_WRITE_BIT |
		GL_MAP_PERSISTENT_BIT |
		GL_MAP_COHERENT_BIT;

	glBindBuffer(GL_ARRAY_BUFFER, id);
	glBufferStorage(GL_ARRAY_BUFFER,
		static_cast<GLsizeiptr>(sizeInBytes),
		nullptr,
		storageFlags);

	persistentPtr = glMapBufferRange(GL_ARRAY_BUFFER,
		0,
		static_cast<GLsizeiptr>(sizeInBytes),
		mapFlags);

	if (!persistentPtr) {
		BT_ERROR("VBO {}: glMapBufferRange failed — falling back to glBufferSubData", id);
		stagingBuffer.resize(sizeInBytes);
	}

	bufferSize = sizeInBytes;
	BT_DEBUG("VBO {}: Persistent coherent mapping {} ({} bytes)",
		id, persistentPtr ? "active" : "FAILED — using staging", sizeInBytes);
}

void VBO::allocateMutable(const void* data, size_t sizeInBytes, GLenum usage) {
	glBindBuffer(GL_ARRAY_BUFFER, id);
	glBufferData(GL_ARRAY_BUFFER,
		static_cast<GLsizeiptr>(sizeInBytes),
		data,
		usage);

	bufferSize = sizeInBytes;

	if (bufferType == BufferType::Streaming) {
		stagingBuffer.resize(sizeInBytes);
		if (data)
			std::memcpy(stagingBuffer.data(), data, sizeInBytes);
	}

	BT_DEBUG("VBO {}: Allocated {} bytes (mutable, usage=0x{:X})", id, sizeInBytes, usage);
}

void VBO::setData(const void* data, size_t sizeInBytes, GLenum usage) {
	if (id == 0) {
		BT_WARN("VBO::setData: buffer not created — calling create() automatically");
		create();
	}

	if (bufferType == BufferType::Static && bufferSize > 0) {
		BT_ERROR("VBO {}: setData called on a Static VBO that already has storage — ignored", id);
		return;
	}

	if (bufferType == BufferType::Streaming && bufferStorageSupported()) {
		allocatePersistent(sizeInBytes);

		if (persistentPtr && data)
			std::memcpy(persistentPtr, data, sizeInBytes);
	} else if (bufferType == BufferType::Static && bufferStorageSupported()) {
		glBindBuffer(GL_ARRAY_BUFFER, id);
		glBufferStorage(GL_ARRAY_BUFFER,
			static_cast<GLsizeiptr>(sizeInBytes),
			data,
			0);

		bufferSize = sizeInBytes;
		BT_DEBUG("VBO {}: Immutable static storage ({} bytes)", id, sizeInBytes);
	} else {
		allocateMutable(data, sizeInBytes, usage);
	}
}

void VBO::updateData(const void* data, size_t size, size_t offset) {
	if (bufferType == BufferType::Static) {
		BT_ERROR("VBO {}: updateData called on a Static VBO — ignored", id);
		return;
	}

	if (id == 0) {
		BT_ERROR("VBO {}: updateData called on uninitialized VBO", id);
		return;
	}

	if (offset + size > size) {
		BT_ERROR("VBO {}: updateData overflow ({} + {} > {})",
			id, offset, size, size);
		return;
	}

	if (persistentPtr) {
		std::memcpy(static_cast<std::byte*>(persistentPtr) + offset, data, size);
		} else {
		if (!stagingBuffer.empty())
			std::memcpy(stagingBuffer.data() + offset, data, size);

		glBindBuffer(GL_ARRAY_BUFFER, id);
		glBufferSubData(GL_ARRAY_BUFFER,
			static_cast<GLintptr>(offset),
			static_cast<GLsizeiptr>(size),
			data);
	}
}

void* VBO::streamingPtr() noexcept {
	if (persistentPtr)
		return persistentPtr;

	if (!stagingBuffer.empty())
		return stagingBuffer.data();

	return nullptr;
}

void VBO::flushStreaming(size_t offset, size_t size) {
	if (persistentPtr)
		return;

	if (stagingBuffer.empty() || id == 0)
		return;

	if (offset + size > size) {
		BT_ERROR("VBO {}: flushStreaming overflow ({} + {} > {})",
			id, offset, size, size);
		return;
	}

	glBindBuffer(GL_ARRAY_BUFFER, id);
	glBufferSubData(GL_ARRAY_BUFFER,
		static_cast<GLintptr>(offset),
		static_cast<GLsizeiptr>(size),
		stagingBuffer.data() + offset);
}

void VBO::lockRange() {
	if (!persistentPtr)
		return;

	if (fence) {
		glDeleteSync(fence);
		fence = nullptr;
	}

	fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}

void VBO::waitFence() {
	if (!persistentPtr || !fence)
		return;

	constexpr GLuint64 timeoutNs = 10'000'000; // 10 ms

	while (true) {
		const GLenum result = glClientWaitSync(fence,
			GL_SYNC_FLUSH_COMMANDS_BIT, timeoutNs);

		if (result == GL_ALREADY_SIGNALED || result == GL_CONDITION_SATISFIED) {
			glDeleteSync(fence);
			fence = nullptr;
			return;
		}

		if (result == GL_WAIT_FAILED) {
			BT_ERROR("VBO {}: glClientWaitSync returned GL_WAIT_FAILED", id);
			glDeleteSync(fence);
			fence = nullptr;
			return;
		}

		}
}

} // namespace Blackthorn::Graphics