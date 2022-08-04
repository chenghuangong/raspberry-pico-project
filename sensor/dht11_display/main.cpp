#include <pico/stdlib.h>
// #include <sstream>
#include "dht11.h"
#include "oled_disp.h"
/*
[material]
1. sensor: DHT11 sensor, vcc = 3.3v
2. display panel: 0.96 inch oled panel, resolution 128x64, driven by SSD1306, vcc= 3.3v
3. board: raspberry pico

[wiring]
DHT11 data line connect to GPIO15
1. DHT11 sensor: data line connect to gpio15
2. display panel: SDA connect to gpio4, SCL connect to gpio5
*/

// gpio setting 
const uint DHT_GPIO = 15;
const uint OLED_SDA = 4;
const uint OLED_SCL = 5;

/*
output style:
TEMP = 13.32℃
RH = 78.21%
*/
std::string format_dht_output(dht_reading& result);
std::string format_dht_output_v2(dht_reading& result);

int main()
{
    stdio_init_all();

    // initialize dht sensor
    dht11 dht11_one{DHT_GPIO};
    dht11_one.init_dev();

    // initialize oled screen
    oled_disp oled_one{OLED_SDA, OLED_SCL};
    oled_one.init_dev();
    sleep_ms(200);

    while (1)
    {
        // auto result = dht11_one.get_temp_and_humidity(); // no filter
        auto result = dht11_one.get_filtered_temp_and_humidity(); // with filter
        printf("Humidity = %.1f%%, Temperture = %.1fC \n", result.humidity, result.temp);
        // std::cout << format_dht_output(result) << '\n';
        
        // send data to oled display
        oled_one << format_dht_output_v2(result);
        sleep_ms(2000);
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
}*/


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