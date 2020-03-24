/*----------------------------------------------------------------------*
 * I2CSoilMoistureSensor.cpp - Arduino library for the Sensor version of*
 * I2C Soil Moisture Sensor version from Chrirp                         *
 * (https://github.com/Miceuz/i2c-moisture-sensor).                     *
 *                                                                      *
 * Ingo Fischer 11Nov2015                                               *
 * https://github.com/Apollon77/I2CSoilMoistureSensor                   *
 *                                                                      *
 * MIT license                                                          *
 *----------------------------------------------------------------------*/

#include <I2CSoilMoistureSensor_Particle.h>

//define release-independent I2C functions
#if defined(SPARK)
#include <application.h>
#define i2cBegin Wire.begin
#define i2cBeginTransmission Wire.beginTransmission
#define i2cEndTransmission Wire.endTransmission
#define i2cRequestFrom Wire.requestFrom
#define i2cRead Wire.read
#define i2cWrite Wire.write
#elif defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
#include <TinyWireM.h>
#define i2cBegin TinyWireM.begin
#define i2cBeginTransmission TinyWireM.beginTransmission
#define i2cEndTransmission TinyWireM.endTransmission
#define i2cRequestFrom TinyWireM.requestFrom
#define i2cRead TinyWireM.receive
#define i2cWrite TinyWireM.send
#elif ARDUINO >= 100
#include <Wire.h>
#define i2cBegin Wire.begin
#define i2cBeginTransmission Wire.beginTransmission
#define i2cEndTransmission Wire.endTransmission
#define i2cRequestFrom Wire.requestFrom
#define i2cRead Wire.read
#define i2cWrite Wire.write
#else
#include <Wire.h>
#define i2cBegin Wire.begin
#define i2cBeginTransmission Wire.beginTransmission
#define i2cEndTransmission Wire.endTransmission
#define i2cRequestFrom Wire.requestFrom
#define i2cRead Wire.receive
#define i2cWrite Wire.send
#endif

// define a static return value for I2C functions
int isSensorReturn = 0;

/*----------------------------------------------------------------------*
 * Constructor.                                                         *
 * Optionally set sensor I2C address if different from default          *
 *----------------------------------------------------------------------*/
I2CSoilMoistureSensor::I2CSoilMoistureSensor(uint8_t addr) : sensorAddress(addr) {
  // nothing to do ... Wire.begin needs to be put outside of class
}

/*----------------------------------------------------------------------*
 * Initializes anything ... it does a reset only at the moment          *
 *----------------------------------------------------------------------*/
void I2CSoilMoistureSensor::begin() {
  resetSensor();
  // dmf 3.27.19 force this delay
  delay(1000);
}

/*----------------------------------------------------------------------*
 * Return measured Soil Moisture Capacitance                            *
 * Moisture is somewhat linear. More moisture will give you higher      *
 * reading. Normally all sensors give about 290 - 310 as value in free  *
 * air at 5V supply.                                                    *
 *----------------------------------------------------------------------*/
unsigned int I2CSoilMoistureSensor::getCapacitance() {
  return readI2CRegister16bitUnsigned(sensorAddress, SOILMOISTURESENSOR_GET_CAPACITANCE);
}

/*----------------------------------------------------------------------*
 * Change I2C address of the sensor to the provided address (1..127)    *
 * and do a reset after it in order for the new address to become       *
 * effective if second parameter is true.                               *
 * Method returns true if the new address is set successfully on sensor.*
 *----------------------------------------------------------------------*/
bool I2CSoilMoistureSensor::setAddress(int addr, bool reset) {
  writeI2CRegister8bit(sensorAddress, SOILMOISTURESENSOR_SET_ADDRESS, addr);
  if (reset) {
    resetSensor();
    delay(1000);
  }
  sensorAddress=addr;
  return (readI2CRegister8bit(sensorAddress, SOILMOISTURESENSOR_GET_ADDRESS) == addr);
}

/*----------------------------------------------------------------------*
 * Return current Address of the Sensor                                 *
 *----------------------------------------------------------------------*/
uint8_t I2CSoilMoistureSensor::getAddress() {
  return sensorAddress;
}

/*----------------------------------------------------------------------*
 * Starts the measurement for the Light sensor. Wait at least 3 seconds *
 * till you call method getLight to get the Light value.                *
 *----------------------------------------------------------------------*/
void I2CSoilMoistureSensor::startMeasureLight() {
  writeI2CRegister8bit(sensorAddress, SOILMOISTURESENSOR_MEASURE_LIGHT);
}

