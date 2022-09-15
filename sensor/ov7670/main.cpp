/*
read image from ov7670 directly by pico gpio and convert to greyscale ascii image,
send to PC through pico COM port.
resolution: 60x80
color format: YUV422

[wiring]
ov7076: SDA -> pico GPIO4
ov7076: SCL -> pico GPIO5
ov7076: VSYNC -> pico GPIO6
ov7076: HREF -> pico GPIO7
ov7076: PCLK -> pico GPIO8
ov7076: XCLK -> pico GPIO21
ov7076: D0 -> pico GPIO9
ov7076: D1 -> pico GPIO10
ov7076: D2 -> pico GPIO11
ov7076: D3 -> pico GPIO12
ov7076: D4 -> pico GPIO13
ov7076: D5 -> pico GPIO14
ov7076: D6 -> pico GPIO15
ov7076: D7 -> pico GPIO16
ov7076: PWDN -> pico GND
ov7076: RST -> pico 3.3V, high = normal, low = reset
*/

#include <stdio.h>
#include <pico/stdlib.h>
#include <hardware/i2c.h>
#include <hardware/clocks.h>
#include <string>
#include "reg_config.h"


void i2c_write_register(i2c_inst_t* i2c, uint8_t addr, uint8_t reg, uint8_t val);   // addr is device address
uint8_t i2c_read_register(i2c_inst_t* i2c, uint8_t addr, uint8_t reg);              // addr is device address

// ov7670 function
void ov7670_init();
void set_size(OV7670_SIZE size);
void set_image_format(OV7670_COLOR color);
void capture_frame();                                    // determin one frame is arrived
void perform_capture_frame();                            // do capture frame
bool capture_frame_callback(repeating_timer_t* rt);      // timer alarm callback function

// images
uint32_t frame_count = 0;
uint8_t yuv_image[60][160]; // bytes sequence is Y,U,Y,V,Y,U,Y,V
char ascii_char_image[60][81];

