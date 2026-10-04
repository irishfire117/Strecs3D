# Linux specific settings for Strecs3D
# Uses system packages (e.g. Arch: vtk, opencascade, lib3mf, pugixml, nlohmann-json,
# gmsh-bin, calculix-ccx) instead of vcpkg.

# Arch's VTK config looks for MPI::MPI_C, which needs the C language enabled
enable_language(C)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
endif()

# Arch's VTK config requires these header-only build deps (fast_float, eigen, utf8cpp).
# If they aren't installed, define empty placeholder targets so VTK's targets resolve;
# Strecs3D itself doesn't use their headers.
find_package(FastFloat QUIET)
find_package(Eigen3 QUIET)
find_package(utf8cpp QUIET)
foreach(_t FastFloat::fast_float Eigen3::Eigen utf8cpp::utf8cpp)
  if(NOT TARGET ${_t})
    message(STATUS "${_t} not found; using empty placeholder target for VTK")
    add_library(${_t} INTERFACE IMPORTED GLOBAL)
  endif()
endforeach()

# The official gmsh SDK (gmsh-bin) ships no CMake config, so provide a minimal one
# that common_settings.cmake's find_package(gmsh CONFIG) can pick up.
find_package(gmsh CONFIG QUIET)
if(NOT gmsh_FOUND)
  find_path(GMSH_INCLUDE_DIR gmsh.h)
  find_library(GMSH_LIBRARY gmsh)
  if(NOT GMSH_INCLUDE_DIR OR NOT GMSH_LIBRARY)
    message(FATAL_ERROR "gmsh not found. Install the gmsh SDK (e.g. AUR gmsh-bin).")
  endif()
  set(_gmsh_cfg_dir "${CMAKE_BINARY_DIR}/gmsh-config")
  file(WRITE "${_gmsh_cfg_dir}/gmshConfig.cmake"
"if(NOT TARGET gmsh::shared)
  add_library(gmsh::shared SHARED IMPORTED)
  set_target_properties(gmsh::shared PROPERTIES
    IMPORTED_LOCATION \"${GMSH_LIBRARY}\"
    INTERFACE_INCLUDE_DIRECTORIES \"${GMSH_INCLUDE_DIR}\")
endif()
")
  set(gmsh_DIR "${_gmsh_cfg_dir}" CACHE PATH "" FORCE)
  message(STATUS "Using gmsh SDK: ${GMSH_LIBRARY}")
endif()

function(apply_linux_settings TARGET_NAME)
  # Arch's lib3mf config points at <prefix>/include/Bindings/Cpp, but the headers
  # are installed under <prefix>/include/lib3mf/Bindings/Cpp
  get_target_property(_lib3mf_inc lib3mf::lib3mf INTERFACE_INCLUDE_DIRECTORIES)
  if(_lib3mf_inc AND NOT EXISTS "${_lib3mf_inc}")
    string(REPLACE "/include/Bindings/" "/include/lib3mf/Bindings/" _lib3mf_inc "${_lib3mf_inc}")
    set_target_properties(lib3mf::lib3mf PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${_lib3mf_inc}")
  endif()

  target_include_directories(${TARGET_NAME} PRIVATE ${OpenCASCADE_INCLUDE_DIR})

  target_link_libraries(${TARGET_NAME} PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    ${VTK_LIBRARIES}
    lib3mf::lib3mf
    libzip::zip
    pugixml::pugixml
    $<IF:$<TARGET_EXISTS:gmsh::shared>,gmsh::shared,gmsh::lib>
    ${OpenCASCADE_LIBRARIES}
    nlohmann_json::nlohmann_json
  )

  target_sources(${TARGET_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/UI/platform/linux/WindowUtils.cpp")

  # Arch's VTK uses the oneTBB SMP backend, whose headers declare functions named
  # emit() that clash with Qt's emit macro. Pre-include them before any Qt header.
  if(EXISTS "/usr/include/oneapi/tbb/profiling.h")
    target_compile_options(${TARGET_NAME} PRIVATE
      "SHELL:-include oneapi/tbb/profiling.h"
      "SHELL:-include oneapi/tbb/detail/_pipeline_filters.h")
  endif()
endfunction()
