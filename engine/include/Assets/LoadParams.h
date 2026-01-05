#pragma once

#include <memory>

namespace Blackthorn::Assets {

struct LoadParams {
	virtual ~LoadParams() = default;
	virtual std::unique_ptr<LoadParams> clone() const = 0;
};

struct PathLoadParams final : LoadParams {
	std::string path;

	explicit PathLoadParams(std::string p) 
		: path(std::move(p))
	{}

	std::unique_ptr<LoadParams> clone() const override {
		return std::make_unique<PathLoadParams>(*this);
	}
};

} // namespace Blackthorn::Assets