#pragma once

#include "Core/Export.h"
#include "Saves/Encryption/IEncryptor.h"

namespace Blackthorn::Saves {

/**
 * @brief XChaCha20-Poly1305 authenticated encryption via libsodium.
 *
 * Uses @c crypto_aead_xchacha20poly1305_ietf_encrypt / @c _decrypt from
 * libsodium. XChaCha20 is chosen over ChaCha20 because the 192-bit nonce
 * allows safe random nonce generation — the probability of nonce collision
 * across any realistic number of save operations is negligible.
 *
 * @par Key size
 * 256 bits (32 bytes), matching @c crypto_aead_xchacha20poly1305_ietf_KEYBYTES.
 *
 * @par Nonce size
 * 192 bits (24 bytes), matching @c crypto_aead_xchacha20poly1305_ietf_NPUBBYTES.
 * A fresh random nonce is generated via @c randombytes_buf on every encrypt call.
 *
 * @par Authentication tag size
 * 128 bits (16 bytes), matching @c crypto_aead_xchacha20poly1305_ietf_ABYTES.
 * The tag provides authenticated encryption — any tampering with the ciphertext
 * or the associated data causes decryption to fail.
 *
 * @par Associated data
 * No additional associated data (AAD) is used. The file header checksum
 * provides integrity verification of the unencrypted header fields independently.
 */
class BLACKTHORN_API XChaCha20Encryptor final : public IEncryptor {
public:
	static constexpr size_t KEY_SIZE = 32;
	static constexpr size_t NONCE_BYTES = 24;
	static constexpr size_t AUTH_TAG_BYTES = 16;

	XChaCha20Encryptor() = default;

	size_t nonceSize() const noexcept override { return NONCE_BYTES; }
	size_t authTagSize() const noexcept override { return AUTH_TAG_BYTES; }

	bool encrypt(
		const IO::ByteBuffer& plaintext,
		IO::ByteBuffer& ciphertext,
		const U8 key[32],
		U8* outNonce,
		U8* outAuthTag
	) override;

	bool decrypt(
		const IO::ByteBuffer& ciphertext,
		IO::ByteBuffer& plaintext,
		const U8 key[32],
		const U8* nonce,
		const U8* authTag
	) override;
};

} // namespace Blackthorn::Saves