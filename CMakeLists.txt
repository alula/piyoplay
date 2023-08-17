cmake_minimum_required(VERSION 3.10.2)
project(piyoplay)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(piyoplay 
    main.cpp
    waves.cpp
    miniaudio_impl.cpp)

if (UNIX)
    target_link_libraries(piyoplay pthread dl)
endif()
