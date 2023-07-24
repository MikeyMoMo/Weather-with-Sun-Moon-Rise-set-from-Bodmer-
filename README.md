# Weather-with-Sun-Moon-Rise-set-from-Bodmer-
This is a modification of code from Bodmer (author or tft_eSPI) with some fixups.  It has several changes.  It is set for a 240x320 screen.  99% of this is from Bodmer with previous contributions by others.  See the code.  This is NOT original to me.  I just did some modifications to it.  

You may want to edit the hourly brilliance table depending on your needs.  It sets the brightness of the display each hour.  Max is 255, min is 0 (off).

Latest addition is an hourly forecast that alternates with the daily forecast based on a switch.  To use this, you need to obtain the library that has the support code for this project, here: https://github.com/Bodmer/OpenWeather
Then after you install, edit the file User_Setup.h and change one line.  That line is on line 9 of the file and defines the maximum number of hours that will be parsed from the hourly forecast.  I think it is set to 6 by default.  You need to change that to 24, save it and compile the program. The 24 enables 24 hours of 2-hourly forecasts on the 2nd screen.

Enjoy.

![20230721_003206](https://github.com/MikeyMoMo/Weather-with-Sun-Moon-Rise-set-from-Bodmer-/assets/15792417/375214a6-1e8c-49d4-b50e-bd7ecaff835b)

![20230721_003216](https://github.com/MikeyMoMo/Weather-with-Sun-Moon-Rise-set-from-Bodmer-/assets/15792417/2901e67f-984e-4662-997d-3347bd6ee2e5)

