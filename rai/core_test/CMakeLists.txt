add_executable (core_test
	blake2.cpp
	blocks.cpp
	parameters.cpp
	secure.cpp
	ed25519.cpp
	lmdb.cpp
	numbers.cpp
	test_util.cpp
	invite.cpp
	common_util.cpp
	cryptopp.cpp
	extensions.cpp
)

#target_compile_definitions(core_test
#		PRIVATE
#			-DRAIBLOCKS_VERSION_MAJOR=${CPACK_PACKAGE_VERSION_MAJOR}
#			-DRAIBLOCKS_VERSION_MINOR=${CPACK_PACKAGE_VERSION_MINOR})
target_link_libraries (core_test gtest_main gtest secure ed25519 blake2 lmdb cryptopp ${Boost_LIBRARIES})