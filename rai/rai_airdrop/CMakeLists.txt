add_executable (rai_airdrop
	airdrop.cpp
	airdrop.hpp
	entry.cpp)

target_link_libraries (rai_airdrop
	rai_common
	secure
	wallet
	${Boost_LIBRARIES}
)