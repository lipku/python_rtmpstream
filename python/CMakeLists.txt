cmake_minimum_required(VERSION 3.6.0 FATAL_ERROR)

project(rtmp_streaming VERSION 1.0.0 DESCRIPTION "rtmp streaming based on ffmpeg")


set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -std=c++14 -Wall")

set ( CMAKE_EXE_LINKER_FLAGS_DEBUG  "${CMAKE_EXE_LINKER_FLAGS_DEBUG}  ")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}  -std=c++14 -Wall -O0 -fno-omit-frame-pointer")

#find_package(PythonInterp 3.6 REQUIRED)
#find_package(PythonLibs 3.6 REQUIRED)

#set(Python_ROOT_DIR "")


#set(BOOST_PYTHONLIB python3)

#find_package(Boost COMPONENTS ${BOOST_PYTHONLIB})

#include(CMakeToolsHelpers OPTIONAL)


#include_directories(${PYTHON_INCLUDE_DIRS})


#include_directories(${Boost_INCLUDE_DIR} ${PYTHON_INCLUDE} "../streamer")
#link_directories(${Boost_LIBRARY_DIR})

include_directories("../streamer")

#find_package(Python 3.7 COMPONENTS Interpreter Development REQUIRED)
if(WIN32)
#windwos,use conda install conda-forge::ffmpeg
set(PYTHON_EXECUTABLE D:\\anaconda3\\envs\\nerfstream\\python.exe)
include_directories("D:\\anaconda3\\envs\\nerfstream\\include")
include_directories("D:\\anaconda3\\pkgs\\ffmpeg-6.1.1-gpl_hb766fab_106\\Library\\include")
set(AVCODEC_LIBRARY "D:\\anaconda3\\pkgs\\ffmpeg-6.1.1-gpl_hb766fab_106\\Library\\lib\\avcodec.lib")
set(AVFORMAT_LIBRARY "D:\\anaconda3\\pkgs\\ffmpeg-6.1.1-gpl_hb766fab_106\\Library\\lib\\avformat.lib")
set(AVUTIL_LIBRARY "D:\\anaconda3\\pkgs\\ffmpeg-6.1.1-gpl_hb766fab_106\\Library\\lib\\avutil.lib")
set(SWSCALE_LIBRARY "D:\\anaconda3\\pkgs\\ffmpeg-6.1.1-gpl_hb766fab_106\\Library\\lib\\swscale.lib")
else()
set(PYTHON_EXECUTABLE /opt/anaconda3/envs/python37/bin/python)
include_directories("/opt/anaconda3/envs/python37/include")
find_library(AVCODEC_LIBRARY avcodec /opt/anaconda3/envs/python37/lib)
find_library(AVFORMAT_LIBRARY avformat /opt/anaconda3/envs/python37/lib)
find_library(AVUTIL_LIBRARY avutil /opt/anaconda3/envs/python37/lib)
find_library(SWSCALE_LIBRARY swscale /opt/anaconda3/envs/python37/lib)
endif()

#add_library(rtmp_streaming SHARED
#    rtmp_streaming.cpp ../streamer/streamer.cpp
#)



#SET_TARGET_PROPERTIES(rtmp_streaming PROPERTIES PREFIX "")

#set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR})

#target_link_libraries(rtmp_streaming  ${AVCODEC_LIBRARY} ${AVFORMAT_LIBRARY} ${AVUTIL_LIBRARY} ${SWSCALE_LIBRARY} ${Boost_LIBRARIES}  boost_numpy3 /usr/lib/x86_64-linux-gnu/libpython3.6m.so)

add_subdirectory(pybind11)
pybind11_add_module(rtmp_streaming rtmp_streaming.cpp ../streamer/streamer.cpp) 
target_link_libraries(rtmp_streaming  PRIVATE ${AVCODEC_LIBRARY} ${AVFORMAT_LIBRARY} ${AVUTIL_LIBRARY} ${SWSCALE_LIBRARY})
