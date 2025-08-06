# find and list all sources files
aux_source_directory(. DIR_COMP_SRCS)


add_library(comp ${DIR_COMP_SRCS})


# Add the standard include files to the build
target_include_directories(comp PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(comp PUBLIC 
    pico_stdlib 
    hardware_spi 
    hardware_pio
    hardware_pwm
    hardware_dma

)

