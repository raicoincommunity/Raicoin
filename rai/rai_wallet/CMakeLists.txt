if (WIN32)
	set (PLATFORM_GUI_TARGET_PROPERTIES WIN32)
else (WIN32)
	set (PLATFORM_GUI_TARGET_PROPERTIES "")
endif (WIN32)

add_executable (rai_wallet
	${PLATFORM_GUI_TARGET_PROPERTIES}
	qt.hpp
	qt.cpp
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