int main()
{
    stdio_init_all();

    sleep_ms(5000);
    printf("start...\n");

    // initialize data structure
    for (size_t i = 0; i < 60; i++)
    {
        for (size_t j = 0; j < 160; j++)
        {
            yuv_image[i][j] = 'a';
        }
    }


    for (size_t i = 0; i < 60; i++)
    {
        for (size_t j = 0; j < 80; j++)
        {
            ascii_char_image[i][j] = '$';
        }
        ascii_char_image[i][80] = '\0';
    }

    
    // initialize i2c
    i2c_init(i2c_instance, 100000);
    gpio_set_function(OV_SDA, GPIO_FUNC_I2C);
    gpio_set_function(OV_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(OV_SDA);
    gpio_pull_up(OV_SCL);

    // initialize gpio, from gpio6 to gpio16
    // 1 1111 1111 1100 0000 = 131008
    gpio_init_mask(131008);
    gpio_set_dir(GPIO_VSYNC, GPIO_IN);
    gpio_set_dir(GPIO_HREF, GPIO_IN);
    gpio_set_dir(GPIO_PCLK, GPIO_IN);
    gpio_set_dir(GPIO_D0, GPIO_IN);
    gpio_set_dir(GPIO_D1, GPIO_IN);
    gpio_set_dir(GPIO_D2, GPIO_IN);
    gpio_set_dir(GPIO_D3, GPIO_IN);
    gpio_set_dir(GPIO_D4, GPIO_IN);
    gpio_set_dir(GPIO_D5, GPIO_IN);
    gpio_set_dir(GPIO_D6, GPIO_IN);
    gpio_set_dir(GPIO_D7, GPIO_IN);

    
    // initialize ov7670
    // first step is set ov7670 clock, then set registers through SCCB
    clock_gpio_init(GPIO_XCLK, CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_CLK_SYS, 10);
    sleep_ms(300);  // add some settling time
    // config sensor
    ov7670_init();
    
    // repeating_timer timer;
    // add_repeating_timer_ms(2000, capture_frame_callback, nullptr, &timer);

    while (1)
    {
        capture_frame();
        sleep_ms(1000);
    }

    return 0;
}


void i2c_write_register(i2c_inst_t* i2c, uint8_t addr, uint8_t reg, uint8_t val)
{
    uint8_t data[2] = {reg, val}; 
    i2c_write_blocking(i2c, addr, data, 2, false);
}


uint8_t i2c_read_register(i2c_inst_t* i2c, uint8_t addr, uint8_t reg)
{
    uint8_t value;
    i2c_write_blocking(i2c, addr, &reg, 1, false);
    i2c_read_blocking(i2c, addr, &value, 1, false);
    return value;
}


void set_size(OV7670_SIZE size)
{
    if (size == OV7670_SIZE::OV7670_SIZE_DIV8)
    {
        for (auto& cmd : ov7670_div8)
            i2c_write_register(i2c_instance, OV7670_ADDR, cmd.addr, cmd.value);
    }
    // TODO add other DIVS
}


void set_image_format(OV7670_COLOR color)
{
    if (color == OV7670_COLOR::OV7670_COLOR_RGB)
    {
        for (auto& cmd : ov7670_rgb)
            i2c_write_register(i2c_instance, OV7670_ADDR, cmd.addr, cmd.value);
    } 
    else if (color == OV7670_COLOR::OV7670_COLOR_YUV)
    {
        for (auto& cmd : ov7670_yuv)
            i2c_write_register(i2c_instance, OV7670_ADDR, cmd.addr, cmd.value);
    }
    // TODO add other colorspace
}


void ov7670_init()
{
    // reset
    i2c_write_register(i2c_instance, OV7670_ADDR, OV7670_REG_COM7, OV7670_COM7_RESET);

    // timing
    i2c_write_register(i2c_instance, OV7670_ADDR, OV7670_REG_CLKRC, 1);     // CLK * 4
    i2c_write_register(i2c_instance, OV7670_ADDR, OV7670_REG_DBLV, 1 << 6); // CLK / 4

    // setup common register
    for (auto& cmd : ov7670_init_cmd)
    {
        i2c_write_register(i2c_instance, OV7670_ADDR, cmd.addr, cmd.value);
    }
    
    // setup image size
    // using 80x60, 4.8kb per frame, using 115200bd(11.52kb/s)
    set_size(OV7670_SIZE::OV7670_SIZE_DIV8);          
    // setup image format
    set_image_format(OV7670_COLOR::OV7670_COLOR_YUV); // 使用80x60分辨率的时候只能使用YUV或RGB，不能使用bayer raw
}


// perform image capture, send to PC by UART
void capture_frame()
{
    bool lframe_pin;
    bool cframe_pin = gpio_get(GPIO_VSYNC);

    while(1)
    {
        lframe_pin = cframe_pin;
        // sleep_us(10);
        cframe_pin = gpio_get(GPIO_VSYNC);
        if (lframe_pin && !cframe_pin)  // falling edge trigger
        {
            printf(">> frame number: %d...\n", ++frame_count);
            perform_capture_frame();    // capture one frame
            break;
        }
    }
}

// image size 80x60
void perform_capture_frame()
{
    uint32_t line_count = 0;
    uint32_t point_count = 0;
    
    bool lline_pin;
    bool cline_pin = gpio_get(GPIO_HREF);
    bool lpoint_pin;
    bool cpoint_pin;

    uint32_t start;
    uint32_t end1;
    uint32_t end2;
    uint32_t end3;
    uint32_t end4;

    uint8_t data;

    // line loop
    while (!gpio_get(GPIO_VSYNC))                   // make sure the line signal still stay in the current frame
    {
        lline_pin = cline_pin;
        cline_pin = gpio_get(GPIO_HREF);

        if (!lline_pin && cline_pin)                // raising edge trigger
        {
            if(++line_count <= 60)
            {
                cpoint_pin = gpio_get(GPIO_PCLK);
                // point loop
                while (gpio_get(GPIO_HREF))
                {
                    lpoint_pin = cpoint_pin;
                    cpoint_pin = gpio_get(GPIO_PCLK);
                    
                    if (!lpoint_pin && cpoint_pin)  // raising edge trigger
                    {
                        if (++point_count <= 160)
                        {
                            data = (GPIO_DATA_MASK & gpio_get_all()) >> GPIO_D0;    // convert D0-D7 to byte
                            yuv_image[line_count - 1][point_count - 1] = data;
                        } 
                        else 
                        {
                            printf("point overflow\n");
                            break;
                        }
                    }            
                }
                point_count = 0;
            } 
            else 
            {
                printf("line overflow\n");
                break;
            }  
        }
    }


    std::string ascii_str_image;
    start = time_us_32();
    // convert YUV data to ascii char image
    for (size_t i = 0; i < 60; i++)
    {
        for (size_t j = 0; j < 80; j++)
        {
            ascii_char_image[i][j] = charmap[70 - uint32_t(yuv_image[i][2 * j] / 3.7)];
        }
        ascii_str_image += (std::string(ascii_char_image[i]) + "\n"); 
    }
    end1 = time_us_32();
    printf(ascii_str_image.c_str()); // 1.2Kb
    end2 = time_us_32();
    printf(">> image convert time: %dus, image transmit time: %dus\n", end1 - start, end2 - end1);
    printf(">> capture frame finished, get %d lines\n", line_count);
}

bool capture_frame_callback(repeating_timer_t* rt)
{
    capture_frame();
    return true;
}

