#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

#include "Assets/AssetStatus.h"
#include "Assets/IAssetLoader.h"
#include "Assets/IStore.h"
#include "Assets/LoadParams.h"

namespace Blackthorn::Assets {

template <typename AssetType>
class Store : public IStore {
public:
	struct Slot {
		std::unique_ptr<AssetType> asset;
		std::unique_ptr<LoadParams> params;

		AssetStatus status = AssetStatus::Missing;
		U64 generation = 0;

		bool async = false; ///< Set by the last load that started this slot. reload() uses it.

		bool isPending(U64 expectedGeneration) const noexcept {
			return status == AssetStatus::Pending
				&& generation == expectedGeneration;
		}
	};

private:
	std::vector<Slot> slots;

	std::unique_ptr<IAssetLoader<AssetType>> syncLoader;
	std::unique_ptr<IAsyncAssetLoader<AssetType>> asyncLoader;

public:
	Store() = default;

	void setSyncLoader(std::unique_ptr<IAssetLoader<AssetType>> loader) {
		syncLoader = std::move(loader);
	}

	void setAsyncLoader(std::unique_ptr<IAsyncAssetLoader<AssetType>> loader) {
		asyncLoader = std::move(loader);
	}

	bool hasLoader() const noexcept {
		return syncLoader != nullptr || asyncLoader != nullptr;
	}

	bool hasSyncLoader() const noexcept {
		return syncLoader != nullptr;
	}

	bool hasAsyncLoader() const noexcept {
		return asyncLoader != nullptr;
	}

	Slot& ensure(U32 name) {
		if (name >= slots.size())
			slots.resize(static_cast<size_t>(name) + 1);

		return slots[name];
	}

	Slot* find(U32 name) noexcept {
		if (name >= slots.size())
			return nullptr;

		return &slots[name];
	}

	const Slot* find(U32 name) const noexcept {
		if (name >= slots.size())
			return nullptr;

		return &slots[name];
	}

	AssetType* get(U32 name) noexcept {
		auto* slot = find(name);
		return slot ? slot->asset.get() : nullptr;
	}

	const AssetType* get(U32 name) const noexcept {
		auto* slot = find(name);
		return slot ? slot->asset.get() : nullptr;
	}

	AssetStatus statusOf(U32 name) const noexcept {
		auto* slot = find(name);
		return slot ? slot->status : AssetStatus::Missing;
	}

	void add(U32 name, std::unique_ptr<AssetType> asset) {
		auto& slot = ensure(name);

		slot.asset = std::move(asset);
		slot.status = slot.asset
			? AssetStatus::Ready
			: AssetStatus::Missing;

		++slot.generation;
	}

	void markPending(U32 name) {
		auto& slot = ensure(name);

		slot.asset.reset();
		slot.status = AssetStatus::Pending;

		++slot.generation;
	}

	bool isPending(U32 name, U64 generation) const noexcept {
		const auto* slot = find(name);
		return slot && slot->isPending(generation);
	}

	U64 beginPending(U32 name) {
		markPending(name);
		return ensure(name).generation;
	}

	void markFailed(U32 name, U64 generation) {
		auto* slot = find(name);

		if (!slot || slot->generation != generation)
			return;

		if (slot->status == AssetStatus::Pending)
			slot->status = AssetStatus::Failed;
	}

	void unload(U32 name) override {
		auto* slot = find(name);

		if (!slot)
			return;

		slot->asset.reset();
		slot->params.reset();
		slot->status = AssetStatus::Missing;

		// Invalidate async operations.
		++slot->generation;
	}

	void clear() override {
		for (auto& slot : slots) {
			slot.asset.reset();
			slot.params.reset();
			slot.status = AssetStatus::Missing;

			// Keep slots and invalidate outstanding operations.
			++slot.generation;
		}
	}

	size_t size() const override {
		size_t count = 0;

		for (const auto& slot : slots) {
			if (slot.asset != nullptr)
				++count;
		}

		return count;
	}

	size_t getMemoryUsage() const override {
		size_t total = 0;

		for (const auto& slot : slots) {
			if (slot.asset) {
				if constexpr (requires { slot.asset->getMemoryUsage(); }) {
					total += slot.asset->getMemoryUsage();
				} else {
					total += sizeof(AssetType);
				}
			}
		}

		return total;
	}

	IAssetLoader<AssetType>* getSyncLoader() noexcept {
		return syncLoader.get();
	}

	IAsyncAssetLoader<AssetType>* getAsyncLoader() noexcept {
		return asyncLoader.get();
	}
};

} // namespace Blackthorn::Assets