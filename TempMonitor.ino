#include <ESP8266WiFi.h>
#include <DHTesp.h>
#include <FirebaseArduino.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include<time.h>

#include "Secrets.h"

#define UTC_OFFSET -10800

#define COUNTER_LIMIT 12

#define SENSOR1_PORT 2
#define SENSOR2_PORT 0

DHTesp dhts[2];
float temps[24];
int counter[2];
int error_counter[2];

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", UTC_OFFSET);
struct tm ts, previous_ts;

char str_buffer[80];

void setup()
{
  dhts[0].setup(SENSOR1_PORT, DHTesp::DHT22);
//  dhts[1].setup(SENSOR2_PORT, DHTesp::DHT22);

  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.println("");
  Serial.print("Conectando");

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.println("Connectado");
  Serial.print ("IP: ");
  Serial.println(WiFi.localIP());

  // Just connect and do a simple check
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  int test = Firebase.getInt("teste");
  
  Serial.print ("Test value from FIREBASE (should be 1): ");
  Serial.println(test);

  // Start the time client
  timeClient.begin();

  counter[0] = counter[1] = 0;
}

tm get_current_date(){
  timeClient.update();
  time_t rawtime = timeClient.getEpochTime();
  
  return *gmtime(&rawtime);
}

void read_sensor_values(int n) {
  if (counter[n] >= COUNTER_LIMIT) return;
  
  float temperature = dhts[n].getTemperature();

  // Checking for errors
  if (dhts[n].getStatus() != 0) {
    Serial.print("Error with sensor: ");
    Serial.print(n);
    Serial.print("\t\t");
    Serial.println(dhts[n].getStatusString());
    
    switch(dhts[n].getStatus()) {
      case DHTesp::ERROR_TIMEOUT: {
        error_counter[0]++;
        break;
      }
      case DHTesp::ERROR_CHECKSUM: {
        error_counter[1]++;
        break;
      }
      default: break;
    }
    return;
  }

  Serial.print("Temperature value: \t\t");
  Serial.println(temperature);

  // Insert using insertion sort
  if (counter[n] == 0) {
    temps[n * 12] = temperature;
    counter[n]++;
    return;
  } else {
    // Insert into the last position by default
    temps[n * 12 + counter[n]] = temperature; 
    
    // Check the correct position
    for (int i = n * 12; i < n * 12 + counter[n]; ++i) {
      if (temperature < temps[i]) {
        // Moving all bigger to new position
        for (int j = n * 12 + counter[n]; j > i; --j) {
          temps[j] = temps[j - 1];
        }
        temps[i] = temperature;
        break;
      }
    }
  }
  counter[n]++;
}

void saveErrorValues() {
  char temp_buffer[80];
  
  if (error_counter[0] != 0) {
    strftime(temp_buffer, 80, "%F/%H/%M", &previous_ts);
    sprintf(str_buffer, "error_timeout/%s", temp_buffer);
    Firebase.setInt(str_buffer, error_counter[0]);
  }
  if (error_counter[1] != 0) {
    strftime(temp_buffer, 80, "%F/%H/%M", &previous_ts);
    sprintf(str_buffer, "error_checksum/%s", temp_buffer);
    Firebase.setInt(str_buffer, error_counter[1]);
  }
}

void saveValues(int n) {
  char temp_buffer[80];
  
  // Getting formated date as YYYY-MM-DD/hh/mm to set the correct path
  strftime(temp_buffer, 80, "%F/%H/%M", &previous_ts);
  sprintf(str_buffer, "status/%d/%s", n, temp_buffer);
  Firebase.setInt(str_buffer, counter[n]);
  
  if (counter[n] <= 2) {
    return;  
  }
  
  float sum = 0;
  for (int i = 1; i < counter[n] - 1; ++i) {
    sum += temps[n * 12 + i];
  }
  float saved_value = sum / (counter[n] - 2);
  
  // Getting formated date as YYYY-MM-DD/hh/mm to set the correct path
  strftime(temp_buffer, 80, "%F/%H/%M", &previous_ts);
  sprintf(str_buffer, "temp/%d/%s", n, temp_buffer);

  // set the value as the average ignoring both the lower and higher values
  Firebase.setFloat(str_buffer, saved_value);

  Serial.print("Salvando no path ");
  Serial.print(str_buffer);
  Serial.print(" o valor ");
  Serial.println(saved_value);
}

void print_info(int n) {
  Serial.print("Sensor ");
  Serial.println(n);
  Serial.print("Medidas lidas: ");
  Serial.println(counter[n]);
  Serial.print("Temperaturas: ");
  for (int i = n * 12 ; i < n * 12 + counter[n]; ++i) {
    Serial.print(temps[i]);
    Serial.print(" ");
  }
  Serial.println();
}

void loop() 
{
  delay(dhts[0].getMinimumSamplingPeriod());
  
  ts = get_current_date();

  // If the minute changed, save the values
  if (previous_ts.tm_min != ts.tm_min) {
    saveValues(0);
//    saveValues(1);
    saveErrorValues();

    // Clear data counter
    counter[0] = counter[1] = 0;
    error_counter[0] = error_counter[1] = 0;

    // update time
    previous_ts = ts;
  }
  
  read_sensor_values(0);
//  read_sensor_values(1);

  strftime(str_buffer, 80, "%x %X", &ts);
  Serial.println(str_buffer);
  print_info(0);
  Serial.println("-----------");
  print_info(1);
  Serial.println("-----------------------------");
  
  delay(2000);
}
