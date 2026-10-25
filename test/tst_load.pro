QT += core gui network testlib concurrent
CONFIG += c++17 console
CONFIG -= app_bundle
TARGET = tst_load
TEMPLATE = app

INCLUDEPATH += ../src

# 真链 Load/Parse/Config/Utils（真实读文件、真实嗅探+解析）；
# Danmaku 与 Local 在 tst_load.cpp 里用最小实现替代。不链 APlayer.cpp：
# create() 会引入 VPlayer/libvlc，测试里也从不构造 APlayer 实例。
SOURCES += \
    tst_load.cpp \
    ../src/Access/Load.cpp \
    ../src/Access/Parse.cpp \
    ../src/Config.cpp \
    ../src/Utils.cpp

HEADERS += \
    ../src/Access/Load.h \
    ../src/Config.h \
    ../src/Local.h \
    ../src/Model/Comment.h \
    ../src/Model/Danmaku.h \
    ../src/Player/APlayer.h
