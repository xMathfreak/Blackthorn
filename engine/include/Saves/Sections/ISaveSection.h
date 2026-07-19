#pragma once

#include <string_view>

#include "Core/Export.h"
#include "IO/ByteBuffer.h"

namespace Blackthorn::Saves {

/**
 * @brief Context passed to @c ISaveSection::write() and @c ISaveSection::read().
 *
 * Provides access to the raw @c ByteBuffer and the schema version recorded
 * in the section table entry, so sections can implement forward-compatible
 * reading when versions differ.
 */
struct BLACKTHORN_API SectionWriteContext {
	/// Buffer to write section payload bytes into.
	IO::ByteBuffer& buffer;
};

struct BLACKTHORN_API SectionReadContext {
	/// Buffer containing this section's payload bytes, positioned at offset 0.
	IO::ByteBuffer& buffer;

	/// Schema version recorded when this section was written.
	/// May differ from the current section version if loading an older save.
	U32 savedVersion = 0;
};

/**
 * @brief Interface for a named, versioned save file section.
 *
 * Each section serializes one logical slice of game state — ECS entities,
 * the sim clock, player inventory, etc. The engine provides built-in
 * implementations for @c bt.world, @c bt.clock, and @c bt.meta. Game code
 * implements this interface for custom sections.
 *
 * @par Registration
 * Sections are registered with @c SaveManager before performing any save or
 * load operation. Both reading and writing go through the same registered
 * instance, so state captured during @c write() can be consumed in @c read().
 *
 * @par Versioning
 * @c getVersion() returns the current schema version of this section. The
 * version is written into the section table. On load, the saved version is
 * passed to @c read() via @c SectionReadContext::savedVersion so the
 * implementation can handle older formats.
 *
 * @code
 * class InventorySection : public ISaveSection {
 * public:
 *     U64 getId() const override { return "game.inventory"_saveid; }
 *     std::string_view getName() const override { return "game.inventory"; }
 *     U32 getVersion() const override { return 1; }
 *
 *     void write(SectionWriteContext& ctx) override {
 *         ctx.buffer.writeU32(static_cast<U32>(items.size()));
 *         for (auto& item : items)
 *             ctx.buffer.writeString(item.id);
 *     }
 *
 *     void read(SectionReadContext& ctx) override {
 *         U32 count = ctx.buffer.readU32();
 *         items.resize(count);
 *         for (auto& item : items)
 *             item.id = ctx.buffer.readString();
 *     }
 *
 *     // Exposed so callers can retrieve the deserialized state after a
 *     // load (see SaveManager's "Accessing loaded data" docs). Without a
 *     // getter like this, read() still populates `items`, but nothing
 *     // outside the class can reach it.
 *     const std::vector<Item>& getItems() const { return items; }
 *
 * private:
 *     std::vector<Item> items;
 * };
 * @endcode
 *
 * @par Accessing data after load()
 * A section instance is not disposable, SaveManager keeps it alive for its
 * own lifetime and reuses the same instance across every save and load.
 * Once SaveManager::load() returns successfully, whatever read() wrote
 * into this instance's member fields (here, `items`) is retrievable
 * immediately via SaveManager::getSection(), downcast to the concrete
 * type, then read through a getter like getItems() above. Sections built
 * around an external reference (a pool, a clock, ...) skip the getter
 * entirely, since read() writes straight into that referenced object.
 */
class BLACKTHORN_API ISaveSection {
public:
	virtual ~ISaveSection() = default;

	/**
	 * @brief Returns the 64-bit FNV-1a hash of this section's name.
	 * This is the ID written into the section table on disk.
	 */
	virtual U64 getId() const = 0;

	/**
	 * @brief Returns the human-readable name of this section.
	 * Used in log output and debug diagnostics.
	 */
	virtual std::string_view getName() const = 0;

	/**
	 * @brief Returns the current schema version of this section.
	 * Increment this when the section's wire format changes.
	 */
	virtual U32 getVersion() const = 0;

	/**
	 * @brief Serializes this section's state into @p ctx.buffer.
	 * Called by @c SaveManager when writing a save document.
	 */
	virtual void write(SectionWriteContext& ctx) = 0;

	/**
	 * @brief Deserializes this section's state from @p ctx.buffer.
	 * Called by @c SaveManager when reading a save document.
	 * @note @p ctx.savedVersion may differ from @c getVersion() if loading
	 *       a save written by an older build.
	 */
	virtual void read(SectionReadContext& ctx) = 0;
};

} // namespace Blackthorn::Saves