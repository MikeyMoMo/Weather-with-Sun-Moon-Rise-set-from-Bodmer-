//  Example from OpenWeather library: https://github.com/Bodmer/OpenWeather
// 1 ijmk Adapted by Bodmer to use the TFT_eSPI library:  https://github.com/Bodmer/TFT_eSPI
//  Adapted by Mike Morrow to do a bunch of other things.  Notes at the end.a

//  This sketch is compatible with the ESP8266 and ESP32

//                           >>>  IMPORTANT  <<<
//         Modify setup in All_Settings.h tab to configure your location, etc

//                >>>  EVEN MORE IMPORTANT TO PREVENT CRASHES <<<
//>>>>>>  For ESP8266 set SPIFFS to at least 2Mbytes before uploading files  <<<<<<

//  ESP8266/ESP32 pin connections to the TFT are defined in the TFT_eSPI library.

//  Original by Daniel Eichhorn, see license at end of file.

//#define SERIAL_MESSAGES // For serial output weather reports
//#define SCREEN_SERVER   // For dumping screen shots from TFT
//#define RANDOM_LOCATION // Test only, selects random weather location every refresh
//#define FORMAT_SPIFFS   // Wipe SPIFFS and all files!

// This sketch uses font files created from the Noto family of fonts as bitmaps
// generated from these fonts may be freely distributed:
// https://www.google.com/get/noto/

// A processing sketch to create new fonts can be found in the Tools folder of TFT_eSPI
// https://github.com/Bodmer/TFT_eSPI/tree/master/Tools/Create_Smooth_Font/Create_font
// New fonts can be generated to include language specific characters. The Noto family
// of fonts has an extensive character set coverage.

// Json streaming parser (do not use IDE library manager version) to use is here:
// https://github.com/Bodmer/JSON_Decoder

#define AA_FONT_SMALL "fonts/NotoSansBold15" // 15 point sans serif bold
#define AA_FONT_LARGE "fonts/NotoSansBold36" // 36 point sans serif bold
/***************************************************************************************
**                          Load the libraries and settings
***************************************************************************************/
#include <Arduino.h>

#include <SPI.h>
#include <TFT_eSPI.h> // https://github.com/Bodmer/TFT_eSPI

// Additional functions
#include "GfxUi.h"          // Attached to this sketch
#include "SPIFFS_Support.h" // Attached to this sketch
#define FORMAT_SPIFFS_IF_FAILED true

#include "Preferences.h"
Preferences preferences;

#ifdef ESP8266
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif

#include <ArduinoOTA.h>
String OTAhostname = "Bodmer_WX";  // For OTA identification.

// check All_Settings.h for adapting to your needs
#include "All_Settings.h"

#include <JSON_Decoder.h> // https://github.com/Bodmer/JSON_Decoder

