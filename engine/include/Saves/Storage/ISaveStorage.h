#pragma once

#include <string>
#include <vector>

#include "Core/Export.h"
#include "IO/ByteBuffer.h"
#include "Saves/SaveFilter.h"
#include "Saves/SaveId.h"

namespace Blackthorn::Saves {

/**
 * @brief Result type for storage operations.
 *
 * Carries a success flag and an optional human-readable error message.
 * Intentionally lightweight, no exception allocation on failure paths.
 */
struct BLACKTHORN_API SaveResult {
	bool ok = false;
	std::string error;

	static SaveResult success() {
		return { true, {} };
	}

	static SaveResult failure(std::string reason) {
		return { false, std::move(reason) };
	}

	explicit operator bool() const noexcept { return ok; }
};

/**
 * @brief Result type for storage read operations.
 */
struct BLACKTHORN_API SaveReadResult {
	bool ok = false;
	std::string error;
	IO::ByteBuffer data;

	static SaveReadResult success(IO::ByteBuffer bytes) {
		SaveReadResult r;
		r.ok = true;
		r.data = std::move(bytes);
		return r;
	}

	static SaveReadResult failure(std::string reason) {
		return { false, std::move(reason), {} };
	}

	explicit operator bool() const noexcept { return ok; }
};

/**
 * @brief Abstract storage backend for save documents.
 *
 * The engine ships one concrete implementation, @c LocalFileSaveStorage.
 * Game code may provide additional implementations (remote server, cloud
 * storage, database) by deriving from this interface and passing the
 * instance to @c SaveManager.
 *
 * @par Responsibilities
 * The storage backend is responsible only for reading and writing opaque
 * byte blobs keyed by @c SaveId. It knows nothing about the document
 * format, encryption, or compression, those are handled by @c SaveDocument
 * before data reaches the backend.
 *
 * @par Listing
 * @c list() returns @c SaveMetadata rather than full documents. Metadata
 * is derived from the unencrypted file header and section table, so the
 * storage layer must be able to parse those without decrypting the payload.
 * @c SaveDocument::parse() handles this.
 *
 * @par Thread safety
 * Implementations are not required to be thread-safe. @c SaveManager
 * serializes all calls to the backend from a single thread.
 */
class BLACKTHORN_API ISaveStorage {
public:
	virtual ~ISaveStorage() = default;

	/**
	 * @brief Writes the serialized document bytes for @p saveId to storage.
	 *
	 * Overwrites any existing save with the same UUID.
	 *
	 * @param saveId  Identity of the save being written.
	 * @param data    Fully serialized document bytes from @c SaveDocument::finalise().
	 * @return @c SaveResult::success() on success, or @c ::failure() with reason.
	 */
	virtual SaveResult write(const SaveId& saveId, const IO::ByteBuffer& data) = 0;

	/**
	 * @brief Reads the raw document bytes for @p saveId from storage.
	 *
	 * @param saveId Identity of the save to read.
	 * @return @c SaveReadResult containing the bytes on success.
	 */
	virtual SaveReadResult read(const SaveId& saveId) = 0;

	/**
	 * @brief Removes the save identified by @p saveId from storage.
	 * No-op if the save does not exist.
	 */
	virtual SaveResult remove(const SaveId& saveId) = 0;

	/**
	 * @brief Returns true if a save with @p saveId exists in storage.
	 */
	virtual bool exists(const SaveId& saveId) = 0;

	/**
	 * @brief Lists saves matching @p filter.
	 *
	 * Reads only the unencrypted header and section table from each
	 * candidate file, does not decrypt payloads.
	 *
	 * @param filter Criteria to match. Pass @c SaveFilter::all() for no filtering.
	 * @return Vector of metadata descriptors, one per matching save.
	 */
	virtual std::vector<SaveMetadata> list(const SaveFilter& filter) = 0;
};

} // namespace Blackthorn::Saves