# ===== Configuration =====
BUILD_DIR := build
CMAKE := cmake
CTEST := ctest

BUILD_TYPE ?= Release

# ===== Default =====
.PHONY: all
all: build

# ===== Clean =====
.PHONY: clean
clean:
	@rm -rf $(BUILD_DIR)


# ===== Build (normal) =====
.PHONY: build
build:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && $(CMAKE) .. \
		-DCMAKE_BUILD_TYPE=$(BUILD_TYPE)
	@$(CMAKE) --build $(BUILD_DIR) -j

# ===== Rebuild (clean + build) =====
.PHONY: rebuild
rebuild: clean build

# ===== Run tests =====
.PHONY: run_tests
run_tests: build
	@cd $(BUILD_DIR) && $(CTEST) --output-on-failure

# ===== Lint (matches the CI clang-format check) =====
.PHONY: lint
lint:
	@find include/componeng src/componeng tests example \
		-name "*.hpp" -o -name "*.cpp" | \
		xargs clang-format --dry-run --Werror

# ===== Format (apply clang-format in place) =====
.PHONY: format
format:
	@find include/componeng src/componeng tests example \
		-name "*.hpp" -o -name "*.cpp" | \
		xargs clang-format -i
