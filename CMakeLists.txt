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

include_directories(/usr/include/cairo /usr/local/include/cairo)

# Compiler Flags
include_directories(${PROJECT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/src)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -Wall -Wextra -Werror-implicit-function-declaration -Wno-unused-parameter -Wno-unused-label")

# Combine the source files into object code to speed up compilation and remove
# some redundancy.
add_library(obj-common OBJECT
	src/common.c
	src/util.c
	src/util-array.c
	src/table.c
)
add_library(obj-design OBJECT
	src/design-cell.c
	src/design-inst.c
)
add_library(obj-lef OBJECT
	src/lef.c
)
add_library(obj-lib OBJECT
	src/lib.c
	src/lib-lexer.c
	src/lib-parser.c
	src/lib-ast.c
)

# Executable
add_executable(phalanx
	src/cell.c
	src/main.c
	$<TARGET_OBJECTS:obj-common>
	$<TARGET_OBJECTS:obj-design>
	$<TARGET_OBJECTS:obj-lib>
	$<TARGET_OBJECTS:obj-lef>
)
target_link_libraries(phalanx gds cairo m)

# Debugging tools
add_executable(lib-debug
	src/lib-debug.c
	$<TARGET_OBJECTS:obj-common>
	$<TARGET_OBJECTS:obj-lib>
)

# Installation
install(TARGETS phalanx RUNTIME DESTINATION bin)
