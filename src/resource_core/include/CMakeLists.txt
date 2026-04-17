add_library(resource_core STATIC
    resource_error.cpp
    file_handle.cpp
    resource_manager.cpp
)

target_include_directories(resource_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)