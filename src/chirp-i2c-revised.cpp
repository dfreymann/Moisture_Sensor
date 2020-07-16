/******************************************************/
//       THIS IS A GENERATED FILE - DO NOT EDIT       //
/******************************************************/

#line 1 "/Users/freymann/Dropbox/Electronics/_CODE/ParticleWorkbench/chirp-i2c/src/chirp-i2c-revised.ino"
// chirp i2c migration to Ubidots STEM and ubidots.h library
// 7.14.20 
// 7.16.20 - working fine. 
// ToDo - revise code to eliminate http stuff; correct Particle publish statement

// chirp-i2c-revised
//
// running on 'dmf-5-photon'
//
// dmf 6.3.15 begin work on water sensor setup
// https://www.tindie.com/products/miceuz/i2c-soil-moisture-sensor/
// code based on example code available at http://wemakethings.net/chirp/
// dmf 4.2.16 see my notes re I2C, & etc.
// dmf 4.3.16 adding and testing system_sleep code. note - expect that initial
//    test code will break the 'timeForEmail' tests. Will need to return to this.
// dmf 4.4.16 update display only if new value available
// dmf 4.8.16 remove email notification; let Ubidots events handle it.
// dmf 4.14.16 - add battery fuel gauge code; use SparkFunMAX17043 library
// dmf 4.14.16 - shift to I2CSoilMoistureSensor library, instead of old functions
// dmf 4.14.16 - added error check for I2C slave available or not
// toDO - use the I2C slave available code (e.g. to notify of failure)
// dmf 4.15.16 - add sleepCanary pin to control external sleep indicator functions
// dmf 4.19.16 - call batteryMonitor.sleep() before putting the system to sleep
//    note - prototype configuration maintains SCL/SDA at 3V3 during system sleep;
//    this may cause issues (as it does for the Chirp)
//    4.22.16: remove this statement for now. doc suggests sleep by setting SDA/SCL
//    to 0V. not seeing any improvement in battery lifetime.
// dmf 4.22.16 odd error (no code change) 'static assertion failed: inc/spark_wiring_cloud.h:78:9'
//    workaround to comment out the Spark.variable() statements (something mentioned
//    in passing on the internets). "stateofcharge" fails as a Spark.variable, while
//    "stateofcharg" is OK, apparently, as of today. 12 character limit? Yes:
//        "Currently, up to 10 cloud variables may be defined and each variable
//        name is limited to a maximum of 12 characters.It is fine to call this
//        function when the cloud is disconnected - the variable will be registered
//        next time the cloud is connected.Prior to 0.4.7 firmware, variables were
//        defined with an additional 3rd parameter to specify the data type of the
//        variable. From 0.4.7 onwards, the system can infer the type from the actual
//        variable. Additionally, the variable address was passed via the address-of
//        operator (&). With 0.4.7 and newer, this is no longer required."
// dmf 10.16.16 - changed incorrect call from runningAvgWet.fill() to
//        runningAvgWet.clear()
// dmf 10.16.16 - added temperature sanity check limits lowHotLimit, highHotLimit
// dmf 10.16.16 - changed "79.0", int VAL pair to double stdTemp, and
//        int stdTempVal pair, since this changes between probes.
//
// dmf 3.30.18 - Have reconfigured probes using waterproof connectors and epoxy-sealed
//        case. Have three Chirps active now: one 'v2.5' (old style) and two 'v2.73'
//        (new style). Measurements after conformal coating (only):
//        v2.5
//          DRY  206  WET  476   TEMP 74deg 303 (NOTE: arbitrary? w/increment of 1/deg)
//        v2.7.3 (wires straight)
//          DRY  313  WET  670   TEMP 74deg 258 (NOTE: thermistor 1/10 deg, so 25.8C)
//        v2.7.3 (wires twisted)
//          DRY  313  WET  680   TEMP 21.1C 225 (NOTE: thermistor 1/10 deg, so 22.5C)
//        Setup configuration with CHIRP_2.5 or CHIRP_2.7.3
// dmf 7.17.18 - setup text alerts, using my previously configured particle.publish("e2")
//        webhook (which is used for the Electron_Humidity monitor to get a Twilio
//        text message. This will be very straightforward to configure.
//        Note: have to edit webhook to trigger from 'Any' device (was previously
//        only triggered from dmf_electron_2)
//
// toDO - should really convert the HTTPClient stuff to a library

