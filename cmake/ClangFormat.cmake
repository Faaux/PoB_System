function(format_pre_built target)
    # Find clang-format
    if (NOT CLANG_FORMAT_EXE)
        set(CLANG_FORMAT_EXE clang-format)
    endif()
    mark_as_advanced(CLANG_FORMAT_EXE)

    if (NOT EXISTS CLANG_FORMAT_EXE)
        find_program(CLANG_FORMAT_EXE_SEARCH NAMES ${CLANG_FORMAT_EXE})
        if(CLANG_FORMAT_EXE_SEARCH)
            set(CLANG_FORMAT_EXE ${CLANG_FORMAT_EXE_SEARCH})
            unset(${CLANG_FORMAT_EXE_SEARCH})
        else()
            message(WARNING "Could not find clang-format, not formatting on build")
            return()
        endif()

        # We found clang-format :)
        
        get_target_property(target_sources ${target} SOURCES)
        get_target_property(target_name ${target} NAME)

        # Absolute paths
        foreach(source ${target_sources})
            get_filename_component(source ${source} ABSOLUTE)
            list(APPEND abs_sources ${source})
        endforeach()

        add_custom_target(
            "${target_name}_format"
            COMMAND ${CLANG_FORMAT_EXE} -style=file -i ${abs_sources}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Formatting ${target_name}"
        )
        add_dependencies(${target} "${target_name}_format")

    endif()
endfunction()
