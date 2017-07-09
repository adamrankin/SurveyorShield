#include <Wire.h>
#include <Adafruit_GPS.h>
#include <Adafruit_MPL3115A2.h>
#include <SPI.h>
#include <SD.h>

// Create the GPS object (RX1/TX1 on Mega)
Adafruit_GPS GPS(&Serial1);

// Set GPSECHO to 'false' to turn off echoing the GPS data to the Serial console, set to 'true' if you want to debug and listen to the raw GPS sentences
#define GPSECHO false

// this keeps track of whether we're using the interrupt, off by default!
boolean usingInterrupt = false;
void useInterrupt(boolean); // Func prototype keeps Arduino 0023 happy

// Create the barometer object
Adafruit_MPL3115A2 baro = Adafruit_MPL3115A2();
bool barometerFound = false;

// Create the logging file reference
File gpsAltitudeLogFile;
char fileName[64];
bool fileNameCreated = false;
bool sdInitialized = false;

void setup()
{
  Serial.begin(115200);

  if(SD.begin())
  {
    sdInitialized = true;
  }

  if (baro.begin())
  {
    barometerFound = true;
  }

  // 9600 NMEA is the default baud rate for MTK - some use 4800
  GPS.begin(9600);

  delay(250);
  
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);

  // 5 Hz update rate- for 9600 baud you'll have to set the output to RMC or RMCGGA only (see above)
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_5HZ);
  GPS.sendCommand(PMTK_API_SET_FIX_CTL_5HZ);

  // Request updates on antenna status, comment out to keep quiet
  GPS.sendCommand(PGCMD_ANTENNA);

  useInterrupt(true);

  delay(1000);
}

// Interrupt is called once a millisecond, looks for any new GPS data, and stores it
SIGNAL(TIMER0_COMPA_vect)
{
  char c = GPS.read();

  // if you want to debug, this is a good time to do it!
  if (GPSECHO)
  {
    if (c)
    {
      UDR0 = c;
    }
    // writing direct to UDR0 is much much faster than Serial.print
    // but only one character can be written at a time.
  }
}

void useInterrupt(boolean v)
{
  if (v)
  {
    // Timer0 is already used for millis() - we'll just interrupt somewhere
    // in the middle and call the "Compare A" function above
    OCR0A = 0xAF;
    TIMSK0 |= _BV(OCIE0A);
    usingInterrupt = true;
  }
  else
  {
    // do not call the interrupt function COMPA anymore
    TIMSK0 &= ~_BV(OCIE0A);
    usingInterrupt = false;
  }
}

uint32_t timer = millis();
void loop()
{
  if (!barometerFound || !GPS.fix || !sdInitialized)
  {
    delay(250);
    return;
  }

  float altitudeMeters = 0.f;
  altitudeMeters = baro.getAltitude(); // meters

  // if a sentence is received, we can check the checksum, parse it...
  if (GPS.newNMEAreceived())
  {
    if (!GPS.parse(GPS.lastNMEA()))
    {
      // this also sets the newNMEAreceived() flag to false
      delay(250);
      return;
    }

    if (!fileNameCreated)
    {
      sprintf(fileName, "terrainData_%d_%d_%d.log", GPS.year, GPS.month, GPS.day);
    }
  }

  // if millis() or timer wraps around, we'll just reset it
  if (timer > millis())
  {
    timer = millis();
  }

  // approximately every 200 milliseconds or so, log the current GPS stats
  if (millis() - timer > 200 && fileNameCreated)
  {
    // Open the file. note that only one file can be open at a time, so you have to close this one before opening another.
    gpsAltitudeLogFile = SD.open(fileName, FILE_WRITE);

    if (gpsAltitudeLogFile)
    {
      // write to file
      gpsAltitudeLogFile.print(GPS.latitude, 4);
      gpsAltitudeLogFile.print(", ");
      gpsAltitudeLogFile.print(GPS.longitude, 4);
      gpsAltitudeLogFile.print(", "); 
      gpsAltitudeLogFile.println(altitudeMeters);
      gpsAltitudeLogFile.close();
    }

    timer = millis(); // reset the timer
  }

  delay(250);
}
