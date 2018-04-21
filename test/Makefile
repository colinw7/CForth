CC = g++
RM = rm

CDEBUG = -g
LDEBUG = -g

INC_DIR = ../include
OBJ_DIR = ../obj
LIB_DIR = ../lib
BIN_DIR = ../bin

all: dirs $(BIN_DIR)/CForthTest

dirs:
	@if [ ! -e ../bin ]; then mkdir ../bin; fi

SRC = \
CForthTest.cpp \

OBJS = $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(SRC))

CPPFLAGS = \
$(CDEBUG) \
-std=c++14 \
-I. \
-I$(INC_DIR) \
-I../../CReadLine/include \
-I../../CUtil/include \

LFLAGS = \
$(LDEBUG) \
-L$(LIB_DIR) \
-L../../CForth/lib \
-L../../CReadLine/lib \
-L../../CFile/lib \
-L../../CStrUtil/lib \
-L../../COS/lib \

LIBS = \
-lCForth -lCReadLine -lCFile -lCStrUtil -lCOS \
-lreadline

clean:
	$(RM) -f $(OBJ_DIR)/*.o
	$(RM) -f $(BIN_DIR)/CForthTest

.SUFFIXES: .cpp

.cpp.o:
	$(CC) -c $< -o $(OBJ_DIR)/$*.o $(CPPFLAGS)

$(OBJ_DIR)/CForthTest.o: CForthTest.cpp
	$(CC) -c CForthTest.cpp -o $(OBJ_DIR)/CForthTest.o $(CPPFLAGS)

$(BIN_DIR)/CForthTest: $(OBJS) $(LIB_DIR)/libCForth.a
	$(CC) $(LDEBUG) -o $(BIN_DIR)/CForthTest $(OBJS) $(LFLAGS) $(LIBS)