#include <OpenWeather.h>  // Latest here: https://github.com/Bodmer/OpenWeather
String wind[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW" };
float hourlySR, hourlyDT, hourlySS;  // 3 times used to find sunset/rise and compare to hourly dt.
float initialHourlyVal = 0.;  // Hour for leftmost column of hourly data.

#include "NTP_Time.h"     // Attached to this sketch, see that tab for library needs
time_t local_time;

/***************************************************************************************
**                          Define the globals and class instances
***************************************************************************************/

TFT_eSPI tft = TFT_eSPI();             // Invoke custom library

OW_Weather ow;      // Weather forecast library instance

OW_current *current; // Pointers to structs that temporarily holds weather data
OW_hourly  *hourly;  // Not used
OW_daily   *daily;

boolean Initial = true;

GfxUi ui = GfxUi(&tft); // Jpeg and bmpDraw functions TODO: pull outside of a class

long lastDownloadUpdate = millis();

//**************************************************************************************
// Setup
//*************************************************************************************/
void setup() {
  Serial.begin(115200); delay(1000);
  Serial.printf("This is \"OpenWeather (Mike)\" version %.2f running from:\r\n", Vers);
  Serial.println(__FILE__);

  tft.begin(); tft.setRotation(2);
  tft.fillScreen(TFT_BLACK);

  pinMode(changeForecastTypePin, INPUT);
  pinMode(show24HoursForecastPin, INPUT);

  // The begin() method opens a “storage space” with a defined namespace.
  // The false argument means that we’ll use it in read/write mode.
  // Use true to open or create the namespace in read-only mode.
  // Name the "folder" we will use and set for read/write.
  preferences.begin("BodmerWX", false);
  doDailyForecast = preferences.getBool("ForecastType", true);
  preferences.end();

#if defined TFT_BL
  ledcSetup(pwmLedChannelTFT, pwmFreq, pwmResolution);
  ledcAttachPin(TFT_BL, pwmLedChannelTFT); // TFT_BL, 0 - 15
  ledcWrite(pwmLedChannelTFT, 100);
#endif

  // With FORMAT_SPIFFS_IF_FAILED set to true, it should format on failure to open.
  // The program may continue to run but will have no ICONS.  Load them, even with the
  //  program running and they will start to appear.  Or just reboot the board.
  SPIFFS.begin();
  listFiles();

  // Enable if you want to erase SPIFFS, this takes some time!
  // then disable and reload sketch to avoid reformatting on every boot!
#ifdef FORMAT_SPIFFS
  tft.setTextDatum(BC_DATUM); // Bottom Centre datum
  tft.drawString("Formatting SPIFFS, please wait!", 120, 195); SPIFFS.format();
#endif

  // Draw splash screen
  if (SPIFFS.exists("/splash/OpenWeather.jpg") == true) ui.drawJpeg("/splash/OpenWeather.jpg", 0, 40);

  //  delay(500);

  // Clear bottom section of screen
  tft.fillRect(0, 206, 240, 320 - 206, TFT_BLACK);

  tft.loadFont(AA_FONT_SMALL);
  tft.setTextDatum(BC_DATUM); // Bottom Centre datum
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);

  tft.drawString("Original by: blog.squix.org", 120, 260);
  tft.drawString("Adapted by: Bodmer", 120, 280);

  tft.setTextColor(TFT_YELLOW, TFT_BLACK);

  delay(2000);

  tft.fillRect(0, 206, 240, 320 - 206, TFT_BLACK);
  
  tft.drawString("Morrow/Bodner Weather v" + String(Vers), 120, 210);
  tft.drawString("Connecting to WiFi", 120, 240);
  tft.drawString(WIFI_SSID, 120, 270);

  tft.setTextPadding(240); // Pad next drawString() text to full width to over-write old text

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Starting WiFi");
  int looper = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (looper++ > 20) ESP.restart();  // 10 seconds then give up and reboot to retry.
  }
  Serial.print("WiFi connected at "); Serial.println(WiFi.localIP());
  Serial.println("Starting OTA.");
  //  tft.drawString("Initialize OTA", 120, 240);  // This is gone so fast it is never seen.
  initOTA();

  tft.setTextDatum(BC_DATUM);
  tft.setTextPadding(tft.width()); // Pad next drawString() text to full width to over-write old text
  tft.drawString(" ", 120, 220);  // Clear line above using set padding width
  tft.drawString("Fetching weather data...", 120, 240);

  // Fetch the time
  udp.begin(localPort);
  syncTime();

  tft.unloadFont();

  ow.partialDataSet(false); // Collect the full set of available data
  tft.fillScreen(TFT_BLACK);

  // Dummy create to make the later "delete" happy.  At least, I think it is needed.
  current = new OW_current;
  daily =   new OW_daily;
  hourly =  new OW_hourly;

}
//**************************************************************************************
// Loop
//*************************************************************************************/
void loop() {

  ArduinoOTA.handle();
  if (digitalRead(changeForecastTypePin) == LOW) {  // Pressed?
    delay(50);
    if (digitalRead(changeForecastTypePin) == LOW) {    // Still pressed?
      Serial.println("Switching mini-forecast type.");
      doDailyForecast = !doDailyForecast;
      // This is done later, on the modulo 10 minute to save wear on the flash.
      //      preferences.begin("BodmerWX", false);
      //      preferences.putBool("ForecastType", doDailyForecast);
      //      preferences.end();
      tft.loadFont(AA_FONT_SMALL);
      drawForecast();  // Redraw the four forecast columns, only.
      tft.unloadFont();
      while (digitalRead(changeForecastTypePin) == LOW); // Wait for unpress
    }
  }

  if (digitalRead(show24HoursForecastPin) == LOW) {  // Pressed?
    delay(50);
    if (digitalRead(show24HoursForecastPin) == LOW) {   // Still pressed?
      Serial.println("Showing 24 2-hourly forecast columns for up to 60 seconds.");
      while (digitalRead(changeForecastTypePin) == LOW); // Wait for unpress
      show24Forecast();
      while (digitalRead(changeForecastTypePin) == LOW);   // Wait for unpress
      // updateData(false) says don't refetch
      Initial = true; updateData(waitThruFetchTime24); Initial = false;
    }
  }

  // Request time (may not result in a call to NTP server) and synchronise the local clock
  syncTime(); local_time = TIMEZONE.toLocal(now(), &tz1_Code);

  if (Initial || (lastSecond != second()))
  {
    if (hour(local_time) == 5 && minute(local_time) == 0 && second(local_time) == 0 &&
        weekday(local_time) == 4) ESP.restart();  // Weekly reboot to mitigate aging problems.

    // Update displayed time first as we may have to wait for a response
    drawTime();

#ifdef SCREEN_SERVER
    screenServer();
#endif
  }

  // Check if we should update weather information. Done hourly.
  // If you want it to be on the quarter hour, just change this to
  //  minute(local_time) & 15.  Same for any other repeat time.
  //  Be sure to also check for second(local_time) == 0 lest it repeat
  //  for the whole minute.
  if (Initial || (lastHour != hour(local_time)))
  {
#if defined TFT_BL
    Serial.printf("Setting screen brightness for hour %i to %i\r\n",
                  hour(local_time), hourlyBrilliance[hour(local_time)]);
    ledcWrite(pwmLedChannelTFT, hourlyBrilliance[hour(local_time)]);  // Reset display blacklighting.
  }
#endif
  if (minute(local_time) % 10 == 0 && second(local_time) == 0) {  // Window open for 2 seconds only.
    preferences.begin("BodmerWX", false);
    bool bTemp = preferences.getBool("ForecastType", true);
    if (bTemp != doDailyForecast) {
      preferences.putBool("ForecastType", doDailyForecast);
      Serial.println("New Daily Forecast preference saved.");
    }
    preferences.end();
  }
  // The "fetchFrequency", in the "if" means update every that many minutes.
  // Same for 10 or whatever, 1 to 60.  There are 1000 free calls per DAY!
  // The second() == 0 part is to ensure that it does not happen more than once
  //  during that minute for a maximum of one time per update window.
  // Comment this and uncomment above for hourly.
  if (Initial || (minute(local_time) % fetchFrequency == 0 && second(local_time) == 0))
    updateData(true);  // Time to get an update from OWM (option=true) and refresh the screen.

  Initial = false;
  lastSecond = second(local_time);
  lastMinute = minute(local_time);
  lastHour   = hour(local_time);
}
/***************************************************************************************
** Show 12 Hourly forcasts spaced by 2 hours.
***************************************************************************************/
void show24Forecast() {

  byte bIndex = 1;
  waitThruFetchTime24 = false;
  tft.loadFont(AA_FONT_SMALL);
  tft.fillScreen(TFT_BLACK);

  String heading = "2-Hourly Forecasts";
  tft.setTextDatum(BC_DATUM);
  tft.drawString(heading, tft.width() / 2, 15);

  drawHourlyForecastDetail(  8,  40, bIndex); bIndex += 2;
  drawHourlyForecastDetail( 66,  40, bIndex); bIndex += 2;
  drawHourlyForecastDetail(124,  40, bIndex); bIndex += 2;
  drawHourlyForecastDetail(182,  40, bIndex); bIndex += 2;
  drawHourlyForecastDetail(  8, 140, bIndex); bIndex += 2;  // Commenting the rest saved 96 bytes.
  drawHourlyForecastDetail( 66, 140, bIndex); bIndex += 2;  // Had to add an int and loop statement.
  drawHourlyForecastDetail(124, 140, bIndex); bIndex += 2;
  drawHourlyForecastDetail(182, 140, bIndex); bIndex += 2;
  drawHourlyForecastDetail(  8, 240, bIndex); bIndex += 2;
  drawHourlyForecastDetail( 66, 240, bIndex); bIndex += 2;
  drawHourlyForecastDetail(124, 240, bIndex); bIndex += 2;
  drawHourlyForecastDetail(182, 240, bIndex);

  while (digitalRead(show24HoursForecastPin) == LOW);     // Still pressed?  Wait here!

  Serial.println("Waiting for early exit click or 60 seconds.");
  for (int loop = 0; loop < 60000; loop++) {
    // See if we waited through an data update time.
    if ((minute() % fetchFrequency) == 0 && second() == 0) waitThruFetchTime24 = true;
    delay(1);
    if (digitalRead(show24HoursForecastPin) == LOW) {  // Pressed?
      delay(50);
      if (digitalRead(show24HoursForecastPin) == LOW)     // Still pressed? If so, exit loop.
        break;  // User requested early exit.
    }
  }
  while (digitalRead(show24HoursForecastPin) == LOW);     // Still pressed?
  tft.unloadFont();
  tft.fillScreen(TFT_BLACK);
}
/***************************************************************************************
**                          Fetch the weather data  and update screen
***************************************************************************************/
// Update the Internet based information and update screen
void updateData(bool doFetch) {
  bool parsed = true;
  // Initial = true;  // Test only
  // Initial = false; // Test only

  if (doFetch) {
    if (Initial)
      drawProgress(20, "Updating time...");
    else
      fillSegment(22, 22, 0, (int) (20 * 3.6), 16, TFT_NAVY);

    if (Initial)
      drawProgress(50, "Updating conditions...");
    else
      fillSegment(22, 22, 0, (int) (50 * 3.6), 16, TFT_NAVY);

    // Delete to free up space
    delete current;
    delete hourly;
    delete daily;
    // Create the structures that hold the retrieved weather
    current = new OW_current;
    daily =   new OW_daily;
    hourly =  new OW_hourly;

#ifdef RANDOM_LOCATION // Randomly choose a place on Earth to test icons etc
    String latitude = "";
    latitude = (random(180) - 90);
    String longitude = "";
    longitude = (random(360) - 180);
    Serial.print("Lat = "); Serial.print(latitude);
    Serial.print(", Lon = "); Serial.println(longitude);
#endif

    //On the ESP8266 (only) the library by default uses BearSSL, another option is to use AXTLS
    //For problems with ESP8266 stability, use AXTLS by adding a false parameter thus       vvvvv
    //ow.getForecast(current, hourly, daily, api_key, latitude, longitude, units, language, false);
    Serial.print("\r\n---->>>> Getting Weather data at "); printLocalTime();
    parsed = ow.getForecast(current, hourly, daily, api_key, latitude, longitude, units, language);

    if (parsed) {
      Serial.println("Data points received on first try.");
    } else {
      Serial.println("Failed to get data points. Trying a second time...");
      parsed = ow.getForecast(current, hourly, daily, api_key, latitude, longitude, units, language);
      if (parsed) {
        Serial.println("Data points received on 2nd try.");
      } else {
        Serial.println("Failed to get data points. Trying last time...");
        parsed = ow.getForecast(current, hourly, daily, api_key, latitude, longitude, units, language);
        if (parsed) {
          Serial.println("Data points received on 3rd try.");
        } else {
          Serial.println("No data received.  Check WiFi and Internet connections.");
          Serial.println("Waiting 1 minute then rebooting to try again.");
          tft.fillScreen(TFT_BLACK);
          tft.loadFont(AA_FONT_SMALL);
          tft.setTextDatum(BC_DATUM);
          tft.setTextColor(TFT_RED, TFT_BLACK);
          tft.drawString("Cannot get weather data.", tft.width() / 2, 100);
          tft.setTextColor(TFT_ORANGE, TFT_BLACK);
          tft.drawString("Waiting 1 minute,",      tft.width() / 2, 150);
          tft.drawString("then rebooting.",          tft.width() / 2, 200);
          delay(60000);
          ESP.restart();
        }
      }
    }
    Serial.print("Free heap = "); Serial.println(ESP.getFreeHeap(), DEC);

    printWeather(); // For debug, turn on output with #define SERIAL_MESSAGES

    if (Initial)
    {
      drawProgress(100, "Done...");
      delay(2000);
      tft.fillScreen(TFT_BLACK);
    }
    else
    {
      fillSegment(22, 22, 0, 360, 16, TFT_NAVY);
      fillSegment(22, 22, 0, 360, 22, TFT_BLACK);
    }

  }  // if (doFetch)
  if (parsed)
  {
    tft.loadFont(AA_FONT_SMALL);
    drawCurrentWeather();
    drawForecast();
    drawAstronomy();
    tft.unloadFont();

    // Update the temperature here so we don't need to keep
    // loading and unloading font which takes time
    tft.loadFont(AA_FONT_LARGE);
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    // Font ASCII code 0xB0 is a degree symbol, but o used instead in small font
    tft.setTextPadding(tft.textWidth(" -88")); // Max width of values

    String weatherText = "";
    weatherText = String(current->temp, 0);  // Make it integer temperature
    tft.drawString(weatherText, 225, 95); //  + "°" symbol is big... use o in small font // was 215
    tft.unloadFont();
  }
  else
  {
    Serial.println("Failed to get weather");
  }
}
/***************************************************************************************
**                          Update progress bar
***************************************************************************************/
void drawProgress(uint8_t percentage, String text) {
  tft.loadFont(AA_FONT_SMALL);
  tft.setTextDatum(BC_DATUM);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.setTextPadding(240);
  tft.drawString(text, 120, 260);

  ui.drawProgressBar(10, 269, 240 - 20, 15, percentage, TFT_WHITE, TFT_BLUE);

  tft.setTextPadding(0);
  tft.unloadFont();
}
/***************************************************************************************
**                          Draw the clock digits
***************************************************************************************/
void drawTime() {
  tft.loadFont(AA_FONT_LARGE);

  // Convert UTC to local time, returns zone code in tz1_Code, e.g "GMT"
  // Now done in loop for all to use.
  //  time_t local_time = TIMEZONE.toLocal(now(), &tz1_Code);

  String timeNow = "";
  if (hour(local_time) < 10)     timeNow += "0"; timeNow += hour(local_time);   timeNow += ":";
  if (minute(local_time) < 10)   timeNow += "0"; timeNow += minute(local_time); timeNow += ":";
  if (second(local_time) < 10)   timeNow += "0"; timeNow += second(local_time);
  tft.setTextDatum(BL_DATUM);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextPadding(tft.textWidth(" 44:44 "));  // String width + margin
  tft.drawString(timeNow, 50, 53);

  drawSeparator(51);

  tft.setTextPadding(0);
  tft.unloadFont();
}
/***************************************************************************************
**                          Draw the current weather
***************************************************************************************/
void drawCurrentWeather() {
  String date = "Updated: " + strDate(current->dt);
  String weatherText = "None";
  String conv = "";

  tft.setTextDatum(BC_DATUM);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.setTextPadding(tft.textWidth(" Updated: Mmm 44 44:44 "));  // String width + margin
  tft.drawString(date, 120, 16);

  String weatherIcon = "";

  String currentSummary = current->main;
  currentSummary.toLowerCase();

  initialHourlyVal  = float(hour(TIMEZONE.toLocal(current->dt, &tz1_Code))) +
                      float(hour(TIMEZONE.toLocal(current->dt, &tz1_Code))) / 60.;
  weatherIcon = getMeteoconIcon(current->id, current->dt, true);

  //uint32_t dt = millis();
  ui.drawBmp("/icon/" + weatherIcon + ".bmp", 0, 53);
  //Serial.print("Icon draw time = "); Serial.println(millis()-dt);

  // Weather Text
  if (language == "en")
    weatherText = current->main;
  else
    weatherText = current->description;

  tft.setTextDatum(BR_DATUM);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);

  int splitPoint = 0;
  int xpos = 235;
  splitPoint =  splitIndex(weatherText);

  tft.setTextPadding(xpos - 100);  // xpos - icon width
  if (splitPoint) {
    Serial.println("Found splitPoint");
    tft.drawString(weatherText.substring(0, splitPoint), xpos, 72);
    tft.drawString(weatherText.substring(splitPoint), xpos, 90);
  } else {
    Serial.println("No splitPoint");
    tft.drawString(weatherText, xpos, 72);
  }
