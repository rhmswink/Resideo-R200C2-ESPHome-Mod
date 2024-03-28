// This driver is based on this post: https://gathering.tweakers.net/forum/list_message/77705490#77705490
// And partially based on this datasheet: https://dfimg.dfrobot.com/nobody/wiki/4c8e1057e1c118e5c72f8ff6147575db.pdf

#include "esphome.h"

// These values must be modified to the pinout of the used device
#define SDA_PIN 8
#define SCL_PIN 9

class Cht8305Sensor : public PollingComponent, public Sensor {
private:

    bool readAck() {
        bool ack;
        while (digitalRead(SCL_PIN) == LOW) {} // Wait for clock pulse
        ack = digitalRead(SDA_PIN) == LOW?true:false;
        while (digitalRead(SCL_PIN) == HIGH) {} // Wait for SCL to go low (end of bit)
        return ack;
    }

    bool readByte(uint8_t *data) {
        for (int i = 0; i < 8; i++) {
            while (digitalRead(SCL_PIN) == LOW) {} // Wait for clock pulse
            *data = (*data << 1) | digitalRead(SDA_PIN);
            while (digitalRead(SCL_PIN) == HIGH) {} // Wait for SCL to go low (end of bit)
        }
        return readAck();
    }

    void readRegister(uint16_t *data) {
        uint8_t receivedByte;

        readByte(&receivedByte); // MSB
        *data = (receivedByte << 8);
        
        readByte(&receivedByte); // LSB        
        *data = *data | receivedByte;
    }

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
        uint8_t receivedByte = 0;
        bool receivedAck;

        uint16_t rawTemp = 0;
        uint16_t rawHum = 0;

        ESP_LOGD("I2C", "wait for the bus to be idle");
        while(digitalRead(SDA_PIN) != HIGH || digitalRead(SCL_PIN) != HIGH) {}

        delayMicroseconds(5); // Wait a short time to ensure stability after start condition

        // Check again to see if the bus is still idle
        if (digitalRead(SDA_PIN) == HIGH && digitalRead(SCL_PIN) == HIGH) {
            ESP_LOGD("I2C", "The bus is probably idle, start listening");
            while (digitalRead(SCL_PIN) == HIGH) {} // Wait for the start bit

            receivedAck = readByte(&receivedByte);

            // First a write request is expected
            if (receivedByte != 0x80 || !receivedAck) {
                ESP_LOGW("I2C", "invalid write request, breaking off. addr=%02X, ack=%u", receivedByte, receivedAck);
                return;
            }
            
            while (digitalRead(SDA_PIN) != HIGH || digitalRead(SCL_PIN) != HIGH) {} // Wait for the stop bit
            while (digitalRead(SCL_PIN) == HIGH) {} // Wait for the start bit

            // Assume that the bus is stable; start reading
            receivedAck = readByte(&receivedByte);

            if (receivedByte != 0x81 || !receivedAck) {
                ESP_LOGW("I2C", "invalid read request, breaking off. addr=%02X, ack=%u", receivedByte, receivedAck);
                return;
            }
            
            readRegister(&rawTemp);
            readRegister(&rawHum);

            double temperature = 165 * (rawTemp / 65535.0) - 40;
            temperature -= 1.4; // as seen on the display
            ESP_LOGD("cht8305", "temperature: %f raw:%i", temperature, rawTemp);
            temperature_sensor->publish_state(temperature);

            double humidity = (rawHum / 65535.0) * 100;
            humidity += 2; // as seen on the display
            ESP_LOGD("cht8305", "humidity: %f raw:%i", humidity, rawHum);
            humidity_sensor->publish_state(humidity);
        } else {
            ESP_LOGW("I2C", "The bus was not idle");
        }
    }
};