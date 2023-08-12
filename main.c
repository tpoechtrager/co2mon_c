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
#define MAX_READING_COUNT 256
#define TIMEOUT_MS 5000

// Global flag to indicate if the program should exit
volatile sig_atomic_t exit_flag = 0;

// Signal handler function to set the exit_flag on signal reception
void handle_signal(int signal __attribute__((unused))) {
    exit_flag = 1;
}

typedef enum {
    RELATIVE_HUMIDITY = 'A',
    TEMPERATURE_KELVIN = 'B',
    TEMPERATURE_FAHRENHEIT = 'F',
    CO2_CONCENTRATION = 'P',
    CO2_CONCENTRATION_UNCALIBRATED = 'q' // unsure
    // BAROMETRIC_PRESSURE_R = 'R', // unsure
    // BAROMETRIC_PRESSURE_V = 'V' // unsure
} ReadingType;

typedef struct {
    uint16_t values[MAX_READING_COUNT];
    uint8_t received[MAX_READING_COUNT];
} Reading;

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

void decode(uint8_t data[5], Reading *reading) {
    if (data[4] != 0x0d) {
        return;
    }

    uint16_t value = (data[1] << 8) | data[2];
    reading->values[data[0]] = value;
    reading->received[data[0]] = 1;
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

int read_readings(hid_device *device, const uint8_t *key, Reading *reading,
                  const ReadingType *required_readings, size_t required_count) {

    // Reset only the required readings to not received
    for (size_t i = 0; i < required_count; i++) {
        reading->received[required_readings[i]] = 0;
    }

    while (1) {
        if (read_one(device, key, reading) != 0) {
            return 1; // Failure
        }

        int all_required_received = 1;

        // Loop through required_readings
        for (size_t i = 0; i < required_count; i++) {
            if (reading->received[required_readings[i]] == 0) {
                all_required_received = 0; // If one hasn't been received, set to 0
                break; // No need to continue checking if one hasn't been received
            }
        }

        // Exit the loop if all required readings are received
        if (all_required_received) {
            break;
        }
    }

    return 0; // Success
}

int parse_command_line_arguments(int argc, char *argv[], uint8_t *key, int *all_flag) {
    const char *key_string = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [options]\n", argv[0]);
            printf("Options:\n");
            printf("  --key <key>   Set the encryption key (16 hex characters)\n");
            printf("  --all         Print all readings\n");
            return 1; // Return to exit the program or continue as needed
        } else if (strcmp(argv[i], "--key") == 0) {
            if (i + 1 < argc) {
                key_string = argv[i + 1];
                i++; // Skip the next argument since it's the key value
            }
        } else if (strcmp(argv[i], "--all") == 0) {
            *all_flag = 1;
        }
    }

    // Parse the key if provided
    if (key_string) {
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



void generate_timestamp_string(char *timestamp_buffer, size_t buffer_size) {
    time_t current_time;
    struct tm *time_info;

    time(&current_time);
    time_info = localtime(&current_time);
    strftime(timestamp_buffer, buffer_size, "%Y-%m-%d %H:%M:%S", time_info);
}

void print_reading(const char *key, uint16_t value, int *printed_values) {
    if (*printed_values > 0) {
        printf(",");
    }
    printf("\n  \"%s\": %u", key, value);
    (*printed_values)++;
}

void print_reading_float(const char *key, double value, int decimal_places, int *printed_values) {
    if (*printed_values > 0) {
        printf(",");
    }
    printf("\n  \"%s\": %.*f", key, decimal_places, value);
    (*printed_values)++;
}

void print_known_reading(const Reading *reading, ReadingType type, const char *key, int *printed_values) {
    if (reading->received[type]) {
        print_reading(key, reading->values[type], printed_values);
    }
}

void print_unknown_reading_value(const Reading *reading, int index, int *printed_values) {
    if (*printed_values > 0) {
        printf(",");
    }
    printf("\n  \"value_%c\": %u", index, reading->values[index]);
    (*printed_values)++;
}

void print_all_reading_as_json(const Reading *reading) {
    char time_buffer[80];
    generate_timestamp_string(time_buffer, sizeof(time_buffer));

    printf("{\n");
    printf("  \"timestamp\": \"%s\"", time_buffer);

    int printed_values = 1; // timestamp

    print_reading("temperature_kelvin", reading->values[TEMPERATURE_KELVIN], &printed_values);
    print_reading_float("temperature_celsius", reading->values[TEMPERATURE_KELVIN] * 0.0625 - 273.15, 4, &printed_values);
    print_reading("co2", (double)reading->values[CO2_CONCENTRATION], &printed_values);

    print_known_reading(reading, CO2_CONCENTRATION_UNCALIBRATED, "co2_uncalibrated(unsure)", &printed_values);
    print_known_reading(reading, RELATIVE_HUMIDITY, "relative_humidity", &printed_values);
    print_known_reading(reading, TEMPERATURE_FAHRENHEIT, "temperature_fahrenheit", &printed_values);

    // Print other received values
    for (int i = 0; i < MAX_READING_COUNT; i++) {
        if (reading->received[i] && i != RELATIVE_HUMIDITY && i != TEMPERATURE_KELVIN &&
            i != TEMPERATURE_FAHRENHEIT && i != CO2_CONCENTRATION && i != CO2_CONCENTRATION_UNCALIBRATED) {
            print_unknown_reading_value(reading, i, &printed_values);
        }
    }

    printf("\n}\n");
    fflush(stdout); // Make sure the output is printed immediately
}

void print_reading_as_json(const Reading *reading) {
    char time_buffer[80];
    generate_timestamp_string(time_buffer, sizeof(time_buffer));

    printf("{\n");
    printf("  \"timestamp\": \"%s\"", time_buffer);

    int printed_values = 1; // timestamp

    print_reading_float("temperature", reading->values[TEMPERATURE_KELVIN] * 0.0625 - 273.15, 4, &printed_values);
    print_reading("co2", (double)reading->values[CO2_CONCENTRATION], &printed_values);

    printf("\n}\n");
    fflush(stdout); // Flush stdout buffer to ensure immediate print
}



int main(int argc, char *argv[]) {
    uint8_t key[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    int all_flag = 0;

    if (parse_command_line_arguments(argc, argv, key, &all_flag) != 0) {
        return 1;
    }

    if (hid_init() != 0) {
        fprintf(stderr, "Failed to initialize HIDAPI library.\n");
        return 1;
    }

    hid_device *device = initialize_device(key);
    if (!device) {
        hid_exit();
        return 1;
    }

    usleep(100000); // Sleep for 100 ms after successful initialization

    // Register signal handlers
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    ReadingType required_readings[] = {TEMPERATURE_KELVIN, CO2_CONCENTRATION};
    Reading reading;
    memset(&reading, 0, sizeof(Reading));

    while (!exit_flag) {
        int result = read_readings(device, key, &reading, required_readings, sizeof(required_readings) / sizeof(ReadingType));

        if (result != 0) {
            fprintf(stderr, "Failed to read valid readings from the device.\n");
            hid_close(device);
            hid_exit();
            return 1;
        }

        if (all_flag) {
            print_all_reading_as_json(&reading);
        } else {
            print_reading_as_json(&reading);
        }
    }

    hid_close(device);
    hid_exit();
    return 0;
}