/* Includes ---------------------------------------------------------------- */
#include <Leaf_guard_inferencing.h>
#include "edge-impulse-sdk/dsp/image/image.hpp"
#include "esp_camera.h"
#include <Wire.h>
#include <U8x8lib.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <base64.h>
 
/* Camera Pins for XIAO ESP32S3 */
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     10
#define SIOD_GPIO_NUM     40
#define SIOC_GPIO_NUM     39
#define Y9_GPIO_NUM       48
#define Y8_GPIO_NUM       11
#define Y7_GPIO_NUM       12
#define Y6_GPIO_NUM       14
#define Y5_GPIO_NUM       16
#define Y4_GPIO_NUM       18
#define Y3_GPIO_NUM       17
#define Y2_GPIO_NUM       15
#define VSYNC_GPIO_NUM    38
#define HREF_GPIO_NUM     47
#define PCLK_GPIO_NUM     13
 
#define BUTTON_PIN        D1 // Physical button pin
 
// Built-in RGB LED pins on XIAO ESP32S3
#define LED_R             21
#define LED_G             17
#define LED_B             18
 
#define EI_CAMERA_RAW_FRAME_BUFFER_COLS 320
#define EI_CAMERA_RAW_FRAME_BUFFER_ROWS 240
#define EI_CAMERA_FRAME_BYTE_SIZE 3
 
// WiFi credentials
const char* ssid = "DEEPAKKUMAR 7314";
const char* password = "0&1N745x";
 
// API details
const char* serverName = "http://192.168.245.145:5000/analyze";
const char* apiUsername = "admin";
const char* apiPassword = "password";
 
/* Private variables ------------------------------------------------------- */
static bool debug_nn = false;
static bool is_initialised = false;
uint8_t *snapshot_buf;
 
/* Camera configuration */
static camera_config_t camera_config = {
    .pin_pwdn = PWDN_GPIO_NUM,
    .pin_reset = RESET_GPIO_NUM,
    .pin_xclk = XCLK_GPIO_NUM,
    .pin_sscb_sda = SIOD_GPIO_NUM,
    .pin_sscb_scl = SIOC_GPIO_NUM,
    .pin_d7 = Y9_GPIO_NUM,
    .pin_d6 = Y8_GPIO_NUM,
    .pin_d5 = Y7_GPIO_NUM,
    .pin_d4 = Y6_GPIO_NUM,
    .pin_d3 = Y5_GPIO_NUM,
    .pin_d2 = Y4_GPIO_NUM,
    .pin_d1 = Y3_GPIO_NUM,
    .pin_d0 = Y2_GPIO_NUM,
    .pin_vsync = VSYNC_GPIO_NUM,
    .pin_href = HREF_GPIO_NUM,
    .pin_pclk = PCLK_GPIO_NUM,
    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,
    .pixel_format = PIXFORMAT_JPEG,
    .frame_size = FRAMESIZE_QVGA,
    .jpeg_quality = 12,
    .fb_count = 1,
    .fb_location = CAMERA_FB_IN_PSRAM,
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
};
 
/* OLED Initialization */
U8X8_SSD1306_128X64_NONAME_HW_I2C u8x8(/* clock=/ 6, / data=/ 5, / reset=*/ U8X8_PIN_NONE);
 
bool ei_camera_init(void);
bool ei_camera_capture(uint32_t img_width, uint32_t img_height, uint8_t *out_buf);
static int ei_camera_get_data(size_t offset, size_t length, float *out_ptr);
String captureAndSendPhoto(camera_fb_t *fb); // Modified to return the API result
 
void setLedColor(bool red, bool green, bool blue) {
    digitalWrite(LED_R, red ? LOW : HIGH); // LOW = LED ON for XIAO
    digitalWrite(LED_G, green ? LOW : HIGH);
    digitalWrite(LED_B, blue ? LOW : HIGH);
}
 
/* I2C Scanner to Debug OLED */
void scanI2C() {
    Serial.println("Scanning I2C bus...");
    byte error, address;
    int nDevices = 0;
 
    for (address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        error = Wire.endTransmission();
 
        if (error == 0) {
            Serial.print("I2C device found at address 0x");
            if (address < 16) Serial.print("0");
            Serial.println(address, HEX);
            nDevices++;
        }
    }
 
    if (nDevices == 0) {
        Serial.println("No I2C devices found");
    } else {
        Serial.println("I2C scan complete");
    }
}
 
