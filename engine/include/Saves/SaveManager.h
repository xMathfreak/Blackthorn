#pragma once

#include <functional>
#include <memory>
#include <span>
#include <unordered_map>
#include <vector>

#include "Core/Export.h"
#include "Saves/SaveConfig.h"
#include "Saves/SaveDocument.h"
#include "Saves/SaveHash.h"
#include "Saves/Storage/ISaveStorage.h"
#include "Saves/Sections/ISaveSection.h"
#include "Saves/Compression/ICompressor.h"
#include "Saves/Encryption/IEncryptor.h"

namespace Blackthorn::Saves {

/**
 * @brief Callable the game provides to derive a 32-byte encryption key.
 *
 * Called by @c SaveManager immediately before encrypting or decrypting a
 * save document. The game is responsible for producing a deterministic key
 * from whatever source it chooses — a build-time constant, a BLAKE2b hash of
 * a game-specific secret combined with the save UUID, etc.
 *
 * @code
 * // Example: derive key from a compile-time secret using libsodium BLAKE2b
 * saveManager.setKeyDeriveFn([](std::span<U8, 32> outKey,
 *                               const SaveId& id,
 *                               U16 formatVersion) {
 *     const char* secret = "my-game-build-secret-v1";
 *     crypto_generichash(outKey.data(), 32,
 *         id.id.bytes.data(), 16,
 *         reinterpret_cast<const U8*>(secret), strlen(secret));
 * });
 * @endcode
 *
 * @param outKey        32-byte buffer to fill with the derived key.
 * @param saveId        Identity of the save being processed.
 * @param formatVersion Engine format version from the document header.
 */
using SaveKeyDeriveFn = std::function<void(
	std::span<U8, 32> outKey,
	const SaveId& saveId,
	U16 formatVersion
)>;

/**
 * @brief Central coordinator for the save system.
 *
 * @c SaveManager owns the compression and encryption pipeline, the section
 * registry, and a reference to the active storage backend. Game code
 * interacts only with this class for all save and load operations.
 *
 * @par Typical setup — config-driven
 * @code
 * SaveConfig cfg;
 * cfg.directory        = "saves";
 * cfg.extension        = ".sav";
 * cfg.compressionLevel = 3;
 * cfg.encryptionEnabled = true;
 * cfg.keyDeriveFn = [](std::span<U8,32> key, const void* id, U16 ver) { ... };
 *
 * SaveManager saves(cfg);
 * saves.registerSection(std::make_unique<WorldSaveSection>(pool, registry));
 * saves.registerSection(std::make_unique<ClockSaveSection>(simClock));
 * saves.registerSection(std::make_unique<MetaSaveSection>("MyGame 1.0"));
 * @endcode
 *
 * @par Typical setup — manual
 * @code
 * auto storage  = std::make_unique<LocalFileSaveStorage>("saves", ".sav");
 * auto compress = std::make_unique<ZstdCompressor>(3);
 * auto encrypt  = std::make_unique<XChaCha20Encryptor>();
 *
 * SaveManager saves;
 * saves.setStorage(std::move(storage));
 * saves.setCompressor(std::move(compress));
 * saves.setEncryptor(std::move(encrypt));
 * saves.setKeyDeriveFn(myKeyFn);
 *
 * saves.registerSection(std::make_unique<WorldSaveSection>(pool, registry));
 * saves.registerSection(std::make_unique<ClockSaveSection>(simClock));
 * saves.registerSection(std::make_unique<MetaSaveSection>("MyGame 1.0"));
 * @endcode
 *
 * @par Saving
 * @code
 * SaveId id = SaveId::generate();
 * id.displayName = "Before final boss";
 * id.worldId     = "overworld";
 *
 * auto result = saves.save(id);
 * if (!result) BT_ERROR("Save failed: {}", result.error);
 * @endcode
 *
 * @par Loading
 * @code
 * auto result = saves.load(saveId);
 * if (!result) BT_ERROR("Load failed: {}", result.error);
 * @endcode
 *
 * @par Partial loading
 * Load only specific sections without running all registered sections:
 * @code
 * saves.loadSections(saveId, { "bt.clock"_saveid, "game.inventory"_saveid });
 * @endcode
 *
 * @par Accessing loaded data
 * @c load() and @c loadSections() only return a @c SaveResult indicating
 * success or failure. They do not hand back the deserialized data
 * directly. This is because @c ISaveSection::read() deserializes straight
 * into the same section instance that was registered, so the loaded state
 * is already sitting on that instance once @c load() returns. There are
 * two ways a section exposes that state, depending on how it was built:
 *
 * - @b Reference-backed sections (e.g. @c WorldSaveSection, @c
 *   ClockSaveSection) write directly into the game object passed to their
 *   constructor (an @c EntityPool, a @c SimClock, ...). Nothing further is
 *   needed as that object already reflects the loaded state.
 * - @b Self-contained sections (e.g. @c MetaSaveSection) own their data as
 *   member fields and expose it through getters. Retrieve the section
 *   instance with @c getSection() and downcast it to read the data:
 *
 * @code
 * auto result = saves.load(saveId);
 * if (!result) {
 *     BT_ERROR("Load failed: {}", result.error);
 *     return;
 * }
 *
 * // Reference-backed: `pool` and `simClock` were already updated in place.
 *
 * // Self-contained: fetch the section and downcast to read its getters.
 * if (auto* meta = saves.getSection("bt.meta"_saveid)) {
 *     auto* metaSection = static_cast<Sections::MetaSaveSection*>(meta);
 *     BT_LOG("Loaded save from {} ({} ticks played)",
 *         metaSection->getEngineVersion(), metaSection->getPlaytimeTicks());
 * }
 *
 * // Custom game sections work the same way:
 * if (auto* inv = saves.getSection("game.inventory"_saveid)) {
 *     auto* inventory = static_cast<InventorySection*>(inv);
 *     for (auto& item : inventory->getItems())
 *         BT_LOG("  {}", item.id);
 * }
 * @endcode
 *
 * @note @c getSection() returns the same instance every time. It is not
 * repopulated per call. The downcast is safe as long as the ID used to
 * look it up matches the concrete type registered under that ID.
 *
 * @par Encryption
 * If no @c IEncryptor is set, documents are compressed only. A warning is
 * logged in debug builds. If no @c ICompressor is set either, documents are
 * written as raw plaintext.
 *
 * @par Thread safety
 * Not thread-safe. All calls must be made from the same thread (typically
 * the simulation thread or a dedicated save thread).
 */
class BLACKTHORN_API SaveManager {
public:
	/**
	 * @brief Default constructor. Subsystems must be configured manually via
	 * @c setStorage(), @c setCompressor(), @c setEncryptor(), and
	 * @c setKeyDeriveFn() before performing any save or load operation.
	 */
	SaveManager() = default;

