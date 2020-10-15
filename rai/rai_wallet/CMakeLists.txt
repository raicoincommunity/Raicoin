if (WIN32)
	set (PLATFORM_GUI_TARGET_PROPERTIES WIN32)
	set (PLATFORM_WALLET_SOURCE plat/windows/icon.cpp ../../Raicoin.rc)
elseif (APPLE)
	set (PLATFORM_WALLET_SOURCE plat/default/icon.cpp)
else ()
	set (PLATFORM_GUI_TARGET_PROPERTIES "")
endif ()

#qt5_add_resources(RES resources.qrc)

add_executable (rai_wallet
	${PLATFORM_GUI_TARGET_PROPERTIES}
	${PLATFORM_WALLET_SOURCE}
	${RES}
	qt.hpp
	qt.cpp
	config.hpp
	config.cpp
	entry.cpp)

if (WIN32)
	set (PLATFORM_WALLET_LIBS Qt5::WinExtras)
else (WIN32)
	set (PLATFORM_WALLET_LIBS)
endif (WIN32)

target_link_libraries (rai_wallet
	Qt5::Gui
	Qt5::Widgets
	${PLATFORM_WALLET_LIBS}
	secure
	rai_common
	wallet
	${Boost_LIBRARIES}
	${OPENSSL_LIBRARIES}
)