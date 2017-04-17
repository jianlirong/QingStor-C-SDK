# - Find YAML
# Find the native YAML includes and library
#
#  YAML_INCLUDE_DIR  - where to find yaml.h, etc.
#  YAML_LIBRARIES    - List of libraries when using YAML.
#  YAML_FOUND        - True if YAML found.

IF (YAML_INCLUDE_DIR AND YAML_LIBRARIES)
  # Already in cache, be silent
  SET(YAML_FIND_QUIETLY TRUE)
ENDIF (YAML_INCLUDE_DIR AND YAML_LIBRARIES)

FIND_PATH(YAML_INCLUDE_DIR yaml.h)

SET(YAML_NAMES yaml)
FIND_LIBRARY(YAML_LIBRARIES NAMES ${YAML_NAMES})

# handle the QUIETLY and REQUIRED arguments and set YAML_FOUND to TRUE if 
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(yaml DEFAULT_MSG YAML_LIBRARIES YAML_INCLUDE_DIR)

MARK_AS_ADVANCED(YAML_LIBRARIES YAML_INCLUDE_DIR)
