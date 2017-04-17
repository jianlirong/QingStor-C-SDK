CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

SET(libqingstor_VERSION_MAJOR 0)
SET(libqingstor_VERSION_MINOR 0)
SET(libqingstor_VERSION_PATCH 1)
SET(libqingstor_VERSION_STRING "${libqingstor_VERSION_MAJOR}.${libqingstor_VERSION_MINOR}.${libqingstor_VERSION_PATCH}")
SET(libqingstor_VERSION_API 1)
SET(libqingstor_ROOT_SOURCES_DIR ${CMAKE_SOURCE_DIR}/src)

IF(ENABLE_DEBUG)
    SET(libqingstor_VERSION_STRING "${libqingstor_VERSION_STRING}d")
ENDIF(ENABLE_DEBUG)

CONFIGURE_FILE(platform.h.in platform.h)

AUTO_SOURCES(files "*.cpp" "RECURSE" "${CMAKE_CURRENT_SOURCE_DIR}")
LIST(APPEND libqingstor_SOURCES ${files})

AUTO_SOURCES(files "*.cc" "RECURSE" "${CMAKE_CURRENT_SOURCE_DIR}")
LIST(APPEND libqingstor_SOURCES ${files})

AUTO_SOURCES(files "lib/*.c" "RECURSE" "${CMAKE_CURRENT_SOURCE_DIR}")
LIST(APPEND libqingstor_SOURCES ${files})

AUTO_SOURCES(files "*.h" "RECURSE" "${CMAKE_CURRENT_SOURCE_DIR}")
LIST(APPEND libqingstor_SOURCES ${files})

AUTO_SOURCES(files "lib/*.h" "RECURSE" "${CMAKE_CURRENT_SOURCE_DIR}")
LIST(APPEND libqingstor_SOURCES ${files})

SET(HEADER 
    qingstor.h)

ADD_LIBRARY(libqingstor-static STATIC ${libqingstor_SOURCES})
ADD_LIBRARY(libqingstor-shared SHARED ${libqingstor_SOURCES})

ADD_CUSTOM_COMMAND(
    TARGET libqingstor-shared libqingstor-static
    PRE_BUILD
    COMMAND echo ${libqingstor_VERSION_STRING} > version
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

TARGET_LINK_LIBRARIES(libqingstor-static pthread)
TARGET_LINK_LIBRARIES(libqingstor-shared pthread)
       
IF(NEED_BOOST)
    INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIR})
    IF(OS_LINUX)
    		TARGET_LINK_LIBRARIES(libqingstor-static boost_thread)
    		TARGET_LINK_LIBRARIES(libqingstor-static boost_chrono)
    		TARGET_LINK_LIBRARIES(libqingstor-static boost_system)
    		TARGET_LINK_LIBRARIES(libqingstor-static boost_atomic)
    		TARGET_LINK_LIBRARIES(libqingstor-static boost_iostreams)
    		TARGET_LINK_LIBRARIES(libqingstor-shared boost_thread)
    		TARGET_LINK_LIBRARIES(libqingstor-shared boost_chrono)
    		TARGET_LINK_LIBRARIES(libqingstor-shared boost_system)
    		TARGET_LINK_LIBRARIES(libqingstor-shared boost_atomic)
    		TARGET_LINK_LIBRARIES(libqingstor-shared boost_iostreams)
    	ELSE(OS_LINUX)
    		TARGET_LINK_LIBRARIES(libqingstor-static boost_thread-mt)
    		TARGET_LINK_LIBRARIES(libqingstor-static boost_chrono-mt)
    		TARGET_LINK_LIBRARIES(libqingstor-static boost_system-mt)
    		TARGET_LINK_LIBRARIES(libqingstor-static boost_atomic-mt)
    		TARGET_LINK_LIBRARIES(libqingstor-static boost_iostreams-mt)
    		TARGET_LINK_LIBRARIES(libqingstor-shared boost_thread-mt)
    		TARGET_LINK_LIBRARIES(libqingstor-shared boost_chrono-mt)
    		TARGET_LINK_LIBRARIES(libqingstor-shared boost_system-mt)
    		TARGET_LINK_LIBRARIES(libqingstor-shared boost_atomic-mt)
    		TARGET_LINK_LIBRARIES(libqingstor-shared boost_iostreams-mt)
    	ENDIF(OS_LINUX)
ENDIF(NEED_BOOST)

IF(NEED_GCCEH)
    TARGET_LINK_LIBRARIES(libqingstor-static gcc_eh)
    TARGET_LINK_LIBRARIES(libqingstor-shared gcc_eh)
ENDIF(NEED_GCCEH)

INCLUDE_DIRECTORIES(${libqingstor_ROOT_SOURCES_DIR})
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})
INCLUDE_DIRECTORIES(${JSONC_INCLUDE_DIR})
INCLUDE_DIRECTORIES(${YAML_INCLUDE_DIR})
INCLUDE_DIRECTORIES(${CURL_INCLUDE_DIR})
INCLUDE_DIRECTORIES(${OPENSSL_INCLUDE_DIR})
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/mock)

TARGET_LINK_LIBRARIES(libqingstor-static ${JSONC_LIBRARIES})
TARGET_LINK_LIBRARIES(libqingstor-shared ${JSONC_LIBRARIES})

TARGET_LINK_LIBRARIES(libqingstor-static ${YAML_LIBRARIES})
TARGET_LINK_LIBRARIES(libqingstor-shared ${YAML_LIBRARIES})

TARGET_LINK_LIBRARIES(libqingstor-static ${OPENSSL_LIBRARIES})
TARGET_LINK_LIBRARIES(libqingstor-shared ${OPENSSL_LIBRARIES})

TARGET_LINK_LIBRARIES(libqingstor-static ${CURL_LIBRARIES})
TARGET_LINK_LIBRARIES(libqingstor-shared ${CURL_LIBRARIES})

SET_TARGET_PROPERTIES(libqingstor-static PROPERTIES OUTPUT_NAME "qingstor")
SET_TARGET_PROPERTIES(libqingstor-shared PROPERTIES OUTPUT_NAME "qingstor")

IF(NEED_BOOST)
    SET_TARGET_PROPERTIES(libqingstor-shared libqingstor-static PROPERTIES LINK_FLAGS "-L${Boost_LIBRARY_DIRS}")
ENDIF(NEED_BOOST)

SET_TARGET_PROPERTIES(libqingstor-shared PROPERTIES 
    VERSION ${libqingstor_VERSION_MAJOR}.${libqingstor_VERSION_MINOR}.${libqingstor_VERSION_PATCH} 
    SOVERSION ${libqingstor_VERSION_API})

INSTALL(TARGETS libqingstor-static libqingstor-shared
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)
INSTALL(FILES ${HEADER} DESTINATION include/qingstor)
INSTALL(FILES libqingstor.pc DESTINATION lib/pkgconfig)
            
SET(libqingstor_SOURCES ${libqingstor_SOURCES} PARENT_SCOPE)
SET(libqingstor_PLATFORM_HEADER_DIR ${CMAKE_CURRENT_BINARY_DIR} PARENT_SCOPE)
SET(libqingstor_ROOT_SOURCES_DIR ${libqingstor_ROOT_SOURCES_DIR} PARENT_SCOPE)
SET(libqingstor_VERSION_STRING ${libqingstor_VERSION_STRING} PARENT_SCOPE)
