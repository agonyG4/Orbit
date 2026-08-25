include_guard(GLOBAL)

function(orbit_add_rust_package)
    set(options)
    set(one_value_args TARGET PACKAGE MANIFEST_DIR ARTIFACT)
    set(multi_value_args)
    cmake_parse_arguments(ORBIT "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

    foreach(required_arg TARGET PACKAGE MANIFEST_DIR ARTIFACT)
        if (NOT ORBIT_${required_arg})
            message(FATAL_ERROR "orbit_add_rust_package requires ${required_arg}")
        endif()
    endforeach()

    find_program(ORBIT_CARGO_EXECUTABLE cargo REQUIRED)

    if (CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        set(orbit_rust_profile_arg --release)
        set(orbit_rust_profile_dir release)
    else()
        set(orbit_rust_profile_arg)
        set(orbit_rust_profile_dir debug)
    endif()

    set(orbit_rust_target_dir "${CMAKE_CURRENT_BINARY_DIR}/cargo-target")
    file(GLOB_RECURSE orbit_rust_sources CONFIGURE_DEPENDS
        "${ORBIT_MANIFEST_DIR}/*.rs"
        "${ORBIT_MANIFEST_DIR}/Cargo.toml")

    add_custom_command(
        OUTPUT "${ORBIT_ARTIFACT}"
        COMMAND "${CMAKE_COMMAND}" -E env
            "CARGO_TARGET_DIR=${orbit_rust_target_dir}"
            "${ORBIT_CARGO_EXECUTABLE}" build
            --workspace
            --locked
            --package "${ORBIT_PACKAGE}"
            ${orbit_rust_profile_arg}
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        DEPENDS "${CMAKE_SOURCE_DIR}/Cargo.toml" "${CMAKE_SOURCE_DIR}/Cargo.lock" ${orbit_rust_sources}
        COMMENT "Building ${ORBIT_PACKAGE}"
        VERBATIM)

    add_custom_target("${ORBIT_TARGET}" DEPENDS "${ORBIT_ARTIFACT}")
endfunction()
