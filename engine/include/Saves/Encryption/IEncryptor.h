#pragma once

#include "Core/Export.h"
#include "IO/ByteBuffer.h"

namespace Blackthorn::Saves {

/**
 * @brief Interface for the authenticated encryption pass in the save pipeline.
 *
 * Encryption is applied after compression:
 *
 *   plaintext  →  compress()  →  encrypt()  →  disk
 *   disk       →  decrypt()   →  decompress() →  plaintext
 *
 * The interface exposes the nonce and authentication tag as explicit out/in
 * parameters so the caller (SaveDocument) can store them in the file header
 * separately from the encrypted payload — keeping the on-disk format clean
 * and ensuring the auth tag can be verified before any decryption work begins.
 */
class BLACKTHORN_API IEncryptor {
public:
	virtual ~IEncryptor() = default;

	/** @brief Size in bytes of the nonce expected by this implementation. */
	virtual size_t nonceSize() const noexcept = 0;

	/** @brief Size in bytes of the authentication tag produced by this implementation. */
	virtual size_t authTagSize() const noexcept = 0;

	/**
	 * @brief Encrypts @p plaintext into @p ciphertext.
	 *
	 * @param plaintext   Input bytes.
	 * @param ciphertext  Output buffer. Existing contents are cleared.
	 *                    Will be exactly the same length as @p plaintext.
	 * @param key         32-byte encryption key.
	 * @param outNonce    Output buffer. Must be at least @c nonceSize() bytes.
	 *                    A fresh random nonce is written here on each call.
	 * @param outAuthTag  Output buffer. Must be at least @c authTagSize() bytes.
	 * @return true on success.
	 */
	virtual bool encrypt(
		const IO::ByteBuffer& plaintext,
		IO::ByteBuffer& ciphertext,
		const U8 key[32],
		U8* outNonce,
		U8* outAuthTag
	) = 0;

	/**
	 * @brief Decrypts @p ciphertext into @p plaintext.
	 *
	 * @param ciphertext  Input bytes.
	 * @param plaintext   Output buffer. Existing contents are cleared.
	 * @param key         32-byte decryption key. Must match the key used to encrypt.
	 * @param nonce       Nonce that was produced during encryption. Must be
	 *                    exactly @c nonceSize() bytes.
	 * @param authTag     Authentication tag produced during encryption. Must be
	 *                    exactly @c authTagSize() bytes. Decryption fails and
	 *                    returns false if the tag does not match.
	 * @return true on success. Returns false if authentication fails,
	 *         indicating the ciphertext has been tampered with or the wrong
	 *         key was supplied.
	 */
	virtual bool decrypt(
		const IO::ByteBuffer& ciphertext,
		IO::ByteBuffer& plaintext,
		const U8 key[32],
		const U8* nonce,
		const U8* authTag
	) = 0;
};

} // namespace Blackthorn::Saves