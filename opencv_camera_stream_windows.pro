QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    codec.cpp \
    main.cpp \
    mainwindow.cpp \
    udpclient.cpp \
    udpserver.cpp


HEADERS += \
    codec.h \
    mainwindow.h \
    udpclient.h \
    udpserver.h


FORMS += \
    mainwindow.ui

INCLUDEPATH += C:\opencv\build\include \
                C:\ffmpeg\install\include

LIBS += -LC:\ffmpeg\install\lib

LIBS += -lWs2_32 \
        -lIphlpapi \
        C:\opencv\Release\bin\libopencv_core4120.dll \
        C:\opencv\Release\bin\libopencv_imgproc4120.dll \
        C:\opencv\Release\bin\libopencv_videoio4120.dll \
        -lavcodec \
        -lavformat \
        -lswscale \
        -lavutil

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
