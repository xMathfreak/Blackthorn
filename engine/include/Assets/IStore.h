#pragma once

#include "Core/Export.h"
#include "Types/Numeric.h"

namespace Blackthorn::Assets {

class BLACKTHORN_API IStore {
public:
	virtual ~IStore() = default;

	virtual void unload(U32 name) = 0;
	virtual void clear() = 0;

	virtual size_t size() const = 0;
	virtual size_t getMemoryUsage() const = 0;
};

} // namespace Blackthorn::Assets