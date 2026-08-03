# Makefile — RuneVault
#
# Targets:
#   make            Build the game binary (build/runevault[.exe]).
#   make run        Build and run the game.
#   make test       Build and run the assert-based test binary.
#   make clean      Remove build artifacts.
#
# Notes:
#   * The provided rng utilities (shuffle.cpp/shuffle.h) live in the project
#     root and are compiled into the game and linked into the tests.
#   * Tests are plain assert-based C++ (no external dependencies).

# ---- Toolchain --------------------------------------------------------------
CXX      := g++
CXXSTD   := -std=c++14
WARN     := -Wall -Wextra
CXXFLAGS := $(CXXSTD) $(WARN) -Iinclude -I.

# Automatic header-dependency generation. For each compiled .cpp the compiler
# emits a sibling .d file (-MMD, user headers only) listing every header the
# translation unit includes, with phantom targets for each header (-MP) so a
# renamed/deleted header never breaks the build. These .d files are -included
# below, so `make` recompiles ONLY the .cpp files affected by a changed header
# and then relinks — no `make clean` needed for correctness.
DEPFLAGS := -MMD -MP

# ---- Portable filesystem helpers (Windows cmd vs POSIX sh) ------------------
ifeq ($(OS),Windows_NT)
  MKDIR  = if not exist "$(subst /,\,$1)" mkdir "$(subst /,\,$1)"
  RMDIR  = if exist "$(subst /,\,$1)" rmdir /s /q "$(subst /,\,$1)"
  EXE    := .exe
else
  MKDIR  = mkdir -p "$1"
  RMDIR  = rm -rf "$1"
  EXE    :=
endif

# ---- Directories ------------------------------------------------------------
SRC_DIR    := src
TEST_DIR   := tests
BUILD_DIR  := build
OBJ_DIR    := $(BUILD_DIR)/obj

# ---- Game sources (RuneVault sources + provided rng utility) ----------------
GAME_SRCS := $(wildcard $(SRC_DIR)/*.cpp) shuffle.cpp
GAME_OBJS := $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(GAME_SRCS))
GAME_BIN  := $(BUILD_DIR)/runevault$(EXE)

# Game objects minus the entry point, for linking into the test binary.
MAIN_OBJ         := $(OBJ_DIR)/$(SRC_DIR)/main.o
GAME_OBJS_NOMAIN := $(filter-out $(MAIN_OBJ),$(GAME_OBJS))

# ---- Test sources -----------------------------------------------------------
TEST_SRCS := $(wildcard $(TEST_DIR)/*.cpp)
TEST_OBJS := $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(TEST_SRCS))
TEST_BIN  := $(BUILD_DIR)/runevault_tests$(EXE)

# ============================================================================
# Game build
# ============================================================================
.PHONY: all
all: $(GAME_BIN)

$(GAME_BIN): $(GAME_OBJS)
	@$(call MKDIR,$(@D))
	$(CXX) $(CXXFLAGS) $^ -o $@

.PHONY: run
run: $(GAME_BIN)
	$(GAME_BIN)

# Generic compile rule for every .cpp -> build/obj/<path>.o. Also emits a
# sibling .d dependency file (DEPFLAGS) consumed via the -include below.
$(OBJ_DIR)/%.o: %.cpp
	@$(call MKDIR,$(@D))
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

# ============================================================================
# Tests (plain assert-based, no external dependencies)
# ============================================================================
.PHONY: test
test: $(TEST_BIN)
	$(TEST_BIN)

$(TEST_BIN): $(TEST_OBJS) $(GAME_OBJS_NOMAIN)
	@$(call MKDIR,$(@D))
	$(CXX) $(CXXFLAGS) $^ -o $@

# ============================================================================
# Auto-generated header dependencies
# ============================================================================
# One .d per object (game + tests). Including them makes each .o depend on every
# header it (transitively) includes, so a header edit recompiles just the
# affected TUs. The leading '-' silences the "no such file" notice on the very
# first build (before any .d exists).
DEPS := $(GAME_OBJS:.o=.d) $(TEST_OBJS:.o=.d)
-include $(DEPS)

# ============================================================================
# Housekeeping
# ============================================================================
.PHONY: clean
clean:
	$(call RMDIR,$(BUILD_DIR))
