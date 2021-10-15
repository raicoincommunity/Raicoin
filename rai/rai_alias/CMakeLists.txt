add_executable (rai_alias
	alias.cpp
	alias.hpp
	config.cpp
	config.hpp
	entry.cpp
	rpc.cpp
	rpc.hpp
	subscribe.cpp
	subscribe.hpp
)

target_link_libraries (rai_alias
	rai_common
	app
	secure
	${Boost_LIBRARIES}
)