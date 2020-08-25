
#include <ESP8266WiFi.h>
#include <DHTesp.h>
#include <FirebaseArduino.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include "Secrets.h"

#include<time.h>

#define UTC_OFFSET -10800

#define COUNTER_LIMIT 12

#define SENSOR1_PORT 0
#define SENSOR2_PORT 2

DHTesp dhts[2];
int test = 0;

struct tm ts, previous_ts;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", UTC_OFFSET);

float temps[24];
float hums[24];

int counter[2];

char str_buffer[80];

void setup()
{
  dhts[0].setup(SENSOR1_PORT, DHTesp::DHT22);
  dhts[1].setup(SENSOR2_PORT, DHTesp::DHT22);

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

  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  test = Firebase.getInt("teste");
  Serial.print ("Test value from FIREBASE (should be 1): ");
  Serial.println(test);

  timeClient.begin();

  counter[0] = counter[1] = 0;
}

tm get_current_date(){
  timeClient.update();
  time_t rawtime = timeClient.getEpochTime();
  
  return *gmtime(&rawtime);
}

void read_sensor_values(int n){
  if (counter[n] >= COUNTER_LIMIT) return;
  
  TempAndHumidity v = dhts[n].getTempAndHumidity();
  if (dhts[n].getStatus() != 0) return;

  Serial.print("Read values: ");
  Serial.println(v.humidity);
  Serial.println(v.temperature);

  if (counter[n] == 0) {
    temps[n * 12] = v.temperature;
    hums[n * 12] = v.humidity;
    counter[n]++;
    return;
  }
  int added = 0;
  for (int i = n * 12; i < n * 12 + counter[n]; ++i){
    if (v.temperature < temps[i]) {
      for (int j = n * 12 + counter[n]; j > i; --j) {
        temps[j] = temps[j - 1];
      }
      temps[i] = v.temperature;
      added = 1;
      break;
    }
  }
  if (!added) temps[n * 12 + counter[n]] = v.temperature;

  added = 0;
  for (int i = n * 12; i < n * 12 + counter[n]; ++i){
    if (v.humidity < hums[i]) {
      for (int j = n * 12 + counter[n]; j > i; --j) {
        hums[j] = hums[j - 1];
      }
      hums[i] = v.humidity;
      added = 1;
      break;
    }
  }
  if (!added) hums[n * 12 + counter[n]] = v.humidity;
  
  counter[n]++;
}

void saveMedianValues(int n) {
  if (counter[n] <= 2) return;
  int mid = n / 2;
  char temp_buffer[80];

  strftime(temp_buffer, 80, "%F/%H/%M", &previous_ts);
  sprintf(str_buffer, "hum%d/%s", n, temp_buffer);
  Firebase.setFloat(str_buffer, hums[n * 12 + mid]);
  Serial.print("Salvando no path ");
  Serial.print(str_buffer);
  Serial.print(" o valor ");
  Serial.println(hums[n * 12 + mid]);
  
  strftime(temp_buffer, 80, "%F/%H/%M", &previous_ts);
  sprintf(str_buffer, "temp%d/%s", n, temp_buffer);
  Firebase.setFloat(str_buffer, temps[n * 12 + mid]);
  Serial.print("Salvando no path ");
  Serial.print(str_buffer);
  Serial.print(" o valor ");
  Serial.println(temps[n * 12 + mid]);
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
  Serial.print("Humidades: ");
  for (int i = n * 12 ; i < n * 12 + counter[n]; ++i) {
    Serial.print(hums[i]);
    Serial.print(" ");
  }
  Serial.println();
}

void loop() 
{
  ts = get_current_date();

  if (previous_ts.tm_min != ts.tm_min) {
    saveMedianValues(0);
    saveMedianValues(1);
    counter[0] = counter[1] = 0;
    
    previous_ts = ts;
  } else {
    read_sensor_values(0);
    read_sensor_values(1);
  }
  strftime(str_buffer, 80, "%x %X", &ts);
  Serial.println(str_buffer);
  print_info(0);
  Serial.println("-----------");
  print_info(1);
  Serial.println("-----------------------------");
  
  delay(5000);
}
