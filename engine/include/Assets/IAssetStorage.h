#pragma once

#include "Core/Export.h"

#include <string>
#include <vector>

namespace Blackthorn::Assets {

class BLACKTHORN_API IAssetStorage {
public:
	virtual ~IAssetStorage() = default;
	virtual size_t size() const = 0;
	virtual size_t getMemoryUsage() const = 0;
	virtual void clear() = 0;
	virtual bool has(const std::string& id) const = 0;
	virtual void remove(const std::string& id) = 0;
	virtual std::vector<std::string> getAllIDs() const = 0;
};

} // namespace Blackthorn::Assets