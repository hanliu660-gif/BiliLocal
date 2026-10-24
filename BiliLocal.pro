QT += \
    core \
    gui \
    network \
    concurrent

# network 只剩一个用途：QLocalSocket/QLocalServer 单实例机制（Qt5 里它们属
# QtNetwork）。弹幕加载已收敛为纯本地文件，不再有网络栈。

TARGET = BiliLocal

TEMPLATE = app

CONFIG += c++17

# 2026-10-24 单平台化：本工程现在只面向 Windows + VLC 后端 + OpenGL/Embed 输出。
# 跨平台配置（linux/macx/android、QMM/NIL 后端、Raster/Quick2/vmem 输出）已删除，
# 需要时从 tag v0.5.0-vmem 或 backup bundle 找回。

SOURCES += \
    src/Access/Load.cpp \
    src/Access/Parse.cpp \
    src/Graphic/Graphic.cpp \
    src/Graphic/GraphicPrivate.cpp \
    src/Graphic/Mode1.cpp \
    src/Graphic/Mode4.cpp \
    src/Graphic/Mode5.cpp \
    src/Graphic/Mode6.cpp \
    src/Graphic/Mode7.cpp \
    src/Graphic/Plain.cpp \
    src/Model/Danmaku.cpp \
    src/Model/Running.cpp \
    src/Model/List.cpp \
    src/Model/Shield.cpp \
    src/Player/APlayer.cpp \
    src/Player/VPlayer.cpp \
    src/Render/ARender.cpp \
    src/Render/ASprite.cpp \
    src/UI/Interface.cpp \
    src/Config.cpp \
    src/Local.cpp \
    src/Utils.cpp

HEADERS += \
    src/Access/Load.h \
    src/Access/Parse.h \
    src/Graphic/Graphic.h \
    src/Graphic/GraphicPrivate.h \
    src/Graphic/Mode1.h \
    src/Graphic/Mode4.h \
    src/Graphic/Mode5.h \
    src/Graphic/Mode6.h \
    src/Graphic/Mode7.h \
    src/Graphic/Plain.h \
    src/Model/Comment.h \
    src/Model/Danmaku.h \
    src/Model/Running.h \
    src/Model/List.h \
    src/Model/Shield.h \
    src/Player/APlayer.h \
    src/Render/ARender.h \
    src/Render/ARenderPrivate.h \
    src/Render/ASprite.h \
    src/Render/ElapsedTimer.h \
    src/UI/Interface.h \
    src/UI/InterfacePrivate.h \
    src/Config.h \
    src/Local.h \
    src/Utils.h

INCLUDEPATH += \
    src

PRECOMPILED_HEADER = \
    src/Common.h

RESOURCES += \
    res/Res.qrc

TRANSLATIONS += \
    res/zh_CN.ts \
    res/zh_TW.ts

CONFIG(debug, debug|release){
DEFINES += GRAPHIC_DEBUG
}

win32{
RC_ICONS = res\icon.ico

DEFINES += \
    BACKEND_VLC \
    RENDER_OPENGL \
    INTERFACE_WIDGET

# 第三方库目录：默认还是作者当年那台机器的路径，
# 换机器时用 qmake "THIRDPARTY=C:/libs" 覆盖，别再改源码。
isEmpty(THIRDPARTY): THIRDPARTY = D:/App/Programming

INCLUDEPATH += \
    $$THIRDPARTY/include

BILILOCAL_ARCH = $$QT_ARCH
isEmpty(BILILOCAL_ARCH): BILILOCAL_ARCH = $${QMAKE_TARGET.arch}
contains(BILILOCAL_ARCH, x86_64){
BILILOCAL_64 = 1
}

isEmpty(BILILOCAL_64){
LIBS += \
    -L$$THIRDPARTY/lib
}
else{
LIBS += \
    -L$$THIRDPARTY/lib/amd64
}

LIBS += -ldwmapi
# Embed 输出的自动化验证钩子：运行时由 BILILOCAL_EMBED_TEST 环境变量触发，正常使用无影响
DEFINES += BILILOCAL_EMBED_TEST
}

contains(DEFINES, INTERFACE_WIDGET){
QT += widgets

SOURCES += \
    src/UI/Widget/Editor.cpp \
    src/UI/Widget/Home.cpp \
    src/UI/Widget/Info.cpp \
    src/UI/Widget/Menu.cpp \
    src/UI/Widget/Prefer.cpp \
    src/UI/Widget/WidgetInterfacePrivate.cpp

HEADERS += \
    src/UI/Widget/Editor.h \
    src/UI/Widget/Home.h \
    src/UI/Widget/Info.h \
    src/UI/Widget/Menu.h \
    src/UI/Widget/Prefer.h \
    src/UI/Widget/WidgetInterfacePrivate.h

message(enable widget interface)
}

contains(DEFINES, RENDER_OPENGL){
SOURCES += \
    src/Render/OpenGL/OpenGLRender.cpp \
    src/Render/OpenGL/OpenGLAtlas.cpp \
    src/Render/OpenGL/SyncTextureSprite.cpp \
    src/Render/OpenGL/EmbedPrivate.cpp
HEADERS += \
    src/Render/OpenGL/OpenGLRender.h \
    src/Render/OpenGL/OpenGLRenderPrivate.h \
    src/Render/OpenGL/OpenGLAtlas.h \
    src/Render/OpenGL/SyncTextureSprite.h \
    src/Render/OpenGL/EmbedPrivate.h

message(enable opengl render embed output)
}

contains(DEFINES, BACKEND_VLC){
LIBS += \
    -lvlc \
    -lvlccore
message(enable vplayer libvlc backend)
}
