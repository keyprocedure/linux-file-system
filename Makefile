ROOTNAME = fsshell
FOPTION =
RUNOPTIONS = SampleVolume 10000000 512
CC = gcc
CFLAGS = -g -Iinclude
LIBS = pthread
DEPS =

SRC_DIR = src
OBJ_DIR = obj

ADDOBJ = fsInit fsFreespace fsHelperFuncs fsDirectory miscDirFunctions \
         fsFreespaceHelper dirIterationFunctions b_io

ADDOBJ_FULL = $(addprefix $(OBJ_DIR)/, $(addsuffix .o, $(ADDOBJ)))

ifeq ($(shell uname -m), aarch64)
	ARCHOBJ = $(OBJ_DIR)/fsLowM1.o
else
	ARCHOBJ = $(OBJ_DIR)/fsLow.o
endif

OBJ = $(OBJ_DIR)/$(ROOTNAME)$(FOPTION).o $(ADDOBJ_FULL) $(ARCHOBJ)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(ROOTNAME)$(FOPTION): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) -lm -lreadline -l$(LIBS)

clean:
	rm -f $(OBJ_DIR)/$(ROOTNAME)$(FOPTION).o $(ADDOBJ_FULL) $(ROOTNAME)$(FOPTION)

run: $(ROOTNAME)$(FOPTION)
	./$(ROOTNAME)$(FOPTION) $(RUNOPTIONS)

vrun: $(ROOTNAME)$(FOPTION)
	valgrind ./$(ROOTNAME)$(FOPTION) $(RUNOPTIONS)
