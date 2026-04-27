# PCB Design
- Made in KiCad with [CDFER's JLCPCB KiCad library](https://github.com/CDFER/JLCPCB-Kicad-Library). 
- Inspiration for antenna and power system taken from [CDFER's live train maps](https://keastudios.co.nz). They are very cool!
- Designed to be fabricated and assembled by JLCPCB
    - PCBA by JLC because tiny components and hundreds of leds are hard to assemble manually. Also, LCSC components are very cheap with JLC.

## Main components
- ESP32-C3FH4 (chip or module)
    - 4MiB built-in flash
    - Chip/module pros and cons (currently chip)
        - Chip is more aesthetical
        - Module has a built-in antenna
- Level Shifter (3V3 data from esp to 5V led data)
    - [TI SN74LV1T34DBVR](https://www.lcsc.com/product-detail/C100024.html)
- Power management
    - USB VBUS to 5V and 3V3
    - Mosfet to shut down 5V rail
- ARGB LEDs: [Xinglight XL-1615RGBC-2812B-S](https://www.lcsc.com/product-detail/C41413180.html)
    - The -S version is less bright and cheaper than the non -S version. 
- OLED Display ([SSD1306 i2c module](https://kauppa.sintosen.com/product/1829/096-tuuman-oled-iic--sarjaportti-oled-nayttomoduuli-128x64-i2c-ssd1306-arduinoon))
    - Can be left out for aesthetics/cost-savings 
    - Added by hand

## Schematic
![Schematic image](./img/schematic.svg)
