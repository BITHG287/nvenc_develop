cmake_minimum_required(VERSION 3.0.0)
project(HEVC VERSION 1.0.0)

set(CMAKE_BUILD_TYPE "Release")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -fPIC")
set(target_name main.out)

# ------ ------ ------ x265 cpu ------ ------ ------ 
# include_directories(/usr/local/ffmpeg_x265_cpu/include)
# include_directories(/usr/local/x265_10bit/include)
# link_directories(/usr/local/ffmpeg_x265_cpu/lib)
# link_directories(/usr/local/x265_10bit/lib)

# include_directories(/usr/local/include/ffnvcodec)

include_directories(./)
include_directories(/usr/local/ffmpeg/include)
link_directories(/usr/local/ffmpeg/lib)
# ------ ------ ------ hevc gpu ------ ------ ------ 
# include_directories(/home/hugui/mnt/work/tools/ffmpeg_gpu/ffmpeg-4.4-hevc-gpu)
# link_directories(/home/hugui/mnt/work/tools/ffmpeg_gpu/ffmpeg-4.4-hevc-gpu/libavcodec)
# link_directories(/home/hugui/mnt/work/tools/ffmpeg_gpu/ffmpeg-4.4-hevc-gpu/libavdevice)
# link_directories(/home/hugui/mnt/work/tools/ffmpeg_gpu/ffmpeg-4.4-hevc-gpu/libavfilter)
# link_directories(/home/hugui/mnt/work/tools/ffmpeg_gpu/ffmpeg-4.4-hevc-gpu/libavformat)
# link_directories(/home/hugui/mnt/work/tools/ffmpeg_gpu/ffmpeg-4.4-hevc-gpu/libavresample)
# link_directories(/home/hugui/mnt/work/tools/ffmpeg_gpu/ffmpeg-4.4-hevc-gpu/libavutil)
# link_directories(/home/hugui/mnt/work/tools/ffmpeg_gpu/ffmpeg-4.4-hevc-gpu/libpostproc)
# link_directories(/home/hugui/mnt/work/tools/ffmpeg_gpu/ffmpeg-4.4-hevc-gpu/libswresample)
# link_directories(/home/hugui/mnt/work/tools/ffmpeg_gpu/ffmpeg-4.4-hevc-gpu/libswscale)

# include_directories()
link_directories(/usr/lib/x86_64-linux-gnu/)

set(DEP_LIBS
avcodec
avdevice
avfilter
avformat
avutil
postproc
swresample
swscale
jsoncpp
)

add_library(nvencoder_static STATIC ./nv_encoder.h ./nv_encoder.cpp)
add_library(nvencoder SHARED ./nv_encoder.h ./nv_encoder.cpp)

add_executable(${target_name} ./main.cpp ./nv_encoder.cpp)
target_link_libraries(${target_name} ${DEP_LIBS})

set(CMAKE_INSTALL_PREFIX ../)
install(FILES ./.build/${target_name} DESTINATION output)
install(FILES ./.build/libnvencoder.so DESTINATION output)
install(FILES ./.build/libnvencoder_static.a DESTINATION output)
