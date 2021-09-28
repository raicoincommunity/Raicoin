add_executable (rai_app_alias
	alias.cpp
	alias.hpp
	config.cpp
	config.hpp
	entry.cpp)

target_link_libraries (rai_node
	rai_common
	node
	secure
	${Boost_LIBRARIES}
)