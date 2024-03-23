// put this file in your esphome folder
// protocol implemented as described in https://en.gassensor.com.cn/Product_files/Specifications/CM1106-C%20Single%20Beam%20NDIR%20CO2%20Sensor%20Module%20Specification.pdf

#include "esphome.h"

class CM1106 : public UARTDevice {
public:
    CM1106(UARTComponent *parent) : UARTDevice(parent) {}

    int16_t getCo2PPM() {
        uint8_t expectedBytes[] = {0x16, 0x05, 0x01};
        int currentPos = 0;
        uint8_t response[NUM_MSG_BYTES] = {0};

        // Check how many bytes the UART RX buffer has stored
        int availableBytes = readUartFillLevel();

        // Check if there is a message in the buffer
        if (availableBytes < NUM_MSG_BYTES) {
            return -1;
        }

        // We are only interested in the last message, drop all others
        while (availableBytes >= (2*NUM_MSG_BYTES)) {
            readUartResponse(response, NUM_MSG_BYTES);
            availableBytes = readUartFillLevel();
        }

        // Check if this is the expected message
        while (currentPos < sizeof(expectedBytes)) {
            if (readUartFillLevel()) {
                readUartResponse(response+currentPos, 1);
            } else {
                return -1;
            }
            
            if (response[currentPos] == expectedBytes[currentPos]) {
                currentPos++;
            }
        }

        // If present, read the data and checksum
        if (readUartFillLevel() >= NUM_MSG_BYTES - sizeof(expectedBytes)) {
            readUartResponse(response+currentPos, NUM_MSG_BYTES - sizeof(expectedBytes));
        } else {
            ESP_LOGW(TAG, "The last message in the buffer was not complete");
            return -1;
        }

        // Process the Co2 value and checksum
        uint8_t checksum = calcCRC(response, sizeof(response));
        int16_t ppm = response[3] << 8 | response[4];
        if (response[7] == checksum) {
            ESP_LOGD(TAG, "CM1106 Received CO₂=%uppm DF3=%02X DF4=%02X", ppm, response[5], response[6]);
            return ppm;
        } else {
            ESP_LOGW(TAG, "Got wrong UART checksum: 0x%02X - Calculated: 0x%02X, ppm data: %u", response[7], checksum, ppm);
            return -1;
        }
        return -1;
    }

private:
    const char *TAG = "cm1106";
    const uint8_t NUM_MSG_BYTES = 8;

    // Checksum: 256-(HEAD+LEN+CMD+DATA)%256
    uint8_t calcCRC(uint8_t* response, size_t len) {
        uint8_t crc = 0;
        // last byte of response is checksum, don't calculate it
        for (int i = 0; i < len - 1; i++) {
            crc -= response[i];
        }
        return crc;
    }

    void readUartResponse(uint8_t *response, size_t responseLen) {
        read_array(response, responseLen);
    }

    // Returns the number of bytes currently in the UART RX Buffer
    int readUartFillLevel() {
        return available();
    }
};

class CM1106Sensor : public PollingComponent, public Sensor {
private:
    CM1106 *cm1106;

public:
    CM1106Sensor(UARTComponent *parent, uint32_t update_interval) : PollingComponent(update_interval) {
        cm1106 = new CM1106(parent);
    }

    float get_setup_priority() const { return setup_priority::DATA; }

    void setup() override {
    }

    void update() override {
        int16_t ppm = cm1106->getCo2PPM();
        if (ppm > -1) {
            publish_state(ppm);
        }
    }

    virtual ~CM1106Sensor() { delete cm1106; }

};