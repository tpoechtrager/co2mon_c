#include <stdio.h>
#include <stdint.h>
#include <hidapi/hidapi.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>

#define VID 0x04d9
#define PID 0xa052
#define BUFFER_SIZE 8
#define TIMEOUT_MS 5000

// Global flag to indicate if the program should exit
volatile sig_atomic_t exit_flag = 0;

// Signal handler function to set the exit_flag on signal reception
void handle_signal(int signal __attribute__((unused))) {
    exit_flag = 1;
}

typedef struct {
    float temperature;
    uint16_t co2;
    int temperature_received;
    int co2_received;
} Reading;

void decode(uint8_t data[5], Reading *reading) {
    if (data[4] != 0x0d) {
        memset(reading, 0, sizeof(Reading));
        return;
    }

    uint16_t value = (data[1] << 8) | data[2];

    switch (data[0]) {
        case 'B':
            reading->temperature = (float)value * 0.0625 - 273.15;
            reading->temperature_received = 1;
            break;
        case 'P':
            reading->co2 = value;
            reading->co2_received = 1;
            break;
        default:;
    }
}

#define SWAP(a, b) do { a ^= b; b ^= a; a ^= b; } while (0)

void decrypt(uint8_t data[8], const uint8_t key[8]) {
    // Perform swaps
    SWAP(data[0], data[2]);
    SWAP(data[1], data[4]);
    SWAP(data[3], data[7]);
    SWAP(data[5], data[6]);

    // XOR with key
    for (int i = 0; i < 8; i++) data[i] ^= key[i];

    // Bit-shifting transformation
    uint8_t tmp_shift = data[7] << 5;
    for (int i = 7; i > 0; i--) {
        data[i] = (data[i - 1] << 5) | (data[i] >> 3);
    }
    data[0] = tmp_shift | (data[0] >> 3);

    // Subtract transformed characters from "Htemp99e"
    for (int i = 0; i < 8; i++) {
        uint8_t m = "Htemp99e"[i];
        data[i] -= (m << 4) | (m >> 4);
    }
}

int read_one(hid_device *device, const uint8_t *key, Reading *reading) {
    uint8_t data[BUFFER_SIZE];
    int read_result = hid_read_timeout(device, data, BUFFER_SIZE, TIMEOUT_MS);

    if (read_result != BUFFER_SIZE) {
        return -1; // Return error code
    }

    uint8_t magic_byte = 0x0d;
    if (data[4] != magic_byte) {
        decrypt(data, key);
    }

    decode(data, reading);
    return 0; // Return success code
}

int read_temperature_and_co2(hid_device *device, const uint8_t *key, Reading *reading) {
    reading->temperature_received = 0;
    reading->co2_received = 0;

    while (!reading->temperature_received || !reading->co2_received) {
        if (read_one(device, key, reading) != 0) {
            return 1; // Failure
        }
    }

    return 0; // Success
}

int parse_command_line_arguments(int argc, char *argv[], uint8_t *key) {
    if (argc == 2) {
        const char *key_string = argv[1];
        int length = strlen(key_string);

        if (length != 16) {
            fprintf(stderr, "Key must be exactly 16 characters long (8 bytes).\n");
            return 1;
        }

        for (int i = 0; i < 8; i++) {
            sscanf(key_string + 2 * i, "%2hhx", &key[i]);
        }
    }

    return 0;
}

hid_device* initialize_device(uint8_t *key) {
    hid_device *device = hid_open(VID, PID, NULL);
    if (!device) {
        fprintf(stderr, "Failed to open the USB device.\n");
        return NULL;
    }

    uint8_t init_frame[9] = {0x00}; // First byte initialized to 0x00
    memcpy(init_frame + 1, key, 8); // Copy key bytes to init_frame

    hid_send_feature_report(device, init_frame, sizeof(init_frame));

    return device;
}

// Function to print readings as JSON with timestamp
void print_reading_with_timestamp(float temperature, uint16_t co2) {
    // Get current time
    time_t current_time;
    struct tm *time_info;
    char time_buffer[80];

    time(&current_time);
    time_info = localtime(&current_time);
    strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", time_info);

    // Print readings as JSON with timestamp
    printf("{\n");
    printf("  \"timestamp\": \"%s\",\n", time_buffer);
    printf("  \"temperature\": %.2f,\n", temperature);
    printf("  \"co2\": %d\n", co2);
    printf("}\n");

    fflush(stdout);
}

int main(int argc, char *argv[]) {
    uint8_t key[8] = {0, 0, 0, 0, 0, 0, 0, 0};

    if (parse_command_line_arguments(argc, argv, key) != 0) {
        return 1;
    }

    hid_device *device = initialize_device(key);
    if (device == NULL) {
        return 1;
    }

    usleep(100000); // Delay for 100 ms

    // Set up the signal handler for SIGINT
    signal(SIGINT, handle_signal);

    while (!exit_flag) {
        Reading reading;
        memset(&reading, 0, sizeof(reading));

        if (read_temperature_and_co2(device, key, &reading) != 0) {
            fprintf(stderr, "Failed to read valid readings from the device.\n");
            hid_close(device);
            return 1;
        }

        // Call the print function with readings and timestamp
        print_reading_with_timestamp(reading.temperature, reading.co2);
    }

    hid_close(device);
    return 0;
}
