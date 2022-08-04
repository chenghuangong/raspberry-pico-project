#ifndef DHT11_H_
#define DHT11_H_

#include <pico/stdlib.h>
#include <stdio.h>
#include <queue>
#include <list>

/* 
dht11 driver, initialized with gpio15
*/

const uint RETRY_TIMES = 3;     // retry times when error data arrived
const uint MAX_TIMINGS = 85;    

struct dht_reading
{
    // default value
    double temp = 25;
    double humidity = 25;
};


class dht11
{
public:
    dht11(uint8_t data_pin);
    ~dht11();

public:
    void init_dev();        // set gpio pin, put device to state read ready
    double get_temp();
    double get_humidity();
    dht_reading& get_temp_and_humidity();

    // 方法一：需要使用定时器持续不断的读取数据，并进行存储，入队列，然后再计算出平均值，调用函数只是取出数据，并不从传感器处获得数据
    // 方法二：快速的读取几个数据，然后求平均值
    double get_filtered_temp();
    double get_filtered_humidity();
    dht_reading get_filtered_temp_and_humidity();

private:
    void read_from_dht();   // get error data three times, use the last read value

    /* 
    read data from dht11
    @param r store dht_reading result (temperature, humidity)
    @return true if reading is succeed, but not care whether the result is reasonable 
    */
    bool do_read(dht_reading& r);
    bool is_read_data_reasonable(dht_reading& r);

private:
    uint8_t data_pin_;
    dht_reading result_;

    std::list<dht_reading> result_list_;
};


#endif