if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	set (PLATFORM_SECURE_SOURCE plat/windows/plat.cpp)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	set (PLATFORM_SECURE_SOURCE plat/posix/plat.cpp)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
	set (PLATFORM_SECURE_SOURCE plat/posix/plat.cpp)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	set (PLATFORM_SECURE_SOURCE plat/darwin/plat.cpp)
else ()
	message ("Unknown platform: ${CMAKE_SYSTEM_NAME}")
endif ()

add_library (secure
	${PLATFORM_SECURE_SOURCE}
	common.cpp
	common.hpp
	lmdb.cpp
	lmdb.hpp
	util.cpp
	util.hpp
	store.cpp
	store.hpp
	ledger.cpp
	ledger.hpp
	http.cpp
	http.hpp
	websocket.cpp
	websocket.hpp
	rpc.cpp
	rpc.hpp
	)

target_link_libraries(secure
	ed25519
	rai_common
	lmdb
	argon2
	${OPENSSL_LIBRARIES}
	${Boost_LIBRARIES})

target_compile_definitions(secure PUBLIC
	-DBOOST_ASIO_HAS_STD_ARRAY=1)