	/**
	 * @brief Constructs and fully configures the save system from @p cfg.
	 *
	 * Creates a @c LocalFileSaveStorage rooted at @c cfg.directory using
	 * @c cfg.extension, sets up the compressor and encryptor according to
	 * the config, and wraps @c cfg.keyDeriveFn into the typed
	 * @c SaveKeyDeriveFn if one is provided.
	 *
	 * @param cfg Engine save configuration. All fields have sensible defaults
	 *            so @c SaveManager(SaveConfig{}) is a valid minimal setup.
	 */
	explicit SaveManager(const SaveConfig& cfg);

	~SaveManager() = default;

	SaveManager(const SaveManager&) = delete;
	SaveManager& operator=(const SaveManager&) = delete;

	SaveManager(SaveManager&&) = default;
	SaveManager& operator=(SaveManager&&) = default;

	/** @brief Sets the storage backend. Must be called before any save/load. */
	void setStorage(std::unique_ptr<ISaveStorage> storage);

	/**
	 * @brief Sets the compressor. Pass null to disable compression.
	 * Compression is applied before encryption.
	 */
	void setCompressor(std::unique_ptr<ICompressor> compressor);

	/**
	 * @brief Sets the encryptor. Pass null to disable encryption.
	 * A warning is logged in debug builds when encryption is disabled.
	 */
	void setEncryptor(std::unique_ptr<IEncryptor> encryptor);

	/**
	 * @brief Sets the key derivation function used to produce encryption keys.
	 * Required when an encryptor is set. Ignored if encryption is disabled.
	 */
	void setKeyDeriveFn(SaveKeyDeriveFn fn);

	/**
	 * @brief Registers a section for participation in save and load operations.
	 *
	 * Sections are invoked in registration order during @c save(). During
	 * @c load(), each section is invoked only if its ID is present in the
	 * save document's section table.
	 *
	 * @param section  Section implementation. Ownership is transferred.
	 */
	void registerSection(std::unique_ptr<ISaveSection> section);

