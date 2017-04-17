# - Find CURL
# Find the native CURL includes and library
#
#  CURL_INCLUDE_DIR  - where to find curl/curl.h, etc.
#  CURL_LIBRARIES    - List of libraries when using CURL.
#  CURL_FOUND        - True if CURL found.

IF (CURL_INCLUDE_DIR AND CURL_LIBRARIES)
  # Already in cache, be silent
  SET(CURL_FIND_QUIETLY TRUE)
ENDIF (CURL_INCLUDE_DIR AND CURL_LIBRARIES)

FIND_PATH(CURL_INCLUDE_DIR curl/curl.h)

SET(CURL_NAMES curl)
FIND_LIBRARY(CURL_LIBRARIES NAMES ${CURL_NAMES})

# handle the QUIETLY and REQUIRED arguments and set CURL_FOUND to TRUE if 
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(curl DEFAULT_MSG CURL_LIBRARIES CURL_INCLUDE_DIR)

MARK_AS_ADVANCED(CURL_LIBRARIES CURL_INCLUDE_DIR)
