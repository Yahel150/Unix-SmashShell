COMPILER := g++
COMPILER_FLAGS := --std=c++11 -Wall
SRCS := Commands.cpp signals.cpp smash.cpp
OBJS := $(subst .cpp,.o,$(SRCS))
SMASH_BIN := smash

.PHONY: all clean

all: $(SMASH_BIN)

$(SMASH_BIN): $(OBJS)
	$(COMPILER) $(COMPILER_FLAGS) $^ -o $@

$(OBJS): %.o: %.cpp
	$(COMPILER) $(COMPILER_FLAGS) -c $^

clean:
	rm -rf $(SMASH_BIN) $(OBJS)
