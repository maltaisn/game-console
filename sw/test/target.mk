
ALL_TESTS := graphics

# run all tests
all_tests: $(addsuffix _test, $(ALL_TESTS))
	cd test; $(foreach t,$(ALL_TESTS),../$(BUILD_DIR)/$(t)_test;)

graphics_test:
	$(MAKE) all TEST_NAME=graphics TEST_SRC=""

