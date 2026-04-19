#pragma once

#include <cstring>
#include <ostream>
#include <string>

#ifdef _WIN32
	#include <winsock2.h>
	#include <ws2tcpip.h>
#else
	#include <arpa/inet.h>
	#include <netdb.h>
	#include <netinet/in.h>
	#include <sys/socket.h>
#endif

#include <SDL3/SDL.h>

#include "Core/Export.h"
#include "Net/Core/ByteBuffer.h"

namespace Blackthorn::Net::Transport {

struct BLACKTHORN_API Endpoint {
	std::string ip = "<invalid>";
	Uint16 port = 0;
};

/**
 * @brief IP version carried by an `Address`.
 */
enum class IPVersion : Uint8 {
	IPv4 = 4,
	IPv6 = 6,
};

inline std::ostream& operator<<(std::ostream& os, IPVersion v) {
	switch (v) {
		case IPVersion::IPv4:
			return os << "IPv4";
		case IPVersion::IPv6:
			return os << "IPv6";
		default:
			return os << "Unknown";
	}
}

/**
 * @brief Platform-agnostic IP address and port.
 *
 * @details Internally stores a @c sockaddr_storage large enough for both IPv4
 * and IPv6 addresses, eliminating the need for template parameters or unions.
 *
 * @section Construction
 *
 * @code
 * // From numeric strings
 * auto addr4 = Address::fromIPv4("192.168.1.1", 7777);
 * auto addr6 = Address::fromIPv6("::1", 7777);
 *
 * // Resolve hostname (prefers IPv4, falls back to IPv6)
 * auto addr  = Address::fromHostname("example.com", 7777);
 *
 * // Any-address (bind to all interfaces)
 * auto any4  = Address::anyIPv4(7777);
 * auto any6  = Address::anyIPv6(7777);
 * @endcode
 *
 * @section Serialization
 *
 * Serializes as:
 *
 * @code
 * [uint8  version]  (4 or 6)
 * [uint16 port]     (host byte order)
 *
 * if IPv4:
 *   [4 bytes  raw IPv4 address, network byte order]
 *
 * if IPv6:
 *   [16 bytes raw IPv6 address, network byte order]
 * @endcode
 */
class BLACKTHORN_API Address {
public:
	Address() {
		std::memset(&storage, 0, sizeof(storage));
	}

	/**
	 * @brief Creates an IPv4 address from a dotted-decimal string and port.
	 * @return A valid address, or an invalid (default-constructed) address
	 *         if parsing fails.
	 */
	static Address fromIPv4(const std::string& ip, Uint16 port) {
		Address addr;
		auto* sa = reinterpret_cast<sockaddr_in*>(&addr.storage);
		sa->sin_family = AF_INET;
		sa->sin_port = htons(port);

		if (inet_pton(AF_INET, ip.c_str(), &sa->sin_addr) != 1)
			return Address{};

		addr.valid = true;
		return addr;
	}

	/**
	 * @brief Creates an IPv6 address from a colon-hex string and port.
	 */
	static Address fromIPv6(const std::string& ip, Uint16 port) {
		Address addr;
		auto* sa = reinterpret_cast<sockaddr_in6*>(&addr.storage);
		sa->sin6_family = AF_INET6;
		sa->sin6_port = htons(port);

		if (inet_pton(AF_INET6, ip.c_str(), &sa->sin6_addr) != 1)
			return Address{};

		addr.valid = true;
		return addr;
	}

	/**
	 * @brief Resolves a hostname to an address, preferring IPv4.
	 *
	 * Performs a blocking `getaddrinfo` call. Do not call this on the
	 * simulation or I/O thread — resolve addresses at startup or on a
	 * worker thread.
	 *
	 * @param hostname Hostname or numeric IP string.
	 * @param port     Port number.
	 * @return Resolved address, or invalid address on failure.
	 */
	static Address fromHostname(const std::string& hostname, Uint16 port);

	/** @brief Binds to all IPv4 interfaces on `port`. */
	static Address anyIPv4(Uint16 port) {
		Address addr;
		auto* sa = reinterpret_cast<sockaddr_in*>(&addr.storage);
		sa->sin_family = AF_INET;
		sa->sin_port = htons(port);
		sa->sin_addr.s_addr = INADDR_ANY;
		addr.valid = true;
		return addr;
	}

	/** @brief Binds to all IPv6 interfaces on `port` (includes IPv4-mapped). */
	static Address anyIPv6(Uint16 port) {
		Address addr;
		auto* sa = reinterpret_cast<sockaddr_in6*>(&addr.storage);
		sa->sin6_family = AF_INET6;
		sa->sin6_port = htons(port);
		sa->sin6_addr = in6addr_any;
		addr.valid = true;
		return addr;
	}

	/** @brief Returns the IP version, or IPv4 if the address is invalid. */
	IPVersion version() const noexcept {
		return storage.ss_family == AF_INET6 ? IPVersion::IPv6 : IPVersion::IPv4;
	}

	/** @brief Returns the IP as a string. */
	std::string ip() const {
		char buf[INET6_ADDRSTRLEN] = {0};

		if (storage.ss_family == AF_INET) {
			const auto* sa = reinterpret_cast<const sockaddr_in*>(&storage);
			if (inet_ntop(AF_INET, &sa->sin_addr, buf, sizeof(buf)) == nullptr)
				return "<invalid>";
		} else if (storage.ss_family == AF_INET6) {
			const auto* sa = reinterpret_cast<const sockaddr_in6*>(&storage);
			if (inet_ntop(AF_INET6, &sa->sin6_addr, buf, sizeof(buf)) == nullptr)
				return "<invalid>";
		} else {
			return "<invalid>";
		}

		return std::string{buf};
	}