//    tft.drawString("Second", xpos, 90);  // Testing  

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(TR_DATUM);
  tft.setTextPadding(0);
  if (units == "metric") {
    tft.drawString("C", 237, 95);  // This is the C or F to the right of the large yellow temp.
    conv = String(current->temp, 0) + "C=" + String((current->temp * 9. / 5.) + 32., 0) + "F";
  } else {
    tft.drawString("F", 237, 95);  // This is the C or F to the right of the large yellow temp.
    conv = String(current->temp, 0) + "F=" + String(5. / 9. * (current->temp - 32.), 0) + "C";
  }
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(BC_DATUM);
  tft.setTextPadding(tft.textWidth(conv));
  tft.drawString(conv, (tft.width() / 2 + 5)/* - (tft.textWidth(conv) / 2)*/, 72);
  tft.setTextPadding(0);

  //Temperature large digits moved to updateData() to save swapping font here

  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  weatherText = String(current->wind_speed, 0);

  if (units == "metric")
    weatherText += " m/s";
  else
    weatherText += " mph";

  tft.setTextDatum(TC_DATUM);
  tft.setTextPadding(tft.textWidth("888 m/s")); // Max string length?
  tft.drawString(weatherText, 124, 136);

  if (units == "imperial")
  {
    weatherText = current->pressure * 0.02953;
    weatherText += " in";
  }
  else
  {
    weatherText = String(current->pressure, 0);
    weatherText += " hPa";
  }

  tft.setTextDatum(TR_DATUM);
  tft.setTextPadding(tft.textWidth(" 8888hPa")); // Max string length?
  tft.drawString(weatherText, 230, 136);

  int windAngle = (current->wind_deg + 22.5) / 45;
  if (windAngle > 7) windAngle = 0;
  //  String wind[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW" };
  ui.drawBmp("/wind/" + wind[windAngle] + ".bmp", 101, 80);  // was 101, 84

  drawSeparator(153);

  tft.setTextDatum(TL_DATUM); // Reset datum to normal
  tft.setTextPadding(0);      // Reset padding width to none
}
/***************************************************************************************
**                          Draw the 4 forecast columns
***************************************************************************************/
// draws the four forecast columns.
// Depending on the setting of the boolean, it will either draw the four following days'
//  forecasts or will draw four forecasts starting with the next hour and skipping to 3
//  hours for each of the following 3 columns.  So, it will show forecast for the next hour,
//  then the forecast for 3 hours out from there and 3 more and 3 more hours out for a total
//  of four columns of hourly forecasts.
//
void drawForecast() {

  int8_t bIndex = 1;

  if (doDailyForecast) {  // Either do four columms of dailies or ...
    drawDailyForecastDetail(  8, 171, bIndex++);
    drawDailyForecastDetail( 66, 171, bIndex++);
    drawDailyForecastDetail(124, 171, bIndex++);
    drawDailyForecastDetail(182, 171, bIndex  );
  } else {  // ...four columns of hourlys starting with the next hour, then +3 hours each * 3.
    drawHourlyForecastDetail(  8, 171, bIndex); bIndex += 3;
    drawHourlyForecastDetail( 66, 171, bIndex); bIndex += 3;
    drawHourlyForecastDetail(124, 171, bIndex); bIndex += 3;
    drawHourlyForecastDetail(182, 171, bIndex );
  }
  drawSeparator(171 + 69);
}
//**************************************************************************************
// Draw 1 hourly forecast column at x, y
//*************************************************************************************/
// helper for the forecast columns
void drawHourlyForecastDetail(uint16_t x, uint16_t y, uint8_t hourIndex) {

  int iHour = hour(TIMEZONE.toLocal(hourly->dt[hourIndex], &tz1_Code));
  if (hourIndex == 1) initialHourlyVal = float(iHour);
  // Save off the hour.  Then any smaller hour uses tomorrow's sunrise/set
  String jHour  = "";
  if (iHour < 10)
    jHour = "0" + String(iHour);
  else
    jHour = String(iHour);
  jHour += ":00";
  tft.setTextDatum(BC_DATUM);

  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.setTextPadding(tft.textWidth("WWWWW"));
  tft.drawString(jHour, x + 25, y);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextPadding(tft.textWidth("-88   -88"));
  String Temp = String(hourly->temp[hourIndex], 0);

  if (units == "metric")
    Temp += " C";
  else
    Temp += " F";

  tft.drawString(Temp, x + 25, y + 17);
  //  Serial.println("\r\nGetting Hourly ICONs");
  //  Serial.printf("hourIndex %i, Hour %i, ID / 100 %i, current->sunrise %i, current->dt %i, current->sunset %i\r\n",
  //                hourIndex,     iHour, hourly->id[hourIndex] / 100, current->sunrise, current->dt, current->sunset);

  // Get ICON name for hourly column
  String weatherIcon = getMeteoconIcon(hourly->id[hourIndex], hourly->dt[hourIndex], true);  // qwer

  ui.drawBmp("/icon50/" + weatherIcon + ".bmp", x, y + 18);

  tft.setTextPadding(0); // Reset padding width to none
}
//**************************************************************************************
// Draw 1 daily forecast column at x, y
//**************************************************************************************
// helper for the forecast columns
void drawDailyForecastDetail(uint16_t x, uint16_t y, uint8_t dayIndex) {

  if (dayIndex >= MAX_DAYS) return;

  String day  = shortDOW[weekday(TIMEZONE.toLocal(daily->dt[dayIndex], &tz1_Code))];
  day.toUpperCase();

  tft.setTextDatum(BC_DATUM);

  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.setTextPadding(tft.textWidth("WWW"));
  tft.drawString(day, x + 25, y);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextPadding(tft.textWidth("-88   -88"));
  String highTemp = String(daily->temp_max[dayIndex], 0);
  String lowTemp  = String(daily->temp_min[dayIndex], 0);
  tft.drawString(lowTemp + "-" + highTemp, x + 25, y + 17);

  // Get ICON name for daily column. This should always show a daytime ICON, never convert.
  String weatherIcon = getMeteoconIcon(daily->id[dayIndex], current->dt, false);

  ui.drawBmp("/icon50/" + weatherIcon + ".bmp", x, y + 18);

  tft.setTextPadding(0); // Reset padding width to none
}
//**************************************************************************************
// Draw Sun & Moon rise/set, Moon phase, cloud cover and humidity
//**************************************************************************************
void drawAstronomy() {

  tft.setTextDatum(BC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextPadding(tft.textWidth(" Last qtr "));

  //  time_t local_time = TIMEZONE.toLocal(current->dt, &tz1_Code);
  uint16_t y = year(local_time);
  uint8_t  m = month(local_time);
  uint8_t  d = day(local_time);
  uint8_t  h = hour(local_time);
  int      ip;
  uint8_t icon = moon_phase(y, m, d, h, &ip);

  tft.drawString(moonPhase[ip], 125, 319);
  ui.drawBmp("/moon/moonphase_L" + String(icon) + ".bmp", 120 - 30, 318 - 16 - 60);

  tft.setTextDatum(BC_DATUM);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.setTextPadding(0); // Reset padding width to none
  tft.drawString(sunStr,  40, 260);
  tft.drawString(moonStr, 40, 299);
  tft.drawString(cloudStr, 195, 260);
  tft.drawString(humidityStr, 195, 299);

  tft.setTextDatum(BR_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextPadding(tft.textWidth(" 88:88/88:88 "));

  String sunRiseSet = strTime(current->sunrise) + "/" + strTime(current->sunset) + " ";
  int dt = rightOffset(sunRiseSet, "/");  // Draw relative to colon to keep them aligned
  tft.drawString(sunRiseSet, 40 + dt, 280);

  String moonRiseSet = strTime(daily->moonrise[0]) + "/" + strTime(daily->moonset[0]) + " ";
  dt = rightOffset(moonRiseSet, "/"); // Draw relative to colon to keep them aligned
  tft.drawString(moonRiseSet, 40 + dt, 319);

  String cloudCover = "";
  cloudCover += current->clouds; cloudCover += "%";

  tft.setTextDatum(BR_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextPadding(tft.textWidth(" 100%"));
  tft.drawString(cloudCover, 210, 280);

  String humidity = "";
  humidity += current->humidity; humidity += "%";

  tft.setTextDatum(BR_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextPadding(tft.textWidth("100%"));
  tft.drawString(humidity, 210, 319);

  tft.setTextPadding(0); // Reset padding width to none
}
/***************************************************************************************
**                          Get the icon file name from the index number
***************************************************************************************/
const char* getMeteoconIcon(uint16_t id, time_t myDT, bool cvtToNight)
{
  time_t iTime;
  //  Serial.printf("Entering getMeteoconIcon with id %i, Convert: %s, time %i\r\n",
  //                id, cvtToNight ? "Yes" : "No", myDT);
  if (cvtToNight) {  // Only needed for the hourly display columns' ICON
    iTime = TIMEZONE.toLocal(myDT, &tz1_Code);
    hourlyDT = float(hour(iTime)) + float(minute(iTime)) / 60.; // Hour for this column.
    if (hourlyDT < initialHourlyVal) {  // If we have wrapped the clock, use tomorrow's sunrise/set.
      // Serial.printf("Using tomorrow's sunrise/set values for start hour %.2f, forecast hour %.2f.\r\n",
      //                initialHourlyVal, hourlyDT);
      iTime = TIMEZONE.toLocal(daily->sunrise[1], &tz1_Code);
      hourlySR = float(hour(iTime)) + float(minute(iTime)) / 60.;

      iTime = TIMEZONE.toLocal(daily->sunset[1], &tz1_Code);
      //      Serial.printf("1-iTime daily sunset[1] %i\r\n", daily->sunset[1]);
      //      Serial.printf("1-iTime daily sunset[1] %i\r\n", iTime);
      //      Serial.printf("1-Hour computed %.2f, Minute %.2f\r\n",
      //                    float(hour(iTime)), float(minute(iTime)) / 60.);
      hourlySS = float(hour(iTime)) + float(minute(iTime)) / 60.;

      //      Serial.printf("hourlyDT %.2f, hourlySR %.2f, hourlySS %.2f\r\n",
      //                    hourlyDT, hourlySR, hourlySS);
    } else {
      //      Serial.printf("Using today's    sunrise/set values for start hour %.2f, current hour %.2f.\r\n",
      //                    initialHourlyVal, hourlyDT);
      iTime = TIMEZONE.toLocal(daily->sunrise[0], &tz1_Code);
      hourlySR = float(hour(iTime)) + float(minute(iTime)) / 60.;

      iTime = TIMEZONE.toLocal(daily->sunset[0], &tz1_Code);
      //      Serial.printf("0-iTime daily sunset[0] %i\r\n", daily->sunset[0]);
      //      Serial.printf("0-iTime daily sunset[0] %i\r\n", iTime);
      //      Serial.printf("0-Hour computed %.2f, Minute %.2f\r\n",
      //                    float(hour(iTime)), float(minute(iTime)) / 60.);
      hourlySS = float(hour(iTime)) + float(minute(iTime)) / 60.;

      //      Serial.printf("hourlyDT %.2f, hourlySR %.2f, hourlySS %.2f\r\n", hourlyDT, hourlySR, hourlySS);
    }
    if (id / 100 == 8 && (hourlyDT <= hourlySR || hourlyDT >= hourlySS)) id += 1000;  // Night for Day.
    //    Serial.printf("Ending id value %i\r\n", id);
  }

  //  if (id / 100 == 2)          return "thunderstorm";
  //  if (id / 100 == 3)          return "drizzle";
  //  if (id / 100 == 4)          return "unknown";
  //  if (id == 500)              return "lightRain";
  //  else if (id == 511)         return "sleet";
  //  else if (id / 100 == 5)     return "rain";
  //  if (id >= 611 && id <= 616) return "sleet";
  //  else if (id / 100 == 6)     return "snow";
  //  if (id / 100 == 7)          return "fog";
  //  if (id == 800)              return "clear-day";
  //  if (id == 801)              return "partly-cloudy-day";
  //  if (id == 802)              return "cloudy";
  //  if (id == 803)              return "cloudy";
  //  if (id == 804)              return "cloudy";
  //  if (id == 1800)             return "clear-night";
  //  if (id == 1801)             return "partly-cloudy-night";
  //  if (id == 1802)             return "cloudy";
  //  if (id == 1803)             return "cloudy";
  //  if (id == 1804)             return "cloudy";
  // Those "else" bits seem totally useless since the action, if true, is to return a value, ending the subroutine.
  if (id / 100 == 2)          return "thunderstorm";
  if (id / 100 == 3)          return "drizzle";
  if (id / 100 == 4)          return "unknown";
  if (id == 500)              return "lightRain";
  if (id == 511)              return "sleet";
  if (id / 100 == 5)          return "rain";
  if (id >= 611 && id <= 616) return "sleet";
  if (id / 100 == 6)          return "snow";
  if (id / 100 == 7)          return "fog";
  if (id == 800)              return "clear-day";
  if (id == 801)              return "partly-cloudy-day";
  if (id == 802)              return "cloudy";
  if (id == 803)              return "cloudy";
  if (id == 804)              return "cloudy";
  if (id == 1800)             return "clear-night";
  if (id == 1801)             return "partly-cloudy-night";
  if (id == 1802)             return "cloudy";
  if (id == 1803)             return "cloudy";
  if (id == 1804)             return "cloudy";
  return "unknown";
}
/***************************************************************************************
**                          Draw screen section separator line
***************************************************************************************/
// if you don't want separators, comment out the tft-line
void drawSeparator(uint16_t y) {
  tft.drawFastHLine(10, y, 240 - 2 * 10, 0x4228);
}
/***************************************************************************************
**                          Determine place to split a line line
***************************************************************************************/
// determine the "space" split point in a long string
int splitIndex(String text)
{
  uint16_t index = 0;
  while ( (text.indexOf(' ', index) >= 0) && ( index <= text.length() / 2 ) ) {
    index = text.indexOf(' ', index) + 1;
  }
  if (index) index--;
  return index;
}
/***************************************************************************************
**                          Right side offset to a character
***************************************************************************************/
// Calculate coord delta from end of text String to start of sub String contained within that text
// Can be used to vertically right align text so for example a colon ":" in the time value is always
// plotted at same point on the screen irrespective of different proportional character widths,
// could also be used to align decimal points for neat formatting
int rightOffset(String text, String sub)
{
  int index = text.indexOf(sub);
  return tft.textWidth(text.substring(index));
}
/***************************************************************************************
**                          Left side offset to a character
***************************************************************************************/
// Calculate coord delta from start of text String to start of sub String contained within that text
// Can be used to vertically left align text so for example a colon ":" in the time value is always
// plotted at same point on the screen irrespective of different proportional character widths,
// could also be used to align decimal points for neat formatting
int leftOffset(String text, String sub)
{
  int index = text.indexOf(sub);
  return tft.textWidth(text.substring(0, index));
}
/***************************************************************************************
**                          Draw circle segment
***************************************************************************************/
// Draw a segment of a circle, centred on x,y with defined start_angle and subtended sub_angle
// Angles are defined in a clockwise direction with 0 at top
// Segment has radius r and it is plotted in defined colour
// Can be used for pie charts etc, in this sketch it is used for wind direction
#define DEG2RAD 0.0174532925 // Degrees to Radians conversion factor
#define INC 2 // Minimum segment subtended angle and plotting angle increment (in degrees)
void fillSegment(int x, int y, int start_angle, int sub_angle, int r, unsigned int colour)
{
  // Calculate first pair of coordinates for segment start
  float sx = cos((start_angle - 90) * DEG2RAD);
  float sy = sin((start_angle - 90) * DEG2RAD);
  uint16_t x1 = sx * r + x;
  uint16_t y1 = sy * r + y;

  // Draw colour blocks every INC degrees
  for (int i = start_angle; i < start_angle + sub_angle; i += INC) {

    // Calculate pair of coordinates for segment end
    int x2 = cos((i + 1 - 90) * DEG2RAD) * r + x;
    int y2 = sin((i + 1 - 90) * DEG2RAD) * r + y;

    tft.fillTriangle(x1, y1, x2, y2, x, y, colour);

    // Copy segment end to segment start for next segment
    x1 = x2;
    y1 = y2;
  }
}
/***************************************************************************************
**                          Print the weather info to the Serial Monitor
***************************************************************************************/
void printWeather(void)
{
#ifdef SERIAL_MESSAGES
  Serial.println("Weather from OpenWeather\n");

  Serial.println("############### Current weather ###############\n");
  Serial.print("dt (time) : "); Serial.println(strDate(current->dt));

  Serial.print("sunrise    : "); Serial.println(strDate(current->sunrise));
  Serial.print("sunrise    : "); Serial.println(current->sunrise);
  Serial.print("sunset     : "); Serial.println(strDate(current->sunset));
  Serial.print("sunset     : "); Serial.println(current->sunset);

  Serial.print("main       : "); Serial.println(current->main);
  Serial.print("temp       : "); Serial.println(current->temp);
  Serial.print("humidity   : "); Serial.println(current->humidity);
  Serial.print("pressure   : "); Serial.println(current->pressure);
  Serial.print("wind_speed : "); Serial.println(current->wind_speed);
  Serial.print("wind_deg   : "); Serial.println(current->wind_deg);
  Serial.print("clouds     : "); Serial.println(current->clouds);
  Serial.print("id         : "); Serial.println(current->id);
  Serial.println();

  Serial.println("###############  Daily weather  ###############\n");
  Serial.println();

  for (int i = 0; i < 5; i++)
  {
    Serial.print("dt (time): "); Serial.println(strDate(daily->dt[i]));
    Serial.print("id       : "); Serial.println(daily->id[i]);
    Serial.print("temp_max : "); Serial.println(daily->temp_max[i]);
    Serial.print("temp_min : "); Serial.println(daily->temp_min[i]);
    // Serial.print("moonrise : "); Serial.println(daily->moonrise[i]);
    // Serial.print("moonset  : "); Serial.println(daily->moonset[i]);
    Serial.print("moonrise : "); Serial.println(strDate(daily->moonrise[i]));
    Serial.print("moonset  : "); Serial.println(strDate(daily->moonset[i]));

    Serial.println();
  }

#endif
}
/***************************************************************************************
**             Convert Unix time to a "local time" time string "12:34"
***************************************************************************************/
String strTime(time_t unixTime)
{
  time_t local_time = TIMEZONE.toLocal(unixTime, &tz1_Code);

  String localTime = "";

  if (hour(local_time) < 10) localTime += "0";
  localTime += hour(local_time);
  localTime += ":";
  if (minute(local_time) < 10) localTime += "0";
  localTime += minute(local_time);

  return localTime;
}
/***************************************************************************************
**  Convert Unix time to a local date + time string "Oct 16 17:18", ends with newline
***************************************************************************************/
String strDate(time_t unixTime)
{
  time_t local_time = TIMEZONE.toLocal(unixTime, &tz1_Code);

  String localDate = "";

  localDate += monthShortStr(month(local_time));
  localDate += " ";
  localDate += day(local_time);
  localDate += " " + strTime(unixTime);

  return localDate;
}
/****************************************************************************/
void initOTA()
/****************************************************************************/
{
  ArduinoOTA.setHostname(OTAhostname.c_str()); //define OTA port hostname
  ArduinoOTA.begin();
}
//====================================================================================
// Print the current UTC time to the Serial monitor
//====================================================================================
void printUTCTime() {
  // Print the UTC hour, minute and second:
  Serial.print("Received NTP UTC time : ");

  uint8_t hh = hour(utc);
  Serial.print(hh); // print the hour (86400 equals secs per day)

  Serial.print(':');
  uint8_t mm = minute(utc);
  if (mm < 10 ) Serial.print('0');
  Serial.print(mm); // print the minute (3600 equals secs per minute)

  Serial.print(':');
  uint8_t ss = second(utc);
  if ( ss < 10 ) Serial.print('0');
  Serial.println(ss); // print the second
}
//====================================================================================
// Print the current Local time to the Serial monitor
//====================================================================================
void printLocalTime() {
  // Print the UTC hour, minute and second:

  uint8_t hh = hour(local_time);
  uint8_t mm = minute(local_time);
  uint8_t ss = second(local_time);
  //  Serial.print(hh); // print the hour (86400 equals secs per day)
  //  Serial.print(':');
  //  if (mm < 10 ) Serial.print('0');
  //  Serial.print(mm); // print the minute (3600 equals secs per minute)
  //  Serial.print(':');
  //  if ( ss < 10 ) Serial.print('0');
  //  Serial.println(ss); // print the second
  Serial.printf("%02i:%02i:%02i\r\n", hh, mm, ss);
}
/**The MIT License (MIT)
  Copyright (c) 2015 by Daniel Eichhorn
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYBR_DATUM HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
  See more at http://blog.squix.ch
*/

//  Changes made by Bodmer:

//  Minor changes to text placement and auto-blanking out old text with background colour padding
//  Moon phase text added (not provided by OpenWeather)
//  Forecast text lines are automatically split onto two lines at a central space (some are long!)
//  Time is printed with colons aligned to tidy display
//  Min and max forecast temperatures spaced out
//  New smart splash startup screen and updated progress messages
//  Display does not need to be blanked between updates
//  Icons nudged about slightly to add wind direction + speed
//  Barometric pressure added

//  Adapted to use the OpenWeather library: https://github.com/Bodmer/OpenWeather
//  Moon phase/rise/set (not provided by OpenWeather) replace with cloud cover and humidity
//  Created and added new 100x100 and 50x50 pixel weather icons, these are in the
//   sketch data folder, press Ctrl+K to view
//  Add moon icons, eliminate all downloads of icons (may lose server!)
//  Adapted to use anti-aliased fonts, tweaked coords
//  Added forecast for 4th day
//  Added cloud cover and humidity in lieu of Moon rise/set
//  Adapted to be compatible with ESP32

//  Changed made by Mike Morrow -- July 2023

//  Taking nothing away from Bodmer!  This is very nice code but it is old.  Things have changed.
//  Various text alignments to make everything match with the new information for moon rise/set.
//  Reboot after 10 seconds of no WiFi connection.  This is needed by some WAPs.
//  Added OTA capability.
//  Restructured loop() code for move flexibility of when to run code by time.
//  Added moon rise and set times aligned with sun rise and set fields.
//  Broke out printUTC and printLocal times into separate routines for printing convenience.
//  Added display dimming PWM code.
//  Added table-driven hourly adjustment of the display brightness by PWM.
//  I also cleaned up the OpenWeather.cpp to get rid of excessive blank lines and to print out
//   the URL being sent in, just in case you need to figure out what's wrong with it.
//  Note: Sometimes, the UTC and local_time epochs are a second out of sync.  I can't, yet, figure
//        out how that is happening.
//  Added a 24 hour prediction for each 2 hours (12 predictions) upon a button press.  It stays up
//   for 60 seconds and will auto-clear or press the same button used to invoke it for early exit.
