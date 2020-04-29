
add_library (wallet
	config.hpp
	config.cpp
	wallet.hpp
	wallet.cpp)

target_link_libraries (wallet
	secure
	rai_common
	${Boost_LIBRARIES}
	${OPENSSL_LIBRARIES}
)