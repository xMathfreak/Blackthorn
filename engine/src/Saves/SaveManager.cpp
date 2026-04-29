#include "Saves/SaveManager.h"

#include <chrono>

#include <sodium.h>

#include "Debug/Logger.h"

namespace Blackthorn::Saves {

void SaveManager::setStorage(std::unique_ptr<ISaveStorage> s) {
	storage = std::move(s);
}

void SaveManager::setCompressor(std::unique_ptr<ICompressor> c) {
	compressor = std::move(c);
}

void SaveManager::setEncryptor(std::unique_ptr<IEncryptor> e) {
	encryptor = std::move(e);
}

void SaveManager::setKeyDeriveFn(SaveKeyDeriveFn fn) {
	keyDeriveFn = std::move(fn);
}

void SaveManager::registerSection(std::unique_ptr<ISaveSection> section) {
	if (!section) {
		BT_WARN("SaveManager::registerSection: null section, ignored");
		return;
	}

	const U64 id = section->getId();

	if (sectionMap.count(id)) {
		BT_WARN(
			"SaveManager: section '{}' (id {:#x}) already registered, ignored",
			section->getName(), id
		);

		return;
	}

	ISaveSection* raw = section.get();
	sectionOrder.push_back(std::move(section));
	sectionMap[id] = raw;

	BT_DEBUG(
		"SaveManager: registered section '{}' v{}",
		raw->getName(), raw->getVersion()
	);
}

ISaveSection* SaveManager::getSection(U64 sectionId) const {
	auto it = sectionMap.find(sectionId);
	return it != sectionMap.end() ? it->second : nullptr;
}

SaveResult SaveManager::save(SaveId& saveId) {
	if (!storage)
		return SaveResult::failure("No storage backend configured");

#ifdef BLACKTHORN_DEBUG
	if (!encryptor)
		BT_WARN("SaveManager: encryption is disabled, save data will not be encrypted");
#endif

	saveId.updatedAt = static_cast<U64>(
		std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()
		).count()
	);

	SaveDocument doc;
	doc.beginWrite(saveId);

	for (const auto& sectionPtr : sectionOrder) {
		ISaveSection* section = sectionPtr.get();

		IO::ByteBuffer sectionBuf;
		SectionWriteContext ctx{ sectionBuf };

		try {
			section->write(ctx);
		} catch (const std::exception& e) {
			return SaveResult::failure(
				std::string("Section '") + std::string(section->getName()) +
				"' threw during write: " + e.what()
			);
		}

		doc.addSection(section->getId(), section->getVersion(), sectionBuf);

		BT_DEBUG("SaveManager: serialized section '{}' ({} bytes)",
			section->getName(), sectionBuf.size());
	}

	U8 key[32]{};
	const bool useEncryption = encryptor != nullptr;

	if (useEncryption) {
		if (!deriveKey(key, saveId))
			return SaveResult::failure("Encryption is enabled but no key derivation function is set");
	}

	IO::ByteBuffer bytes = doc.finalise(
		compressor.get(),
		useEncryption ? encryptor.get() : nullptr,
		useEncryption ? key : nullptr
	);

	sodium_memzero(key, sizeof(key));

	SaveResult result = storage->write(saveId, bytes);

	if (result) {
		BT_LOG("SaveManager: saved '{}' ({} bytes, {} sections)",
			saveId.displayName.empty() ? saveId.id.toString() : saveId.displayName,
			bytes.size(),
			sectionOrder.size()
		);
	}

	return result;
}

SaveResult SaveManager::load(const SaveId& saveId) {
	return loadImpl(saveId, nullptr);
}

SaveResult SaveManager::loadSections(
	const SaveId&                saveId,
	const std::vector<U64>& sectionIds
) {
	return loadImpl(saveId, &sectionIds);
}

