#pragma once

namespace Blackthorn::Editor {

template <typename T>
struct ComponentInspectorTraits {

};

template <typename T>
struct ComponentInspector {
	static constexpr const char* name() { return "Component"; }

	static bool draw(T& component);
};

} // namespace Blackthorn::Editor