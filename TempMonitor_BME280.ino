#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#include <FirebaseArduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include<time.h>

#include "Secrets.h"

#define UTC_OFFSET -10800
#define FIREBASE_SENSOR_ID 2

Adafruit_BME280 bme;

LiquidCrystal_I2C lcd(0x27,16,2);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", UTC_OFFSET);
struct tm ts, previous_ts;

tm get_current_date(){
  timeClient.update();
  time_t rawtime = timeClient.getEpochTime();
  
  return *gmtime(&rawtime);
}

int temp_counter = 0;
float temps[24];
int hum_counter = 0;
float hums[24];

char str_buffer[80];

byte custom[8] = {
  0b00111,          // Caractere customizado
  0b00101,
  0b00111,
  0b00000,
  0b00000,
  0b00000,
  0b00000,
  0b00000
};

void setup() {
  Serial.begin(115200);
  lcd.begin(16, 2);
  lcd.init();
  lcd.setBacklight(HIGH);
  lcd.createChar(0, custom);
  
  // Conecting to WIFI
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("\nConectando");

  int conn_i = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    lcd.setCursor(0,0);
    lcd.print("Conectando");
    lcd.setCursor(0,1);
    for (int i = 0; i < conn_i; ++i) {
      lcd.print(".");
    }
    if (++conn_i > 16) conn_i = 0;
  }
  
  Serial.println("\nConnectado");
  Serial.print ("IP: ");
  Serial.println(WiFi.localIP());
  
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Conectado");
  lcd.setCursor(0,1);
  lcd.print(WiFi.localIP());

  delay(2000);

  // Sensor test
  Serial.println(F("BME280 test"));

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Teste sensor");
  lcd.setCursor(0,1);

  bool status = bme.begin(0x76);  
  if (!status) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    lcd.print("Fail");
  } else {
    lcd.print("Ok");
  }
  delay(2000);

  // Firebase
  Serial.println(F("Firebase test"));
  
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Teste Firebase");
  lcd.setCursor(0,1);
  
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  if (Firebase.getInt("teste") == 1) {
    Serial.println("Could not get value from Firebase");
    lcd.print("Ok");
  } else {
    lcd.print("Fail");
  }
  delay(2000);

  // Current Time
  Serial.println(F("Time test"));
  
  ts = get_current_date();
  previous_ts = ts;

  strftime(str_buffer, 80, "%F/%H/%M", &previous_ts);
  Serial.print("Current time is ");
  Serial.println(str_buffer);
  
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Current Time");
  lcd.setCursor(0,1);
  lcd.print(str_buffer);

  delay(2000);
}

int delay_counter = -1;
int light_counter = 0;
int has_light = 1;

void loop() {
  if (++delay_counter % 50 == 0){
    delay_counter = 0;
    readValuesBME280();

    ts = get_current_date();

    // If the minute changed, save the values
    if (previous_ts.tm_min != ts.tm_min) {
      saveValues();
  
      // Clear data counter
      temp_counter = 0;
      // Clear data counter
      hum_counter = 0;
  
      // update time
      previous_ts = ts;
    }
  }
  
  if (!digitalRead(0)) {
    has_light = 1;
    light_counter = 0;
    lcd.setBacklight(HIGH);
  } else {
    if (has_light) {
      if (++light_counter % 50 == 0) {
        has_light = 0;
        lcd.setBacklight(LOW);
      }
    }
  }

  delay(100);
}

void readValuesBME280() {
  float temp = bme.readTemperature();
  float hum = bme.readHumidity();
  
  Serial.println("BME280:");
  Serial.print("Temperature = ");
  Serial.print(temp);
  Serial.println(" *C");

  Serial.print("Humidity = ");
  Serial.print(hum);
  Serial.println(" %");

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Temp:");
  lcd.setCursor(6,0);
  lcd.print(temp);
  lcd.write(0);
  lcd.print("C");
  lcd.setCursor(0,1);
  lcd.print("Hum:");
  lcd.setCursor(6,1);
  lcd.print(hum);
  lcd.print("%");

  if (temp_counter == 0) {
      temps[0] = temp;
      temp_counter++;
      return;
  } else if (temp_counter < 12) {
    // Insert into the last position by default
    temps[temp_counter] = temp; 
    
    // Check the correct position
    for (int i = 0; i < temp_counter; ++i) {
      if (temp < temps[i]) {
        // Moving all bigger to new position
        for (int j = temp_counter; j > i; --j) {
          temps[j] = temps[j - 1];
        }
        temps[i] = temp;
        break;
      }
    }
    temp_counter++;
  }

  if (hum_counter == 0) {
      hums[0] = hum;
      hum_counter++;
      return;
  } else if (hum_counter < 12) {
    // Insert into the last position by default
    hums[hum_counter] = hum; 
    
    // Check the correct position
    for (int i = 0; i < hum_counter; ++i) {
      if (hum < hums[i]) {
        // Moving all bigger to new position
        for (int j = hum_counter; j > i; --j) {
          hums[j] = hums[j - 1];
        }
        hums[i] = hum;
        break;
      }
    }
    hum_counter++;
  }
}

void saveValues() {
  char aux_buffer[80];

  // Temperatue
  if (temp_counter <= 2) {
    return;  
  }

  float sum = 0;
  for (int i = 1; i < temp_counter - 1; ++i) {
    sum += temps[i];
  }
  float saved_value = sum / (temp_counter - 2);
  
  // Getting formated date as YYYY-MM-DD/hh/mm to set the correct path
  strftime(aux_buffer, 80, "%F/%H/%M", &previous_ts);
  sprintf(str_buffer, "temp/%d/%s", FIREBASE_SENSOR_ID, aux_buffer);

  // set the value as the average ignoring both the lower and higher values
  Firebase.setFloat(str_buffer, saved_value);

  Serial.print("Salvando no path ");
  Serial.print(str_buffer);
  Serial.print(" o valor ");
  Serial.println(saved_value);

  // Temperatue
  if (hum_counter <= 2) {
    return;  
  }

  sum = 0;
  for (int i = 1; i < hum_counter - 1; ++i) {
    sum += hums[i];
  }
  saved_value = sum / (hum_counter - 2);
  
  // Getting formated date as YYYY-MM-DD/hh/mm to set the correct path
  strftime(aux_buffer, 80, "%F/%H/%M", &previous_ts);
  sprintf(str_buffer, "hum/%d/%s", FIREBASE_SENSOR_ID, aux_buffer);

  // set the value as the average ignoring both the lower and higher values
  Firebase.setFloat(str_buffer, saved_value);

  Serial.print("Salvando no path ");
  Serial.print(str_buffer);
  Serial.print(" o valor ");
  Serial.println(saved_value);
}
