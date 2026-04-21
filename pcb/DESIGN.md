# PCB Design

## Main components
- ESP32-C3FH4 (chip or module)
    - Chip is more aesthetical
    - Module has a built-in antenna
- Level Shifter (3V3 data from esp to 5V led data)
    - [TI SN74LV1T34DBVR](https://www.lcsc.com/product-detail/C100024.html)
- Power management
    - USB VBUS to 5V and 3V3
    - (Maybe) power off mosfet to shut down 5V rail
- ARGB LEDs: [Xinglight XL-1615RGBC-2812B-S](https://www.lcsc.com/product-detail/C41413180.html)
    - The -S version is less bright and cheaper than the non -S version. 
- OLED Display (SSD1306 i2c module)
    - Can be left out for aesthetics/cost-savings 
    - Added by hand