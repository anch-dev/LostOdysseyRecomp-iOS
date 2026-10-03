# Recompiled game code: the XenonRecomp output in ppc/ built as one static library.
# Regenerate ppc/ with: python tools/ppc_codegen.py generate
# (ppc/ is gitignored; run tools/build_tools.bat first)
project(LostOdysseyRecompLib LANGUAGES CXX)

set(LO_PREBUILT_RECOMP_LIBRARY "" CACHE FILEPATH
    "Optional ABI-compatible recompiled PPC archive; empty builds generated sources")
set(LO_PREBUILT_RECOMP_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ppc" CACHE PATH
    "Generated PPC headers matching LO_PREBUILT_RECOMP_LIBRARY")
if(LO_PREBUILT_RECOMP_LIBRARY)
    if(NOT EXISTS "${LO_PREBUILT_RECOMP_LIBRARY}" OR IS_DIRECTORY "${LO_PREBUILT_RECOMP_LIBRARY}")
        message(FATAL_ERROR "LO_PREBUILT_RECOMP_LIBRARY must name an existing archive")
    endif()
    foreach(_header IN ITEMS ppc_config.h ppc_context.h ppc_recomp_shared.h)
        if(NOT EXISTS "${LO_PREBUILT_RECOMP_INCLUDE_DIR}/${_header}")
            message(FATAL_ERROR "Missing prebuilt PPC header: ${LO_PREBUILT_RECOMP_INCLUDE_DIR}/${_header}")
        endif()
    endforeach()
    add_library(LostOdysseyRecompLib STATIC IMPORTED GLOBAL)
    set_target_properties(LostOdysseyRecompLib PROPERTIES
        IMPORTED_LOCATION "${LO_PREBUILT_RECOMP_LIBRARY}")
    target_include_directories(LostOdysseyRecompLib INTERFACE
        "${LO_PREBUILT_RECOMP_INCLUDE_DIR}"
        "${CMAKE_SOURCE_DIR}/tools/XenonRecomp/thirdparty/simde")
    target_compile_features(LostOdysseyRecompLib INTERFACE cxx_std_20)
    message(STATUS "Using the supplied PPC archive: ${LO_PREBUILT_RECOMP_LIBRARY}")
    return()
endif()

file(GLOB LO_PPC_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/ppc/ppc_recomp.*.cpp")
if(NOT LO_PPC_SOURCES OR NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ppc/ppc_func_mapping.cpp")
    message(FATAL_ERROR "No recompiled sources in LostOdysseyRecompLib/ppc; run XenonRecomp first")
endif()
list(APPEND LO_PPC_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/ppc/ppc_func_mapping.cpp")

add_library(LostOdysseyRecompLib STATIC ${LO_PPC_SOURCES})
target_include_directories(LostOdysseyRecompLib PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/ppc"
    "${CMAKE_SOURCE_DIR}/tools/XenonRecomp/thirdparty/simde")
target_compile_features(LostOdysseyRecompLib PUBLIC cxx_std_20)

if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "The recompiled code requires clang-cl (uses __int128, weak aliases, builtins)")
endif()
# Same spirit as UnleashedRecompLib: no exceptions, no RTTI, warnings off for generated code.
if(WIN32)
    target_compile_options(LostOdysseyRecompLib PRIVATE /w /EHs-c- /GR- -fno-strict-aliasing
        # MMIO stores (eieio-prefixed) are routed to the runtime's command processor.
        "/FI${CMAKE_SOURCE_DIR}/LostOdysseyRecomp/gpu/ppc_mmio.h")
else()
    target_compile_options(LostOdysseyRecompLib PRIVATE -w -fno-exceptions -fno-rtti -fno-strict-aliasing
        -include "${CMAKE_SOURCE_DIR}/LostOdysseyRecomp/gpu/ppc_mmio.h")
endif()
