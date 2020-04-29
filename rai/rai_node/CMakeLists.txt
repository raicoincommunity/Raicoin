add_executable (rai_node
	cli.cpp
	cli.hpp
	daemon.cpp
	daemon.hpp
	entry.cpp)

target_link_libraries (rai_node
	rai_common
	node
	secure
	${Boost_LIBRARIES}
)