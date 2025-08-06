qt6_standard_project_setup()

add_executable(ump-keyboard
    main.cpp
    keyboard_controller.cpp
    keyboard_widget.cpp
    keyboard_widget.h
)

target_link_libraries(ump-keyboard 
    Qt6::Core
    Qt6::Widgets
    libremidi
)

target_include_directories(ump-keyboard PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)