# aqua_NODEMCU

Using NodeMCU for aquarium lights control with OTA

For DS3231 Library use https://github.com/NorthernWidget/DS3231/releases

Rest of the libraries can be found within Arduino IDE Libraries Manager. Please google them for "how-to" if you cannot figure it out.

Please add http://arduino.esp8266.com/stable/package_esp8266com_index.json in File --> Preferences --> Additional Boards Manager URLs to get support for all ESO Boards.

Pin Configuration:

DS3231 and 128x64 OLED using I2C and are connected to

NodeMCU --> Device <br/>
D1 --> SCL <br/>
D2 --> SDA <br/>
3.3 --> VCC <br/>
G --> GND <br/>

Yes connect both the I2C devices to the same pin (purpose of I2C). If you are facing I2C device address related issues then please Google it. It is very common and easy to fix.

For the 4 Channel relay board

NodeMCU --> Device <br/>
D3 --> In1 <br/>
D5 --> In2 <br/>
D6 --> In3 <br/>
D7 --> In4 <br/>

For GND and VCC, use appropriate separate power supply (don't take power from NodeMCU, may burn). If you are using separate powersupplies for NodeMCU and Relay, then make sure to connect both the GNDs of Node and Powersupply together, else the relay module won't work.

-Parts: <br/>
<img src="https://m.media-amazon.com/images/I/71TWos73PrL._SL1100_.jpg" alt="Relay Board" width="200" height="200"> <br/>
4 Channel 5V Relay Board Module with Optocouplers <br/><br/>

<img src="https://m.media-amazon.com/images/I/41RP9FjC+jL.jpg" alt="DS3231" width="200" height="200"> <br/>
DS3231 AT24C32 IIC Precision RTC <br/><br/>


<img src="https://m.media-amazon.com/images/I/51lIrI5vnQL.jpg" alt="NodeMCU" width="200" height="200"> <br/>
NodeMCU-ESP8266 Development Board ESP12E <br/><br/>
 
