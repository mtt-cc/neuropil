# Copyright (C) 2015-2016 Franklin "Snaipe" Mathieu.
# Redistribution and use of this file is allowed according to the terms of the MIT license.
# For details see the LICENSE file distributed with Criterion.

set (PROJECT_VERSION "2.2.0")
set (PROJECT_SOVERSION 3)
set (LOCALEDIR_REL "share/locale")
set (LOCALEDIR "${CMAKE_INSTALL_PREFIX}/${LOCALEDIR_REL}")
string (TOLOWER "${PROJECT_NAME}" PROJECT_NAME_LOWER)

add_definitions(
  -DCRITERION_BUILDING_DLL=1
  -DPB_ENABLE_MALLOC=1
)

set (CMAKE_C_FLAGS_DEFAULT "${CMAKE_C_FLAGS}")
set (CMAKE_CXX_FLAGS_DEFAULT "${CMAKE_CXX_FLAGS}")

if (MSVC)
  set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /SAFESEH:NO")
  add_definitions (-D_CRT_SECURE_NO_WARNINGS=1)
else ()
  if (WIN32)
    set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-no-undefined")
  endif ()

  set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror -g -std=gnu99")
  if (CMAKE_CXX_COMPILER_WORKS)
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -g ${CXX11_FLAG}")
  endif ()
endif ()

include_directories(SYSTEM
  /usr/local/include
  /usr/include/GNUstep
)
