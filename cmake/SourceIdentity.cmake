# Buildfix8: hash source CONTENT, not archive/extraction timestamps.
# Shared by configure and package-time validation. No network or compiler here.
include_guard(GLOBAL)
function(fc_source_identity root out_id out_sha out_files)
    set(_fixed CMakeLists.txt LATEST_BUILD_ID.txt vcpkg.json cmake/SourceIdentity.cmake package/FreedomControlRuntime.esp package/SKSE/Plugins/FreedomControl.creatures.json)
    foreach(_required IN LISTS _fixed)
        if(NOT EXISTS "${root}/${_required}" OR IS_DIRECTORY "${root}/${_required}")
            message(FATAL_ERROR "Incomplete source tree: ${root}/${_required}")
        endif()
    endforeach()
    file(READ "${root}/LATEST_BUILD_ID.txt" _id LIMIT 256)
    string(STRIP "${_id}" _id)
    if(NOT _id MATCHES "^FC-[A-Za-z0-9.-]+$")
        message(FATAL_ERROR "Invalid LATEST_BUILD_ID.txt. Expected one ASCII build identifier.")
    endif()
    # GLOB is used for hashing only; CMakeLists still explicitly lists compiled sources.
    # Escape literal '[' in a directory name for CMake glob syntax.
    string(REPLACE "[" "[[]" _glob "${root}")
    file(GLOB_RECURSE _code LIST_DIRECTORIES FALSE RELATIVE "${root}"
        "${_glob}/src/*.cpp" "${_glob}/src/*.h" "${_glob}/src/*.hpp"
        "${_glob}/include/*.h" "${_glob}/include/*.hpp")
    foreach(_required IN ITEMS src/VMArgumentsContract18.cpp include/fc/VMArguments18.hpp src/ApiContract17.cpp include/fc/Win32MacroCleanup17.hpp src/BuildIdentity.cpp src/BuildIdentity.hpp
            src/Plugin.cpp src/Engine.cpp src/SexLab16.cpp src/Overlay.cpp src/Core.cpp
            src/PlayerCustomization.cpp include/fc/PlayerCustomization.hpp
            src/Input13.cpp src/Input13.hpp src/GameInput13.cpp include/fc/InputPolicy13.hpp
            src/Kernel11.cpp src/PlayerFreedom14.cpp include/fc/PlayerFreedomPolicy14.hpp include/fc/CreatureFollowPolicy.hpp src/QuestCenter11.cpp src/KernelSave11.cpp src/CrimeHooks11.cpp src/CrimeHooks11.hpp include/fc/KernelPolicy.hpp
            src/PCH.h src/Engine.hpp src/Overlay.hpp src/Follower10.cpp src/Spawner10.cpp src/WorldFreedom10.cpp src/Runtime10.hpp include/fc/FreedomPolicy.hpp
            include/fc/Core.hpp include/fc/RuntimePolicy.hpp include/fc/HotkeyState.hpp)
        if(NOT EXISTS "${root}/${_required}" OR IS_DIRECTORY "${root}/${_required}")
            message(FATAL_ERROR "Incomplete source tree: ${root}/${_required}")
        endif()
    endforeach()
    set(_inputs ${_fixed} ${_code})
    list(REMOVE_DUPLICATES _inputs)
    list(SORT _inputs)
    set(_manifest "FreedomControl-source-sha256-v1\n")
    set(_paths "")
    foreach(_name IN LISTS _inputs)
        file(SHA256 "${root}/${_name}" _hash)
        string(APPEND _manifest "${_name}=${_hash}\n")
        list(APPEND _paths "${root}/${_name}")
    endforeach()
    string(SHA256 _sha "${_manifest}")
    set(${out_id} "${_id}" PARENT_SCOPE)
    set(${out_sha} "${_sha}" PARENT_SCOPE)
    set(${out_files} "${_paths}" PARENT_SCOPE)
endfunction()

function(fc_apply_build_identity target root)
    fc_source_identity("${root}" _id _sha _inputs)
    # Applies to ALL plugin translation units, not CommonLib or other dependencies.
    # Changed contents => changed compiler command => rebuild even with old ZIP mtimes.
    target_compile_definitions(${target} PRIVATE
        "FC_BUILD_ID=\"${_id}\"" "FC_SOURCE_SHA256=\"${_sha}\"")
    target_sources(${target} PRIVATE "${root}/src/BuildIdentity.cpp")
    set_source_files_properties("${root}/src/BuildIdentity.cpp" PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${_inputs})
    message(STATUS "FreedomControl build ID: ${_id}")
    message(STATUS "FreedomControl source SHA256: ${_sha}")
endfunction()
