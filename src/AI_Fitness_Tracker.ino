#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "I2Cdev.h"
#include "MPU6050.h"
#include <math.h>

MPU6050 mpu;

// Raw data
int16_t ax, ay, az;
int16_t gx, gy, gz;

// Smoothed acceleration
float smoothedA = 1.0;
float alpha = 0.7; // smoothing factor

// Rep counting
enum SquatPhase {IDLE, GOING_DOWN, IN_HOLE, GOING_UP, AT_TOP};
SquatPhase phase = IDLE;
SquatPhase lastPhase = IDLE; // track last phase for broadcasting
int repCount = 0;

// Timing
unsigned long lastRepTime = 0;
const unsigned long repDebounce = 600; // ms
unsigned long downStart = 0;
unsigned long holeStart = 0;
unsigned long holeEnd = 0;
unsigned long upEnd = 0;
unsigned long topStart = 0;

// BLE
BLEServer* pServer = nullptr;
BLECharacteristic* pCharacteristic = nullptr;
bool deviceConnected = false;
#define SERVICE_UUID        "12345678-1234-1234-1234-1234567890ab"
#define CHARACTERISTIC_UUID "abcd1234-5678-90ab-cdef-1234567890ab"

class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) { deviceConnected = true; }
  void onDisconnect(BLEServer* pServer) { deviceConnected = false; }
};

void setup() {
  Wire.begin();
  Serial.begin(115200);

  // MPU6050 setup
  mpu.initialize();
  if (!mpu.testConnection()) {
    Serial.println("MPU6050 connection failed!");
    while (true);
  }
  Serial.println("MPU6050 ready (phase broadcaster)");

  // BLE setup
  BLEDevice::init("SquatTracker");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  pCharacteristic->addDescriptor(new BLE2902());
  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->start();

  Serial.println("BLE ready, advertising as SquatTracker");

  // Countdown
  Serial.println("Start in 3...");
  delay(1000);
  Serial.println("2...");
  delay(1000);
  Serial.println("1...");
  delay(1000);
  Serial.println("Go!");
}

void broadcastPhase() {
  if (deviceConnected && phase != lastPhase) {
    lastPhase = phase;
    char buf[50];
    snprintf(buf, sizeof(buf), "REP %d | PHASE: %d", repCount, phase);
    pCharacteristic->setValue(buf);
    pCharacteristic->notify();
  }
}

void loop() {
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

  float Ax = ax / 16384.0;
  float Ay = ay / 16384.0;
  float Az = az / 16384.0;

  float A_total = sqrt(Ax*Ax + Ay*Ay + Az*Az);
  smoothedA = alpha * smoothedA + (1 - alpha) * A_total;

  unsigned long now = millis();

  switch (phase) {
    case IDLE:
      if (smoothedA > 1.05 && (now - lastRepTime > repDebounce)) {
        phase = GOING_DOWN;
        downStart = now;
        Serial.println("GOING DOWN");
      }
      break;

    case GOING_DOWN:
      if (smoothedA < 1.02) {
        phase = IN_HOLE;
        holeStart = now;
        Serial.println("IN HOLE");
      }
      break;

    case IN_HOLE:
      if (smoothedA > 1.05) {
        phase = GOING_UP;
        holeEnd = now;
        Serial.println("GOING UP");
      }
      break;

    case GOING_UP:
      if (smoothedA < 1.0) {
        phase = AT_TOP;
        upEnd = now;
        topStart = now;
        repCount++;
        Serial.println("AT TOP");
      }
      break;

    case AT_TOP:
      if (smoothedA > 1.05) {
        phase = GOING_DOWN;
        downStart = now;
        lastRepTime = now;
        Serial.println("GOING DOWN");
      }
      break;
  }

  broadcastPhase(); // send phase changes over BLE

  delay(25); // ~40Hz
}
