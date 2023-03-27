# This file should be placed in the root directory of your project.
# Then modify the CMakeLists.txt file in the root directory of your
# project to incorporate the testing dashboard.

# Enable testing and include CTest
ENABLE_TESTING()
INCLUDE(CTest)

# Set project name and nightly start time
set(PROJECT_NAME "DD4hep")
set(NIGHTLY_START_TIME "01:00:00 UTC")

# Set drop information
set(DROP_METHOD "http")
set(DROP_SITE "aidasoft.desy.de")
set(DROP_LOCATION "/CDash/submit.php?project=${PROJECT_NAME}")
set(DROP_SITE_CDASH TRUE)

# Submit test results to dashboard
if(DROP_SITE_CDASH)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/CTestCustom.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/CTestCustom.cmake)
endif()

# TODO: Add tests and test targets
