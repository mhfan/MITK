
include_directories(${US_INCLUDE_DIRS})

add_definitions(-DUS_RCC_EXECUTABLE_NAME=\"${US_RCC_EXECUTABLE_NAME}\")

set(srcs usResourceCompiler.cpp)
if(US_ENABLE_RESOURCE_COMPRESSION)
  list(APPEND srcs usResourceCompressor.c)
endif()

add_executable(${US_RCC_EXECUTABLE_NAME} ${srcs})
if(WIN32)
  target_link_libraries(${US_RCC_EXECUTABLE_NAME} Shlwapi)
endif()
