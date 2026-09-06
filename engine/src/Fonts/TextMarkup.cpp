#include "Fonts/TextMarkup.h"

#include <algorithm>
#include <stack>

namespace Blackthorn::Fonts {

TextStyle parseTag(
	std::string_view tag,
	const TextStyle& cur
) {
	TextStyle s = cur;
	size_t a = 0;

	while (a < tag.size()) {
		size_t b = tag.find(';', a);

		if (b == std::string_view::npos)
			b = tag.size();

		std::string_view tok = tag.substr(a, b - a);

		while (!tok.empty() && std::isspace(tok.front()))
			tok.remove_prefix(1);

		while (!tok.empty() && std::isspace(tok.back()))
			tok.remove_suffix(1);

		if (tok == "bold")
			s.bold = true;

		if (tok == "italic")
			s.italic = true;

		if (tok.starts_with("color") || tok.starts_with("colour")) {
			size_t pos = tok.find('=');

			if (pos != std::string_view::npos) {
				std::string_view hex = tok.substr(pos + 1);

				if (!hex.empty() && hex.front() == '#')
					hex.remove_prefix(1);

				// Only 4/8 digit hex modifies alpha
				const bool hexHasAlpha = (hex.size() == 4 || hex.size() == 8);
				const float prevAlpha = s.color.a;

				s.color = Math::fromHex(hex);

				if (!hexHasAlpha)
					s.color.a = prevAlpha;
			}
		}

		// opacity takes priority over color's alpha value
		if (tok.starts_with("alpha") || tok.starts_with("opacity")) {
			size_t pos = tok.find('=');

			if (pos != std::string_view::npos) {
				float value = s.color.a;

				try {
					value = std::stof(std::string(tok.substr(pos + 1)));
				} catch (const std::exception&) {
					value = s.color.a;
				}

				s.color = Math::withAlpha(s.color, std::clamp(value, 0.0f, 1.0f));
			}
		}

		a = b + 1;
	}

	return s;
}

MarkupResult parseMarkup(std::string_view text) {
	MarkupResult out;
	std::stack<TextStyle> stk;
	stk.push(TextStyle{});

	for (size_t i = 0; i < text.size(); ++i) {
		if (text[i] == '[') {
			size_t j = text.find(']', i);
			if (j == std::string_view::npos) {
				out.plainText += text[i];
				out.charStyle.push_back(stk.top());
				continue;
			}

			std::string_view inside = text.substr(i + 1, j - i - 1);
			if (inside == "/") {
				if (stk.size() > 1)
					stk.pop();
			} else {
				stk.push(parseTag(inside, stk.top()));
			}

			i = j;
		} else if (text[i] == '\\' && i + i < text.size() && text[i + 1] == '[') {
			out.plainText += '[';
			out.charStyle.push_back(stk.top());
			++i;
		} else {
			out.plainText += text[i];
			out.charStyle.push_back(stk.top());
		}
	}

	return out;
}

} //namespace Blackthorn::Fonts