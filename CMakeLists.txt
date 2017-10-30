cmake_minimum_required(VERSION 3.5)
project( picam )
SET(COMPILE_DEFINITIONS -Werror)

MESSAGE( STATUS CC_SYSROOT: ${CC_SYSROOT})
MESSAGE( STATUS CC_SYSROOT_LIB: ${CC_SYSROOT_LIB})
set(CMAKE_CROSSCOMPILE OFF CACHE BOOL "is crosscompiled")
message(STATUS "CMAKE_CROSSCOMPILE ${CMAKE_CROSSCOMPILE}")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11")


include_directories(${CC_SYSROOT}/usr/local/include)

file(GLOB PICAM_SRC "picam/*.cpp" "picam/*.c")
#Create App
add_executable(picam_demo main.cpp gl_scene.cpp ${PICAM_SRC})

find_library(STD_LIB stdc++ PATHS /usr/lib/gcc/arm-linux-gnueabihf/6)
target_link_libraries(picam_demo ${STD_LIB}) #Needed for crosscompile

#USERLAND
include_directories(${CC_SYSROOT}/opt/vc/include)
include_directories(${CC_SYSROOT}/opt/vc/userland-master/interface/vcos)
include_directories(${CC_SYSROOT}/opt/vc/userland-master)
include_directories(${CC_SYSROOT}/opt/vc/userland-master/interface/vcos/pthreads)
include_directories(${CC_SYSROOT}/opt/vc/userland-master/interface/vmcs_host/linux)
include_directories(${CC_SYSROOT}/opt/vc/userland-master/host_applications/linux/libs/bcm_host/include)

find_library(MMAL_CORE_LIB mmal_core PATHS /opt/vc/lib ~/userland-master/build/lib ${CC_SYSROOT}/opt/vc/lib)
find_library(MMAL_UTIL_LIB mmal_util PATHS /opt/vc/lib ~/userland-master/build/lib ${CC_SYSROOT}/opt/vc/lib)
find_library(MMAL_VC_CLIENT_LIB mmal_vc_client PATHS /opt/vc/lib ~/userland-master/build/lib ${CC_SYSROOT}/opt/vc/lib)
find_library(VCOS_LIB vcos PATHS /opt/vc/lib ~/userland-master/build/lib ${CC_SYSROOT}/opt/vc/lib)
find_library(BCM_HOST_LIB bcm_host PATHS /opt/vc/lib ~/userland-master/build/lib ${CC_SYSROOT}/opt/vc/lib)
find_library(GLESv2_LIB GLESv2 PATHS /opt/vc/lib ~/userland-master/build/lib ${CC_SYSROOT}/opt/vc/lib)
find_library(EGL_LIB EGL PATHS /opt/vc/lib ~/userland-master/build/lib ${CC_SYSROOT}/opt/vc/lib)
MESSAGE( STATUS "USERLAND: " ${MMAL_CORE_LIB} ${MMAL_UTIL_LIB} ${MMAL_VC_CLIENT_LIB} ${VCOS_LIB} ${BCM_HOST_LIB} ${GLESv2_LIB} ${EGL_LIB}  ${PICAM_LIB} )
target_link_libraries(picam_demo dl pthread m ${PICAM_LIB} ${MMAL_CORE_LIB} ${MMAL_UTIL_LIB} ${MMAL_VC_CLIENT_LIB} ${VCOS_LIB} ${BCM_HOST_LIB} ${GLESv2_LIB} ${EGL_LIB})

