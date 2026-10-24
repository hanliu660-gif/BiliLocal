QT += core gui network testlib concurrent
CONFIG += c++17 console
CONFIG -= app_bundle
TARGET = tst_parse
TEMPLATE = app

INCLUDEPATH += ../src

# 只链被测的解析与工具实现；Danmaku/Shield 等模型类依赖 Local 全家，
# 不在本测试范围（见 tst_parse.cpp 头注）
SOURCES += \
    tst_parse.cpp \
    ../src/Access/Parse.cpp \
    ../src/Utils.cpp
