#pragma once

#include <vector>

#include <glad/gl.h>

#include "Core/Export.h"

namespace Blackthorn::Graphics {

/**
 * @brief Specifies the intended update pattern for a VBO's storage.
 *
 * This drives which OpenGL storage strategy is selected at construction:
 *
 * - @c Static: Data is written once and never updated. Maps to
 *   @c glBufferStorage (immutable storage, GL 4.4 / ARB_buffer_storage)
 *   when available, falling back to @c glBufferData with @c GL_STATIC_DRAW.
 *   Calling @c updateData() on a Static VBO is a no-op and logs an error.
 *
 * - @c Streaming: Data is rewritten every frame (e.g. the quad batch
 *   buffer). Maps to persistent coherent mapping
 *   (@c GL_MAP_WRITE_BIT | @c GL_MAP_PERSISTENT_BIT | @c GL_MAP_COHERENT_BIT)
 *   when @c GL_ARB_buffer_storage is available (GL 4.4+). Falls back to
 *   @c glBufferData / @c glBufferSubData with @c GL_DYNAMIC_DRAW on older
 *   drivers. The fallback is fully transparent, callers use the same
 *   @c updateData() / @c streamingPtr() API regardless of which path is active.
 */
enum class BufferType {
	Static,
	Streaming
};

/**
 * @brief RAII wrapper for an OpenGL Vertex Buffer Object (GL_ARRAY_BUFFER)
 *
 * @section storage_modes Storage modes
 * The buffer chooses its OpenGL storage strategy based on @c BufferType and
 * runtime driver capability:
 *
 * | BufferType  | ARB_buffer_storage available? | Strategy                   |
 * |-------------|-------------------------------|----------------------------|
 * | Static      | yes                           | glBufferStorage (immutable)|
 * | Static      | no                            | glBufferData GL_STATIC_DRAW|
 * | Streaming   | yes                           | Persistent coherent map    |
 * | Streaming   | no                            | glBufferSubData fallback   |
 *
 * @section persistent_mapping Persistent mapping
 * When the persistent path is active, @c streamingPtr() returns a pointer
 * directly into GPU-visible memory. The caller writes vertex data there and
 * then calls @c lockRange() before issuing a draw call and @c waitFence()
 * before writing to the same region again. This eliminates the CPU stall
 * that @c glBufferSubData causes when the driver must wait for the GPU to
 * finish consuming the previous upload.
 *
 * When the fallback path is active, @c streamingPtr() returns the CPU
 * staging buffer allocated by the VBO, and @c flushStreaming() uploads it
 * via @c glBufferSubData. @c lockRange() / @c waitFence() become no-ops.
 *
 * @note Requires a valid OpenGL context to be current on the calling thread.
 */
class BLACKTHORN_API VBO {
private:
	/// OpenGL buffer object handle (0 if uninitialized)
	GLuint id = 0;

	/// Size of the buffer in bytes
	size_t bufferSize = 0;

	BufferType bufferType = BufferType::Streaming;

	/// Non null when GL_ARB_buffer_storage is available and BufferType::Streaming.
	/// Points directly into the persistently mapped GPU buffer.
	void* persistentPtr = nullptr;

	/// CPU staging buffer used on the glBufferSubData fallback path.
	/// Empty when persistent mapping is active.
	std::vector<std::byte> stagingBuffer;

	/// Fence inserted after each draw in the persistent path.
	GLsync fence = nullptr;
	static bool bufferStorageSupported();

	void allocatePersistent(size_t sizeInBytes);
	void allocateMutable(const void* data, size_t sizeInBytes, GLenum usage);

public:
	/**
	 * @brief Constructs an empty VBO without creating the OpenGL buffer
	 *
	 * Call create() explicitly or use the constructor taking createNow = true
	 * before uploading data.
	 */
	VBO() = default;

	/**
	 * @brief Equivalent to @c VBO(BufferType::Streaming, createNow).
	 * Prefer the @c BufferType overload for new code.
	 *
	 * @param createNow If true, @c create() is called immediately.
	 */
	explicit VBO(bool createNow);

	/**
	 * @brief Constructs a VBO and optionally creates the OpenGL buffer.
	 * @param createNow If true, create() is called immediately.
	 */
	explicit VBO(BufferType type, bool createNow = false);

	/**
	 * @brief Destroys the VBO and releases the OpenGL buffer.
	 *
	 * Safe to call even if the buffer was never created.
	 */
	~VBO();

	/// Copy construction is disabled (unique ownership)
	VBO(const VBO&) = delete;

	/// Copy assignment is disabled (unique ownership)
	VBO& operator=(const VBO&) = delete;

	/**
	 * @brief Move-constructs a VBO, transferring ownership.
	 * @param other VBO to move from.
	 *
	 * The moved-from object is left in an invalid but destructible state.
	 */
	VBO(VBO&& other) noexcept;

	/**
	 * @brief Move-assigns a VBO, transferring ownership.
	 * @param other VBO to move from.
	 * @return Reference to this object.
	 */
	VBO& operator=(VBO&& other) noexcept;

