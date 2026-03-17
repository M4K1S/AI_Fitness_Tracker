#include "I2Cdev.h"
#include "MPU6050.h"
#include <math.h>

MPU6050 mpu;

// Raw data
int16_t ax, ay, az;
int16_t gx, gy, gz;

// Smoothing
float smoothedA = 1.0;
float alpha = 0.7; // smoothing factor

// Rep counting
bool inSquat = false;
int repCount = 0;

// Timing
unsigned long lastRepTime = 0;
const unsigned long repDebounce = 600; // ms
unsigned long downTime = 0;

void setup() {
  Wire.begin();
  Serial.begin(115200);

  mpu.initialize();
  if (!mpu.testConnection()) {
    Serial.println("MPU6050 connection failed!");
    while (true);
  }
  Serial.println("MPU6050 ready (barbell squat tracker with speed)");
}

void loop() {
  // Read accelerometer + gyro
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

  // Convert to g
  float Ax = ax / 16384.0;
  float Ay = ay / 16384.0;
  float Az = az / 16384.0;

  // Total acceleration magnitude (rotation-independent)
  float A_total = sqrt(Ax*Ax + Ay*Ay + Az*Az);

  // Smooth signal
  smoothedA = alpha * smoothedA + (1 - alpha) * A_total;

  unsigned long now = millis();

  // Detect bar going DOWN
  if (!inSquat && smoothedA > 1.05 && (now - lastRepTime > repDebounce)) {
    inSquat = true;
    downTime = now; // record start of rep
    Serial.println("DOWN");
  }

  // Detect bar coming UP → count rep & measure speed
  else if (inSquat && smoothedA < 0.95 && (now - lastRepTime > repDebounce)) {
    inSquat = false;
    repCount++;
    lastRepTime = now;

    unsigned long upTime = now;
    float repDuration = (upTime - downTime) / 1000.0; // seconds

    Serial.print("REP: ");
    Serial.print(repCount);
    Serial.print(" | Speed: ");
    Serial.print(repDuration, 2);
    Serial.println(" s");
  }

  // Optional: print total acceleration occasionally
  static unsigned long lastPrint = 0;
  if (now - lastPrint > 1000) {
    Serial.print("Total g: ");
    Serial.println(smoothedA, 3);
    lastPrint = now;
  }

  delay(50); // stable 20Hz update
}
