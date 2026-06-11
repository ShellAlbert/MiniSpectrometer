QT += widgets serialport

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    zhistoryframe.cpp \
    zmainwindow.cpp \
    zringbuffer.cpp \
    zsingleframe.cpp \
    zspectrumcanvas.cpp \
    zuartparser.cpp \
    zuartworker.cpp

HEADERS += \
    zhistoryframe.h \
    zmainwindow.h \
    zringbuffer.h \
    zsingleframe.h \
    zspectrumcanvas.h \
    zuartparser.h \
    zuartworker.h

TRANSLATIONS += \
    SpectrumPlotter_zh_CN.ts
CONFIG += lrelease
CONFIG += embed_translations

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