SaveResult SaveManager::loadImpl(
	const SaveId&                saveId,
	const std::vector<U64>* filter
) {
	if (!storage)
		return SaveResult::failure("No storage backend configured");

	SaveReadResult readResult = storage->read(saveId);
	if (!readResult)
		return SaveResult::failure("Storage read failed: " + readResult.error);

	SaveDocument doc;
	if (!doc.parse(readResult.data)) {
		return SaveResult::failure(
			"Failed to parse save file, invalid format or corrupted header");
	}

	if (doc.getHeader().formatVersion > SAVE_FORMAT_VERSION) {
		return SaveResult::failure(
			"Save was written by a newer engine format version (" +
			std::to_string(doc.getHeader().formatVersion) +
			"), cannot load with current version (" +
			std::to_string(SAVE_FORMAT_VERSION) + ")"
		);
	}

	U8 key[32]{};
	const bool isEncrypted = hasFlag(
		static_cast<DocumentFlags>(doc.getHeader().flags),
		DocumentFlags::Encrypted
	);

	if (isEncrypted) {
		if (!encryptor)
			return SaveResult::failure("Save is encrypted but no encryptor is configured");

		if (!deriveKey(key, saveId))
			return SaveResult::failure("Save is encrypted but no key derivation function is set");
	}

	IO::ByteBuffer payload = doc.decryptAndDecompress(
		isEncrypted ? encryptor.get() : nullptr,
		compressor.get(),
		isEncrypted ? key : nullptr
	);

	sodium_memzero(key, sizeof(key));

	if (payload.size() == 0 && doc.getHeader().plaintextSize > 0)
		return SaveResult::failure("Failed to decrypt/decompress save payload");

	size_t sectionsLoaded = 0;

	for (const auto& tableEntry : doc.getSectionTable()) {
		if (filter) {
			bool wanted = false;
			for (U64 id : *filter) {
				if (id == tableEntry.sectionId) {
					wanted = true;
					break;
				}
			}

			if (!wanted)
				continue;
		}

		ISaveSection* section = getSection(tableEntry.sectionId);

		if (!section) {
			BT_WARN(
				"SaveManager: save contains unregistered section {:#x}, skipping",
				tableEntry.sectionId
			);

			continue;
		}

		U32 savedVersion = 0;
		IO::ByteBuffer sectionData = doc.getSectionData(
			tableEntry.sectionId, payload, savedVersion);

		if (sectionData.size() == 0 && tableEntry.size > 0) {
			BT_WARN(
				"SaveManager: section '{}' has zero bytes in payload, skipping",
				section->getName()
			);

			continue;
		}

		SectionReadContext ctx{ sectionData, savedVersion };

		try {
			section->read(ctx);
		} catch (const std::exception& e) {
			return SaveResult::failure(
				std::string("Section '") + std::string(section->getName()) +
				"' threw during read: " + e.what()
			);
		}

		if (savedVersion != section->getVersion()) {
			BT_WARN(
				"SaveManager: section '{}' version mismatch (saved v{}, current v{})",
				section->getName(), savedVersion, section->getVersion()
			);
		}

		++sectionsLoaded;
		BT_DEBUG("SaveManager: loaded section '{}'", section->getName());
	}

	BT_LOG("SaveManager: loaded '{}' ({} sections)",
		saveId.displayName.empty() ? saveId.id.toString() : saveId.displayName,
		sectionsLoaded
	);

	return SaveResult::success();
}

SaveResult SaveManager::remove(const SaveId& saveId) {
	if (!storage)
		return SaveResult::failure("No storage backend configured");

	return storage->remove(saveId);
}

bool SaveManager::exists(const SaveId& saveId) {
	if (!storage)
		return false;

	return storage->exists(saveId);
}

std::vector<SaveMetadata> SaveManager::list(const SaveFilter& filter) {
	if (!storage)
		return {};

	return storage->list(filter);
}

bool SaveManager::deriveKey(U8 outKey[32], const SaveId& saveId) const {
	if (!keyDeriveFn) {
		BT_ERROR("SaveManager: key derivation function not set");
		return false;
	}

	keyDeriveFn(
		std::span<U8, 32>(outKey, 32),
		saveId,
		SAVE_FORMAT_VERSION
	);

	return true;
}

} // namespace Blackthorn::Saves