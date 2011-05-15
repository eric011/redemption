cmake_minimum_required (VERSION 2.6)

# CONFIGURING PATHS
set(CMAKE_INSTALL_PREFIX "/usr/local")
set(CMAKE_VERBOSE_MAKEFILE true)


project (ReDemPtion)

add_library(localobjects STATIC
    core/widget.cpp
    core/session.cpp
    core/mainloop.cpp
    core/config.cpp
    core/keymap.cpp
    core/rsa_keys.cpp
    core/ssl_calls.cpp
    core/d3des.cpp
)

add_definitions(-DFIXTURES_PATH="${CMAKE_SOURCE_DIR}/tests/fixtures" 
                -DSHARE_PATH="${CMAKE_INSTALL_PREFIX}/share/rdpproxy"
                -DCFG_PATH="${CMAKE_INSTALL_PREFIX}/etc/rdpproxy"
)

# FIND libs
find_library(SSL    ssl)
find_library(CRYPTO crypto)
find_library(DL     dl)
find_library(X11    X11)

if (SSL)
    message("-- Found openssl   (${SSL})")
endif (SSL)
if (CRYPTO)
    message("-- Found libcrypto (${CRYPTO})")
endif (CRYPTO)
if (DL)
    message("-- Found libdl     (${DL})")
endif (DL) 
if (X11)
    message("-- Found libX11     (${X11})")
endif (X11) 


# FIND boost
set(Boost_USE_STATIC_LIBS        ON)
set(Boost_USE_MULTITHREADED      ON)
set(Boost_USE_STATIC_RUNTIME    OFF)
find_package( Boost 1.46.0 COMPONENTS program_options unit_test_framework)

include_directories(
    ${Boost_INCLUDE_DIRS}
    core
    front
    capture
)

add_executable(rdpproxy
    #files
    core/main.cpp
)
target_link_libraries (rdpproxy
    #libs
    localobjects
    ${SSL}
    ${CRYPTO}
    ${DL}
    ${Boost_LIBRARIES}
)

# NOW ABOUT FUNCTIONAL TESTS ------------------------------------------------- #
add_executable(test_server_rdp
    #files
    tests/test_server_rdp_layer.cpp
)
target_link_libraries (test_server_rdp
    #libs
    localobjects
    ${SSL}
    ${CRYPTO}
    ${DL}
    ${Boost_LIBRARIES}
)

# another one
add_executable(test_bouncing_bitmap
    #files
    tests/test_server_rdp_bouncing_bitmap.cpp
)
target_link_libraries (test_bouncing_bitmap
    #libs
    localobjects
    ${SSL}
    ${CRYPTO}
    ${DL}
    ${Boost_LIBRARIES}
)

# and another one
add_executable(test_server_rdp_bitmap
    #files
    tests/test_server_rdp_bitmap.cpp
)
target_link_libraries (test_server_rdp_bitmap
    #libs
    localobjects
    ${SSL}
    ${CRYPTO}
    ${DL}
    ${Boost_LIBRARIES}
)

# and another one
add_executable        (xrdp-genkeymap utils/genkeymap/genkeymap.cpp)
target_link_libraries (xrdp-genkeymap ${X11})

# and another one
add_executable(xrdp-keygen
    #files
    utils/keygen/keygen.cpp
)
target_link_libraries (xrdp-keygen
    #libs
    localobjects
    ${SSL}
    ${CRYPTO}
    ${DL}
    ${Boost_LIBRARIES}
)

# INSTALL stuff
install (TARGETS 
            rdpproxy
            test_server_rdp
            test_bouncing_bitmap
            test_server_rdp_bitmap
            xrdp-genkeymap
            xrdp-keygen 
         DESTINATION bin)
install (DIRECTORY
            sys/etc
            sys/share
         DESTINATION .)

# FINALLY ABOUT UNIT TESTS --------------------------------------------------- #
foreach(unitname IN ITEMS
    test_widget
    test_bitmap
    test_bitmap_cache
    test_bitmap_perf
    test_stream
    test_config
    test_font
    test_rect
    test_compact_to_aligned
    test_region
    test_strings
    test_keymap
    test_rsa_keys
    test_ordersstate
    test_context_as_map
    test_dico
    test_colors)

    message(" > adding unit test ${unitname}")
    add_executable(${unitname} tests/${unitname}.cpp)
    target_link_libraries(${unitname} localobjects ${SSL} ${CRYPTO} ${DL} ${Boost_LIBRARIES})
    add_test(NAME ${unitname} COMMAND "./${unitname}")
endforeach(unitname)

enable_testing()
