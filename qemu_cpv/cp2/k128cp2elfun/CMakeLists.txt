include_directories (${CMAKE_CURRENT_SOURCE_DIR}/include ${SOFTFLOAT_INCLUDE})
#add_definitions (-std=gnu99)

file (GLOB MODULE_SOURCES "*.c*")
add_library (k128cp2elfun STATIC ${MODULE_SOURCES})

