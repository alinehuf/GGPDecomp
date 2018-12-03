# Makefile

DEBUG=yes

CC = g++
BUILD_DIR=build
#SRC = $(wildcard *.cpp)
#OBJ=$(patsubst %.cpp,$(BUILD_DIR)/%.o,$(SRC))
EXEC = playerGGP


SRC = Gate.cpp Circuit.cpp IBFinder.cpp Propnet.cpp GateFactory.cpp YapData.cpp YapTranslator.cpp SimpleTerm.cpp Game.cpp Term.cpp GdlFactory.cpp GdlTools.cpp Grounder.cpp Splitter6.cpp UctSinglePlayer.cpp main.cpp

OBJ = $(BUILD_DIR)/Gate.o $(BUILD_DIR)/Circuit.o $(BUILD_DIR)/IBFinder.o $(BUILD_DIR)/Propnet.o $(BUILD_DIR)/GateFactory.o $(BUILD_DIR)/YapData.o $(BUILD_DIR)/YapTranslator.o $(BUILD_DIR)/SimpleTerm.o $(BUILD_DIR)/Game.o $(BUILD_DIR)/Term.o $(BUILD_DIR)/GdlFactory.o $(BUILD_DIR)/GdlTools.o $(BUILD_DIR)/Grounder.o $(BUILD_DIR)/Splitter6.o $(BUILD_DIR)/UctSinglePlayer.o $(BUILD_DIR)/main.o


ifeq ($(DEBUG),yes)
	CFLAGS = -Wall -g
else
	CFLAGS = -O3
endif

CFLAGS += -std=gnu++11   -I/usr/local/yap/include/yap++  -I../../Yap++/Yap++
LFLAGS = -L../../Yap++/build  #-L/usr/local/yap/lib -lyap
YAP_LINK= -lyap++ #-Wl,-Bstatic -lyap++

all: $(BUILD_DIR) $(BUILD_DIR)/$(EXEC)

ifeq ($(DEBUG),yes)
	@echo "Génération en mode debug"
else
	@echo "Génération en mode release"
endif

# $@ is the left side of the :
# $< is the first item in the dependencies list
# $^ is the right side of the :
# -MM : output a rule suitable for make describing the dependencies of the main source file
# -MT : change the target of the rule emitted by dependency generation

# include all the .d files corresponding to the .o files in OBJ
-include $(OBJ:.o=.d)

$(BUILD_DIR)/%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@
	$(CC) $(CFLAGS) -MM $< -MT $@ >$(BUILD_DIR)/$*.d

$(BUILD_DIR)/$(EXEC): $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)  $(YAP_LINK)



$(BUILD_DIR):
	@mkdir -p $@

clean:
	@rm -f *~
	@rm -f $(BUILD_DIR)/*.o $(BUILD_DIR)/*.d

cleanall: clean
	@rm -rf $(BUILD_DIR)


