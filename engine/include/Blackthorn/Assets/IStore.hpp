#pragma once

#include "Blackthorn/Core/Export.hpp"
#include "Blackthorn/Types/Numeric.hpp"

namespace Blackthorn::Assets {

class BLACKTHORN_API IStore {
public:
	virtual ~IStore() = default;

	virtual void unload(U32 name) = 0;
	virtual void clear() = 0;

	[[nodiscard]] virtual size_t size() const = 0;
	[[nodiscard]] virtual size_t getMemoryUsage() const = 0;
};

} // namespace Blackthorn::Assets