	/**
	 * @brief Creates the OpenGL buffer object.
	 *
	 * If the buffer already exist, this function has no effect.
	 */
	void create();

	/**
	 * @brief Destroys the OpenGL buffer object.
	 *
	 * After calling this, isValid() will return false.
	 */
	void destroy();

	/**
	 * @brief Binds this VBO to GL_ARRAY_BUFFER.
	 *
	 * @pre The buffer must be valid.
	 */
	void bind() const;

	/**
	 * @brief Unbinds any VBO from GL_ARRAY_BUFFER.
	 */
	static void unbind();

	/**
	 * @brief Allocates and uploads data to the buffer.
	 * @tparam T Element type of the data.
	 * @param data SOurce data to upload.
	 * @param usage OpenGL usage hint (e.g. GL_STATIC_DRAW).
	 *
	 * If the buffer has not yet been created, it will be created automatically.
	 * Any existing buffer storage is replaced.
	 */
	template <typename T>
	void setData(const std::vector<T>& data, GLenum usage = GL_STATIC_DRAW) {
		setData(data.data(), data.size() * sizeof(T), usage);
	}

	/**
	 * @brief Allocates and uploads raw data to the buffer.
	 * @param data Pointer to the source data.
	 * @param sizeInBytes Size of the data in bytes.
	 * @param usage OpenGL usage hint.
	 *
	 * Replaces any existing buffer storage.
	 */
	void setData(const void* data, size_t sizeInBytes, GLenum usage = GL_STATIC_DRAW);

	/**
	 * @brief Updates a sub-range of a @c Streaming VBO.
	 *
	 * On the persistent path this is equivalent to writing through
	 * @c streamingPtr() then calling @c flushStreaming(). On the fallback
	 * path it calls @c glBufferSubData.
	 *
	 * @param data   Source data.
	 * @param size   Size in bytes.
	 * @param offset Byte offset into the buffer.
	 */
	void updateData(const void* data, size_t size, size_t offset = 0);

	/**
	 * @brief Updates a sub-range of the buffer.
	 * @tparam T Element type of the data.
	 * @param data Source data to upload.
	 * @param offset Byte offset into the buffer.
	 *
	 * @warning The update must not exceed the current size of the buffer.
	 * If it does the operation is aborted.
	 */
	template <typename T>
	void updateData(const std::vector<T>& data, size_t offset = 0) {
		updateData(data.data(), data.size(), offset);
	}

	/**
	 * @brief Returns a pointer into which vertex data may be written directly.
	 *
	 * - Persistent path: points directly into GPU-coherent memory.
	 *   Writes are immediately visible to the GPU after the next draw call.
	 * - Fallback path: points into the CPU staging buffer.
	 *   Call @c flushStreaming() to upload the written range to the GPU.
	 *
	 * @return Non-null pointer after a successful @c setData() call, nullptr otherwise.
	 */
	void* streamingPtr() noexcept;

	/**
	 * @brief Uploads a range from the CPU staging buffer to the GPU.
	 *
	 * No-op on the persistent path (data is already visible to the GPU).
	 * On the fallback path this calls @c glBufferSubData for the written range.
	 *
	 * Typically called once per @c Renderer::flush() after all vertices
	 * for the current batch have been written into @c streamingPtr().
	 *
	 * @param offset Byte offset of the first modified byte.
	 * @param size   Number of bytes to upload.
	 */
	void flushStreaming(size_t offset, size_t size);

	/**
	 * @brief Inserts a fence sync after a draw call.
	 *
	 * Signals the CPU when the GPU has finished consuming the range most
	 * recently submitted. Call this immediately after @c glDrawElements /
	 * @c glDrawArrays for the batch that used this buffer.
	 *
	 * No-op on the fallback path.
	 */
	void lockRange();

	/**
	 * @brief Blocks until the GPU signals the most recent fence.
	 *
	 * Call before writing into @c streamingPtr() for the next batch to
	 * ensure the GPU is no longer reading from the region about to be
	 * overwritten.
	 *
	 * No-op on the fallback path.
	 */
	void waitFence();

	/**
	 * @brief Returns the OpenGL buffer handle.
	 */
	[[nodiscard]]
	GLuint getID() const { return id; }

	/**
	 * @brief Returns the size of the buffer in bytes.
	 */
	[[nodiscard]]
	size_t getSize() const { return bufferSize; }

	/**
	 * @brief Checks whether the buffer has been created.
	 * @return True if the OpenGL buffer exists.
	 */
	[[nodiscard]]
	bool isValid() const { return id != 0; }

	/**
	 * @brief Returns true if the persistent coherent mapping path is active.
	 *
	 * False means @c glBufferSubData fallback is in use.
	 */
	[[nodiscard]]
	bool isPersistentMapped() const noexcept { return persistentPtr != nullptr; }

	[[nodiscard]]
	BufferType getBufferType() const noexcept { return bufferType; }
};

} // namespace Blackthorn::Graphics