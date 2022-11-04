#define BLYNK_TEMPLATE_ID "TMPL35W5K72p"
#define BLYNK_DEVICE_NAME "smart"

#define BLYNK_FIRMWARE_VERSION "0.1.11" // 급수일자, 급수량, 수동 급수 추가 버전

#define BLYNK_PRINT Serial

// PIN 설정
#define WATER_SENSOR_PIN 15         // 수위 센서 아날로그 핀 번호
#define SOIL_MOISTURE_SENSOR_PIN 14 // 토양 습도 센서 아날로그 핀 번호
#define ILLUMINANCE_SENSOR_PIN 13   // 조도 센서 아날로그 핀 번호
#define LED_PIN D3                  // blynk 입력 데이터 -> 하드웨어 출력 테스트 led 핀 번호
#define A_IA D6                     // 펌프
#define A_IB D7                     // 펌프
#define SIG_PIN A0                  // 멀티플렉서 아날로그핀
#define S0 D10                      // 멀티플렉서 디지털 핀
#define S1 D11                      // 멀티플렉서 디지털 핀
#define S2 D12                      // 멀티플렉서 디지털 핀
#define S3 D13                      // 멀티플렉서 디지털 핀

//#define BLYNK_DEBUG

#define APP_DEBUG

#include "BlynkEdgent.h"

BlynkTimer timer;                    // Blynk 스팸 방지 용 delay 타이머
BlynkTimer wateringIntervalDayTimer; // 급수일자 타이머
BlynkTimer waterHeightTimer;         // 물받침 수위 체크 타이머

// 센서값 변수 설정
int WaterHeight;         // 물받침 수위
int SoilMoisture;        // 토양습도
int Illuminance;         // 조도
int WateringIntervalDay; // 급수일자
int WateringDayDuration; // 급수일 시간 간격
int WaterSupplyMax;      // 최대 급수량
bool PumpState = false;  // 펌프작동 상태 플래그

// 물받침 수위 체크 타이머 설정 변수
unsigned long cur_time = 0;                         // 현재 시간 변수
unsigned long water_height_pre_time = 0;            // 수위 변수
unsigned long water_height_log_time = 0;            // 수위 로그 변수
unsigned long day_pre_time = 0;                     // 급수일 변수
unsigned long day_log_time = 0;                     // 급수일 로그 변수
unsigned long pump_pre_time = 0;                    // 펌프 변수
unsigned long pump_log_time = 0;                    // 펌프 로그 변수
const int waterHeightCheckDuration = 1 * 10 * 1000; // 수위 체크 시간 간격
const int dayHeightDuration = 1 * 10 * 1000;        // 급수일 중 물받침 수위 체크 시간 간격
const int pumpDuration = 2 * 1 * 1000;              // 급수 중 급수 로직 간격

// 아날로그 센서값 게더링 함수
void getAnalSensor()
{
  // 수위 센서 값
  WaterHeight = readMux(WATER_SENSOR_PIN);
  WaterHeight = map(WaterHeight, 0, 1000, 0, 100);
  // 토양 습도 센서 값
  SoilMoisture = readMux(SOIL_MOISTURE_SENSOR_PIN);
  SoilMoisture = map(SoilMoisture, 1023, 0, 0, 100);
  // 조도 센서 값
  Illuminance = readMux(ILLUMINANCE_SENSOR_PIN);
  Illuminance = map(Illuminance, 1023, 0, 0, 100);
}

// 센서값 Blynk 연결
// 수위 센서 값
void sendWaterHeight()
{
  Serial.println("수위: " + String(WaterHeight) + "%");
  Blynk.virtualWrite(V0, WaterHeight);
}
// 토양 습도 센서 값
void sendSoilMoisture()
{
  Serial.println("토양 습도: " + String(SoilMoisture) + "%");
  Blynk.virtualWrite(V1, SoilMoisture);
}
// 조도 센서 값
void sendIlluminance()
{
  Serial.println("조도: " + String(Illuminance) + "%");
  Blynk.virtualWrite(V7, Illuminance);
}

// 불 켜기
void ledOn()
{
  digitalWrite(LED_PIN, HIGH);
  Blynk.virtualWrite(V3, HIGH);
}
// 불 끄기
void ledOff()
{
  digitalWrite(LED_PIN, LOW);
  Blynk.virtualWrite(V3, LOW);
}

// 펌프 작동
void runPump()
{
  Blynk.virtualWrite(V2, "물 먹는 중 ^_^");
  digitalWrite(A_IA, HIGH);
  digitalWrite(A_IB, LOW);
  ledOn();
  delay(3000);
}
// 펌프 멈춤
void stopPump()
{
  digitalWrite(A_IA, LOW);
  digitalWrite(A_IB, LOW);
  ledOff();
}

// 물받침 경고 로직
void warnWaterHeightHandler()
{
  if (WaterHeight < 5)
  {
    Serial.println("물 받침 물 부족 !!!!");
    Blynk.virtualWrite(V2, "물 부족 !!!!");
  }
  else
  {
    Serial.println("물 받침 물 안 부족");
    Blynk.virtualWrite(V2, "기분 좋아여 :)");
  }
}

// 물받침 높이 체크 로직 함수
void checkWaterHeightHandler()
{
  if (PumpState == false)
  {
    if (cur_time - water_height_pre_time >= waterHeightCheckDuration)
    {
      warnWaterHeightHandler();
      water_height_pre_time = cur_time;
    }
    else
    {
      if (cur_time - water_height_log_time >= 3000)
      {
        Serial.println("물높이 체크 기다리는 중" + String(cur_time - water_height_pre_time) + "&" + String(cur_time) + "&" + String(water_height_log_time));
        water_height_log_time = cur_time;
      }
    }
  }
}

