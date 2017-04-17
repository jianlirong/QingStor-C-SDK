# - Find JSON-C
# Find the native JSON-C includes and library
#
#  JSONC_INCLUDE_DIR  - where to find json/json.h, etc.
#  JSONC_LIBRARIES    - List of libraries when using JSONC.
#  JSONC_FOUND        - True if JSONC found.

IF (JSONC_INCLUDE_DIR AND JSONC_LIBRARIES)
  # Already in cache, be silent
  SET(JSONC_FIND_QUIETLY TRUE)
ENDIF (JSONC_INCLUDE_DIR AND JSONC_LIBRARIES)

FIND_PATH(JSONC_INCLUDE_DIR json/json.h)

SET(JSONC_NAMES json)
FIND_LIBRARY(JSONC_LIBRARIES NAMES ${JSONC_NAMES})

# handle the QUIETLY and REQUIRED arguments and set JSONC_FOUND to TRUE if 
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(jsonc DEFAULT_MSG JSONC_LIBRARIES JSONC_INCLUDE_DIR)

MARK_AS_ADVANCED(JSONC_LIBRARIES JSONC_INCLUDE_DIR)
