add_executable (rai_token
	token.cpp
	token.hpp
	config.cpp
	config.hpp
	entry.cpp
	rpc.cpp
	rpc.hpp
	subscribe.cpp
	subscribe.hpp
)

target_link_libraries (rai_token
	rai_common
	app
	secure
	${Boost_LIBRARIES}
)