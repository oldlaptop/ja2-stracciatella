include(ExternalProject)

externalproject_add(libstracciatella
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/rust"
    INSTALL_DIR "${CMAKE_BINARY_DIR}/lib-stracciatella"
    BUILD_ALWAYS ON
    CMAKE_ARGS
        ${EXTERNAL_PROJECT_CMAKE_ARGS}
        -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/lib-stracciatella
        -DLIBSTRACCIATELLA_TARGET=${LIBSTRACCIATELLA_TARGET}
)

set(STRACCIATELLA_INCLUDE_DIR
    "${CMAKE_BINARY_DIR}/lib-stracciatella/include"
)
set(STRACCIATELLA_LIBRARIES
    "${CMAKE_BINARY_DIR}/lib-stracciatella/lib/${CMAKE_STATIC_LIBRARY_PREFIX}stracciatella${CMAKE_STATIC_LIBRARY_SUFFIX}"
)
