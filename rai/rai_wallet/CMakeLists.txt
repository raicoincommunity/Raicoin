if (WIN32)
	set (PLATFORM_GUI_TARGET_PROPERTIES WIN32)
	set (PLATFORM_WALLET_SOURCE plat/windows/icon.cpp ../../Raicoin.rc)
else (WIN32)
	set (PLATFORM_GUI_TARGET_PROPERTIES "")
endif (WIN32)

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

target_link_libraries (rai_wallet
	Qt5::Gui
	Qt5::Widgets
	Qt5::WinExtras
	secure
	rai_common
	wallet
	${Boost_LIBRARIES}
	${OPENSSL_LIBRARIES}
)