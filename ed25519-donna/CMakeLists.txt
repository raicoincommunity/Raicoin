add_library (ed25519
	ed25519-donna-portable.h
	ed25519-hash-custom.h
	ed25519-randombytes-custom.h
	ed25519.h
	ed25519.c)

target_compile_definitions(ed25519 PUBLIC
	-DED25519_CUSTOMHASH
	-DED25519_CUSTOMRNG)

find_package (OpenSSL 1.1 EXACT REQUIRED)
	include_directories(${OPENSSL_INCLUDE_DIR})
	message("OpenSSL include dir: ${OPENSSL_INCLUDE_DIR}")
	message("OpenSSL libraries: ${OPENSSL_LIBRARIES}")
	message("OpenSSL lib: ${OPENSSL_SSL_LIBRARY}")
	message("Crypto lib: ${OPENSSL_CRYPTO_LIBRARY}")

target_link_libraries(ed25519 ${OPENSSL_LIBRARIES} rai_custom)