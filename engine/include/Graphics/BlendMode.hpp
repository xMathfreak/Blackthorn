#pragma once

#include <glad/gl.h>

#include "Types/Numeric.h"

namespace Blackthorn::Graphics {

enum class BlendMode : U8 {
	/// No blend state.
	Unset,
	/// Straight alpha: src * a + dst * (1 - a).
	Alpha,
	/// Additive: stc * a + dst.
	Additive,
	/// Premultiplied alpha: src + dst * (1 - a).
	PremultAlpha,
	/// No blending.
	Opaque
};

inline void setBlendMode(BlendMode mode) {
	switch (mode) {
	case Graphics::BlendMode::Alpha:
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		break;

	case Graphics::BlendMode::Additive:
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		break;

	case Graphics::BlendMode::PremultAlpha:
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		break;

	case Graphics::BlendMode::Opaque:
		glDisable(GL_BLEND);
		break;

	case Graphics::BlendMode::Unset:
		break;
	}
}

} // namespace Blackthorn::Graphics