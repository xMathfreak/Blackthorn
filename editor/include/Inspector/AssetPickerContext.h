#pragma once

namespace Blackthorn::Assets { class AssetManager; }
namespace Blackthorn::Editor::Assets { class AssetDirectoryCache; }

namespace Blackthorn::Editor::Inspector {

/**
 * @brief Ambient context giving ComponentInspector<T> specializations
 * access to the asset system, without widening
 * ComponentInspector<T>::draw()'s fixed (T&) -> bool signature for every
 * specialization that doesn't need it.
 *
 * Set once per frame by Panels::Inspector before invoking any registered
 * draw function.
 */
class AssetPickerContext {
public:
	static void set(Editor::Assets::AssetDirectoryCache* cache, Blackthorn::Assets::AssetManager* manager) {
		activeCache = cache;
		activeManager = manager;
	}

	static Editor::Assets::AssetDirectoryCache* cache() { return activeCache; }
	static Blackthorn::Assets::AssetManager* manager() { return activeManager; }

private:
	static inline Editor::Assets::AssetDirectoryCache* activeCache = nullptr;
	static inline Blackthorn::Assets::AssetManager* activeManager = nullptr;
};

} // namespace Blackthorn::Editor::Inspector