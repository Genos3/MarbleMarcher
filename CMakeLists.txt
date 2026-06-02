cmake_minimum_required(VERSION 3.5)
cmake_policy(SET CMP0079 NEW)

project(MarbleMarcher)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_FLAGS_RELEASE "-O2")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")

## DEPENDENCIES

find_package(Eigen3 REQUIRED)
find_package(SFML 2.5 COMPONENTS system window graphics audio REQUIRED)

## TARGETS

add_subdirectory(src)

target_link_libraries(MarbleMarcherSources PUBLIC
  Eigen3::Eigen
)

target_include_directories(MarbleMarcherSources PUBLIC
  ${SFML_INCLUDE_DIR}
  ${CMAKE_SOURCE_DIR}/src
)

target_compile_definitions(MarbleMarcherSources PRIVATE SFML_STATIC)

if(WIN32)
  add_executable(MarbleMarcher WIN32
    src/Main.cpp
    src/Resource.rc
    assets/icon.ico
  )
  
  set_target_properties(MarbleMarcher PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}
  )
  
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set_target_properties(MarbleMarcher PROPERTIES
      OUTPUT_NAME MarbleMarcher_dbg
    )
  endif()
  
  set_source_files_properties(icon.ico Resource.rc PROPERTIES LANGUAGE RC)
else()
  add_executable(MarbleMarcher src/Main.cpp)
endif()

target_compile_definitions(MarbleMarcher PRIVATE SFML_STATIC)

target_link_libraries(MarbleMarcher
  MarbleMarcherSources
  sfml-system
  sfml-window
  sfml-graphics
  sfml-audio
)
