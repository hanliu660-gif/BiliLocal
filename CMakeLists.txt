# CMake 版构建（2026-10-25 起）：与 BiliLocal.pro 等价的平行实现。
# 用法（MinGW + Qt 5.15.2）：
#   cmake -G "MinGW Makefiles" -B build-cmake \
#         -DCMAKE_BUILD_TYPE=Release \
#         -DQt5_DIR=<qt>/lib/cmake/Qt5
#   cmake --build build-cmake -j
# 用法（MSVC + Qt 6.8 msvc2022_64，Phase 2 起）：
#   cmake -G "Visual Studio 17 2022" -A x64 -B build-cmake6 \
#         -DQt6_DIR=<qt6>/lib/cmake/Qt6
#   cmake --build build-cmake6 --config Release -j
# THIRDPARTY 默认指向仓库外的 deps\thirdparty，可用 -DTHIRDPARTY=... 覆盖。
# MinGW 链 gendef/dlltool 再生的 lib/amd64/*.dll.a；MSVC 直接链官方
# 3.0.24 SDK 的 lib/msvc/*.lib（从 deps\dl\vlc-3.0.24-win64.7z 的 sdk\lib 再取）。
cmake_minimum_required(VERSION 3.16)

project(BiliLocal VERSION 0.5.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()

# 中文 Windows 的 MSVC 默认按 GBK 读源码；仓库源码是无 BOM UTF-8（带中文的
# 文件才有 BOM），不加这条注释里的 UTF-8 序列会被 GBK 误读，搅坏后续声明
if(MSVC)
	add_compile_options(/utf-8)
endif()

# Qt6：QOpenGL* 移入 OpenGL 模块、QOpenGLWindow 移入 OpenGLWidgets、
# QTextCodec 移入 Core5Compat（GBK 老弹幕文件必须靠它，QStringConverter 只覆盖 UTF 系）
find_package(Qt6 6.8 REQUIRED COMPONENTS
	Core Gui Widgets Network Concurrent OpenGL OpenGLWidgets Core5Compat Test LinguistTools)

set(THIRDPARTY "${CMAKE_CURRENT_SOURCE_DIR}/../deps/thirdparty" CACHE PATH "VLC SDK root (include + lib/amd64)")

# CI 上只编测试（单测不链 libvlc，不需要 SDK）；主程序默认仍随全量构建
option(BILILOCAL_BUILD_APP "Build the main application (needs the VLC SDK)" ON)

# ---- 主程序 ---------------------------------------------------------------

if(BILILOCAL_BUILD_APP)
set(BILILOCAL_SOURCES
	src/Access/Load.cpp
	src/Access/Parse.cpp
	src/Graphic/Graphic.cpp
	src/Graphic/GraphicPrivate.cpp
	src/Graphic/Mode1.cpp
	src/Graphic/Mode4.cpp
	src/Graphic/Mode5.cpp
	src/Graphic/Mode6.cpp
	src/Graphic/Mode7.cpp
	src/Graphic/Plain.cpp
	src/Model/Danmaku.cpp
	src/Model/Running.cpp
	src/Model/List.cpp
	src/Model/Shield.cpp
	src/Player/APlayer.cpp
	src/Player/VPlayer.cpp
	src/Render/ARender.cpp
	src/Render/ASprite.cpp
	src/UI/Interface.cpp
	src/Config.cpp
	src/Local.cpp
	src/Utils.cpp
	# INTERFACE_WIDGET
	src/UI/Widget/Editor.cpp
	src/UI/Widget/Home.cpp
	src/UI/Widget/Info.cpp
	src/UI/Widget/Menu.cpp
	src/UI/Widget/Prefer.cpp
	src/UI/Widget/WidgetInterfacePrivate.cpp
	# RENDER_OPENGL
	src/Render/OpenGL/OpenGLRender.cpp
	src/Render/OpenGL/OpenGLAtlas.cpp
	src/Render/OpenGL/SyncTextureSprite.cpp
	src/Render/OpenGL/EmbedPrivate.cpp
)

set(BILILOCAL_HEADERS
	src/Access/Load.h
	src/Access/Parse.h
	src/Graphic/Graphic.h
	src/Graphic/GraphicPrivate.h
	src/Graphic/Mode1.h
	src/Graphic/Mode4.h
	src/Graphic/Mode5.h
	src/Graphic/Mode6.h
	src/Graphic/Mode7.h
	src/Graphic/Plain.h
	src/Model/Comment.h
	src/Model/Danmaku.h
	src/Model/Running.h
	src/Model/List.h
	src/Model/Shield.h
	src/Player/APlayer.h
	src/Render/ARender.h
	src/Render/ARenderPrivate.h
	src/Render/ASprite.h
	src/Render/ElapsedTimer.h
	src/UI/Interface.h
	src/UI/InterfacePrivate.h
	src/UI/Widget/Editor.h
	src/UI/Widget/Home.h
	src/UI/Widget/Info.h
	src/UI/Widget/Menu.h
	src/UI/Widget/Prefer.h
	src/UI/Widget/WidgetInterfacePrivate.h
	src/Render/OpenGL/OpenGLRender.h
	src/Render/OpenGL/OpenGLRenderPrivate.h
	src/Render/OpenGL/OpenGLAtlas.h
	src/Render/OpenGL/SyncTextureSprite.h
	src/Render/OpenGL/EmbedPrivate.h
	src/Config.h
	src/Local.h
	src/Utils.h
)

add_executable(BiliLocal WIN32
	${BILILOCAL_SOURCES}
	${BILILOCAL_HEADERS}
	res/Res.qrc
	res/BiliLocal.rc
)

target_include_directories(BiliLocal PRIVATE
	src
	${THIRDPARTY}/include
)

if(MSVC)
	# /utf-8 已全局加（含测试目标）；NOMINMAX：Qt 的 qt_windows.h 已自带，
	# 但对直接 #include <windows.h> 的 TU 再钉一道
	target_compile_definitions(BiliLocal PRIVATE NOMINMAX)
	target_link_directories(BiliLocal PRIVATE ${THIRDPARTY}/lib/msvc)
	target_link_libraries(BiliLocal PRIVATE
		Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Concurrent
		Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Core5Compat
		dwmapi libvlc libvlccore
	)
else()
	target_link_directories(BiliLocal PRIVATE ${THIRDPARTY}/lib/amd64)
	target_link_libraries(BiliLocal PRIVATE
		Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Concurrent
		Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Core5Compat
		dwmapi vlc vlccore
	)
endif()

# 与 qmake 逐项对齐：单平台三件套 + 验证钩子（运行时由 BILILOCAL_EMBED_TEST
# 环境变量触发）；Debug 另加 GRAPHIC_DEBUG
target_compile_definitions(BiliLocal PRIVATE
	BACKEND_VLC
	RENDER_OPENGL
	INTERFACE_WIDGET
	BILILOCAL_EMBED_TEST
	$<$<CONFIG:Debug>:GRAPHIC_DEBUG>
)

target_precompile_headers(BiliLocal PRIVATE src/Common.h)

set_target_properties(BiliLocal PROPERTIES
	AUTOMOC ON
	AUTORCC ON
)

qt6_add_translation(BILILOCAL_QM
	res/zh_CN.ts
	res/zh_TW.ts
)
# qt*_add_translation 只声明输出不排产——不挂 ALL 目标的话 CMake 树从编不出 qm
#（Phase 1/2 之前 dist\locale 的 qm 都是 qmake 时代的产品，此处首次真正由 CMake 生成）
add_custom_target(translations ALL DEPENDS ${BILILOCAL_QM})

if(MINGW)
	target_link_options(BiliLocal PRIVATE $<$<CONFIG:Release>:-Wl,-s>)
endif()

endif() # BILILOCAL_BUILD_APP

# ---- 测试 ------------------------------------------------------------------
# 每个目标对应 test/ 下的一个 .pro；桩与最小实现在各自 tst_*.cpp 里，
# 不链接 libvlc（APlayer/Danmaku 由测试文件自给）。

function(bili_test name)
	cmake_parse_arguments(ARG "" "" "SOURCES;HEADERS;LIBS" ${ARGN})
	add_executable(${name} ${ARG_SOURCES} ${ARG_HEADERS})
	target_include_directories(${name} PRIVATE src)
	target_link_libraries(${name} PRIVATE Qt6::Core Qt6::Gui Qt6::Test Qt6::Core5Compat ${ARG_LIBS})
	set_target_properties(${name} PROPERTIES AUTOMOC ON)
endfunction()

bili_test(tst_parse
	SOURCES test/tst_parse.cpp src/Access/Parse.cpp src/Utils.cpp
	HEADERS src/Access/Parse.h
	LIBS Qt6::Network Qt6::Concurrent
)

bili_test(tst_config
	SOURCES test/tst_config.cpp src/Config.cpp
	HEADERS src/Config.h src/Local.h
)

bili_test(tst_shield
	SOURCES test/tst_shield.cpp src/Config.cpp src/Model/Shield.cpp
	HEADERS src/Config.h src/Local.h src/Model/Shield.h
)

bili_test(tst_load
	SOURCES test/tst_load.cpp src/Access/Load.cpp src/Access/Parse.cpp src/Config.cpp src/Utils.cpp
	HEADERS src/Access/Load.h src/Config.h src/Local.h src/Model/Comment.h src/Model/Danmaku.h src/Player/APlayer.h
	LIBS Qt6::Network Qt6::Concurrent
)

bili_test(tst_list
	SOURCES test/tst_list.cpp src/Model/List.cpp src/Access/Load.cpp src/Access/Parse.cpp src/Config.cpp src/Utils.cpp
	HEADERS src/Model/List.h src/Access/Load.h src/Config.h src/Local.h src/Model/Comment.h src/Model/Danmaku.h src/Player/APlayer.h
	LIBS Qt6::Network Qt6::Concurrent
)

enable_testing()
add_test(NAME tst_parse  COMMAND tst_parse)
add_test(NAME tst_config COMMAND tst_config)
add_test(NAME tst_shield COMMAND tst_shield)
add_test(NAME tst_load   COMMAND tst_load)
add_test(NAME tst_list   COMMAND tst_list)
