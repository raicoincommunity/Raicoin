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
	topic.cpp
	topic.hpp
	swaphelper.cpp
	swaphelper.hpp
	chainwaiting.cpp
	chainwaiting.hpp
	crosschain/config.cpp
	crosschain/config.hpp
	crosschain/crosschain.cpp
	crosschain/crosschain.hpp
	crosschain/event.cpp
	crosschain/event.hpp
	crosschain/parsers/base.hpp
	crosschain/parsers/evm.hpp
	crosschain/parsers/evm.cpp
)

target_link_libraries (rai_token
	rai_common
	app
	secure
	${Boost_LIBRARIES}
)