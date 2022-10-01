// Comment this out to disable prints and save space
#define BLYNK_PRINT Serial

// ESP8266 baud rate:
#define ESP8266_BAUD 9600

// PIN 설정
#define WATER_SENSOR_PIN A0 // 수위 센서 아날로그 핀 번호
#define DHT_SEONSOR_PIN 7 // 온습도 센서 디지털핀 번호
#define LED_PIN 11 // blynk 입력 데이터 -> 하드웨어 출력 테스트 led 핀 번호
#define PIEZO_PIN 12 // 피에조 핀

#include <ESP8266_Lib.h>
#include <BlynkSimpleShieldEsp8266.h>
#include "DHT.h"
#include <SoftwareSerial.h>
#include "index.h"

// Software Serial 핀 설정
SoftwareSerial EspSerial(2, 3); // RX, TX
ESP8266 wifi(&EspSerial);

DHT dht; // 온습도 센서 라이브러리
BlynkTimer timer; // Blynk 스팸 방지 용 delay 타이머

// 센서값 변수 설정
int Temperature;
int Humidity;
int WaterHeight;

// 수분 습도 dht 센서값 
void sendDHT()
{
  // Print serial messages
  Serial.print("습도: " + String(Humidity) + " %");
  Serial.print("\t");
  Serial.println("온도: " + String(Temperature) + " C");

  // Update Blynk data
  Blynk.virtualWrite(V0, Temperature);
  Blynk.virtualWrite(V1, Humidity);
}

// 수위 센서 값
void sendWaterHeight()
{
  Serial.println("수위: " + String(WaterHeight));
  Blynk.virtualWrite(V2, WaterHeight);
}

// 타이머 적용할 콜백함수
void setTimerAction(){
  sendWaterHeight();
  sendDHT();
}

// 초기 설정
void setup()
{
  // Debug console baud rate 설정으로 시리얼 모니터와 일치시켜야함 
  Serial.begin(9600); // 시리얼 통신 연결

  // PIN 연결
  dht.setup(DHT_SEONSOR_PIN); // 온습도
  pinMode(LED_PIN, OUTPUT); // led
  pinMode(PIEZO_PIN, OUTPUT); // led

  // ESP8266 baud rate 설정
  EspSerial.begin(ESP8266_BAUD);

  delay(10);

// Blynk 서버 연결
  Blynk.begin(auth, wifi, ssid, pass, "blynk.cloud", 80);
  timer.setInterval(3000L, setTimerAction); // blynk timer 실행 (delay시간, 콜백함수)
}

/* 
가상 핀 V3 값 변화가 생길 때마다 실행되는 Blynk 내장 함수
값을 이용해 특정 동작을 하드웨어에서 실행시킬 수 있음
가상 핀을 이용해 가상핀 1개의 변경 상황에 여러가지 하드웨어 동작을 실행시킬 수 있음 
*/ 
BLYNK_WRITE(V3) // 핀번호 파라미터로 연결
{
  /*
  Blynk.Cloud 상의 데이터 값을 param으로 읽어올 수 있으며
  asInt, asStrng, asFloat 사용가능
  */
  if(param.asInt() == 1)
  {
    digitalWrite(LED_PIN, HIGH);
  }
  else
  {
    digitalWrite(LED_PIN, LOW);
  }
}

// 실행 루프
void loop()
{
  Temperature = dht.getTemperature(); // 온도 변수 값 설정
  Humidity = dht.getHumidity(); // 습도 변수 값 설정
  WaterHeight = analogRead(WATER_SENSOR_PIN); // 수위 변수 값 설정

  // 수위 400 이상인 경우 0.5초간 피에조 부저 경고음 울림
  if (WaterHeight > 400)
  {
    digitalWrite(PIEZO_PIN, HIGH);
    delay(500);
    digitalWrite(PIEZO_PIN, LOW);
  }
  
  Blynk.run();
  timer.run();
  delay(1000);  
}