	/**
	 * @brief Returns the registered section with the given ID, or null.
	 *
	 * This is the primary way to pull data back out after a successful
	 * @c load() for sections that own their data (as opposed to
	 * reference-backed sections, which write directly into a game object
	 * you already hold). The returned pointer must be @c static_cast to the
	 * concrete section type before its data can be read. See
	 * "Accessing loaded data" in the class-level docs above for a worked
	 * example.
	 *
	 * @param sectionId FNV-1a hash of the section name, e.g. @c "bt.meta"_saveid.
	 * @return Pointer to the registered @c ISaveSection instance, or @c nullptr
	 *         if no section with that ID has been registered.
	 */
	ISaveSection* getSection(U64 sectionId) const;

	/** @brief Convenience overload accepting the section name string. */
	ISaveSection* getSection(std::string_view name) const {
		return getSection(saveHash(name));
	}

	/**
	 * @brief Writes a complete save document by invoking all registered
	 *        sections.
	 *
	 * @param saveId Identity of the save. @c SaveId::updatedAt is set to the
	 *               current time before writing.
	 * @param makeBackup When true and backups are enabled in the active config,
	 *                   a duplicate is written alongside the primary save using
	 *                   the backup extension and @c SaveFlags::Backup. Pass
	 *                   false to suppress the backup for this call (the engine
	 *                   uses this for the shutdown autosave).
	 *
	 * @return @c SaveResult::success() on success. If the primary save succeeds
	 *         but the backup fails, a warning is logged and success is still
	 *         returned — a failed backup must not prevent the primary save from
	 *         being reported as successful.
	 */
	[[nodiscard]]
	SaveResult save(SaveId& saveId, bool makeBackup = true);

	/**
	 * @brief Reads a save document and dispatches to all registered sections
	 * whose IDs are present in the document.
	 *
	 * Sections not present in the document are silently skipped. Registered
	 * sections that are not present in the document are also silently skipped.
	 *
	 * @param saveId  Identity of the save to load.
	 * @return @c SaveResult::success() on success.
	 */
	[[nodiscard]]
	SaveResult load(const SaveId& saveId);

	/**
	 * @brief Reads a save document but only dispatches to the specified sections.
	 *
	 * Useful for loading a subset of state — e.g. reading only @c bt.clock
	 * to resume the tick counter without reconstructing the full world.
	 *
	 * @param saveId     Identity of the save to load.
	 * @param sectionIds Set of section ID hashes to process. Others are skipped.
	 * @return @c SaveResult::success() on success.
	 */
	[[nodiscard]]
	SaveResult loadSections(
		const SaveId& saveId,
		const std::vector<U64>& sectionIds
	);

	/** @brief Removes a save from storage. */
	SaveResult remove(const SaveId& saveId);

	/** @brief Returns true if a save with the given identity exists in storage. */
	bool exists(const SaveId& saveId);

	/** @brief Lists saves in storage matching the given filter. */
	std::vector<SaveMetadata> list(const SaveFilter& filter = SaveFilter::all());

	void setBackupsEnabled(bool enabled) { backupsEnabled = enabled; }

	bool isBackupsEnabled() const { return backupsEnabled; }

private:
	std::unique_ptr<ISaveStorage> storage;
	std::unique_ptr<ISaveStorage> backupStorage;
	std::unique_ptr<ICompressor> compressor;
	std::unique_ptr<IEncryptor> encryptor;
	SaveKeyDeriveFn keyDeriveFn;
	bool backupsEnabled = false;

	// Ordered for deterministic write order
	std::vector<std::unique_ptr<ISaveSection>> sectionOrder;
	std::unordered_map<U64, ISaveSection*> sectionMap;

	/**
	 * @brief Derives a 32-byte key into @p outKey for the given save.
	 * Returns false if no key derive function is set.
	 */
	bool deriveKey(U8 outKey[32], const SaveId& saveId) const;

	/**
	 * @brief Core implementation shared by @c load() and @c loadSections().
	 * @p filter is null to process all registered sections.
	 */
	SaveResult loadImpl(
		const SaveId& saveId,
		const std::vector<U64>* filter
	);

	/**
	 * @brief Writes a backup copy of @p primaryBytes alongside the primary
	 *        save, using the backup extension and @c SaveFlags::Backup.
	 *
	 *        The backup @c SaveId is derived from @p saveId with @c
	 *        SaveFlags::Backup OR'd in. Its UUID is identical to the primary so
	 *        the storage backend resolves it to a sibling file — same
	 *        directory, different extension.
	 *
	 * @param saveId The primary save's identity.
	 * @param primaryBytes The already-serialized document bytes to duplicate.
	 *                     Re-using the primary bytes avoids a second
	 *                     compress+encrypt cycle.
	 */
	void writeBackup(const SaveId& saveId, const IO::ByteBuffer& primaryBytes);
};

} // namespace Blackthorn::Saves