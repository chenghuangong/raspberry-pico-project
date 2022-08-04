#include "dht11.h"

dht11::dht11(uint8_t data_pin) 
    : data_pin_(data_pin)
{ 
}

dht11::~dht11() {}

void dht11::init_dev()
{
    // if gpio > 30, will stop running
    gpio_init(data_pin_);
}

double dht11::get_temp()
{
    read_from_dht();
    return result_.temp;
}

double dht11::get_humidity()
{
    read_from_dht();
    return result_.humidity;
}

dht_reading& dht11::get_temp_and_humidity()
{
    read_from_dht();
    return result_;
}

double dht11::get_filtered_temp()
{
    return 0;
}

double dht11::get_filtered_humidity()
{
    return 0;
}

dht_reading dht11::get_filtered_temp_and_humidity()
{
    read_from_dht();
    result_list_.push_back(result_);
    
    // only store five elements
    if (result_list_.size() > 5)
    {
        result_list_.pop_front();
    }

    // return the average value
    dht_reading r;
    r.temp = 0;
    r.humidity = 0;

    for (auto&& data : result_list_)
    {
        r.temp += data.temp;
        r.humidity += data.humidity;
    }

    r.temp = r.temp / result_list_.size();
    r.humidity = r.humidity / result_list_.size();
    return r;
}

void dht11::read_from_dht()
{
    dht_reading result;
    for (size_t i = 0; i < RETRY_TIMES; i++)
    {
        if (do_read(result) && is_read_data_reasonable(result))
        {
            result_ = result;
            printf("read times = %d.", i);
            return;
        }
    }   
}


bool dht11::do_read(dht_reading& result)
{
    // DHT data format: 
    // data[0:4] = 1byte humidity int part | 1byte humidity fraction part | 1byte temp int part | 1byte temp fraction part | check sum 
    // total 5bytes, 
    // check sum = char(SUM(data[0:4)))
    int data[5] = {0, 0, 0, 0, 0};
    uint last = 1;
    uint bit_count = 0;

    // pull down data pin > 18ms, start receive ready
    gpio_set_dir(data_pin_, GPIO_OUT);
    gpio_put(data_pin_, 0);               
    sleep_ms(20);

    // change to receive mode         
    gpio_set_dir(data_pin_, GPIO_IN);
    sleep_us(1);

    /* 
    DHT response starts with low(80us) + up(80us) = 160us
    then followed by data stream    '0': low(50us) + up(28us) = 78us
                                    '1': low(50us) + up(70us) = 120us
    so response contains: 41 voltage filps, gpio reading times is 82
    one reading max_time = 160 + 120 * 40 = 4.96ms, 
                min_time = 160 + 78 * 40 = 3.28ms
    */

    for (uint i = 0; i < MAX_TIMINGS; i++)
    {
        uint count = 0;     // count time
        while (gpio_get(data_pin_) == last)
        {
            count++;
            sleep_us(1);
            if (count == 255) break;    // overtime
        }

        last = gpio_get(data_pin_);
        if (count == 255) break;

        // previous 1 voltage filp is DHT response, not data
        if ((i >= 4) && (i % 2 == 0)) 
        {
            data[bit_count / 8] <<= 1;
            // DHT11 is very sensitivity to the time delay, 
            // so the count value should depend on the actual situation
            // github https://github.com/raspberrypi/pico-examples/issues/11
            if (count > 30) 
            {
                data[bit_count / 8] |= 1;
            }
            bit_count++;
        }   
    }

    // check sum
    if (bit_count >= 40 && (((data[0] + data[1] + data[2] + data[3]) & 0xff) == data[4]))
    {
        result.humidity = data[0] + data[1] / 10.0;
        result.humidity = result.humidity > 100 ? data[0] : result.humidity;

        result.temp = data[2] + data[3] / 10.0;
        result.temp = result.temp > 125 ? data[2] : result.temp;

        // negative temperature
        if (data[2] & 0x80)
        {
            result.temp *= -1;
        }
        return true;
    }
    return false;
}


bool dht11::is_read_data_reasonable(dht_reading& r)
{
    /* 
    the sensor is used to detect the weather
    set the temperature limit 70 degrees celsius
    0 < humidity <= 100 
    */
   return r.temp < 70 && r.humidity <= 100 && r.humidity > 0;
}