QT += core gui network testlib concurrent
CONFIG += c++17 console
CONFIG -= app_bundle
TARGET = tst_list
TEMPLATE = app

INCLUDEPATH += ../src

# 真链 List/Load/Parse/Config/Utils；Danmaku 与 APlayer 在 tst_list.cpp 里用
# 最小子类/最小实现替代（APlayer 只为发 mediaChanged，不引 libvlc）。
# 刻意不注册 Config 实例，touch() 的自动保存链被掐断，测试不落盘。
SOURCES += \
    tst_list.cpp \
    ../src/Model/List.cpp \
    ../src/Access/Load.cpp \
    ../src/Access/Parse.cpp \
    ../src/Config.cpp \
    ../src/Utils.cpp

HEADERS += \
    ../src/Model/List.h \
    ../src/Access/Load.h \
    ../src/Config.h \
    ../src/Local.h \
    ../src/Model/Comment.h \
    ../src/Model/Danmaku.h \
    ../src/Player/APlayer.h
