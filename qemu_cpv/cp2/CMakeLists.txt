#include_directories (${CMAKE_CURRENT_SOURCE_DIR}/../common ${PROJECT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/k128cp2elfun/include)
include_directories (${CMAKE_CURRENT_SOURCE_DIR}/../common ${CMAKE_CURRENT_SOURCE_DIR}/../k64 ${CMAKE_CURRENT_SOURCE_DIR}/k128cp2elfun/include)
include_directories (${SOFTFLOAT_INCLUDE} /usr/include/libxml2)
#add_definitions (-std=gnu99)

#http://stackoverflow.com/questions/2687136/how-do-i-make-build-rules-in-cmake-to-preprocess-lazy-c-lzz-files-that-genera
#http://solo-oboroten.livejournal.com/94941.html

#%.xmlc: %.xml
#	${CMAKE_CURRENT_SOURCE_DIR}/xml2string.sh $< $*_xmlfile_string > $@
function(add_xml_file output filename)
  # Only process *.xml files
  get_filename_component(ext ${filename} EXT)
  set(header_extension ".xml")
  if(NOT (ext STREQUAL "${header_extension}" AND EXISTS "${filename}"))
    return()
  endif()

  find_program(XML_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/xml2string.sh)
  IF(NOT XML_COMMAND)
     MESSAGE(FATAL_ERROR "Unable to find 'xml2string.sh' program")
  ENDIF(NOT XML_COMMAND)

  set(header_extension ".xmlc")
  get_filename_component(base ${filename} NAME_WE)
  get_filename_component(dir  ${filename} PATH)
  set(outfile ${dir}/${base}${header_extension}) # change filename extension
  set(${output} ${${output}} ${outfile} PARENT_SCOPE)

  #message("outfile=${outfile}, DEPENDS ${filename}")
  add_custom_command(
    OUTPUT ${outfile}
    COMMAND ${XML_COMMAND} ${filename} ${base}_xmlfile_string > ${outfile}
    DEPENDS "${filename}"
  )

#  set_source_files_properties(${outfile} PROPERTIES GENERATED TRUE)
endfunction()

#.m4.inc:
#	m4 $< > $@
function(add_m4_file output filename)
  # Only process *.m4 files
  get_filename_component(ext ${filename} EXT)
  set(header_extension ".m4")
  if(NOT (ext STREQUAL "${header_extension}" AND EXISTS "${filename}"))
    return()
  endif()

  find_program(M4_COMMAND m4)
  IF(NOT M4_COMMAND)
     MESSAGE(FATAL_ERROR "Unable to find 'm4' program")
  ENDIF(NOT M4_COMMAND)

  set(header_extension ".inc")
  get_filename_component(base ${filename} NAME_WE)
  get_filename_component(dir  ${filename} PATH)
  set(outfile ${dir}/${base}${header_extension}) # change filename extension
  set(${output} ${${output}} ${outfile} PARENT_SCOPE)

  #message("outfile=${outfile}, DEPENDS ${filename}")
  add_custom_command(
    OUTPUT ${outfile}
    COMMAND ${M4_COMMAND} ${filename} > ${outfile}
    DEPENDS "${filename}"
  )

#  set_source_files_properties(${outfile} PROPERTIES GENERATED TRUE)
endfunction()

set(INPUT_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/k128cp2_config.xml
  ${CMAKE_CURRENT_SOURCE_DIR}/k128cp2_resetstate.xml
  ${CMAKE_CURRENT_SOURCE_DIR}/instr_index.m4
  ${CMAKE_CURRENT_SOURCE_DIR}/instr_float.m4
)
foreach(file ${INPUT_SOURCES})
  add_m4_file(GENERATED_SOURCES ${file}) # - .inc files
endforeach()

foreach(file ${INPUT_SOURCES})
  add_xml_file(GENERATED_SOURCES ${file}) # - .xmlc files
endforeach()


#source_group("" FILES ${INPUT_SOURCES})
#source_group(generated FILES ${GENERATED_SOURCES})


file (GLOB MODULE_SOURCES "*.c*")
#file (GLOB MODULE_SOURCES "*.c*" "k128cp2elfun/*.c*" )

#add_library (cp2 STATIC ${MODULE_SOURCES})
add_library (cp2 STATIC ${MODULE_SOURCES} ${GENERATED_SOURCES})

