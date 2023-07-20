//  Use the OpenWeather library: https://github.com/Bodmer/OpenWeather

//  The weather icons and fonts are in the sketch data folder, press Ctrl+K
//  to view.

//            >>>       IMPORTANT TO PREVENT CRASHES      <<<
//>>>>>>  Set SPIFFS to at least 1.5Mbytes before uploading files  <<<<<<

//                >>>           DON'T FORGET THIS             <<<
//  Upload the fonts and icons to SPIFFS using the "Tools"  "ESP32 Sketch Data Upload"
//  or "ESP8266 Sketch Data Upload" menu option in the IDE.
//  To add this option follow instructions here for the ESP8266:
//  https://github.com/esp8266/arduino-esp8266fs-plugin
//  To add this option follow instructions here for the ESP32:
//  https://github.com/me-no-dev/arduino-esp32fs-plugin

//  Close the IDE and open again to see the new menu option.

// You can change the number of hours and days for the forecast in the
// "User_Setup.h" file inside the OpenWeather library folder.
// By default this is 6 hours (can be up to 48) and 5 days
// (can be up to 8 days = today plus 7 days). This sketch requires
// at least 5 days of forecast. Forecast hours can be set to 1 as
// the hourly forecast data is not used in this sketch.

//////////////////////////////
// Setttings defined below

#define Vers 3.00

#define WIFI_SSID      "MikeysWAP"
#define WIFI_PASSWORD  "Noogly99"

#define TIMEZONE HK // See NTP_Time.h tab for other "Zone references", UK, usMT etc

// Update changed to hourly.  For 15 minutes, see loop() code.
// Update every 15 minutes, up to 1000 request per day are free (viz average of ~40 per hour)
//const int UPDATE_INTERVAL_SECS = 60 * 60UL; // 15 minutes

// Pins for the TFT interface are defined in the User_Config.h file inside the TFT_eSPI library

// For units use "metric" or "imperial"
const String units = "imperial";

// Sign up for a key and read API configuration info here:
// https://openweathermap.org/, change x's to your API key
const String api_key = "2874af657bd3f25f664000b1cbaddc66";

// Set the forecast longitude and latitude to at least 4 decimal places
const String latitude =  "18.512989"; // 90.0000 to -90.0000 negative for Southern hemisphere
const String longitude = "120.737259"; // 180.000 to -180.000 negative for West

// For language codes see https://openweathermap.org/current#multi
const String language = "en"; // Default language = en = English

// Short day of week abbreviations used in 4 day forecast (change to your language)
const String shortDOW [8] = {"???", "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};

bool doDailyForecast = true;  // This will be toggled by a switch depression, later, in loop().
#define changeForecastTypePin 37  // Change between daily and hourly forecasts in the four columns.

// Change the labels to your language here:
const char sunStr[]        = "Sun";
const char moonStr[]       = "Moon";
const char cloudStr[]      = "Cloud";
const char humidityStr[]   = "Humidity";
const String moonPhase [8] = {"New", "Waxing", "1st qtr", "Waxing",
                              "Full", "Waning", "Last qtr", "Waning"
                             };
// This sets the display brightness by the hour.  Range is 0 (off) to 255 (full brightness)
//                        00   01   02   03   04   05
int hourlyBrilliance[] = {30,  30,  30,  30,  30,  60,   //  0- 5
                          //06 07   08   09   10   11
                          90, 110, 130, 140, 150, 160,   //  6-11
                          //12  13   14   15   16   17
                          160, 160, 160, 160, 160, 150,  // 12-17
                          //18   19    20   21   22  23
                          140,  130,  110,  90,  70, 50  // 18-23
                         };
// End of user settings

// Setting PWM properties, do not change this!
const int pwmFreq = 5000;
const int pwmResolution = 8;
const int pwmLedChannelTFT = 4;  // Backlight PWM channel number

//////////////////////////////
