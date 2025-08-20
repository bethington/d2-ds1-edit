# Install script for directory: C:/Users/benam/source/cpp/win_ds1edit_20250818/allegro-4.4.3.1/addons/allegrogl

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/Users/benam/source/cpp/win_ds1edit_20250818/allegro-install")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "RelWithDebInfo")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY OPTIONAL FILES "C:/Users/benam/source/cpp/win_ds1edit_20250818/allegro-build/lib/RelWithDebInfo/alleggl.lib")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE SHARED_LIBRARY FILES "C:/Users/benam/source/cpp/win_ds1edit_20250818/allegro-build/lib/RelWithDebInfo/alleggl.dll")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include" TYPE FILE FILES "C:/Users/benam/source/cpp/win_ds1edit_20250818/allegro-4.4.3.1/addons/allegrogl/include/alleggl.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/allegrogl" TYPE FILE FILES "C:/Users/benam/source/cpp/win_ds1edit_20250818/allegro-4.4.3.1/addons/allegrogl/include/allegrogl/gl_ext.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/allegrogl" TYPE FILE FILES "C:/Users/benam/source/cpp/win_ds1edit_20250818/allegro-4.4.3.1/addons/allegrogl/include/allegrogl/gl_header_detect.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/allegrogl" TYPE FILE FILES "C:/Users/benam/source/cpp/win_ds1edit_20250818/allegro-build/addons/allegrogl/include/allegrogl/alleggl_config.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/allegrogl/GLext" TYPE FILE FILES "C:/Users/benam/source/cpp/win_ds1edit_20250818/allegro-4.4.3.1/addons/allegrogl/include/allegrogl/GLext/glx_ext_alias.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/allegrogl/GLext" TYPE FILE FILES "C:/Users/benam/source/cpp/win_ds1edit_20250818/allegro-4.4.3.1/addons/allegrogl/include/allegrogl/GLext/glx_ext_api.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/allegrogl/GLext" TYPE FILE FILES "C:/Users/benam/source/cpp/win_ds1edit_20250818/allegro-4.4.3.1/addons/allegrogl/include/allegrogl/GLext/glx_ext_defs.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/allegrogl/GLext" TYPE FILE FILES "C:/Users/benam/source/cpp/win_ds1edit_20250818/allegro-4.4.3.1/addons/allegrogl/include/allegrogl/GLext/glx_ext_list.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/allegrogl/GLext" TYPE FILE FILES "C:/Users/benam/source/cpp/win_ds1edit_20250818/allegro-4.4.3.1/addons/allegrogl/include/allegrogl/GLext/gl_ext_alias.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/allegrogl/GLext" TYPE FILE FILES "C:/Users/benam/source/cpp/win_ds1edit_20250818/allegro-4.4.3.1/addons/allegrogl/include/allegrogl/GLext/gl_ext_api.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/allegrogl/GLext" TYPE FILE FILES "C:/Users/benam/source/cpp/win_ds1edit_20250818/allegro-4.4.3.1/addons/allegrogl/include/allegrogl/GLext/gl_ext_defs.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/allegrogl/GLext" TYPE FILE FILES "C:/Users/benam/source/cpp/win_ds1edit_20250818/allegro-4.4.3.1/addons/allegrogl/include/allegrogl/GLext/gl_ext_list.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/allegrogl/GLext" TYPE FILE FILES "C:/Users/benam/source/cpp/win_ds1edit_20250818/allegro-4.4.3.1/addons/allegrogl/include/allegrogl/GLext/wgl_ext_alias.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/allegrogl/GLext" TYPE FILE FILES "C:/Users/benam/source/cpp/win_ds1edit_20250818/allegro-4.4.3.1/addons/allegrogl/include/allegrogl/GLext/wgl_ext_api.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/allegrogl/GLext" TYPE FILE FILES "C:/Users/benam/source/cpp/win_ds1edit_20250818/allegro-4.4.3.1/addons/allegrogl/include/allegrogl/GLext/wgl_ext_defs.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/allegrogl/GLext" TYPE FILE FILES "C:/Users/benam/source/cpp/win_ds1edit_20250818/allegro-4.4.3.1/addons/allegrogl/include/allegrogl/GLext/wgl_ext_list.h")
endif()

