# Compiler and flags
CXX := g++
CXXFLAGS := -std=c++20 -pthread -O3 -g -ggdb -DENABLE_THREAD_PINNING=1
DEBUG_FLAGS := -std=c++20 -fsanitize=address,undefined,thread -D_GLIBCXX_DEBUG -pthread -O3 -g
PERF_FLAGS := -std=c++20 -pthread -O3 -g -ggdb -fno-omit-frame-pointer -DENABLE_THREAD_PINNING=1 -DPERF

# Directories
SRC_DIR := $(PWD)
# INC_DIR := include
OUT_DIR := build
BUILD_DIR := $(OUT_DIR)

# Source files and target
SOURCES := $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS := $(SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
# Generate a list of dependency files (.d files)
DEPENDS := $(OBJECTS:.o=.d)
TARGET := $(OUT_DIR)/main
DEBUG_TARGET := $(OUT_DIR)/main_debug
PERF_TARGET := $(OUT_DIR)/main_perf

# Default rule (release build)
all: $(OUT_DIR) $(TARGET)

# Release build
$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Debug build
debug: CXXFLAGS := $(DEBUG_FLAGS)
debug: $(OUT_DIR) $(DEBUG_TARGET)

$(DEBUG_TARGET): $(OBJECTS)
	$(CXX) $(DEBUG_FLAGS) -o $@ $^

# Perf time breakdown build
perf: CXXFLAGS := $(PERF_FLAGS) #update because get used for object files
perf: $(OUT_DIR) $(PERF_TARGET)

$(PERF_TARGET): $(OBJECTS)
	$(CXX) $(PERF_FLAGS) -o $@ $^

# Object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
# Use -MMD and -MP to generate the dependency files
# MMD: create .d file per header to ensure user header file change triggers recompilation
# MP: remove make error if header file is deleted
# $(CXX) $(CXXFLAGS) -I$(INC_DIR) -MMD -MP -c $< -o $@
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

# Create output directory
$(OUT_DIR):
	mkdir -p $(OUT_DIR)

# Clean rule
clean:
	rm -rf $(OUT_DIR)

# Include the generated dependency files
-include $(DEPENDS)

.PHONY: all debug clean