/* Reinitialize I2C and OLED */
void reinitializeI2C() {
    Serial.println("Reinitializing I2C and OLED...");
    Wire.end(); // End previous I2C session
    Wire.begin(5, 6); // Reinitialize I2C on SDA = GPIO 5 (D5), SCL = GPIO 6 (D4)
    u8x8.begin(); // Reinitialize OLED
    u8x8.setFlipMode(1);
    u8x8.setFont(u8x8_font_chroma48medium8_r);
    Serial.println("I2C and OLED reinitialized");
}
 
/* Setup Function */
void setup() {
    Serial.begin(115200);
    while (!Serial);
 
    // Connect to WiFi
    WiFi.begin(ssid, password);
    Serial.print("Connecting to Wi-Fi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWi-Fi connected");
    Serial.print("ESP32 IP: ");
    Serial.println(WiFi.localIP());
 
    // Initialize I2C and scan for devices
    Wire.begin(5, 6); // SDA = GPIO 5 (D5), SCL = GPIO 6 (D4)
    scanI2C();
 
    // Initialize OLED
    Serial.println("Initializing OLED...");
    u8x8.begin();
    u8x8.setFlipMode(1);
    u8x8.setFont(u8x8_font_chroma48medium8_r);
    u8x8.clear();
    u8x8.setCursor(0, 0);
    u8x8.print("OLED Test");
    Serial.println("OLED Test message displayed");
    delay(2000); // Show test message for 2 seconds
 
    u8x8.clear();
    u8x8.setCursor(0, 0);
    u8x8.print("Waiting for Btn");
    Serial.println("Waiting for Btn message displayed");
 
    // Initialize button with INPUT_PULLUP
    pinMode(BUTTON_PIN, INPUT_PULLUP);
 
    // Initialize RGB LED pins
    pinMode(LED_R, OUTPUT);
    pinMode(LED_G, OUTPUT);
    pinMode(LED_B, OUTPUT);
 
    setLedColor(false, false, false); // All OFF initially
 
    // Initialize camera
    if (!ei_camera_init()) {
        Serial.println("Failed to initialize camera!");
        u8x8.clear();
        delay(100);
        u8x8.setCursor(0, 0);
        u8x8.print("Camera Init Fail");
        delay(100);
    } else {
        Serial.println("Camera initialized");
    }
}
 
