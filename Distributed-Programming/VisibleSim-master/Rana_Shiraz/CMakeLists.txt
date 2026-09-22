cmake_minimum_required(VERSION 4.3)
project(Rana_Shiraz)

set(CMAKE_CXX_STANDARD 14)

include_directories(Question2/tpAppMotion)
include_directories(Question3/tpAppMotion)

add_executable(Rana_Shiraz
        Question2/tpAppMotion/tpAppMotion.cpp
        Question2/tpAppMotion/tpAppMotionCode.cpp
        Question2/tpAppMotion/tpAppMotionCode.hpp
        Question3/tpAppMotion/tpAppMotion.cpp
        Question3/tpAppMotion/tpAppMotionCode.cpp
        Question3/tpAppMotion/tpAppMotionCode.hpp)
