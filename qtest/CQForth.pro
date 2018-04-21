TEMPLATE = app

QT += widgets

TARGET = CQForthTest

DEPENDPATH += .

MOC_DIR = .moc

QMAKE_CXXFLAGS += -std=c++14

CONFIG += debug

SOURCES += \
CQForth.cpp \

HEADERS += \
CQForth.h \

DESTDIR     = ../bin
OBJECTS_DIR = ../obj
LIB_DIR     = ../lib

PRE_TARGETDEPS = \
$(LIB_DIR)/libCForth.a \

INCLUDEPATH = \
. \
../include \

unix:LIBS += \
-L$$LIB_DIR \
-L../../CForth/lib \
\
-lCForth \
