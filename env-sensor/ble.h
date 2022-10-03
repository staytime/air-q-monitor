#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>


static const auto BLE_CH_RN = BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY;


BLEServer *pBLEServer = NULL;
BLEService *envService = NULL;

static const BLEUUID GATT_UUID_ENV((uint16_t) 0x181A);
BLECharacteristic temperatureCharacteristic(BLEUUID((uint16_t)0x2A6E), BLE_CH_RN);
BLECharacteristic humidityCharacteristic(BLEUUID((uint16_t)0x2A6f), BLE_CH_RN);
uint8_t bleENVSerData[3] = {0, 0, 0};

static const BLEUUID GATT_UUID_BATTERY((uint16_t) 0x180F);
BLEService *batteryService = NULL;
BLECharacteristic batteryCharacteristic(BLEUUID((uint16_t) 0x2A19), BLE_CH_RN);
uint8_t bleBATSerData[1] = {100};

// const BLEUUID GATT_UUID_DEVICE_INFO((uint16_t) 0x180A);
// BLEService *deviceInformationService = NULL;
// BLECharacteristic modelNumberStringCharacteristic(BLEUUID((uint16_t)0x2A24),
//                                                   BLECharacteristic::PROPERTY_READ);

static const char SERVICE_UUID_DATAREPORT[] = "e384bb23-1e86-4f7d-b652-8ff6a8c659be";
static const char GATT_UUID_FULLREPORT[] = "39349961-56e3-4cce-a388-24b9d09cd2f9";
BLEService *pDataReportService = NULL;
BLECharacteristic *pDataReportCharacteristic = NULL;

bool ble_connection_status = false;
bool ble_restart_adv_flag = false;
class MyServerCallbacks : public BLEServerCallbacks {

  void onDisconnect(BLEServer *pServer) {
    // request restarting advertising
    ble_restart_adv_flag = true;
    ble_connection_status = false;
  }
  void onConnect(BLEServer* pServer) {
    ble_connection_status = true;
  }
};

void init_ble() {
  BLEDevice::init("ESP32ENV");
  pBLEServer = BLEDevice::createServer();
  pBLEServer->setCallbacks(new MyServerCallbacks());



  envService = pBLEServer->createService(GATT_UUID_ENV);
  envService->addCharacteristic(&temperatureCharacteristic);
  temperatureCharacteristic.addDescriptor(new BLE2902());

  envService->addCharacteristic(&humidityCharacteristic);
  humidityCharacteristic.addDescriptor(new BLE2902());



  batteryService = pBLEServer->createService(GATT_UUID_BATTERY);
  batteryService->addCharacteristic(&batteryCharacteristic);
  batteryCharacteristic.setValue(bleBATSerData, 1);
  batteryCharacteristic.addDescriptor(new BLE2902());
  


  // deviceInformationService = pBLEServer->createService(deviceInformationUUID);
  // deviceInformationService->addCharacteristic(&modelNumberStringCharacteristic);
  // modelNumberStringCharacteristic.setValue((DEVICE_MODLE_NUMBER, 9); 


  
  pDataReportService = pBLEServer->createService(SERVICE_UUID_DATAREPORT);
  pDataReportCharacteristic = pDataReportService->createCharacteristic(GATT_UUID_FULLREPORT, BLE_CH_RN);
  //pDataReportCharacteristic->setValue(lastest_state.get_s_buffer(), InstrumentState::SERIALZE_BUFFER_SIZE);
  pDataReportCharacteristic->addDescriptor(new BLE2902());

  

  envService->start();
  batteryService->start();
  pDataReportService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(envService->getUUID());
  pAdvertising->addServiceUUID(pDataReportService->getUUID());
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);
  BLEDevice::startAdvertising();

  
  
}
