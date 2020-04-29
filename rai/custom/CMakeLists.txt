add_library(rai_custom
	ed25519.cpp)

target_link_libraries(rai_custom
	blake2 rai_common)