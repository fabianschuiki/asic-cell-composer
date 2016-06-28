# Copyright (c) 2016 Fabian Schuiki
cmake_minimum_required(VERSION 2.6)
project(phalanx)

# Default to a release build, making the project easier to package. If you plan
# on writing code, call cmake with the -DCMAKE_BUILD_TYPE=debug option.
if (NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE release)
endif()
message("## phalanx ${CMAKE_BUILD_TYPE} build")

add_executable(phalanx
	src/main.c
)
