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
int repCount = 0;

// Timing
unsigned long lastRepTime = 0;
const unsigned long repDebounce = 600; // ms
unsigned long downStart = 0;
unsigned long holeStart = 0;
unsigned long holeEnd = 0;
unsigned long upEnd = 0;
unsigned long topStart = 0;

void setup() {
  Wire.begin();
  Serial.begin(115200);

  mpu.initialize();
  if (!mpu.testConnection()) {
    Serial.println("MPU6050 connection failed!");
    while (true);
  }
  Serial.println("MPU6050 ready (full squat timing tracker)");

  // Countdown calibration
  Serial.println("Start in 3...");
  delay(1000);
  Serial.println("2...");
  delay(1000);
  Serial.println("1...");
  delay(1000);
  Serial.println("Go!");
}

void loop() {
  // Read MPU
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

  float Ax = ax / 16384.0;
  float Ay = ay / 16384.0;
  float Az = az / 16384.0;

  // Total acceleration magnitude
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
      if (smoothedA < 1.02) { // stationary at bottom
        phase = IN_HOLE;
        holeStart = now;
        Serial.println("IN HOLE");
      }
      break;

    case IN_HOLE:
      if (smoothedA > 1.05) { // moving up
        phase = GOING_UP;
        holeEnd = now;
        Serial.println("GOING UP");
      }
      break;

    case GOING_UP:
      if (smoothedA < 1.0) { // stationary at top
        phase = AT_TOP;
        upEnd = now;
        topStart = now;
        Serial.println("AT TOP");

        // Count rep
        repCount++;

        float timeDown = (holeStart - downStart) / 1000.0;
        float timeInHole = (holeEnd - holeStart) / 1000.0;
        float timeUp = (upEnd - holeEnd) / 1000.0;

        Serial.print("REP ");
        Serial.print(repCount);
        Serial.print(" | Down: ");
        Serial.print(timeDown, 2);
        Serial.print(" s | Hole: ");
        Serial.print(timeInHole, 2);
        Serial.print(" s | Up: ");
        Serial.print(timeUp, 2);
        Serial.print(" s | Top: counting...");
        Serial.println(" (ongoing)");
      }
      break;

    case AT_TOP:
      if (smoothedA > 1.05) { // starting next rep
        phase = GOING_DOWN;
        downStart = now;
        lastRepTime = now;
        Serial.println("GOING DOWN");
      }
      break;
  }

  delay(25); // ~20Hz
}