/* Main Loop */
void loop() {
    // Default OLED message when button is not pressed
    if (digitalRead(BUTTON_PIN) != LOW) {
        u8x8.clear();
        delay(100);
        u8x8.setCursor(0, 0);
        u8x8.print("Waiting for Btn");
        setLedColor(false, false, false); // Ensure LEDs are off
        delay(100); // Small delay to avoid excessive looping
        return;
    }
 
    // Button pressed: Capture image
    Serial.println("Button Pressed: Capturing Image...");
    u8x8.clear();
    delay(100); // Ensure OLED has time to clear
    u8x8.setCursor(0, 0);
    u8x8.print("Capturing...");
    delay(100); // Ensure OLED has time to display "Capturing..."
 
    delay(100); // Small debounce delay
 
    snapshot_buf = (uint8_t*)malloc(EI_CAMERA_RAW_FRAME_BUFFER_COLS * EI_CAMERA_RAW_FRAME_BUFFER_ROWS * EI_CAMERA_FRAME_BYTE_SIZE);
    if (snapshot_buf == nullptr) {
        Serial.println("Failed to allocate snapshot buffer!");
        reinitializeI2C(); // Reinitialize I2C before OLED update
        u8x8.clear();
        delay(100);
        u8x8.setCursor(0, 0);
        u8x8.print("Memory Error");
        delay(10000); // Show error for 10 seconds
        return;
    }
 
    ei::signal_t signal;
    signal.total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT;
    signal.get_data = &ei_camera_get_data;
 
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Camera capture failed");
        reinitializeI2C(); // Reinitialize I2C before OLED update
        u8x8.clear();
        delay(100);
        u8x8.setCursor(0, 0);
        u8x8.print("Capture Failed");
        free(snapshot_buf);
        delay(10000); // Show error for 10 seconds
        return;
    }
 
    if (!fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, snapshot_buf)) {
        Serial.println("Conversion failed");
        reinitializeI2C(); // Reinitialize I2C before OLED update
        u8x8.clear();
        delay(100);
        u8x8.setCursor(0, 0);
        u8x8.print("Conversion Fail");
        free(snapshot_buf);
        esp_camera_fb_return(fb);
        delay(10000); // Show error for 10 seconds
        return;
    }
 
    if (!ei_camera_capture((size_t)EI_CLASSIFIER_INPUT_WIDTH, (size_t)EI_CLASSIFIER_INPUT_HEIGHT, snapshot_buf)) {
        Serial.println("Failed to capture image");
        reinitializeI2C(); // Reinitialize I2C before OLED update
        u8x8.clear();
        delay(100);
        u8x8.setCursor(0, 0);
        u8x8.print("Capture Failed");
        free(snapshot_buf);
        esp_camera_fb_return(fb);
        delay(10000); // Show error for 10 seconds
        return;
    }
 
    ei_impulse_result_t result = {0};
    EI_IMPULSE_ERROR err = run_classifier(&signal, &result, debug_nn);
    if (err != EI_IMPULSE_OK) {
        Serial.printf("ERR: Failed to run classifier (%d)\n", err);
        reinitializeI2C(); // Reinitialize I2C before OLED update
        u8x8.clear();
        delay(100);
        u8x8.setCursor(0, 0);
        u8x8.print("Classifier Error");
        free(snapshot_buf);
        esp_camera_fb_return(fb);
        delay(10000); // Show error for 10 seconds
        return;
    }
 
    // Variable to store the detected label
    const char* detected_label = "No object found"; // Default
    bool is_healthy = false;
    bool is_diseased = false;
 
    // Process Results
#if EI_CLASSIFIER_OBJECT_DETECTION == 1
    for (uint32_t i = 0; i < result.bounding_boxes_count; i++) {
        ei_impulse_result_bounding_box_t bb = result.bounding_boxes[i];
        if (bb.value == 0) continue;
 
        // Debug: Print the raw label to check for discrepancies
        Serial.print("Raw label: [");
        Serial.print(bb.label);
        Serial.println("]");
 
        Serial.printf("Found %s (%f)\n", bb.label, bb.value);
 
        // Check for "Healthy leaf" or "Disease leaf"
        if (strcmp(bb.label, "Healthy leaf") == 0) {
            detected_label = "Healthy leaf";
            is_healthy = true;
        } else if (strcmp(bb.label, "Disease leaf") == 0) {
            detected_label = "Disease leaf";
            is_diseased = true;
        }
    }
#endif
 
    // Set LED colors based on the initial detection (before API call)
    if (is_healthy) {
        setLedColor(false, true, false); // Green for healthy
    } else if (is_diseased) {
        setLedColor(true, false, false); // Red for diseased
    } else {
        setLedColor(false, false, false); // Turn OFF for no object
    }
 
    String final_label = detected_label;
    // Send the image to the API if a leaf is detected and update the label based on API response
    if (is_healthy || is_diseased) {
        Serial.println("Sending image to API...");
        final_label = captureAndSendPhoto(fb); // Get the label from the API response
    } else {
        Serial.println("No leaf detected, skipping API call.");
    }
 
    // Update OLED Display with the final label (after API call)
    Serial.println("Attempting to update OLED with final result...");
    Serial.print("Final label to display: ");
    Serial.println(final_label);
    reinitializeI2C(); // Ensure I2C is stable before OLED update
    u8x8.clear();
    delay(200); // Increased delay to ensure OLED clears
    u8x8.setCursor(0, 0);
    Serial.println("Writing to OLED...");
    u8x8.print(final_label);
 
    // Update LED colors based on the API result
    if (final_label == "Healthy leaf") {
        setLedColor(false, true, false); // Green for healthy
    } else if (final_label == "Disease leaf") {
        setLedColor(true, false, false); // Red for diseased
    } else {
        setLedColor(false, false, false); // Turn OFF for no object or error
    }
 
    Serial.println("OLED update complete");
 
    free(snapshot_buf);
    esp_camera_fb_return(fb);
    delay(10000); // Show result for 10 seconds before returning to waiting state
}
 
