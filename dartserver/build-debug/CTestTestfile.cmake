# CMake generated Testfile for 
# Source directory: /Users/noahg/github/cam-stream/dartserver
# Build directory: /Users/noahg/github/cam-stream/dartserver/build-debug
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(x01_engine "/Users/noahg/github/cam-stream/dartserver/build-debug/test_x01")
set_tests_properties(x01_engine PROPERTIES  _BACKTRACE_TRIPLES "/Users/noahg/github/cam-stream/dartserver/CMakeLists.txt;35;add_test;/Users/noahg/github/cam-stream/dartserver/CMakeLists.txt;0;")
add_test(game_manager "/Users/noahg/github/cam-stream/dartserver/build-debug/test_game_manager")
set_tests_properties(game_manager PROPERTIES  _BACKTRACE_TRIPLES "/Users/noahg/github/cam-stream/dartserver/CMakeLists.txt;39;add_test;/Users/noahg/github/cam-stream/dartserver/CMakeLists.txt;0;")
add_test(cricket_engine "/Users/noahg/github/cam-stream/dartserver/build-debug/test_cricket")
set_tests_properties(cricket_engine PROPERTIES  _BACKTRACE_TRIPLES "/Users/noahg/github/cam-stream/dartserver/CMakeLists.txt;43;add_test;/Users/noahg/github/cam-stream/dartserver/CMakeLists.txt;0;")
add_test(round_clock_engine "/Users/noahg/github/cam-stream/dartserver/build-debug/test_roundclock")
set_tests_properties(round_clock_engine PROPERTIES  _BACKTRACE_TRIPLES "/Users/noahg/github/cam-stream/dartserver/CMakeLists.txt;47;add_test;/Users/noahg/github/cam-stream/dartserver/CMakeLists.txt;0;")
add_test(zone_parse "/Users/noahg/github/cam-stream/dartserver/build-debug/test_zone")
set_tests_properties(zone_parse PROPERTIES  _BACKTRACE_TRIPLES "/Users/noahg/github/cam-stream/dartserver/CMakeLists.txt;77;add_test;/Users/noahg/github/cam-stream/dartserver/CMakeLists.txt;0;")
add_test(persistence "/Users/noahg/github/cam-stream/dartserver/build-debug/test_db")
set_tests_properties(persistence PROPERTIES  _BACKTRACE_TRIPLES "/Users/noahg/github/cam-stream/dartserver/CMakeLists.txt;111;add_test;/Users/noahg/github/cam-stream/dartserver/CMakeLists.txt;0;")
subdirs("detection")