// flag to turn on Serial.print()s
#define DOTEST

// 3.30.18 flag for probe attached (comment to select)
#define CHIRP_25        // CHIRP v2.5
//#define CHIRP_273   // CHIRP v2.7.3

// define values based on model probe
#if defined(CHIRP_25)
  #define DRYVAL  206      // Conversion values (measurements 3.30.18)
  #define WETVAL  476
  #define TEMPSTD 74.0
  #define TEMPVAL 303
#elif defined(CHIRP_273)
  #define DRYVAL  313      // Conversion values (measurements 3.30.18)
  #define WETVAL  680
  #define TEMPSTD 32.0
  #define TEMPVAL 0         // will use as flag
#endif

// This #include statement was automatically added by the Spark IDE.
#include "application.h"                    // Particle Default
void setup();
void loop();
#line 86 "/Users/freymann/Dropbox/Electronics/_CODE/ParticleWorkbench/chirp-i2c/src/chirp-i2c-revised.ino"
#include "I2CSoilMoistureSensor_Particle.h" // Chirp I2C code
#include "do_DogLcd.h"                      // DOG LCD Display
#include "SparkFunMAX17043.h"               // MAX17043 Battery gauge
#include "HttpClient.h"                     // Communicate With Ubidots
#include "elapsedMillis.h"                  // Elapsed Timer
#include "RunningAverage.h"                 // Moving Average
#include "math.h"                           // For Some Reason...
// 7.14.20 
# include "Ubidots.h"


// externalize access tokens
#include "ubidots_tokens.h"
// externalize wifi tokens
#include "wifi_tokens.h"

// Note that the default I2C address for the Chirp is 0x20 (software adjustable)
// and the default address for the MAX17043 chip is 0x36 (fixed)

// Define the Chirp
I2CSoilMoistureSensor chirp;
// Define the fuel gauge
MAX17043 batteryMonitor;
// Define the http connection
HttpClient http;
// Define the elapsed timers
elapsedMillis timeElapsed;
elapsedMillis timeForSleep;
// Define the DOGM display
DogLcdhw lcd(0, 0, 12, 11, 10, -1); // SPARK test configuration
// Define a running average of the capacitance measurements (10 measurements)
RunningAverage runningAvgWet(10);

// http headers need to be set at init
http_header_t headers[] = {
    { "Content-Type", "application/json" },
    { "X-Auth-Token" , TOKEN },
    { NULL, NULL } // NOTE: Always terminate headers will NULL
};
http_request_t request;
http_response_t response;

// Set the humidity ranging values
const int DRY = DRYVAL;
const int WET = WETVAL;
// These values only required for CHIRP_2.5
const double stdTemp = TEMPSTD; // temperature in Farenheit at which...
const int stdTempVal = TEMPVAL; // ...an integer temperature value was read.
//
const double CtoF=1.8;

// Sleep and Ubidots upload interval settings
// CAREFUL!  On some systems: unsigned int: 0 to 65535 ONLY
// The Core/Photon stores a 4 byte (32-bit) value, ranging
// from 0 to 4,294,967,295 (2^32 - 1).
// Set how long to sleep - note this is in SECONDS
// note: can't update while in sleep mode. use safe mode, or wait until wake
// 6.22.18 change to every 2 hours
// unsigned int howLongToSleep=6900;         // seconds; sleep for 1h 55m
unsigned int howLongToSleep=21300;           // 4.10.20 change to sleep 5h 55m
// and here a timer to send the system to sleep after
// wakeup (so this sets wake time - time in MILLISECONDS)
unsigned int elapsedToSleep=300000;         // run and read for 5 minutes
// Here, set up for to report to Ubidots after 4 minutes (in MILLISECONDS);
// unsigned int elapsedToSendA=180000;    // Upload to Ubidots every 3 minutes
unsigned int elapsedToSendA=60000;
unsigned int elapsedToSendB=15000;        // 15 second intervals after
//
// with this Sleep configuration this means we expect:
//    > sleep 55 minutes
//    > wake up and measure for 3 minutes
//    > upload to Ubidots (4 values at 15 second intervals)
//    > back to sleep 5 minutes after wakeup.
//

