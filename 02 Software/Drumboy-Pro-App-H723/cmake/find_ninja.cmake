# Included from the toolchain files: when Ninja is neither given
# (CMAKE_MAKE_PROGRAM) nor on PATH, use the newest copy the STM32 VS Code
# extension bundles under ~/Library/Application Support/stm32cube/bundles.

if(NOT CMAKE_MAKE_PROGRAM)
    find_program(_NINJA_ON_PATH ninja)
    if(NOT _NINJA_ON_PATH)
        file(GLOB _NINJA_BUNDLES "$ENV{HOME}/Library/Application Support/stm32cube/bundles/ninja/*/bin/ninja")
        list(SORT _NINJA_BUNDLES COMPARE NATURAL ORDER DESCENDING)
        if(_NINJA_BUNDLES)
            list(GET _NINJA_BUNDLES 0 _NINJA)
            set(CMAKE_MAKE_PROGRAM "${_NINJA}" CACHE FILEPATH "Ninja from the STM32 extension bundles")
        endif()
    endif()
    unset(_NINJA_ON_PATH CACHE)
endif()
