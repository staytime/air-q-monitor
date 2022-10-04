#include <Adafruit_CCS811.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_Si7021.h>
#include <Preferences.h>

#include "display.h"
#include "fixpoint.h"
#include "ble.h"

typedef uint64_t TIMECOUNTER_T;
static const auto HOLDING_PROCESS_DELAY = 50;

// define sensor sweet
Adafruit_CCS811 ccs;
Adafruit_BMP280 bmp;
Adafruit_Si7021 hygrometer = Adafruit_Si7021();

inline void serializeWrite2Buf(uint8_t *const buf, const size_t &offset, const uint16_t& data) {
  auto temp = data;
  for(size_t i = 0; i < 2; ++i) {
    buf[offset + i] = (uint8_t) (temp & 0xff);
    temp >>= 8;
  }
}

class InstrumentState {
public:
  static const size_t SERIALZE_BUFFER_SIZE = 14;
  static const size_t OFFSET_TEMPERATURE = 0;
  static const size_t OFFSET_HUMIDITY = 2;
  static const size_t OFFSET_PRESSURE = 4;
  static const size_t OFFSET_CO2 = 8;
  static const size_t OFFSET_TVOC = 10;
  static const size_t OFFSET_BASELINE = 12;

  T_FIXPOINT_16F4 temperature;
  T_FIXPOINT_16F4 humidity;
  T_FIXPOINT_32F4 pressure;

  uint16_t co2;
  uint16_t tvoc;
  uint16_t baseline;

  InstrumentState():
    temperature(25),
    humidity(50),
    pressure(10000),
    co2(400),
    tvoc(0),
    baseline(49963)
  {}

};
uint8_t bleDataReportSerData[14] = {0, 0, 0, 0, 0,
0, 0, 0, 0, 0,
0, 0, 0, 0};

InstrumentState lastest_state;
Preferences instrument_persistent;



// All time perio is in the ms
static const TIMECOUNTER_T TS_BASE_PERIO = 200;
static const TIMECOUNTER_T TS_1_SECOND = 1000 / TS_BASE_PERIO;

static const TIMECOUNTER_T TS_BLOCK_HYGROMETER = TS_1_SECOND;
static const TIMECOUNTER_T TS_OFFSET_HYGROMETER = 0;

static const TIMECOUNTER_T TS_BLOCK_CCS811_ENV = 5 * TS_1_SECOND;
static const TIMECOUNTER_T TS_OFFSET_CCS811_ENV = 1 * TS_1_SECOND;

static const TIMECOUNTER_T TS_BLOCK_CCS811_UPDATING = 2 * TS_1_SECOND;
static const TIMECOUNTER_T TS_OFFSET_CCS811_UPDATING = 1 * TS_1_SECOND + 1;

static const TIMECOUNTER_T TS_BLOCK_CCS811_BASELINE = 60 * 60 * TS_1_SECOND;
static const TIMECOUNTER_T TS_OFFSET_CCS811_BASELINE = 30 * 60 * TS_1_SECOND;

static const TIMECOUNTER_T TS_CYCLE_SIZE = TS_BLOCK_HYGROMETER * TS_BLOCK_CCS811_ENV * TS_BLOCK_CCS811_UPDATING * TS_BLOCK_CCS811_BASELINE;
TIMECOUNTER_T time_sharing_counter = 0;

#include "esp_system.h"
#define WDT_TIMEOUT 3000 * 1000
hw_timer_t *wdt_timer = NULL;
void IRAM_ATTR resetModule() {
  Serial.printf("reboot @%d\n", time_sharing_counter);
  esp_restart();
}



void setup() {
  wdt_timer = timerBegin(0, 80, true);                  //timer 0, div 80
  timerAttachInterrupt(wdt_timer, &resetModule, true);  //attach callback
  timerAlarmWrite(wdt_timer, WDT_TIMEOUT, false);
  timerAlarmEnable(wdt_timer);                          //enable interrupt
  timerWrite(wdt_timer, 0);
  Serial.begin(9600);

  Serial.println("Init persistent storage");
  instrument_persistent.begin("istm", false);


  Serial.println("Init bmp 280");
  const auto status = bmp.begin(0x76);
  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode. */
                  Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
                  Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
                  Adafruit_BMP280::FILTER_X16,      /* Filtering. */
                  Adafruit_BMP280::STANDBY_MS_500); /* Standby time. */

  if (!status) {
    Serial.println("Could not init BMP280 :-<");
    while (true) {
      delay(HOLDING_PROCESS_DELAY);
    }
  }

  Serial.println("Init CCS811");
  if (!ccs.begin()) {
    Serial.println("Could not init CCS811 :-<");
    while (true) {
      delay(HOLDING_PROCESS_DELAY);
    }
  }

  while (!ccs.available()) {
    Serial.println("wait ccs811");
    delay(HOLDING_PROCESS_DELAY);
  }

  Serial.println("Init hygrometer");
  if (!hygrometer.begin()) {
    Serial.println("Could not init hygrometer :-<");
    while (true) {
      delay(HOLDING_PROCESS_DELAY);
    }
  }

  // disable onboard heater
  hygrometer.heater(false);

  timerWrite(wdt_timer, 0);

  if(!display.begin(SCREEN_ADDRESS, false)) {
    Serial.println(F("LCD init failed"));
    for(;;); // Don't proceed, loop forever
  }
  
  display.clearDisplay();
  delay(100);
  Serial.printf("started\n");
  display.setTextSize(1);
  display.setTextColor(1);
  display.setContrast(0);  

  for(int i = 60;i > 0; --i) {
    timerWrite(wdt_timer, 0);
    display.clearDisplay();
    delay(50);
    display.setCursor(0, 0);
    display.printf("Sensor are stablizing\n");
    display.printf("%02d", i);
    display.display();
    delay(950);
  }

  const uint16_t tmp = instrument_persistent.getUShort("ccs811.baseline", 0);
  Serial.printf("recorver ccs811 baseline %d\n", tmp);
  if(tmp != 0) {
    lastest_state.baseline = tmp;
    Serial.printf("set baseline %d\n", lastest_state.baseline);
    ccs.setBaseline(lastest_state.baseline);
  }

  init_ble();
}

