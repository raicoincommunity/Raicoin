add_library (app
	app.cpp
	app.hpp
	blockcache.cpp
	blockcache.hpp
	blockconfirm.cpp
	blockconfirm.hpp
	bootstrap.cpp
	bootstrap.hpp
	subscribe.cpp
	subscribe.hpp
	config.cpp
	config.hpp
	provider.cpp
	provider.hpp
	rpc.cpp
	rpc.hpp
)

target_link_libraries (app
	secure
	rai_common
	${Boost_LIBRARIES}
	${OPENSSL_LIBRARIES}
)