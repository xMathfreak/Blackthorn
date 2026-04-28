#include "Net/Transport/Address.h"

#include "Debug/Logger.h"

namespace Blackthorn::Net::Transport {

Address Address::fromHostname(const std::string& hostname, U16 port) {
	addrinfo hints{};
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;

	std::string portStr = std::to_string(port);
	addrinfo* results = nullptr;

	if (
		getaddrinfo(hostname.c_str(), portStr.c_str(), &hints, &results) != 0
		|| results == nullptr
	) {
		BT_WARN("Address::fromHostname: failed to resolve '{}'", hostname);
		return {};
	}

	Address ipv4Result;
	Address ipv6Result;

	for (const addrinfo* rp = results; rp != nullptr; rp = rp->ai_next) {
		if (rp->ai_family == AF_INET && !ipv4Result.isValid()) {
			const auto* sa = reinterpret_cast<const sockaddr_in*>(rp->ai_addr);
			char ipStr[INET_ADDRSTRLEN]{};
			inet_ntop(AF_INET, &sa->sin_addr, ipStr, sizeof(ipStr));
			ipv4Result = fromIPv4(ipStr, port);
		} else if (rp->ai_family == AF_INET6 && !ipv6Result.isValid()) {
			const auto* sa = reinterpret_cast<const sockaddr_in6*>(rp->ai_addr);
			char ipStr[INET6_ADDRSTRLEN]{};
			inet_ntop(AF_INET6, &sa->sin6_addr, ipStr, sizeof(ipStr));
			ipv6Result = fromIPv6(ipStr, port);
		}
	}

	freeaddrinfo(results);

	if (ipv4Result.isValid())
		return ipv4Result;

	if (ipv6Result.isValid())
		return ipv6Result;

	BT_WARN("Address::fromHostname: no usable address for '{}'", hostname);
	return {};
}

std::string Address::toString() const {
	if (!valid)
		return "<invalid>";

	char ipStr[INET6_ADDRSTRLEN]{};

	if (storage.ss_family == AF_INET) {
		const auto* sa = reinterpret_cast<const sockaddr_in*>(&storage);
		inet_ntop(AF_INET, &sa->sin_addr, ipStr, sizeof(ipStr));
		return std::string(ipStr) + ":" + std::to_string(ntohs(sa->sin_port));
	}

	const auto* sa = reinterpret_cast<const sockaddr_in6*>(&storage);
	inet_ntop(AF_INET6, &sa->sin6_addr, ipStr, sizeof(ipStr));
	return "[" + std::string(ipStr) + "]:" + std::to_string(ntohs(sa->sin6_port));
}

} // namespace Blackthorn::Net::Transport