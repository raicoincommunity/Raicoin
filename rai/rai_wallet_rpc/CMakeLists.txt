add_executable (rai_wallet_rpc
	wallet_rpc.cpp
	wallet_rpc.hpp
	entry.cpp)

target_link_libraries (rai_wallet_rpc
	rai_common
	secure
	wallet
	${Boost_LIBRARIES}
)