// 수동 급수
void manualWatering()
{
  if (WaterHeight < 5)
  {
    warnWaterHeightHandler();
  }
  else
  {
    runPump();
    PumpState = true;
    Serial.println("수동 급수 중");
  }
}

/*
  *** 급수 로직
  일자 간격으로 함수가 실행되므로
  한번 급수 완료된 이후에는 같은 날 습도가 최대보다 다시 낮아져도
  다시 급수되면 안 된다
*/
void supplyWaterHandler()
{
  while (SoilMoisture < WaterSupplyMax)
  {
    if (cur_time - pump_pre_time >= pumpDuration)
    {
      getAnalSensor();
      setTimerAction();

      if (WaterHeight >= 5)
      {
        runPump();
        PumpState = true;
        Serial.println("급수 중, 최대 급수량" + String(WaterSupplyMax));
      }
      else
      {
        warnWaterHeightHandler();
        stopPump();
        PumpState = false;
      }
      water_height_pre_time = cur_time;
    }
  }
  stopPump();
  PumpState = false;
  Serial.println("급수 완료 또는 급수 필요없음" + String(SoilMoisture));
}

// 급수일에 맞춰서 급수 실행 함수
void wateringDayHandler()
{
  if (cur_time - day_pre_time >= WateringDayDuration)
  {
    Serial.println("급수일 간격 실행 부 날짜" + String(WateringIntervalDay));
    Serial.println("급수일 간격 실행 부 간격" + String(WateringDayDuration));

    supplyWaterHandler();
    day_pre_time = cur_time;
  }
  else
  {
    if (cur_time - day_log_time >= 3000)
    {
      Serial.println("급수일 간격 else 날짜" + String(WateringIntervalDay));
      Serial.println("급수일 간격 else 간격" + String(WateringDayDuration));
      Serial.println("급수일 기다리는 중" + String(cur_time - day_pre_time) + "&" + String(cur_time) + "&" + String(day_log_time));
      day_log_time = cur_time;
    }
  }
}

// 타이머 적용할 콜백함수
void setTimerAction()
{
  sendWaterHeight();
  sendSoilMoisture();
  sendIlluminance();
}

// Blynk 가상 핀으로 부터 값 가져오기
BLYNK_WRITE(V3) // 펌프 led
{
  int ledValue = param.asInt();
  digitalWrite(LED_PIN, ledValue);
}

BLYNK_WRITE(V4) // 급수일자
{
  int day = param.asInt();
  WateringIntervalDay = day;
  WateringDayDuration = 1000 * 10 * day; // 급수 일자 시간 초단위 설정
  // WateringDayDuration = 1000 * 60 * 60 * 24 * day; // 급수 일자 시간 초단위 설정
  Serial.println(String(WateringIntervalDay));
}

BLYNK_WRITE(V6) // 최대 급수량
{
  int max = param.asInt();
  WaterSupplyMax = max;
  Serial.println(String(WaterSupplyMax));
}

BLYNK_WRITE(V9) // 수동 급수
{
  int wateringState = param.asInt();
  if (wateringState == 1)
  {
    manualWatering();
  }
  else
  {
    stopPump();
    PumpState = false;
    Serial.println("수동 급수 완료" + String(SoilMoisture));
  }
}

BLYNK_CONNECTED()
{
  Blynk.syncVirtual(V4); // 급수일자 동기화
  Blynk.syncVirtual(V6); // 급수량 동기화
}

void setup()
{
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT); // led
  pinMode(A_IA, OUTPUT); // 모터드라이버 모듈 단자는 출력모드 설정
  pinMode(A_IB, OUTPUT);
  pinMode(S0, OUTPUT);
  pinMode(S1, OUTPUT);
  pinMode(S2, OUTPUT);
  pinMode(S3, OUTPUT);

  digitalWrite(S0, LOW);
  digitalWrite(S1, LOW);
  digitalWrite(S2, LOW);
  digitalWrite(S3, LOW);
  delay(100);

  BlynkEdgent.begin();
  timer.setInterval(3000L, setTimerAction); // blynk timer 실행 (delay시간, 콜백함수)
}

void loop()
{
  cur_time = millis();
  getAnalSensor();

  if (PumpState == false)
  {
    stopPump();
  }
  

  BlynkEdgent.run();
  timer.run();
  wateringDayHandler();
  checkWaterHeightHandler();
  delay(1000);
}

// 멀티플렉서를 이용한 아날로그 값 반환 함수
int readMux(int channel)
{
  int controlPin[] = {S0, S1, S2, S3};
  int muxChannel[16][4] = {
      {0, 0, 0, 0}, // channel 0
      {1, 0, 0, 0}, // channel 1
      {0, 1, 0, 0}, // channel 2
      {1, 1, 0, 0}, // channel 3
      {0, 0, 1, 0}, // channel 4
      {1, 0, 1, 0}, // channel 5
      {0, 1, 1, 0}, // channel 6
      {1, 1, 1, 0}, // channel 7
      {0, 0, 0, 1}, // channel 8
      {1, 0, 0, 1}, // channel 9
      {0, 1, 0, 1}, // channel 10
      {1, 1, 0, 1}, // channel 11
      {0, 0, 1, 1}, // channel 12
      {1, 0, 1, 1}, // channel 13
      {0, 1, 1, 1}, // channel 14
      {1, 1, 1, 1}  // channel 15
  };
  // loop through the 4 sig
  for (int i = 0; i < 4; i++)
  {
    digitalWrite(controlPin[i], muxChannel[channel][i]);
  }
  // read the value at the SIG pin
  int val = analogRead(SIG_PIN); // return the value
  return val;
}
