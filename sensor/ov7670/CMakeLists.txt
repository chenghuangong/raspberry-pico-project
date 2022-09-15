add_executable(ov7670 main.cpp)

target_link_libraries(ov7670 pico_stdlib hardware_i2c)

pico_add_extra_outputs(ov7670)


# enable
pico_enable_stdio_usb(ov7670 1)
# disable
pico_enable_stdio_uart(ov7670 0)