/*----------------------------------------------------------------------*
 * Read the Light Measurement from the sensor. When used without        *
 * parameter or parameter value is false then a former call to          *
 * method startMeasureLight and a waiting time of at least 3 seconds is *
 * expected.                                                            *
 * Alternatively use true as parameter and the method does the call to  *
 * startMeasureLight and a 3 seconds delay automatically and no former  *
 * call is needed.                                                      *
 * The measurement gives 65535 in a dark room away form desk lamp - so  *
 * more light, lower reading. When it's dark, it takes longer to        *
 * measure light, reading the light register while measurement is in    *
 * progress (e.g. wait time too short) will return the previous reading.*
 * Be aware, light sensor is pretty noisy.                              *
 *----------------------------------------------------------------------*/
unsigned int I2CSoilMoistureSensor::getLight(bool wait) {
  if (wait) {
    startMeasureLight();
    delay(3000);
  }
  return readI2CRegister16bitUnsigned(sensorAddress, SOILMOISTURESENSOR_GET_LIGHT);
}

/*----------------------------------------------------------------------*
 * Read the Temperature Measurement. Temperature is measured by the     *
 * thermistor on the tip of the sensor. Calculated absolute measurement *
 * accuracy is better than 2%. The returned value is in degrees Celsius *
 * with factor 10, so need to divide by 10 to get real value            *
 *----------------------------------------------------------------------*/
int I2CSoilMoistureSensor::getTemperature() {
  return readI2CRegister16bitSigned(sensorAddress, SOILMOISTURESENSOR_GET_TEMPERATURE);
}

/*----------------------------------------------------------------------*
 * Resets sensor. Give the sensor 0.5-1 second time to boot up after    *
 * reset.                                                               *
 *----------------------------------------------------------------------*/
void I2CSoilMoistureSensor::resetSensor() {
  writeI2CRegister8bit(sensorAddress, SOILMOISTURESENSOR_RESET);
}

/*----------------------------------------------------------------------*
 * Get Firmware Version. 0x22 means 2.2                                  *
 *----------------------------------------------------------------------*/
uint8_t I2CSoilMoistureSensor::getVersion() {
  return readI2CRegister8bit(sensorAddress, SOILMOISTURESENSOR_GET_VERSION);
}

// In the following helper methods...
// i2cEndTransmission() (actually, Wire.endTransmission()) returns
// isSensorReturn, byte value which indicates the status of the
// transmission:
//     0: success
//     1: busy timeout upon entering endTransmission()
//     2: START bit generation timeout
//     3: end of address transmission timeout
//     4: data byte transfer timeout
//     5: data byte transfer succeeded, busy timeout immediately after

// Also note: treat these methods as ISRs - any Serial.print()
// debugging statements in these functions completely screw up i2C.

/*----------------------------------------------------------------------*
 * Helper method to write an 8 bit value to the sensor via I2C          *
 *----------------------------------------------------------------------*/
void I2CSoilMoistureSensor::writeI2CRegister8bit(int addr, int value) {
  i2cBeginTransmission(addr);
  i2cWrite(value);
  isSensorReturn = i2cEndTransmission();
}

/*----------------------------------------------------------------------*
 * Helper method to write an 8 bit value to the sensor via I2C to the   *
 * given register                                                       *
 *----------------------------------------------------------------------*/
void I2CSoilMoistureSensor::writeI2CRegister8bit(int addr, int reg, int value) {
  i2cBeginTransmission(addr);
  i2cWrite(reg);
  i2cWrite(value);
  isSensorReturn = i2cEndTransmission();
}

/*----------------------------------------------------------------------*
 * Helper method to read a 16 bit unsigned value from the given register*
 *----------------------------------------------------------------------*/
unsigned int I2CSoilMoistureSensor::readI2CRegister16bitUnsigned(int addr, int reg) {
  i2cBeginTransmission(addr);
  i2cWrite(reg);
  isSensorReturn = i2cEndTransmission();
  delay(20);
  i2cRequestFrom(addr, 2);
  unsigned int t = i2cRead() << 8;
  t = t | i2cRead();
  return t;
}

/*----------------------------------------------------------------------*
 * Helper method to read a 16 bit signed value from the given register*
 *----------------------------------------------------------------------*/
int I2CSoilMoistureSensor::readI2CRegister16bitSigned(int addr, int reg) {
  i2cBeginTransmission(addr);
  i2cWrite(reg);
  isSensorReturn = i2cEndTransmission();
  delay(20);
  i2cRequestFrom(addr, 2);
  int t = i2cRead() << 8;
  t = t | i2cRead();
  return t;
}

/*----------------------------------------------------------------------*
 * Helper method to read a 8 bit value from the given register          *
 *----------------------------------------------------------------------*/
uint8_t I2CSoilMoistureSensor::readI2CRegister8bit(int addr, int reg) {
  i2cBeginTransmission(addr);
  i2cWrite(reg);
  isSensorReturn = i2cEndTransmission();
  delay(20);
  i2cRequestFrom(addr, 1);
  return i2cRead();
}