// the sleepCanary pin will be held LOW; when the Photon enters SLEEP mode,
// the pin will be high impendance. will wire indicator LED and
// power-to-peripherals MOSFETs controlled by this pin.
int sleepCanary = D6;

// test for 'too dry' to trigger email
double tooDryValue = 10.0;

// temperature sanity check limits (disconnected sensor, etc)
double lowHotLimit = -30.0;
double highHotLimit = 110.0;

// control flags      // A: water
bool doBNext=FALSE;   // B: temp
bool doCNext=FALSE;   // C: voltage
bool doDNext=FALSE;   // D: soc
// return values
int badReturn = 0;
// test flags
bool isWireThere = FALSE;
bool chirpIsThere = FALSE;
bool gaugeIsThere = FALSE;

// Spark Variables to be registered in Setup()
double waterValue = 0.0;
double tempValue = 0.0;
// save values in loop()
double waterValueLast = 0.0;
double tempValueLast = 0.0;

// Battery monitoring
double cellVoltage = 0;
double stateOfCharge = 0;
bool inAlert = FALSE;

// dmf 7.17.18
String alertBody = "The plants need water! WaterValue is ";
bool sendLowAlert = FALSE;    // flag to avoid multiple low watervalue texts

// dmf 3.27.19
uint8_t chirpAddress = 0;
uint8_t chirpSoftVersion = 0;

// 7.10.20 Ubidots migration
Ubidots ubidots(UBIDOTS_TOKEN, UBI_EDUCATIONAL, UBI_TCP); 

// SETUP
void setup() {

    // setup our watchdog pin FIRST, since this powers-on peripherals!
    // Note: recovery from SLEEP_MODE_DEEP always passes through SETUP,
    // so this should work to turn everything back on.
    pinMode (sleepCanary, OUTPUT);
    digitalWrite(sleepCanary, LOW);
    delay (500);

    // initialize i2C
    Wire.begin();

#ifdef DOTEST
    // initialize Serial
    Serial.begin(9600);
#endif

    // find a delay here needed for good Wire/Serial behavior in setup()
    delay(1000);  // dmf 3.27.19 changed 500 to 1000

#ifdef DOTEST
    isWireThere = Wire.isEnabled();
    Serial.print("Wire isEnabled? (Should be 1 here): ");
    Serial.println(isWireThere);
#endif

    // initialize the Chirp (writes a reset)
    chirp.begin();
    delay(500);
    if (!isSensorReturn) chirpIsThere = TRUE;

#ifdef DOTEST
    Serial.print("Chirp isSensorReturn: ");
    Serial.println(isSensorReturn);
    Serial.print("Chirp chirpIsThere: ");
    Serial.println(chirpIsThere);
#endif

    // dmf 3.27.19
    chirpAddress = chirp.getAddress();
    chirpSoftVersion = chirp.getVersion();
#ifdef DOTEST
    Serial.print("Chirp address: 0x");
    if (chirpAddress<16) Serial.print("0");
    Serial.println(chirpAddress,HEX);
    Serial.print("Chirp software Version (eg 0x26 is v2.6): 0x");
    if (chirpSoftVersion<16) Serial.print("0");
    Serial.println(chirpSoftVersion,HEX);
#endif

    // initialize the DOGM display
    lcd.begin(DOG_LCDhw_M162, DOG_LCDhw_VCC_3V3, -1, -1);  // SPARK test configuration
    delay(500);

    // initialize the fuel gauge
    badReturn = batteryMonitor.reset();
    if (!badReturn) {
      gaugeIsThere=TRUE;
      batteryMonitor.quickStart();
      delay(500);
      batteryMonitor.setThreshold(20);
    }
#ifdef DOTEST
    Serial.print("Battery badReturn ");
    Serial.println(badReturn);
    Serial.print("Battery gaugeIsThere ");
    Serial.println(gaugeIsThere);
#endif

    // initialize the running average to 0
    // NOTE: still, the code initializes funny so that garbage
    // is printed until the 10 values have been read.
    // runningAvgWet.fillValue(0.0,10);
    // dmf 10.16.16 change this to the following -
    runningAvgWet.clear();

    // Setup for Ubidots
    request.hostname = "things.ubidots.com";
    request.port = 80;

    // Register Spark variables 
    // 3.24.20 now 'Particle' variables
    Particle.variable("waterValue", &waterValue, DOUBLE);
    Particle.variable("tempValue", &tempValue, DOUBLE);
    Particle.variable("cellvoltage", &cellVoltage, DOUBLE);
    Particle.variable("stateofcharg", &stateOfCharge, DOUBLE);

    // setup onboard LED to indicate activity
    pinMode (D7, OUTPUT);
    digitalWrite(D7, LOW);

    // no cursor needed
    lcd.noCursor();

}