// helper for checking timing
inline const bool time_share_check(const TIMECOUNTER_T &block, const uint32_t &offset) {
  //return (time_sharing_counter % block) == offset;
  return (time_sharing_counter + offset) % block == 0;
}

void loop() {
  if (time_share_check(TS_BLOCK_HYGROMETER, TS_OFFSET_HYGROMETER)) {
    {
      const T_FIXPOINT_16F4 temp = hygrometer.readTemperature();
      if(lastest_state.temperature != temp) {
        lastest_state.temperature = temp;
        Serial.printf("current temperature %.1f\n", lastest_state.temperature.to_float());

        const int p = (temp.to_float() * 100);
        bleENVSerData[0] = p & 0xff;
        bleENVSerData[1] = p >> 8;

        temperatureCharacteristic.setValue(bleENVSerData, 2);
        temperatureCharacteristic.notify();
      }
    }

    {
      const T_FIXPOINT_16F4 temp = hygrometer.readHumidity();
      if (lastest_state.humidity != temp) {
        lastest_state.humidity = temp;
        Serial.printf("current humidity %0.0f\n", temp.to_float());

        const int p = (lastest_state.humidity.to_int());
        bleENVSerData[2] = p & 0xff;
        humidityCharacteristic.setValue(&bleENVSerData[2], 1);
        humidityCharacteristic.notify();
      }
    }

    lastest_state.pressure = bmp.readPressure();
  }

  if (time_share_check(TS_BLOCK_CCS811_ENV, TS_OFFSET_CCS811_ENV)) {
    Serial.printf("update env for ccs811 %f %f\n",
                  lastest_state.humidity.to_float(),
                  lastest_state.temperature.to_float());

    ccs.setEnvironmentalData(lastest_state.humidity.to_float(), lastest_state.temperature.to_float());
  }

  if (time_share_check(TS_BLOCK_CCS811_UPDATING, TS_OFFSET_CCS811_UPDATING)) {
    if(ccs.available()) {
      if(!ccs.readData()) {
        lastest_state.co2 = ccs.geteCO2();
        lastest_state.tvoc = ccs.getTVOC();
        lastest_state.baseline = ccs.getBaseline();

        serializeWrite2Buf(bleDataReportSerData, 0, lastest_state.temperature);
        serializeWrite2Buf(bleDataReportSerData, 2, lastest_state.humidity);
        serializeWrite2Buf(bleDataReportSerData, 4, lastest_state.pressure);
        serializeWrite2Buf(bleDataReportSerData, 8, lastest_state.co2);
        serializeWrite2Buf(bleDataReportSerData, 10, lastest_state.tvoc);
        serializeWrite2Buf(bleDataReportSerData, 12, lastest_state.baseline);

        pDataReportCharacteristic->setValue(bleDataReportSerData, lastest_state.SERIALZE_BUFFER_SIZE);
        pDataReportCharacteristic->notify();
      }
    }
  }

  if(time_share_check(TS_BLOCK_CCS811_BASELINE, TS_OFFSET_CCS811_BASELINE)) {
    instrument_persistent.putUShort("ccs811.baseline", lastest_state.baseline);
    Serial.printf("store ccs811 baseline %d\n", lastest_state.baseline);
  }

  // handle time sharing counter
  ++time_sharing_counter;
  time_sharing_counter %= TS_CYCLE_SIZE;

  display.clearDisplay();
  delay(50);
  display.setCursor(0, 0);
  display.printf("Temperature: %dC\n", lastest_state.temperature.to_int());
  display.printf("Humidity: %d%%\n", lastest_state.humidity.to_int());
  display.printf("Pressure: %.1f hPa\n", lastest_state.pressure.to_float() / 100.0);
  display.printf("CO2: %dppm\n", lastest_state.co2);
  display.printf("TVOC: %dppb\n", lastest_state.tvoc);
  display.printf("Baseline: %d\n", lastest_state.baseline);
  if(ble_connection_status) {
    display.printf("BLE CONNECT: YES\n");
  }
  else {
    display.printf("BLE CONNECT: NO\n");
  }
  display.printf("TS: %d\n", time_sharing_counter);

  display.display();



  if (ble_restart_adv_flag) {
    Serial.println("restart advertising");
    pBLEServer->startAdvertising();
    ble_restart_adv_flag = false;
    delay(HOLDING_PROCESS_DELAY);
  }
  timerWrite(wdt_timer, 0);
  delay(TS_BASE_PERIO - 50);
}
