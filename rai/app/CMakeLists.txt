add_library (app
	app.cpp
    app.hpp
	blockcache.cpp
	blockcache.hpp
)

target_link_libraries (app
	secure
	rai_common
	${Boost_LIBRARIES}
	${OPENSSL_LIBRARIES}
)