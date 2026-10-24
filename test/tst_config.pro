QT += core gui testlib
CONFIG += c++17 console
CONFIG -= app_bundle
TARGET = tst_config
TEMPLATE = app

INCLUDEPATH += ../src

# 只链 Config；Local 服务定位器（lApp）与 Utils::localPath 在 tst_config.cpp
# 里用最小桩替代，因此本测试不需要 APlayer/Load/Danmaku 那一套。
SOURCES += \
    tst_config.cpp \
    ../src/Config.cpp

HEADERS += \
    ../src/Config.h
