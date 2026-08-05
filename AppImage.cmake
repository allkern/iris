function(make_appimage)
	set(optional)
	set(args EXE NAME DIR_ICON ICON OUTPUT_NAME)
	set(list_args ASSETS)
	cmake_parse_arguments(
		PARSE_ARGV 0
		ARGS
		"${optional}"
		"${args}"
		"${list_args}"
	)

	if(${ARGS_UNPARSED_ARGUMENTS})
		message(WARNING "Unparsed arguments: ${ARGS_UNPARSED_ARGUMENTS}")
	endif()


    # AppImageKit ships one tool per architecture, so pick the one that can
    # actually run on this machine.
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
        set(AIT_ARCH "aarch64")
    elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
        set(AIT_ARCH "x86_64")
    else()
        message(FATAL_ERROR "make_appimage: no appimagetool for '${CMAKE_SYSTEM_PROCESSOR}'")
    endif()

    # download AppImageTool if needed
    SET(AIT_PATH "${CMAKE_BINARY_DIR}/AppImageTool.AppImage" CACHE INTERNAL "")
    if (NOT EXISTS "${AIT_PATH}")
        file(DOWNLOAD
            "https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-${AIT_ARCH}.AppImage"
            "${AIT_PATH}"
        )
        execute_process(COMMAND chmod +x ${AIT_PATH})
    endif()

    # make the AppDir
    set(APPDIR "${CMAKE_BINARY_DIR}/AppDir")
    file(REMOVE_RECURSE "${APPDIR}")       # remove if leftover
    file(MAKE_DIRECTORY "${APPDIR}")

    # copy executable to appdir
    file(COPY "${ARGS_EXE}" DESTINATION "${APPDIR}" FOLLOW_SYMLINK_CHAIN)
    get_filename_component(EXE_NAME "${ARGS_EXE}" NAME)

    # create the script that will launch the AppImage
file(WRITE "${APPDIR}/AppRun" 
"#!/bin/sh
cd \"$(dirname \"$0\")\";
./${EXE_NAME} $@"
    )
    execute_process(COMMAND chmod +x "${APPDIR}/AppRun")
    
    # copy assets to appdir
    file(COPY ${ARGS_ASSETS} DESTINATION "${APPDIR}")

    # copy icon thumbnail
    file(COPY ${ARGS_DIR_ICON} DESTINATION "${APPDIR}")
    get_filename_component(THUMB_NAME "${ARGS_DIR_ICON}" NAME)
    file(RENAME "${APPDIR}/${THUMB_NAME}" "${APPDIR}/.DirIcon")

    # copy icon highres
    file(COPY ${ARGS_ICON} DESTINATION "${APPDIR}")
    get_filename_component(ICON_NAME "${ARGS_ICON}" NAME)
    get_filename_component(ICON_EXT "${ARGS_ICON}" EXT)
    file(RENAME "${APPDIR}/${ICON_NAME}" "${APPDIR}/${ARGS_NAME}${ICON_EXT}")

    # Create the .desktop file
    file(WRITE "${APPDIR}/${ARGS_NAME}.desktop" 
    "[Desktop Entry]
Type=Application
Name=${ARGS_NAME}
Icon=${ARGS_NAME}
Categories=X-None;"    
    )

    # Invoke AppImageTool. ARCH picks the runtime it embeds; without it the tool
    # guesses from the host and refuses to build when it guesses wrong.
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E env ARCH=${AIT_ARCH}
                ${AIT_PATH} ${APPDIR} ${ARGS_OUTPUT_NAME}
    )
    
    file(REMOVE_RECURSE "${APPDIR}")
endfunction()
