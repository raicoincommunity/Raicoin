if (RAICOIN_SECURE_RPC)
	set (secure_rpc_sources rpc_secure.cpp rpc_secure.hpp)
endif ()

add_library (node
	${secure_rpc_sources}
	blockprocessor.hpp
	blockprocessor.cpp
	blockquery.hpp
	blockquery.cpp
	bootstrap.hpp
	bootstrap.cpp
	config.cpp
	config.hpp
	dumper.hpp
	dumper.cpp
	election.hpp
	election.cpp
	gapcache.hpp
	gapcache.cpp
	message.hpp
	message.cpp
	network.hpp
	network.cpp
	node.hpp
	node.cpp
	peer.hpp
	peer.cpp
	rpc.hpp
	rpc.cpp
	subscribe.hpp
	subscribe.cpp
	syncer.hpp
	syncer.cpp
	rewarder.hpp
	rewarder.cpp
	validator.hpp
	validator.cpp
	)

target_link_libraries (node
	secure
	rai_common
	lmdb
	xxhash
	${Boost_LIBRARIES})
