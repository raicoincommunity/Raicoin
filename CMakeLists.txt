cmake_minimum_required (VERSION 3.4)
project (rai)

set (CPACK_PACKAGE_VERSION_MAJOR "1")
set (CPACK_PACKAGE_VERSION_MINOR "5")
set (CPACK_PACKAGE_VERSION_PATCH "0")

set (TAG_VERSION_STRING "V${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}")
add_definitions(-DTAG_VERSION_STRING=${TAG_VERSION_STRING})

set(CMAKE_INSTALL_RPATH "@executable_path/../Frameworks")

option(RAI_ASAN_INT "Enable ASan+UBSan+Integer overflow" OFF)
option(RAI_ASAN "Enable ASan+UBSan" OFF)
option(RAI_TSAN "Enable TSan" OFF)
option(RAICOIN_SIMD_OPTIMIZATIONS "Enable CPU-specific SIMD optimizations (SSE/AVX or NEON, e.g.)" OFF)


set (RAI_TEST ON CACHE BOOL "")
set (RAI_GUI OFF CACHE BOOL "")

set (ACTIVE_NETWORK TEST CACHE STRING "Selects which network parameters are used")
set_property (CACHE ACTIVE_NETWORK PROPERTY STRINGS TEST BETA LIVE)

if (NOT CMAKE_BUILD_TYPE)
	set (CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif (NOT CMAKE_BUILD_TYPE)

set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

set (CMAKE_C_STANDARD 11)
set (CMAKE_C_STANDARD_REQUIRED ON)

set (CMAKE_CXX_STANDARD 14)
set (CMAKE_CXX_STANDARD_REQUIRED ON)


include_directories (${CMAKE_SOURCE_DIR})

if (WIN32)
	add_definitions(-D_WIN32_WINNT=0x0600
					-DWINVER=0x0600
					-DWIN32_LEAN_AND_MEAN)

	if (${RAI_TSAN} OR ${RAI_ASAN} OR ${RAI_ASAN_INT})
		message (WARNING "Cannot use TSAN or ASAN on Windows, sanitizers ignored")
	endif()
else ()
	add_compile_options (-Werror=switch)
	if ((${RAI_TSAN} AND ${RAI_ASAN}) OR
	    (${RAI_TSAN} AND ${RAI_ASAN_INT}))
		message (WARNING "Cannot use TSAN/ASAN together, defaulting to ASAN")
	endif()

	if (${RAI_ASAN} OR ${RAI_ASAN_INT})
		if (${RAI_ASAN_INT})
			add_compile_options(-fsanitize=address,undefined,integer)
		else ()	
			add_compile_options(-fsanitize=address,undefined)
		endif()
		add_definitions(-DED25519_NO_INLINE_ASM)
	elseif (${RAI_TSAN})
		add_compile_options(-fsanitize=thread)
		if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
			add_compile_options("-fsanitize-blacklist=${PROJECT_SOURCE_DIR}/tsan_clang_blacklist")
		endif()
		add_definitions(-DED25519_NO_INLINE_ASM)
	endif()

	if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(i.86|x86(_64)?)$")
		if (ENABLE_AVX2)
			add_compile_options (-mavx2 -mbmi -mbmi2)
		endif ()
	endif ()
endif ()


if (WIN32)
	set (PLATFORM_LINK_FLAGS "") 
	add_definitions(/bigobj)
else ()
	if (APPLE)
		set (PLATFORM_LINK_FLAGS "-framework Foundation")
	else ()
		set (PLATFORM_LINK_FLAGS "-static-libgcc -static-libstdc++")
	endif ()

	if (${RAI_ASAN_INT})
		set (PLATFORM_LINK_FLAGS "${PLATFORM_LINK_FLAGS} -fsanitize=address,undefined,integer")
	elseif (${RAI_ASAN})
		set (PLATFORM_LINK_FLAGS "${PLATFORM_LINK_FLAGS} -fsanitize=address,undefined")
	elseif (${RAI_TSAN})
		set (PLATFORM_LINK_FLAGS "${PLATFORM_LINK_FLAGS} -fsanitize=thread")
		if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
			set (PLATFORM_LINK_FLAGS "${PLATFORM_LINK_FLAGS} -fsanitize-blacklist=${PROJECT_SOURCE_DIR}/tsan_clang_blacklist")
		endif()
	endif()
endif ()

SET( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${PLATFORM_LINK_FLAGS}" )


find_package (OpenSSL 1.1 REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})
add_definitions (-DRAICOIN_SECURE_RPC)
message("OpenSSL include dir: ${OPENSSL_INCLUDE_DIR}")
message("OpenSSL libraries: ${OPENSSL_LIBRARIES}")
message("OpenSSL lib: ${OPENSSL_SSL_LIBRARY}")
message("Crypto lib: ${OPENSSL_CRYPTO_LIBRARY}")

set(Boost_USE_STATIC_LIBS        ON)
set(Boost_USE_MULTITHREADED      ON)

find_package (Boost 1.66.0 REQUIRED COMPONENTS date_time filesystem system log log_setup thread program_options regex chrono atomic)
include_directories(${Boost_INCLUDE_DIRS})


if (WIN32)
	set (BLAKE2_IMPLEMENTATION "blake2/blake2b.c")
else ()
	if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(i.86|x86(_64)?)$")
		set (BLAKE2_IMPLEMENTATION "blake2/blake2b.c")
	else ()
		set (BLAKE2_IMPLEMENTATION "blake2/blake2b-ref.c")
	endif ()
endif ()

add_library (blake2
	blake2/blake2-config.h
	blake2/blake2-impl.h
	blake2/blake2.h
	${BLAKE2_IMPLEMENTATION})

target_compile_definitions (blake2 PRIVATE -D__SSE2__)

add_library (lmdb
	lmdb/libraries/liblmdb/lmdb.h
	lmdb/libraries/liblmdb/mdb.c
	lmdb/libraries/liblmdb/midl.c)
if (WIN32)
	target_link_libraries (lmdb ntdll)
endif ()

add_subdirectory (ed25519-donna)

set (BUILD_SHARED OFF CACHE BOOL "")
set (BUILD_TESTING OFF CACHE BOOL "")
set (USE_INTERMEDIATE_OBJECTS_TARGET OFF CACHE BOOL "")

set (CRYPTOPP_EXTRA "")
if (WIN32)
	set (CRYPTOPP_EXTRA cryptopp/x64dll.asm
					cryptopp/x64masm.asm)
	enable_language(ASM)
	enable_language(ASM_MASM)
	# similar to SSE2 settings
	add_definitions(-DCRYPTOPP_DISABLE_SSSE3
					-DCRYPTOPP_DISABLE_AESNI)
endif ()

if (CRYPTOPP_CUSTOM OR NOT RAICOIN_SIMD_OPTIMIZATIONS)
	set (CRYPTOPP_LIBRARY cryptopp)
	add_library (cryptopp
		cryptopp/algparam.cpp
		cryptopp/asn.cpp
		cryptopp/basecode.cpp
		cryptopp/cpu.cpp
		cryptopp/cryptlib.cpp
		cryptopp/default.cpp
		cryptopp/des.cpp
		cryptopp/dessp.cpp
		cryptopp/dll.cpp
		cryptopp/ec2n.cpp
		cryptopp/ecp.cpp
		cryptopp/filters.cpp
		cryptopp/fips140.cpp
		cryptopp/gcm.cpp
		cryptopp/gf2n.cpp
		cryptopp/gfpcrypt.cpp
		cryptopp/hex.cpp
		cryptopp/hmac.cpp
		cryptopp/hrtimer.cpp
		cryptopp/integer.cpp
		cryptopp/iterhash.cpp
		cryptopp/misc.cpp
		cryptopp/modes.cpp
		cryptopp/mqueue.cpp
		cryptopp/nbtheory.cpp
		cryptopp/oaep.cpp
		cryptopp/osrng.cpp
		cryptopp/pubkey.cpp
		cryptopp/queue.cpp
		cryptopp/randpool.cpp
		cryptopp/rdtables.cpp
		cryptopp/rijndael.cpp
		cryptopp/rng.cpp
		cryptopp/sha.cpp
		cryptopp/simple.cpp
		cryptopp/winpipes.cpp
		${CRYPTOPP_EXTRA})
else ()
	set(CRYPTOPP_LIBRARY cryptopp-static)
	add_subdirectory (cryptopp)
endif ()

if (CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
	set(ARGON_CORE phc-winner-argon2/src/opt.c)
else ()
	set(ARGON_CORE phc-winner-argon2/src/ref.c)
endif ()

add_library(argon2
	phc-winner-argon2/src/argon2.c
	phc-winner-argon2/include/argon2.h
	phc-winner-argon2/src/core.c
	phc-winner-argon2/src/thread.c
	phc-winner-argon2/src/encoding.c
	${ARGON_CORE})

target_include_directories(argon2 PUBLIC phc-winner-argon2/include)
target_include_directories(argon2 PUBLIC phc-winner-argon2/src)
target_include_directories(argon2 PUBLIC blake2)

add_library(xxhash
	xxhash/xxhash.c
	xxhash/xxhash.h)


add_subdirectory(rai/custom)
add_subdirectory(rai/secure)
add_subdirectory(rai/common)
add_subdirectory(rai/node)
add_subdirectory(rai/rai_node)
add_subdirectory(rai/wallet)
add_subdirectory(rai/rai_airdrop)
add_subdirectory(rai/rai_wallet_rpc)

if (RAI_GUI)
	if (WIN32)
		set (PLATFORM_QT_PACKAGES WinExtras)
	else ()
		set (PLATFORM_QT_PACKAGES)
	endif ()
	find_package (Qt5 REQUIRED COMPONENTS Core Gui Widgets Test ${PLATFORM_QT_PACKAGES})
	add_subdirectory(rai/rai_wallet)
	set_target_properties (rai_wallet PROPERTIES COMPILE_FLAGS "-DQT_NO_KEYWORDS -DACTIVE_NETWORK=${ACTIVE_NETWORK} -DBOOST_ASIO_HAS_STD_ARRAY=1")
endif ()

if (RAI_TEST)
	if (WIN32)
		if (MSVC_VERSION)
			if (MSVC_VERSION GREATER_EQUAL 1910)
				add_definitions(-DGTEST_LANG_CXX11=1)
				add_definitions(-DGTEST_HAS_TR1_TUPLE=0)
			endif ()
		endif ()
		set (gtest_force_shared_crt ON)
	else ()
		set(gtest_force_shared_crt OFF)
	endif()

	add_subdirectory (gtest)
	# FIXME: This fixes gtest include directories without modifying gtest's
	# CMakeLists.txt. Ideally we should use GTest::GTest and GTest::Main as
	# dependencies but it requires building gtest differently
	set_target_properties (gtest PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
		"${CMAKE_SOURCE_DIR}/gtest/include")

	add_subdirectory (rai/core_test)
endif ()

if (RAI_GUI)
	if (APPLE)
		install (TARGETS rai_wallet DESTINATION Raicoin.app/Contents/MacOS)
		install (FILES Info.plist DESTINATION Raicoin.app/Contents)
		install (FILES qt.conf DESTINATION Raicoin.app/Contents/Resources)
		install (DIRECTORY ${Qt5_DIR}/../../QtCore.framework DESTINATION Raicoin.app/Contents/Frameworks)
		install (DIRECTORY ${Qt5_DIR}/../../QtDBus.framework DESTINATION Raicoin.app/Contents/Frameworks)
		install (DIRECTORY ${Qt5_DIR}/../../QtGui.framework DESTINATION Raicoin.app/Contents/Frameworks)
		install (DIRECTORY ${Qt5_DIR}/../../QtPrintSupport.framework DESTINATION Raicoin.app/Contents/Frameworks)
		install (DIRECTORY ${Qt5_DIR}/../../QtTest.framework DESTINATION Raicoin.app/Contents/Frameworks)
		install (DIRECTORY ${Qt5_DIR}/../../QtWidgets.framework DESTINATION Raicoin.app/Contents/Frameworks)
		install (FILES "${Qt5_DIR}/../../../plugins/platforms/libqcocoa.dylib" DESTINATION Raicoin.app/Contents/PlugIns/platforms)
		install (FILES Raicoin.icns DESTINATION Raicoin.app/Contents/Resources)
		install (FILES ${CMAKE_SOURCE_DIR}/pem/cacert.pem DESTINATION Raicoin.app/Contents/MacOS)

		set(APP "\${CMAKE_INSTALL_PREFIX}/Raicoin.app") #${APP}/Contents/Frameworks/${LIBSSL_FILE}
		get_filename_component(LIBCRYPTO ${OPENSSL_CRYPTO_LIBRARY} REALPATH)
		get_filename_component(LIBCRYPTO_FILE ${LIBCRYPTO} NAME)
		get_filename_component(LIBSSL ${OPENSSL_SSL_LIBRARY} REALPATH)
		get_filename_component(LIBSSL_FILE ${LIBSSL} NAME)
		install (FILES ${LIBCRYPTO} DESTINATION Raicoin.app/Contents/Frameworks)
		install(CODE "execute_process(COMMAND install_name_tool -change ${LIBCRYPTO} @rpath/${LIBCRYPTO_FILE} ${APP}/Contents/MacOS/rai_wallet)")
		install (FILES ${LIBSSL} DESTINATION Raicoin.app/Contents/Frameworks)
		install(CODE "execute_process(COMMAND install_name_tool -change ${LIBSSL} @rpath/${LIBSSL_FILE} ${APP}/Contents/MacOS/rai_wallet)")
	elseif (WIN32)
		if ("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
			set (WIN_REDIST vc_redist.x64.exe)
		else ()
			set (WIN_REDIST vc_redist.x86.exe)
		endif ()
		set (CPACK_NSIS_EXTRA_INSTALL_COMMANDS "ExecWait '\\\"$INSTDIR\\\\${WIN_REDIST}\\\" /quiet /norestart'
		WriteRegDWORD HKLM 'SYSTEM\\\\CurrentControlSet\\\\Services\\\\EventLog\\\\Raicoin\\\\Raicoin' 'TypesSupported' '0x7'
		WriteRegStr HKLM 'SYSTEM\\\\CurrentControlSet\\\\Services\\\\EventLog\\\\Raicoin\\\\Raicoin' 'EventMessageFile' '$INSTDIR\\\\rai_wallet.exe'")
		set (CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "DeleteRegKey HKLM 'SYSTEM\\\\CurrentControlSet\\\\Services\\\\EventLog\\\\Raicoin'")
		set (CPACK_PACKAGE_INSTALL_DIRECTORY "raicoin")
		set (CPACK_NSIS_MUI_ICON ${CMAKE_SOURCE_DIR}/Raicoin.ico)
		set (CPACK_NSIS_DISPLAY_NAME "Raicoin")
		set (CPACK_PACKAGE_NAME "raicoin-wallet")
		set (CPACK_NSIS_PACKAGE_NAME "Raicoin")
		set (CPACK_NSIS_URL_INFO_ABOUT "https://raicoin.org")
		set (CPACK_NSIS_CONTACT "info@raicoin.org")
		set (CPACK_NSIS_MENU_LINKS "rai_wallet.exe" "Raicoin Wallet" "https://raicoin.org" "Raicoin website")
		set (CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
		get_target_property (Qt5WindowsPlugin Qt5::QWindowsIntegrationPlugin LOCATION)
		get_filename_component (Qt5_bin_DIR ${Qt5_DIR}/../../../bin ABSOLUTE)
		install (TARGETS rai_wallet DESTINATION .)
		install (FILES ${CMAKE_CURRENT_BINARY_DIR}/${WIN_REDIST} DESTINATION .)
		install (FILES ${Qt5_bin_DIR}/libGLESv2.dll DESTINATION .)
		install (FILES ${Qt5_bin_DIR}/Qt5Core.dll DESTINATION .)
		install (FILES ${Qt5_bin_DIR}/Qt5Gui.dll DESTINATION .)
		install (FILES ${Qt5_bin_DIR}/Qt5Widgets.dll DESTINATION .)
		install (FILES ${Qt5_bin_DIR}/Qt5WinExtras.dll DESTINATION .)
		install (FILES ${OPENSSL_ROOT_DIR}/bin/libcrypto-1_1-x64.dll DESTINATION .)
		install (FILES ${OPENSSL_ROOT_DIR}/bin/libssl-1_1-x64.dll DESTINATION .)
		install (FILES ${CMAKE_SOURCE_DIR}/pem/cacert.pem DESTINATION .)
		install (FILES ${Qt5WindowsPlugin} DESTINATION platforms)
	else ()
	endif ()
endif ()

set (CPACK_RESOURCE_FILE_LICENSE ${CMAKE_SOURCE_DIR}/LICENSE)
include (CPack)



