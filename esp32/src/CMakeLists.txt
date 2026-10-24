FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.c)
idf_component_register(SRCS ${app_sources} INCLUDE_DIRS ".")
# Messages and paths are cut to fit their buffers on purpose.
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-format-truncation)
