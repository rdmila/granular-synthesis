cmake_minimum_required(VERSION 4.3)
project(synth C CXX)

include(FetchContent)

set(CMAKE_CXX_STANDARD_REQUIRED true)
set(CMAKE_CXX_STANDARD 20)

add_executable(gui main.cpp spectrogram.cpp)
# Sliding DFT

#block()
#set(CMAKE_POLICY_VERSION_MINIMUM 4.3)
  FetchContent_Declare(
    SlidingDFT
    URL https://raw.githubusercontent.com/bronsonp/SlidingDFT/refs/heads/master/sliding_dft.hpp
    URL_HASH SHA256=caa63faa3dd4e83abf8a04664d60bb540fbd68ad3d720334226f1408e13cb132
    DOWNLOAD_NO_EXTRACT true
  )
  FetchContent_MakeAvailable(SlidingDFT)
  #unset(CMAKE_POLICY_VERSION_MINIMUM)

  add_library(SlidingDFT INTERFACE ${slidingdft_SOURCE_DIR}/sliding_dft.hpp)
  target_include_directories(SlidingDFT INTERFACE ${slidingdft_SOURCE_DIR})
  #endblock()
# MiniAudio

FetchContent_Declare(
  MiniAudioHeader 
  URL https://raw.githubusercontent.com/mackron/miniaudio/refs/heads/master/miniaudio.h
  URL_HASH SHA256=ac7af4de748b7e26b777f37e01cee313a308a7296a3eb080e2906b320cc55c89
  DOWNLOAD_NO_EXTRACT true
)
FetchContent_Declare(
  MiniAudioSrc 
  URL https://raw.githubusercontent.com/mackron/miniaudio/refs/heads/master/miniaudio.c 
  URL_HASH SHA256=ab1984bb9804ffd7b0303813595d0b345a8a86c34da1daffc353a14b34102a65
  DOWNLOAD_NO_EXTRACT true
)
FetchContent_MakeAvailable(MiniAudioHeader MiniAudioSrc)

add_library(MiniAudio ${miniaudiosrc_SOURCE_DIR}/miniaudio.c)
target_include_directories(MiniAudio PUBLIC ${miniaudioheader_SOURCE_DIR})

# set(audio_libraries "MiniAudio SlidingDFT")

find_package(OpenGL REQUIRED)
#target_link_libraries(gui OpenGL::GL)

find_package(glfw3 3.4 REQUIRED)
#target_link_libraries(gui glfw)

FetchContent_Declare(
  ImGui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git 
  GIT_TAG v1.92.7
)
# set(FETCHCONTENT_SOURCE_DIR_IMGUI "${CMAKE_SOURCE_DIR}/imgui")
FetchContent_MakeAvailable(ImGui)

add_library(DearImGui
  ${imgui_SOURCE_DIR}/imgui.cpp ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp)

target_include_directories(DearImGui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)

target_link_libraries(DearImGui PRIVATE OpenGL::GL)
target_link_libraries(DearImGui PRIVATE glfw)

target_link_libraries(gui PRIVATE SlidingDFT MiniAudio DearImGui OpenGL::GL)

set(sanitize_options "-fsanitize=undefined")

target_compile_options(gui PRIVATE ${sanitize_options})
target_link_options(gui PRIVATE ${sanitize_options})

