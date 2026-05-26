#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

typedef enum {
    SENSOR_INIT_STOP_AMES = 0,
    SENSOR_INIT_OPEN,
    SENSOR_INIT_GET_VER,
    SENSOR_READY_POLL,
    SENSOR_ERROR
} SensorState_t;

typedef struct {
    float speed;
    float direction;
    SensorState_t state;
    uint32_t last_sync;
    uint8_t id;
} Sensor_t;

// Copy of the updated parse function from sensor.c for verification
int Sensor_Parse(Sensor_t *sensor, char *buffer) {
    if (!buffer || strlen(buffer) < 5) return 0;

    // Option D: Version response check (for init) - check first to avoid digit-overlap conflicts in version string
    if (sensor->state == SENSOR_INIT_GET_VER) {
        if (strstr(buffer, "DVU") || strstr(buffer, "VER")) {
            return 2; 
        }
    }

    char *parse_ptr = NULL;
    
    // Option A: Response with @id:0 prefix (Network mode)
    char *colon_ptr = strchr(buffer, ':');
    if (colon_ptr != NULL) {
        parse_ptr = colon_ptr + 2; // Skip ":0 "
    } else {
        // Option B: Response without prefix (Command mode)
        // Find the first digit in the buffer
        char *ptr = buffer;
        while (*ptr && !(*ptr >= '0' && *ptr <= '9')) {
            ptr++;
        }
        if (*ptr) {
            parse_ptr = ptr;
        }
    }

    if (parse_ptr != NULL) {
        int status;
        float s_act, s_avg, s_max, s_min, s_unused;
        float d_act, d_avg, d_max, d_min, d_unused;
        
        // Try parsing 10 fields (standard DVU message)
        if (sscanf(parse_ptr, "%d %f %f %f %f %f %f %f %f %f", 
                   &status, &s_act, &s_avg, &s_max, &s_min, &s_unused, &d_act, &d_avg, &d_max, &d_min) >= 7) {
            sensor->speed = s_act;
            sensor->direction = d_act;
            return 1;
        }

        // Try parsing 3 fields (M 1 message: id, speed, direction)
        int parsed_id;
        float s_val, d_val;
        if (sscanf(parse_ptr, "%d %f %f", &parsed_id, &s_val, &d_val) == 3) {
            sensor->speed = s_val;
            sensor->direction = d_val;
            return 1;
        }
    }

    // Option C: Spd= Dir= debug line
    char *spd_ptr = strstr(buffer, "Spd=");
    char *dir_ptr = strstr(buffer, "Dir=");
    if (spd_ptr && dir_ptr) {
        if (sscanf(spd_ptr, "Spd=%f", &sensor->speed) == 1 && 
            sscanf(dir_ptr, "Dir=%f", &sensor->direction) == 1) {
            return 1;
        }
    }

    return 0;
}

int main() {
    Sensor_t sensor;
    sensor.speed = -1.0f;
    sensor.direction = -1.0f;
    sensor.id = 1;
    sensor.state = SENSOR_READY_POLL;

    // Test 1: M 1 style response (user's input)
    char test_buf1[] = "0 0.05 153\r\n";
    printf("Running Test 1 (3-field M 1 format)...\n");
    int res1 = Sensor_Parse(&sensor, test_buf1);
    printf("Result: %d, Speed: %f, Dir: %f\n", res1, sensor.speed, sensor.direction);
    assert(res1 == 1);
    assert(sensor.speed == 0.05f);
    assert(sensor.direction == 153.0f);
    printf("Test 1 PASSED!\n\n");

    // Test 2: Standard 10-field response
    char test_buf2[] = "0 6.15 5.94 9.53 3.41 303 298 358 0\r\n";
    printf("Running Test 2 (10-field standard format)...\n");
    int res2 = Sensor_Parse(&sensor, test_buf2);
    printf("Result: %d, Speed: %f, Dir: %f\n", res2, sensor.speed, sensor.direction);
    assert(res2 == 1);
    assert(sensor.speed == 6.15f);
    assert(sensor.direction == 298.0f);
    printf("Test 2 PASSED!\n\n");

    // Test 3: Network prefixed M 1 response
    char test_buf3[] = "@1:0 0 1.25 180\r\n";
    printf("Running Test 3 (Network prefixed 3-field format)...\n");
    int res3 = Sensor_Parse(&sensor, test_buf3);
    printf("Result: %d, Speed: %f, Dir: %f\n", res3, sensor.speed, sensor.direction);
    assert(res3 == 1);
    assert(sensor.speed == 1.25f);
    assert(sensor.direction == 180.0f);
    printf("Test 3 PASSED!\n\n");

    // Test 4: Version query response in SENSOR_INIT_GET_VER state
    sensor.state = SENSOR_INIT_GET_VER;
    char test_buf4[] = "DVU VER 1.0.0\r\n";
    printf("Running Test 4 (Version format)...\n");
    int res4 = Sensor_Parse(&sensor, test_buf4);
    printf("Result: %d\n", res4);
    assert(res4 == 2);
    printf("Test 4 PASSED!\n\n");

    printf("ALL PARSER TESTS PASSED SUCCESSFULLY!\n");
    return 0;
}
