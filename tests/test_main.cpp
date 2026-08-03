// test_main.cpp
// RuneVault — single entry point for the assert-based test suite.
//
// All test cases live in the other tests/*.cpp files and register themselves
// via the RV_TEST macro (see test_util.h). This file just runs them.

#include "test_util.h"

int main() { return rv::test::run_all(); }
