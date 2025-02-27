cmake_minimum_required(VERSION 3.0.2)
project(monitor_log)

## Compile as C++11, supported in ROS Kinetic and newer
# add_compile_options(-std=c++11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
## Find catkin macros and libraries
## if COMPONENTS list like find_package(catkin REQUIRED COMPONENTS xyz)
## is used, also find other catkin packages
# 自动处理 Qt 的 moc
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
find_package(catkin REQUIRED COMPONENTS
  eigen_conversions
  nav_msgs
  roscpp
  rospy
  sensor_msgs
  std_msgs
  rosbag
  private_msgs
)
find_package(gflags REQUIRED)
find_package(glog REQUIRED)
find_package(Qt5 COMPONENTS Core Widgets REQUIRED)
catkin_package(
   INCLUDE_DIRS include
#  LIBRARIES monitor_log
#  CATKIN_DEPENDS eigen_conversions nav_msgs roscpp rospy sensor_msgs std_msgs
#  DEPENDS system_lib
)

###########
## Build ##
###########

## Specify additional locations of header files
## Your package locations should be listed before other locations
include_directories(
  include
  ${catkin_INCLUDE_DIRS}
  ${GFLAGS_INCLUDE_DIRS}
  ${GLOG_INCLUDE_DIRS}
)
# 源文件
set(SOURCES
    src/main.cpp
    src/LogFinder.cpp
    src/mainwindows.cpp
)

# 头文件
set(HEADERS
    include/LogFinder.h
    include/mainwindows.h
)
add_executable(monitor_log src/monitor_log.cpp)
target_link_libraries(monitor_log 
                    ${catkin_LIBRARIES}
                    ${GFLAGS_LIBRARIES}
                    -lglog)

add_executable(log_ui ${SOURCES} ${HEADERS})
target_link_libraries(log_ui Qt5::Core Qt5::Widgets stdc++fs)

install(TARGETS ${PROJECT_NAME}
  ARCHIVE DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  RUNTIME DESTINATION ${CATKIN_GLOBAL_BIN_DESTINATION}
)
## Mark cpp header files for installation
# install(DIRECTORY include/${PROJECT_NAME}/
#   DESTINATION ${CATKIN_PACKAGE_INCLUDE_DESTINATION}
#   FILES_MATCHING PATTERN "*.h"
#   PATTERN ".svn" EXCLUDE
# )

install (FILES 
    launch/run.launch
    DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}/launch
)

install(PROGRAMS
        script/clear_log.sh
        script/findlog.sh
        DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
        )

