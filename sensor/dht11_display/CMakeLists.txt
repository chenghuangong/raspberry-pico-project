add_executable(dht11_display main.cpp dht11.cpp oled_disp.cpp)

target_link_libraries(dht11_display pico_stdlib hardware_i2c)

pico_add_extra_outputs(dht11_display)


# enable
pico_enable_stdio_usb(dht11_display 1)
# disable
pico_enable_stdio_uart(dht11_display 0)