	/** @brief Returns the port in host byte order. */
	Uint16 port() const noexcept {
		if (storage.ss_family == AF_INET6)
			return ntohs(reinterpret_cast<const sockaddr_in6*>(&storage)->sin6_port);

		return ntohs(reinterpret_cast<const sockaddr_in*>(&storage)->sin_port);
	}

	/** @brief Returns the IP and Port. */
	Endpoint getDetails() const {
		return { ip(), port() };
	}

	/** @brief Returns true if the address was successfully constructed. */
	bool isValid() const noexcept { return valid; }

	/**
	 * @brief Returns a human-readable string in the form `"ip:port"`.
	 */
	std::string toString() const;

	/** @brief Raw pointer to the underlying `sockaddr_storage`. */
	const sockaddr* raw() const noexcept {
		return reinterpret_cast<const sockaddr*>(&storage);
	}

	sockaddr* raw() noexcept {
		return reinterpret_cast<sockaddr*>(&storage);
	}

	/** @brief Size of the active sockaddr struct in bytes. */
	socklen_t rawSize() const noexcept {
		return static_cast<socklen_t>(
			storage.ss_family == AF_INET6
				? sizeof(sockaddr_in6)
				: sizeof(sockaddr_in)
		);
	}

	/**
	 * @brief Writes the address into `buf` in the fixed layout described
	 * in the class documentation.
	 */
	void serialize(Core::ByteBuffer& buf) const {
		buf.writeU8(static_cast<Uint8>(version()));
		buf.writeU16(port());

		if (version() == IPVersion::IPv4) {
			const auto* sa = reinterpret_cast<const sockaddr_in*>(&storage);
			const Uint8* bytes = reinterpret_cast<const Uint8*>(&sa->sin_addr);
			buf.writeBytes(bytes, 4);
		} else {
			const auto* sa = reinterpret_cast<const sockaddr_in6*>(&storage);
			const Uint8* bytes = reinterpret_cast<const Uint8*>(&sa->sin6_addr);
			buf.writeBytes(bytes, 16);
		}
	}

	/**
	 * @brief Reads an address from `buf` written by `serialize()`.
	 * @return Deserialized address, or invalid address on parse error.
	 */
	static Address deserialize(Core::ByteBuffer& buf) {
		Address addr;
		Uint8 ver = buf.readU8();
		Uint16 p = buf.readU16();

		if (ver == 4) {
			Uint8 bytes[4];
			buf.readBytes(bytes, 4);

			auto* sa = reinterpret_cast<sockaddr_in*>(&addr.storage);
			sa->sin_family = AF_INET;
			sa->sin_port = htons(p);
			std::memcpy(&sa->sin_addr, bytes, 4);
			addr.valid = true;
		} else if (ver == 6) {
			Uint8 bytes[16];
			buf.readBytes(bytes, 16);

			auto* sa = reinterpret_cast<sockaddr_in6*>(&addr.storage);
			sa->sin6_family = AF_INET6;
			sa->sin6_port = htons(p);
			std::memcpy(&sa->sin6_addr, bytes, 16);
			addr.valid = true;
		}

		return addr;
	}

	bool operator==(const Address& other) const noexcept {
		if (storage.ss_family != other.storage.ss_family)
			return false;

		if (storage.ss_family == AF_INET) {
			const auto* a = reinterpret_cast<const sockaddr_in*>(&storage);
			const auto* b = reinterpret_cast<const sockaddr_in*>(&other.storage);
			return a->sin_port == b->sin_port &&
				std::memcmp(&a->sin_addr, &b->sin_addr, 4) == 0;
		}

		const auto* a = reinterpret_cast<const sockaddr_in6*>(&storage);
		const auto* b = reinterpret_cast<const sockaddr_in6*>(&other.storage);

		return a->sin6_port == b->sin6_port &&
			std::memcmp(&a->sin6_addr, &b->sin6_addr, 16) == 0;
	}

	bool operator!=(const Address& other) const noexcept {
		return !(*this == other);
	}

private:
	sockaddr_storage storage{};
	bool valid = false;
};

} // namespace Blackthorn::Net::Transport

namespace std {

template <>
struct hash<Blackthorn::Net::Transport::Address> {
	size_t operator()(const Blackthorn::Net::Transport::Address& addr) const noexcept {
		size_t h = std::hash<Uint16>{}(addr.port());

		if (addr.version() == Blackthorn::Net::Transport::IPVersion::IPv4) {
			const auto* sa = reinterpret_cast<const sockaddr_in*>(addr.raw());
			h ^= std::hash<Uint32>{}(sa->sin_addr.s_addr) * 2654435761ULL;
		} else {
			const auto* sa = reinterpret_cast<const sockaddr_in6*>(addr.raw());
			const Uint8* b = reinterpret_cast<const Uint8*>(&sa->sin6_addr);
			for (int i = 0; i < 16; ++i)
				h ^= size_t(b[i]) << (i % 8);
		}

		return h;
	}
};

} // namespace std