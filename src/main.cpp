#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <Adafruit_Sensor.h>
#include <ESP32Time.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define DHTPIN 2        // Pin where the DHT22 sensor is connected
#define DHTTYPE DHT22   // Type of the DHT sensor

DHT dht(DHTPIN, DHTTYPE);

ESP32Time rtc(0);

void display_temperature(float temperature) {
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Temperatura:");

  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 10);
  display.print(temperature);
  display.print(" C");

  display.display();
}

void display_time(int hour, int minute, int second) {
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Hora:");

  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 10);

  if(hour < 10)
    display.print("0");
  display.print(hour);
  display.print(":");

  if(minute < 10)
    display.print("0");
  display.print(minute);
  display.print(":");
  
  if(second < 10)
    display.print("0");
  display.print(second);

  display.display();
}

float take_temperature(float previous_temperature) {
  float temperature = dht.readTemperature();
  if (isnan(temperature)) {
    Serial.println("Failed to read temperature from DHT sensor!");
    return previous_temperature;
  }else{
    return temperature;
  }
}

// Function to run on Core 0
void core0Task(void *parameter) { // takes care of the display
  
  float temperature = 0.0;
  int hour = 0;
  int minute = 0;
  int second = 0;
  int entry_sec = 0;

  while (1) {

    while(1){
      second = rtc.getSecond();
      minute = rtc.getMinute();
      hour = rtc.getHour();
      display.clearDisplay();
      display_time(hour, minute, second);
      if(second%10 == 0 && second != entry_sec){
        entry_sec = second;
        break;
      }
      delay(50);
    }

    while(1){
      temperature = take_temperature(temperature); // Take the temperature and stores the last one
      display.clearDisplay();
      display_temperature(temperature);
      second = rtc.getSecond();
      if(second%10 == 0 && second != entry_sec){
        entry_sec = second;
        break;
      }
      delay(50);
    }
  }
}

// Function to run on Core 1
void core1Task(void *parameter) { // takes care of the temperature and time
  float temperature1 = 0.0;
  while (1) {
    delay(100);
  }
}

void setup() {
  Serial.begin(115200);
  rtc.setTime(0, 0, 0, 9, 12, 2023);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  dht.begin();

  // Create tasks for both cores
  xTaskCreatePinnedToCore(
    core0Task,      // Function to run on Core 0
    "Core0Task",    // Task name
    10000,          // Stack size (bytes)
    NULL,           // Task parameters
    1,              // Priority
    NULL,           // Task handle
    0               // Core ID (Core 0)
  );

  xTaskCreatePinnedToCore(
    core1Task,      // Function to run on Core 1
    "Core1Task",    // Task name
    10000,          // Stack size (bytes)
    NULL,           // Task parameters
    1,              // Priority
    NULL,           // Task handle
    1               // Core ID (Core 1)
  );
}

void loop() {
}