/* Camera Initialization */
bool ei_camera_init(void) {
    if (is_initialised) return true;
 
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x\n", err);
        return false;
    }
 
    sensor_t * s = esp_camera_sensor_get();
    if (s->id.PID == OV3660_PID) {
        s->set_vflip(s, 1);
        s->set_brightness(s, 1);
        s->set_saturation(s, 0);
    }
 
    is_initialised = true;
    return true;
}
 
/* Capture Image */
bool ei_camera_capture(uint32_t img_width, uint32_t img_height, uint8_t *out_buf) {
    if (!is_initialised) {
        Serial.println("ERR: Camera is not initialized");
        return false;
    }
 
    if ((img_width != EI_CAMERA_RAW_FRAME_BUFFER_COLS) || (img_height != EI_CAMERA_RAW_FRAME_BUFFER_ROWS)) {
        ei::image::processing::crop_and_interpolate_rgb888(
            out_buf,
            EI_CAMERA_RAW_FRAME_BUFFER_COLS,
            EI_CAMERA_RAW_FRAME_BUFFER_ROWS,
            out_buf,
            img_width,
            img_height);
    }
 
    return true;
}
 
/* Send Image to API and Return the Result Label */
String captureAndSendPhoto(camera_fb_t *fb) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected");
        return "WiFi Error";
    }
 
    HTTPClient http;
    WiFiClient client;
 
    String boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
    String contentType = "multipart/form-data; boundary=" + boundary;
 
    String bodyStart = "--" + boundary + "\r\n";
    bodyStart += "Content-Disposition: form-data; name=\"image\"; filename=\"leaf_image.jpg\"\r\n";
    bodyStart += "Content-Type: image/jpeg\r\n\r\n";
 
    String bodyEnd = "\r\n--" + boundary + "--\r\n";
 
    int totalLen = bodyStart.length() + fb->len + bodyEnd.length();
    uint8_t* fullBody = (uint8_t*)malloc(totalLen);
    if (!fullBody) {
        Serial.println("Failed to allocate memory for request body.");
        return "Memory Error";
    }
 
    memcpy(fullBody, bodyStart.c_str(), bodyStart.length());
    memcpy(fullBody + bodyStart.length(), fb->buf, fb->len);
    memcpy(fullBody + bodyStart.length() + fb->len, bodyEnd.c_str(), bodyEnd.length());
 
    http.begin(client, serverName);
    http.addHeader("Content-Type", contentType);
 
    // Basic Auth
    String authStr = String(apiUsername) + ":" + String(apiPassword);
    String authEncoded = base64::encode(authStr);
    http.addHeader("Authorization", "Basic " + authEncoded);
 
    int httpResponseCode = http.POST(fullBody, totalLen);
    String result_label = "API Error";
 
    if (httpResponseCode > 0) {
        Serial.printf("Image sent. Response: %d\n", httpResponseCode);
        String response = http.getString();
        Serial.println("API Response: " + response);
 
        // Parse the API response (assuming it returns a string like "Healthy leaf" or "Disease leaf")
        // This is a simple parsing approach; adjust based on your API's actual response format
        if (response.indexOf("Healthy") != -1) {
            result_label = "Healthy leaf";
        } else if (response.indexOf("Disease") != -1) {
            result_label = "Disease leaf";
        } else {
            result_label = "Unknown Result";
        }
    } else {
        Serial.printf("Failed to connect. HTTP code: %d\n", httpResponseCode);
    }
 
    free(fullBody);
    http.end();
    return result_label;
}
 
/* Get Camera Data */
static int ei_camera_get_data(size_t offset, size_t length, float *out_ptr) {
    size_t pixel_ix = offset * 3;
    size_t pixels_left = length;
    size_t out_ptr_ix = 0;
 
    while (pixels_left != 0) {
        out_ptr[out_ptr_ix] = (snapshot_buf[pixel_ix + 2] << 16) + (snapshot_buf[pixel_ix + 1] << 8) + snapshot_buf[pixel_ix];
        out_ptr_ix++;
        pixel_ix += 3;
        pixels_left--;
    }
    return 0;
}