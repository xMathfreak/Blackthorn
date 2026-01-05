#pragma once

#include "Core/Export.h"

#include <memory>

namespace Blackthorn::Assets {

struct BLACKTHORN_API LoadParams {
	virtual ~LoadParams() = default;
	virtual std::unique_ptr<LoadParams> clone() const = 0;
};

struct BLACKTHORN_API PathLoadParams final : LoadParams {
	std::string path;

	explicit PathLoadParams(std::string p) 
		: path(std::move(p))
	{}

	std::unique_ptr<LoadParams> clone() const override {
		return std::make_unique<PathLoadParams>(*this);
	}
};

} // namespace Blackthorn::Assets