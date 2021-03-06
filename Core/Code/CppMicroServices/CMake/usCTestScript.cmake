
macro(build_and_test)

  set(CTEST_SOURCE_DIRECTORY ${US_SOURCE_DIR})
  set(CTEST_BINARY_DIRECTORY "${CTEST_DASHBOARD_ROOT}/${CTEST_PROJECT_NAME}_${CTEST_DASHBOARD_NAME}")

  #if(NOT CTEST_BUILD_NAME)
  #  set(CTEST_BUILD_NAME "${CMAKE_SYSTEM}_${CTEST_COMPILER}_${CTEST_DASHBOARD_NAME}")
  #endif()

  ctest_empty_binary_directory(${CTEST_BINARY_DIRECTORY})

  ctest_start("Experimental")

  if(NOT EXISTS "${CTEST_BINARY_DIRECTORY}/CMakeCache.txt")
    file(WRITE "${CTEST_BINARY_DIRECTORY}/CMakeCache.txt" "${CTEST_INITIAL_CACHE}")
  endif()

  ctest_configure(RETURN_VALUE res)
  if (res)
    message(FATAL_ERROR "CMake configure error")
  endif()
  ctest_build(RETURN_VALUE res)
  if (res)
    message(FATAL_ERROR "CMake build error")
  endif()

  ctest_test(RETURN_VALUE res PARALLEL_LEVEL ${CTEST_PARALLEL_LEVEL})
  if (res)
   message(FATAL_ERROR "CMake test error")
  endif()


  if(WITH_MEMCHECK AND CTEST_MEMORYCHECK_COMMAND)
    ctest_memcheck()
  endif()

  if(WITH_COVERAGE AND CTEST_COVERAGE_COMMAND)
    ctest_coverage()
  endif()

  #ctest_submit()

endmacro()

function(create_initial_cache var _shared _threading _sf _cxx11 _autoload)

  set(_initial_cache "
      US_BUILD_TESTING:BOOL=ON
      US_BUILD_SHARED_LIBS:BOOL=${_shared}
      US_ENABLE_THREADING_SUPPORT:BOOL=${_threading}
      US_ENABLE_SERVICE_FACTORY_SUPPORT:BOOL=${_sf}
      US_USE_C++11:BOOL=${_cxx11}
      US_ENABLE_AUTOLOADING_SUPPORT:BOOL=${_autoload}
      ")

  set(${var} ${_initial_cache} PARENT_SCOPE)

  if(_shared)
    set(CTEST_DASHBOARD_NAME "shared")
  else()
    set(CTEST_DASHBOARD_NAME "static")
  endif()

  if(_threading)
    set(CTEST_DASHBOARD_NAME "${CTEST_DASHBOARD_NAME}-threading")
  endif()
  if(_sf)
    set(CTEST_DASHBOARD_NAME "${CTEST_DASHBOARD_NAME}-servicefactory")
  endif()
  if(_cxx11)
    set(CTEST_DASHBOARD_NAME "${CTEST_DASHBOARD_NAME}-cxx11")
  endif()
  if(_autoload)
    set(CTEST_DASHBOARD_NAME "${CTEST_DASHBOARD_NAME}-autoloading")
  endif()

  set(CTEST_DASHBOARD_NAME ${CTEST_DASHBOARD_NAME} PARENT_SCOPE)

endfunction()

#=========================================================

set(CTEST_PROJECT_NAME CppMicroServices)

if(NOT CTEST_PARALLEL_LEVEL)
  set(CTEST_PARALLEL_LEVEL 1)
endif()


#            SHARED THREADING SERVICE_FACTORY C++11 AUTOLOAD

set(config0     0       0            0          0      0     )
set(config1     0       0            0          0      1     )
set(config2     0       0            0          1      0     )
set(config3     0       0            0          1      1     )
set(config4     0       0            1          0      0     )
set(config5     0       0            1          0      1     )
set(config6     0       0            1          1      0     )
set(config7     0       0            1          1      1     )
set(config8     0       1            0          0      0     )
set(config9     0       1            0          0      1     )
set(config10    0       1            0          1      0     )
set(config11    0       1            0          1      1     )
set(config12    0       1            1          0      0     )
set(config13    0       1            1          0      1     )
set(config14    0       1            1          1      0     )
set(config15    0       1            1          1      1     )
set(config16    1       0            0          0      0     )
set(config17    1       0            0          0      1     )
set(config18    1       0            0          1      0     )
set(config19    1       0            0          1      1     )
set(config20    1       0            1          0      0     )
set(config21    1       0            1          0      1     )
set(config22    1       0            1          1      0     )
set(config23    1       0            1          1      1     )
set(config24    1       1            0          0      0     )
set(config25    1       1            0          0      1     )
set(config26    1       1            0          1      0     )
set(config27    1       1            0          1      1     )
set(config28    1       1            1          0      0     )
set(config29    1       1            1          0      1     )
set(config30    1       1            1          1      0     )
set(config31    1       1            1          1      1     )

foreach(i ${US_BUILD_CONFIGURATION})
  create_initial_cache(CTEST_INITIAL_CACHE ${config${i}})
  message("Testing build configuration: ${CTEST_DASHBOARD_NAME}")
  build_and_test()
endforeach()

