#include "Inspector/InspectorRegistry.h"

namespace Blackthorn::Editor::Inspector {

InspectorRegistry& InspectorRegistry::instance() {
	static InspectorRegistry reg;
	return reg;
}

} // namespace Blackthorn::Editor::Inspector