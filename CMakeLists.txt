# Copyright (c) 2016 Fabian Schuiki
cmake_minimum_required(VERSION 2.6)
project(phalanx)

# Add in dependencies.
add_subdirectory(deps/libgds)

# Default to a release build, making the project easier to package. If you plan
# on writing code, call cmake with the -DCMAKE_BUILD_TYPE=debug option.
if (NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE release)
endif()
message("## phalanx ${CMAKE_BUILD_TYPE} build")

include_directories(/usr/include/cairo)

# Compiler Flags
include_directories(${PROJECT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/src)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -Wall -Wextra -Werror-implicit-function-declaration -Wno-unused-parameter -Wno-unused-label")

# Executable
add_executable(phalanx
	src/common.c
	src/cell.c
	src/util.c
	src/util-array.c
	src/lef.c
	src/main.c
)
target_link_libraries(phalanx gds cairo m)

# Installation
install(TARGETS phalanx RUNTIME DESTINATION bin)
