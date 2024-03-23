#include "esphome.h"
#define SDA_PIN 8
#define SCL_PIN 9

class Cht8305Sensor : public PollingComponent, public Sensor {
private:

public:
    Sensor *temperature_sensor = new Sensor();
    Sensor *humidity_sensor = new Sensor();
    Cht8305Sensor(uint32_t update_interval) : PollingComponent(update_interval) {

    }

    float get_setup_priority() const { return setup_priority::DATA; }

    void setup() override {
        pinMode(SDA_PIN, INPUT);
        pinMode(SCL_PIN, INPUT);
    }

    void update() override {

        // Check for a possible start of a new transaction
        while(digitalRead(SDA_PIN) == HIGH && digitalRead(SCL_PIN) == HIGH) {}
        ESP_LOGD("I2C", "wait for pins to be high");
        if (digitalRead(SDA_PIN) == HIGH && digitalRead(SCL_PIN) == HIGH) {
            delayMicroseconds(5); // Wait a short time to ensure stability after start condition

            // Check again to see if the bus is still idle
            if (digitalRead(SDA_PIN) == HIGH && digitalRead(SCL_PIN) == HIGH) {
                // Now, it's likely the bus is idle, start listening
                while (digitalRead(SCL_PIN) == HIGH) {} // Wait for the start of a new byte

                // Assume that the bus is stable; start reading bits
                byte receivedByte = 0;
                for (int i = 0; i < 7; i++) {
                    while (digitalRead(SCL_PIN) == LOW) {} // Wait for clock pulse
                    receivedByte = (receivedByte << 1) | digitalRead(SDA_PIN);
                    while (digitalRead(SCL_PIN) == HIGH) {} // Wait for SCL to go low (end of bit)
                }

                // read operation bit
                bool ReadOp;
                while (digitalRead(SCL_PIN) == LOW) {} // Wait for clock pulse
                ReadOp = digitalRead(SDA_PIN) == HIGH?true:false;
                while (digitalRead(SCL_PIN) == HIGH) {} // Wait for SCL to go low (end of bit)

                // read ACK/NACK
                bool ACK;
                while (digitalRead(SCL_PIN) == LOW) {} // Wait for clock pulse
                ACK = digitalRead(SDA_PIN) == LOW?true:false;
                while (digitalRead(SCL_PIN) == HIGH) {} // Wait for SCL to go low (end of bit)


                // this should be the address
                ESP_LOGD("I2C", "received address %02X, op:%s result:%s", receivedByte, ReadOp?"R":"W", ACK?"ACK":"NACK");
                if (receivedByte != 0x40 || !ReadOp || !ACK) {
                    ESP_LOGW("I2C", "invalid data, breaking off. addr=%02X, read=%u, ack=%u", receivedByte, ReadOp, ACK);
                    return;
                }

                bool bits[36];
                for (int i = 0; i < 36; i++) {
                    while (digitalRead(SCL_PIN) == LOW) {} // Wait for clock pulse
                    bits[i] = digitalRead(SDA_PIN);
                    while (digitalRead(SCL_PIN) == HIGH) {} // Wait for SCL to go low (end of bit)
                }

                uint16_t temp_raw = bits[0];
                for (int i=1; i<8; i++) {
                    temp_raw = (temp_raw << 1) | bits[i];
                }
                //i=8 is ack
                for (int i=9; i<17; i++) {
                    temp_raw = (temp_raw << 1) | bits[i];
                }
                double temperature = 165 * (temp_raw / 65535.0) - 40;
                temperature -= 1.4; // as seen on the display
                ESP_LOGD("cht8305", "temperature: %f raw:%i", temperature, temp_raw);
                temperature_sensor->publish_state(temperature);

                // i=17 is ack
                uint16_t hum_raw = bits[18];
                for (int i=19; i<26; i++) {
                    hum_raw = (hum_raw << 1) | bits[i];
                }
                // i=26 is ack
                for (int i=27; i<35; i++) {
                    hum_raw = (hum_raw << 1) | bits[i];
                }
                double humidity = (hum_raw / 65535.0) * 100;
                humidity += 2; // as seen on the display
                ESP_LOGD("cht8305", "humidity: %f raw:%i", humidity, hum_raw);
                humidity_sensor->publish_state(humidity);
            }
        }
    }
};