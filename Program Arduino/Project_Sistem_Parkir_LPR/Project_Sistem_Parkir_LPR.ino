#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <SoftwareSerial.h>

SoftwareSerial barcode(5,4); // RX/D1 | TX/D2

String inputString = "";  
bool stringComplete = false;  

char ssid[] = "HIMATE";     // your network SSID (name)
char password[] = "BukanWIFIgratis"; // your network key
String ip_server = "http://192.168.43.60";
String url_masuk = "/project/c_masuk?idcard=";
String url_keluar = "/project/c_keluar?idcard=";
String id_nim = "";
String id_plate = "";
int flag = 0;
int flag_kirim = 0;
int flag_output = 0;
int flag_pintu = 0;
unsigned long prevmilis = 0;
unsigned long timemilis = 0;

int set_jarak = 50; // 20cm

const int echo1 = 14; // Pin Echo Ultrasonik 1 /D5
const int trig1 = 16; // Pin Trigger Ultrasonik 1 /D0
const int echo2 = 13; // Pin Echo Ultrasonik 2 /D7
const int trig2 = 15; // Pin Trigger Ultrasonik 2 /D8
const int limit_up = 12; // Limit Switch Atas /D6
const int limit_down = 10; // Limit Switch Bawah /SD3
const int motor_right = 0; // BTS7960 Right /D3
const int motor_left = 2; // BTS7960 Left /D4

// Disini fungsi untuk membaca jarak menggunakan sensor ultrasonik
unsigned int ukur_jarak(const int trigger,const int  echo)
{
  digitalWrite(trigger, LOW);                   
  delayMicroseconds(2);
  digitalWrite(trigger, HIGH);                  
  delayMicroseconds(10);
  digitalWrite(trigger, LOW);                   
  unsigned int timer = pulseIn(echo, HIGH);        
  unsigned int jarak= timer/58;   
  return jarak;    
}

void setup() {
  Serial.begin(9600);
  barcode.begin(9600); 
  pinMode(trig1, OUTPUT); 
  pinMode(echo1, INPUT); 
  pinMode(trig2, OUTPUT); 
  pinMode(echo2, INPUT); 
  pinMode(limit_up, INPUT_PULLUP); 
  pinMode(limit_down, INPUT_PULLUP); 
  pinMode(motor_right, OUTPUT); 
  pinMode(motor_left, OUTPUT); 
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.println();
  Serial.print("Connecting to Wifi ...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  prevmilis = millis();
  timemilis = millis();
}

void loop() {
  while (Serial.available()) {
    // get the new byte:
    char inChar = (char)Serial.read();
    if (inChar == '\r') {
      stringComplete = false;
    } else if (inChar == '\n') {
      stringComplete = true;
    } else {
      inputString += inChar;
    }
  }
  if(stringComplete) {
    Serial.println(inputString);
    int len = inputString.length();
    id_plate = inputString.substring(0,len-1);
    inputString = "";
    stringComplete = false;
  }
  
  if(millis()-timemilis>=2000) {
    int jarak_masuk = ukur_jarak(trig1,echo1);
    Serial.print("jarak_masuk = ");
    Serial.println(jarak_masuk);
    int jarak_keluar = ukur_jarak(trig2,echo2);
    Serial.print("jarak_keluar = ");
    Serial.println(jarak_keluar);
    if(jarak_masuk<=set_jarak) {
      flag_output = 1;
    } else if(jarak_keluar<=set_jarak) {
      flag_output = 2;
    }
    if(flag_pintu==1 || flag_pintu==2) {
      id_nim = "";
      id_plate = "";
      flag_kirim = 0;
      flag_pintu = 0;
      while(jarak_masuk<=set_jarak || jarak_keluar<=set_jarak) {
        jarak_masuk = ukur_jarak(trig1,echo1);
        Serial.print("jarak_masuk = ");
        Serial.println(jarak_masuk);
        jarak_keluar = ukur_jarak(trig2,echo2);
        Serial.print("jarak_keluar = ");
        Serial.println(jarak_keluar);
        if(digitalRead(limit_up)==HIGH) {
          Serial.println("Palang Terbuka");
          digitalWrite(motor_right,HIGH); // palang terbuka
          digitalWrite(motor_left,LOW);
        } else {
          Serial.println("Motor Berhenti");
          digitalWrite(motor_right,LOW); // motor off
          digitalWrite(motor_left,LOW);
        }
        delay(100);
        yield();
      }
      for(int x=0;x<=10;x++) {
        Serial.println("Proses Object Menjauh");
        delay(100);
        yield();
      }
      while(analogRead(A0)!=1024) {
        Serial.println("Palang Menutup");
        digitalWrite(motor_right,LOW); // palang tertutup
        digitalWrite(motor_left,HIGH);
        delay(100);
        yield();
      }
      Serial.println("Motor Berhenti");
      digitalWrite(motor_right,LOW); // motor off
      digitalWrite(motor_left,LOW);
    }
    yield();
    timemilis = millis();
  }
  while(barcode.available()>0) {
    String data_ = barcode.readString();
    id_nim = data_.substring(0,data_.length()-1);
    //char inChar = (char)barcode.read();
    if(flag_kirim==0) {
      //id_card += String(inChar);
      flag = 1;
    }
    prevmilis = millis();
  }
  if(millis()-prevmilis>2000 && flag==1) {
    flag_kirim = 1;
    flag = 0;
    Serial.print("id_card : ");
    Serial.println(id_nim);
    delay(100);
  }
  // wait for WiFi connection
  if (WiFi.status() == WL_CONNECTED && flag_kirim==1 && flag_output!=0 && id_plate!="") {
    if(flag_output==1) {
      HTTPClient http;
      Serial.print("[HTTP] begin...\n");
      String http_ = ip_server + url_masuk + id_nim + "&idplate=" + id_plate;
      Serial.println(http_);
      http.begin(http_);
      Serial.print("[HTTP] GET...\n");
      // start connection and send HTTP header
      int httpCode = http.GET();
      if (httpCode > 0) {
        Serial.printf("[HTTP] GET... code: %d\n", httpCode);
        if (httpCode == HTTP_CODE_OK) {
          id_nim = "";
          id_plate = "";
          String payload = http.getString();
          Serial.println(payload);
          if(payload=="ok") {
            Serial.println("Palang Terbuka");
            flag_pintu = 1;
            flag_kirim = 2;
          } else if(payload=="error") {
            Serial.println("Tidak Terdaftar");
            flag_kirim = 0;
          }
        } else{
           Serial.println("Tidak Terdaftar");
           flag_kirim = 0;
        }
      } else {
        Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
        delay(5000);
      }
      http.end();
    } else if(flag_output==2) {
      HTTPClient http;
      Serial.print("[HTTP] begin...\n");
      String http_ = ip_server + url_keluar + id_nim + "&idplate=" + id_plate;
      Serial.println(http_);
      http.begin(http_);
      Serial.print("[HTTP] GET...\n");
      // start connection and send HTTP header
      int httpCode = http.GET();
      if (httpCode > 0) {
        Serial.printf("[HTTP] GET... code: %d\n", httpCode);
        if (httpCode == HTTP_CODE_OK) {
          id_nim = "";
          id_plate = "";
          String payload = http.getString();
          Serial.println(payload);
          if(payload=="ok") {
            Serial.println("Palang Terbuka");
            flag_pintu = 2;
            flag_kirim = 2;
          } else if(payload=="error") {
            Serial.println("Data Tidak Cocok");
            flag_kirim = 0;
          }
        }
      } else {
        Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
        delay(5000);
      }
      http.end();
    }
  }
}
