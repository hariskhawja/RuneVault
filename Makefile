# C++14 build with generated header dependencies.
CXX      := g++
CXXSTD   := -std=c++14
WARN     := -Wall -Wextra
CXXFLAGS := $(CXXSTD) $(WARN) -Iinclude -I.

# Recompile affected sources when headers change.
DEPFLAGS := -MMD -MP

ifeq ($(OS),Windows_NT)
  MKDIR  = if not exist "$(subst /,\,$1)" mkdir "$(subst /,\,$1)"
  RMDIR  = if exist "$(subst /,\,$1)" rmdir /s /q "$(subst /,\,$1)"
  EXE    := .exe
else
  MKDIR  = mkdir -p "$1"
  RMDIR  = rm -rf "$1"
  EXE    :=
endif

SRC_DIR    := src
TEST_DIR   := tests
BUILD_DIR  := build
OBJ_DIR    := $(BUILD_DIR)/obj

rwildcard = $(foreach d,$(wildcard $(1)/*),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))

GAME_SRCS := $(call rwildcard,$(SRC_DIR),*.cpp) shuffle.cpp
GAME_OBJS := $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(GAME_SRCS))
GAME_BIN  := $(BUILD_DIR)/runevault$(EXE)

MAIN_OBJ         := $(OBJ_DIR)/$(SRC_DIR)/main.o
GAME_OBJS_NOMAIN := $(filter-out $(MAIN_OBJ),$(GAME_OBJS))

TEST_SRCS := $(wildcard $(TEST_DIR)/*.cpp)
TEST_OBJS := $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(TEST_SRCS))
TEST_BIN  := $(BUILD_DIR)/runevault_tests$(EXE)

.PHONY: all
all: $(GAME_BIN)

$(GAME_BIN): $(GAME_OBJS)
	@$(call MKDIR,$(@D))
	$(CXX) $(CXXFLAGS) $^ -o $@

.PHONY: run
run: $(GAME_BIN)
	$(GAME_BIN)

$(OBJ_DIR)/%.o: %.cpp
	@$(call MKDIR,$(@D))
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

.PHONY: test
ifeq ($(strip $(TEST_SRCS)),)
test:
	@echo No test sources found in tests/.
	@exit 1
else
test: $(TEST_BIN)
	$(TEST_BIN)

$(TEST_BIN): $(TEST_OBJS) $(GAME_OBJS_NOMAIN)
	@$(call MKDIR,$(@D))
	$(CXX) $(CXXFLAGS) $^ -o $@
endif

DEPS := $(GAME_OBJS:.o=.d) $(TEST_OBJS:.o=.d)
-include $(DEPS)

.PHONY: clean
clean:
	$(call RMDIR,$(BUILD_DIR))
