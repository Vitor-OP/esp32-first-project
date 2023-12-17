#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <Adafruit_Sensor.h>
#include <ESP32Time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#define pomodoro_trabalho 60 // 1 hour in milliseconds
#define pomodoro_pausa 15 // 15 minutes in milliseconds

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define DHTPIN 2        // Pin where the DHT22 sensor is connected
#define DHTTYPE DHT22   // Type of the DHT sensor

#define BUTTON1_PIN 26 // Replace with your ESP32's GPIO pin number
#define BUTTON2_PIN 25 // Replace with your ESP32's GPIO pin number

//for dealing with button debouncing
volatile unsigned long lastInterruptTime1 = 0;
volatile unsigned long lastInterruptTime2 = 0;
const unsigned long debounceTime = 300; // debounce time in milliseconds

// Declare a mutex
SemaphoreHandle_t mutex;

//for the timer
volatile bool startStop = false;
volatile bool reset = false;
volatile int timer_milliseconds = 0;
volatile int timer_seconds = 0;
volatile int timer_minutes = pomodoro_trabalho;


void IRAM_ATTR handleButton1Press() {
  unsigned long currentTime = millis();
  if (currentTime - lastInterruptTime1 > debounceTime) {
      startStop = !startStop;
      lastInterruptTime1 = currentTime;
  }
}

void IRAM_ATTR handleButton2Press() {
  unsigned long currentTime = millis();
  if (currentTime - lastInterruptTime2 > debounceTime) {
      reset = true;
      lastInterruptTime2 = currentTime;
  }
}

DHT dht(DHTPIN, DHTTYPE);

ESP32Time rtc(0);

void display_temperature(float temperature) {
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Temperatura: ");
  display.print(temperature);
  display.print(" C");
}

void display_time(int hour, int minute, int second) {
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 25);
  display.print("Hora: ");

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
}

void display_timer(int minute, int second, int milliseconds) {
  display.setCursor(0, 8);
  display.setTextSize(2);
  if(minute < 10)
    display.print("0");
  display.print(minute);
  display.print(":");
  if(second < 10)
    display.print("0");
  display.print(second);
  display.print(":");
  if(milliseconds < 100 && milliseconds >= 10){
    display.print("0");
  }else if(milliseconds < 10){
    display.print("00");
  }
  display.print(milliseconds);
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

    second = rtc.getSecond();
    minute = rtc.getMinute();
    hour = rtc.getHour();
    display.clearDisplay();
    display_time(hour, minute, second);

    display_timer(timer_minutes, timer_seconds, timer_milliseconds);
    
    temperature = take_temperature(temperature); // Take the temperature and stores the last one

    display_temperature(temperature);

    display.display();
    delay(50);
  }
}

// Function to run on Core 1
void core1Task(void *parameter) { // takes care of the temperature and time

  unsigned long int current_pomodoro_milis = pomodoro_trabalho*1000*60;
  unsigned long int endTime = millis() + current_pomodoro_milis;
  unsigned long int currentTime = 0;
  unsigned long int remaning_time = current_pomodoro_milis;
  int milliseconds;
  int minutes;
  int seconds;
  bool paused = false;

  while (1) {
    currentTime = millis();
    if(reset){
      reset = false;
      //if reset is pressed while the timer is running
      if(startStop){
        endTime = currentTime + current_pomodoro_milis;
        remaning_time = current_pomodoro_milis;
        timer_milliseconds = 0;
        timer_seconds = 0;
        timer_minutes = current_pomodoro_milis/1000/60;
      }else{//if reset is pressed while the timer is stopped
        if(timer_minutes == pomodoro_trabalho && timer_seconds == 0 && timer_milliseconds == 0){//changes to pause if the timer is in work
          current_pomodoro_milis = pomodoro_pausa*1000*60;
          minutes = pomodoro_pausa;
          timer_minutes = minutes;
          endTime = currentTime + current_pomodoro_milis;
          remaning_time = current_pomodoro_milis;
        }else if (timer_minutes == pomodoro_pausa && timer_seconds == 0 && timer_milliseconds == 0){//changes to work if the timer is in pause
          minutes = pomodoro_trabalho;
          current_pomodoro_milis = pomodoro_trabalho*1000*60;
          timer_minutes = minutes;
          endTime = currentTime + current_pomodoro_milis;
          remaning_time = current_pomodoro_milis;
        }else{//if the timer is in work or pause and reset is pressed
          minutes = current_pomodoro_milis/1000/60;
          seconds = 0;
          milliseconds = 0;
          timer_milliseconds = 0;
          timer_seconds = 0;
          timer_minutes = minutes;
          endTime = currentTime + current_pomodoro_milis;
          remaning_time = current_pomodoro_milis;
        }
      }
    }
    if(startStop){
      remaning_time = endTime - currentTime;
      
      seconds = remaning_time/1000;
      minutes = seconds/60;
      milliseconds = remaning_time - seconds*1000;
      seconds = seconds - minutes*60;

      timer_milliseconds = milliseconds;
      timer_seconds = seconds;
      timer_minutes = minutes;

    }else{
      endTime = currentTime + remaning_time;
    }
  }
}

void setup() {
  // Create a mutex
  mutex = xSemaphoreCreateMutex();
  Serial.begin(115200);
  rtc.setTime(0, 0, 0, 9, 12, 2023);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  dht.begin();

  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON1_PIN), handleButton1Press, FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTON2_PIN), handleButton2Press, FALLING);

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