// LOOP
void loop() {

    unsigned int c;
    unsigned int t;
    unsigned int l;
    int capacitance;
    int temperature;
    int lightlevel;
    double howWet;
    double avgWet;
    double howHot;
    bool newValue = FALSE;

/* MAKE READINGS */

    // read capacitance (i.e. humidity) register
    c = chirp.getCapacitance();
#ifdef DOTEST
    Serial.println(" ");
    Serial.print("capacitance ");
    Serial.println(c);
#endif
    capacitance = c;

    // read temperature register
    t = chirp.getTemperature();
#ifdef DOTEST
    Serial.print("temperature ");
    Serial.println(t);
#endif
    temperature = t;

    // request light measurement, wait and read light register
// l = chirp.getLight(TRUE);
//#ifdef DOTEST
//  Serial.print("lightlevel ");
//  Serial.println(l);
//#endif
// lightlevel = l;

    if (gaugeIsThere) {
      // read the battery voltage
      cellVoltage = batteryMonitor.getVoltage();

      // read the state of charge
      stateOfCharge = batteryMonitor.getSOC();

      // check for an alert
      inAlert = batteryMonitor.getAlert();
    }

#ifdef DOTEST
    Serial.print("battery voltage ");
    Serial.println(cellVoltage);
    Serial.print("state of charge ");
    Serial.println(stateOfCharge);
    Serial.print("alert ");
    if (inAlert) Serial.println("YES");
    else Serial.println("NO");
#endif

/* PROCESS MEASUREMENTS */

    // simple interpolation based on capacitance = 208 DRY and = 495 IN WATER
    howWet = ((float) capacitance - DRY) / (WET - DRY);
#ifdef DOTEST
    Serial.println(" ");
    Serial.print("howWet ");
    Serial.println(howWet, 2);
#endif

    if (howWet < 0) howWet = 0;
    if (howWet > 1) howWet = 1;

    runningAvgWet.addValue(howWet);

    // get the average, set the Spark variable, limit significant digits and convert to %
    avgWet = runningAvgWet.getAverage();
    waterValue = floor((avgWet*1000)+0.5)/10;

    if (waterValue != waterValueLast) {
      newValue = TRUE;
      waterValueLast = waterValue;
    }

    if (stdTempVal) {               // flag based on existance of stdTempVal
      // simple interpolation based on 79degF = 318 and  1degC = 1 increment
      // NOTE: This depends on the particular probe!!
      // howHot = 79.0 + (temperature - VAL)*CtoF;
      // dmf 10.16.16 generalized this -
      howHot = stdTemp + (temperature - stdTempVal)*CtoF;
    } else {
      // temperature value is direct read in 10ths of degree C
      howHot = stdTemp + (temperature/10)*CtoF;
    }

#ifdef DOTEST
    Serial.println(" ");
    Serial.print("howHot ");
    Serial.println(howHot, 1);
#endif

    // limit significant digits
    howHot = floor(howHot*10+0.5)/10;

    // sanity check temperature value (disconnected sensor, etc)
    if ((howHot < lowHotLimit) || (howHot > highHotLimit)) {
      howHot = 98.6;         // dummy value
    }

    // set the Spark variable
    tempValue = howHot;

    if (tempValue != tempValueLast) {
      newValue = TRUE;
      tempValueLast = tempValue;
    }

/* DISPLAY MEASUREMENTS */

    if (newValue) {   // update display with new values

      // clear the DOGM display
      lcd.clear();

      // report the capacitance humidity reading to the DOGM display
      lcd.setCursor(0,0);
      lcd.print("howWet");
      if (waterValue<10){
          lcd.setCursor(9,0);
      } else {
          lcd.setCursor(8,0);
        }
      lcd.print(waterValue,1);
      lcd.setCursor(12,0);
      lcd.print("%");

      // report the temperature reading to the DOGM display
      lcd.setCursor(0,1);
      lcd.print("howHot");
      lcd.setCursor(8,1);
      lcd.print(howHot,1);
      lcd.setCursor(12,1);
      lcd.print(char(223));

    }

#ifdef DOTEST
    Serial.print("timeElapsed ");
    Serial.println(timeElapsed);
    Serial.print("elapsedToSendA ");
    Serial.println(elapsedToSendA);
#endif

/* UPLOAD MEASUREMENTS */

    // if time_elapsed and doHTTP then upload reading A
    // otherwise if less_time_elapsed and doHTTP then upload reading B
    if (timeElapsed > elapsedToSendA) {

#ifdef DOTEST
        Serial.println("in http send A ");
#endif

        // 7.14.20 at this point we have all the measurements (above).
        // Previously, measurements were uploaded one at a time (hence the doBnext, etc)
        // Now, we just upload all to ubidots once enough time has elapsed to 
        // generate a running average. 

        // 7.10.20 Send all measurements at the same time
        ubidots.add(VARIABLE_SOC, stateOfCharge);
        ubidots.add(VARIABLE_VOLTAGE, cellVoltage);
        ubidots.add(VARIABLE_TEMPERATURE, howHot);
        ubidots.add(VARIABLE_WATER_VALUE, waterValue); 

        bool bufferSent = false;
        bufferSent = ubidots.send(UBIDOTS_DEVICE); 

        if (bufferSent) {
           Serial.println("Values sent by the device");
        }


        // Send the fractional 'wetness' to Ubidots...
//        request.path = "/api/v1.6/variables/" VAR_WATERVALUE "/values";
//        request.body = "{\"value\":" + String(waterValue) + "}";
//        http.post(request, response, headers);


        // dmf 7.17.18
        if (waterValue < 20.0 && sendLowAlert) {  // exploit previously configured Twilio webhook
            alertBody = alertBody + String::format("%.1f",waterValue) + "%!";
            bool success = Particle.publish("e2", alertBody);
            if (!success) {
              #ifdef DOTEST
                Serial.println("Particle.publish with e2 message FAILED");
              #endif
            }
            sendLowAlert = FALSE;
        }

        // reset timeElapsed [tested at 30s, next event will be B at 5m30s]
        timeElapsed = 0;
        doBNext = TRUE;

    } else if (doBNext && (timeElapsed > elapsedToSendB)) {

#ifdef DOTEST
        Serial.println("in http send B ");
#endif

        // Send the sensor temperature to Ubidots...
//        request.path = "/api/v1.6/variables/" VAR_TEMPVALUE "/values";
//        request.body = "{\"value\":" + String(howHot) + "}";
//        http.post(request, response, headers);


        // reset timeElapsed
        timeElapsed = 0;
        doBNext = FALSE;
        doCNext = TRUE;

    } else if (doCNext && (timeElapsed > elapsedToSendB)) {

#ifdef DOTEST
        Serial.println("in http send C ");
#endif

        // Send the battery voltage to Ubidots...
//        request.path = "/api/v1.6/variables/" VAR_VOLTAGE "/values";
//        request.body = "{\"value\":" + String(cellVoltage) + "}";
//        http.post(request, response, headers);

        // reset timeElapsed
        timeElapsed = 0;
        doCNext = FALSE;
        doDNext = TRUE;

    } else if (doDNext && (timeElapsed > elapsedToSendB)) {

#ifdef DOTEST
        Serial.println("in http send D ");
#endif

        // Send the battery state of charge to Ubidots...
//        request.path = "/api/v1.6/variables/" VAR_BATTSOC "/values";
//        request.body = "{\"value\":" + String(stateOfCharge) + "}";
//        http.post(request, response, headers);

        // reset timeElapsed
        timeElapsed = 0;
        doDNext = FALSE;

    }

/* PAUSE */

    // Flash the LED to say that we've done something
    digitalWrite(D7, HIGH);
    delay(1000);
    digitalWrite(D7, LOW);
    delay(2000);

/* AND SLEEP */

    // make the decision to goto sleep
    if (timeForSleep > elapsedToSleep) {
      lcd.clear();              // clear the DOGM display
//    batteryMonitor.sleep();   // put the fuel gauge to sleep (will reset() at start)
      System.sleep(SLEEP_MODE_DEEP,howLongToSleep);
      timeForSleep = 0;         // Actually, don't need this, since Wake will restart
    }

}
