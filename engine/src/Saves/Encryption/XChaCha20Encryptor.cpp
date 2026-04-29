#include "Saves/Encryption/XChaCha20Encryptor.h"

#include <sodium.h>

#include "Debug/Logger.h"

namespace Blackthorn::Saves {

static_assert(
	XChaCha20Encryptor::NONCE_BYTES == crypto_aead_xchacha20poly1305_ietf_NPUBBYTES,
	"XChaCha20Encryptor nonce size does not match libsodium constant"
);

static_assert(
	XChaCha20Encryptor::AUTH_TAG_BYTES == crypto_aead_xchacha20poly1305_ietf_ABYTES,
	"XChaCha20Encryptor auth tag size does not match libsodium constant"
);

static_assert(
	XChaCha20Encryptor::KEY_SIZE == crypto_aead_xchacha20poly1305_ietf_KEYBYTES,
	"XChaCha20Encryptor key size does not match libsodium constant"
);

bool XChaCha20Encryptor::encrypt(
	const IO::ByteBuffer& plaintext,
	IO::ByteBuffer& ciphertext,
	const U8 key[32],
	U8* outNonce,
	U8* outAuthTag
) {
	randombytes_buf(outNonce, NONCE_BYTES);

	const size_t plaintextLen = plaintext.size();
	std::vector<U8> buf(plaintextLen);

	unsigned long long tagLen = 0;

	const int rc = crypto_aead_xchacha20poly1305_ietf_encrypt_detached(
		buf.data(),
		outAuthTag,
		&tagLen,
		plaintext.data(),
		static_cast<unsigned long long>(plaintextLen),
		nullptr, 0,
		nullptr,
		outNonce,
		key
	);

	if (rc != 0) {
		BT_ERROR("XChaCha20Encryptor: encryption failed");
		return false;
	}

	ciphertext.clear();
	ciphertext.writeBytes(buf.data(), plaintextLen);
	return true;
}

bool XChaCha20Encryptor::decrypt(
	const IO::ByteBuffer& ciphertext,
	IO::ByteBuffer& plaintext,
	const U8 key[32],
	const U8* nonce,
	const U8* authTag
) {
	const size_t ciphertextLen = ciphertext.size();
	std::vector<U8> buf(ciphertextLen);

	const int rc = crypto_aead_xchacha20poly1305_ietf_decrypt_detached(
		buf.data(),
		nullptr,
		ciphertext.data(),
		static_cast<unsigned long long>(ciphertextLen),
		authTag,
		nullptr, 0,
		nonce,
		key
	);

	if (rc != 0) {
		BT_ERROR("XChaCha20Encryptor: authentication failed: wrong key or corrupted data");
		return false;
	}

	plaintext.clear();
	plaintext.writeBytes(buf.data(), ciphertextLen);
	return true;
}

} // namespace Blackthorn::Saves