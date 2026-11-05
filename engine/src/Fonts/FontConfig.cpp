#include "Blackthorn/Fonts/FontConfig.hpp"

namespace Blackthorn::Fonts {

FontConfig FontConfig::current;

void FontConfig::setCurrent(const FontConfig& cfg) {
	current = cfg;
}

const FontConfig& FontConfig::getCurrent() {
	return current;
}

} // namespace Blackthorn