/*
 * Copyright (c) 2016 Marcos Vives Del Sol
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "common.h"

#include <stdio.h>

#include <mbedtls/entropy.h>

int generate_random_salt(uint8_t * salt) {
	int ret;

	mbedtls_entropy_context entropyctx;
	mbedtls_entropy_init(&entropyctx);

	ret = mbedtls_entropy_func(&entropyctx, salt, SALTSIZE);
	if (ret) {
		mbedtls_perror("Entropy gather failed", ret);
		mbedtls_entropy_free(&entropyctx);
		return 1;
	}
	mbedtls_entropy_free(&entropyctx);

	return 0;
}

int generate_random_iv(uint8_t * iv) {
	int ret;
	
	mbedtls_entropy_context entropyctx;
	mbedtls_entropy_init(&entropyctx);
	
	ret = mbedtls_entropy_func(&entropyctx, iv, IVSIZE);
	if (ret) {
		mbedtls_perror("Entropy gather failed", ret);
		mbedtls_entropy_free(&entropyctx);
		return 1;
	}
	mbedtls_entropy_free(&entropyctx);
	
	return 0;
	
}

int main(int argc, char ** argv) {
	int ret;

	uint8_t iv[IVSIZE];
	ret = generate_random_iv(iv);
	if (ret) {
		return ret;
	}
	
	uint8_t salt[SALTSIZE];
	ret = generate_random_salt(salt);
	if (ret) {
		return ret;
	}
	
	char password[MAXPASSLEN];

	ret = initialize(argc, argv, password);
	if (ret) {
		return ret;
	}

	mbedtls_gcm_context aesgcm;
	ret = prepare_aes(password, salt, iv, &aesgcm, MBEDTLS_GCM_ENCRYPT);
	if (ret) {
		return ret;
	}

	if (fwrite(iv, IVSIZE, 1, stdout) != 1) {
		perror("Failed to write iv");
		mbedtls_gcm_free(&aesgcm);
		return 1;
	}
	
	if (fwrite(salt, SALTSIZE, 1, stdout) != 1) {
		perror("Failed to write random seed");
		mbedtls_gcm_free(&aesgcm);
		return 1;
	}

	size_t readbytes;
	do {
		uint8_t plain[CHUNKSIZE];
		uint8_t cipher[CHUNKSIZE];

		readbytes = fread(plain, 1, CHUNKSIZE, stdin);
		if (readbytes < CHUNKSIZE && !feof(stdin)) {
			perror("Failed to read plaintext");
			mbedtls_gcm_free(&aesgcm);
			return 1;
		}

		ret = mbedtls_gcm_update(&aesgcm, readbytes, plain, cipher);
		if (ret) {
			mbedtls_perror("Failed to execute AES encryption", ret);
			mbedtls_gcm_free(&aesgcm);
			return 1;
		}

		if (fwrite(cipher, 1, readbytes, stdout) != readbytes) {
			perror("Failed to write ciphertext");
			mbedtls_gcm_free(&aesgcm);
			return 1;
		}
	} while (readbytes == CHUNKSIZE);

	uint8_t tag[CHUNKSIZE];
	ret = mbedtls_gcm_finish(&aesgcm, tag, sizeof(tag));
	mbedtls_gcm_free(&aesgcm);
	if (ret) {
		mbedtls_perror("Failed to finish GCM", ret);
		return 1;
	}

	if (fwrite(tag, sizeof(tag), 1, stdout) == 0) {
		perror("Failed to write tail chunk");
	}

	return 0;
}
