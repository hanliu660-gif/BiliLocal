QT += core gui testlib
CONFIG += c++17 console
CONFIG -= app_bundle
TARGET = tst_shield
TEMPLATE = app

INCLUDEPATH += ../src

# 真链 Config 与 Shield；Local（含 moc）与 Utils::localPath 在 tst_shield.cpp
# 里用最小桩替代，配置目录指到临时目录。不需要 APlayer/Danmaku 那一套。
SOURCES += \
    tst_shield.cpp \
    ../src/Config.cpp \
    ../src/Model/Shield.cpp

HEADERS += \
    ../src/Config.h \
    ../src/Local.h \
    ../src/Model/Shield.h
