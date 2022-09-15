#include <pico/stdlib.h>
#include <hardware/uart.h>
#include "dht11.h"
#include "oled_disp.h"

/*
[material]
1. sensor: DHT11 sensor, vcc = 3.3v
2. display panel: vcc = 3.3v, 0.96 inch oled panel, resolution 128x64, driven by SSD1306
3. esp01s wifi module: vcc = 3.3v
4. board: raspberry pico(power the pico board on vsys, 5V)

[wiring]
DHT11 data line connect to GPIO15
1. DHT11 sensor: data line connect to gpio22
2. display panel: SDA connect to gpio2, SCL connect to gpio3(I2C1)
3. esp01s wifi module(UART): TX connect to gpio1(pico RX), RX connect to gpio0(pico TX)

[notice]
1. wait 15s for establishing the wifi connection
2. when boots esp01s, should keep tx and rx at high voltage, but pico gpio0(TX) and gpio1(RX) default voltage = 0V,
so, pull up 10K resistance should add to gpio0 and gpio1
*/

// gpio setting 
const uint DHT_GPIO = 22;
const uint OLED_SDA = 2;
const uint OLED_SCL = 3;
auto i2c_instance = i2c1;


// 使用GPIO0 和 GPIO1来作为UART PIN
#define UART_ID uart0
#define BAUD_RATE 115200
#define UART_TX_PIN 0
#define UART_RX_PIN 1
const uint LED_PIN = PICO_DEFAULT_LED_PIN;
bool is_led_on = true;

/*
output style:
TEMP = 13.32℃
RH = 78.21%
*/
std::string format_dht_output(dht_reading& result);
std::string format_dht_output_v2(dht_reading& result);
std::string format_uart_output(dht_reading& result);
void write_to_uart(const std::string& msg);

int main()
{
    sleep_ms(15000);    // waiting for the wlan connection
    stdio_init_all();


    // init i2c
    i2c_init(i2c_instance, 400 * 1000);
    gpio_set_function(OLED_SDA, GPIO_FUNC_I2C);
    gpio_set_function(OLED_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(OLED_SDA);
    gpio_pull_up(OLED_SCL);


    // initialize dht sensor
    dht11 dht11_one{DHT_GPIO};
    dht11_one.init_dev();


    // initialize oled screen
    oled_disp oled_one{i2c_instance, OLED_SDA, OLED_SCL};
    oled_one.init_dev();
    sleep_ms(200);


    // initialize UART module
    uart_init(UART_ID, BAUD_RATE);
    // Set the TX and RX pins by using the function select on the GPIO
    // see datasheet for more information on function select
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    sleep_ms(500);
    uart_puts(UART_ID, "AT\r\n");
    sleep_ms(500);
    // 连接到服务器
    uart_puts(UART_ID, "AT+CIPSTART=\"TCP\",\"192.168.31.2\",12800\r\n");
    sleep_ms(500);


    // initialzie led
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 1);


    while (1)
    {
        // auto result = dht11_one.get_temp_and_humidity(); // no filter
        auto result = dht11_one.get_filtered_temp_and_humidity(); // with filter
        printf("Humidity = %.1f%%, Temperture = %.1fC \n", result.humidity, result.temp);
        // std::cout << format_dht_output(result) << '\n';
        
        // send data to oled display
        oled_one << format_dht_output_v2(result);

        // send data to uart
        write_to_uart(format_uart_output(result));

        sleep_ms(1000);
    }
    
    return 0;
}


/* 
// 增加了sstream后，文件的尺寸变得很大，到700kb
std::string format_dht_output(dht_reading& result)
{
    std::ostringstream oss;
    oss << std::fixed;
    oss.precision(1);
    oss << "TEMP = " << result.temp << "C, RH = " << result.humidity << "%";
    return oss.str();
}
*/


// 文件尺寸183kb
std::string format_dht_output_v2(dht_reading& result)
{
    // to_string精确到小数点后五位
    std::string output = "TEMP = ";
    std::string temp = std::to_string(result.temp);
    output += temp.substr(0, temp.find('.') + 2) + "_@\nRH = "; // ℃占用两个字符，因此为了方便起见，使用_@来替代
    temp = std::to_string(result.humidity);
    output += temp.substr(0, temp.find('.') + 2) + "%";
    return output;
}


std::string format_uart_output(dht_reading& result)
{
    // to_string精确到小数点后五位
    std::string output = "TEMP = ";
    std::string temp = std::to_string(result.temp);
    output += temp.substr(0, temp.find('.') + 2) + ", RH = ";
    temp = std::to_string(result.humidity);
    output += temp.substr(0, temp.find('.') + 2) + "%";
    return output;
}


// 2022年8月7日添加UART模块
void write_to_uart(const std::string& msg)
{
    std::string len = "AT+CIPSEND=" + std::to_string(msg.size()) + "\r\n";
    uart_puts(UART_ID, len.c_str());
    sleep_ms(500);
    uart_puts(UART_ID, msg.c_str());
    (is_led_on = !is_led_on) ? gpio_put(LED_PIN, 1) : gpio_put(LED_PIN, 0);
    sleep_ms(500);
}