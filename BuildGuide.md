# Build guide

We will go over part ordering and then hands on assembly.

## Ordering PCB

The PCB is the most expensive part of this project and can cost upwards of 200$ depending on shipping cost and manufacturer. It's ordered from a manufacturer like [JLCPCB](https://jlcpcb.com/). You should order the PCB assembled so as a PCBA. We ordered from [JLCPCB](https://jlcpcb.com/) and made ready to use files in the [jlcpcb folder](./pcb/jlcpcb/). The production files folder includes ready zip files, bom and clp files that can just be added when ordering. We also included the gerber files in non zip format  in the [`/pcb/jlcpcb/gerber`](./pcb/jlcpcb/gerber/) folder. There is a minimum order amount of 5 so you will most likely have to order extra if you want to make this project.

Here are somethings to keep in mind when ordering from JLCPCB/what options we selected for the order.

* We ordered the pcb in black with a white silkscreen.
* Lead free HASL
* Standard PCBA (required because of the amount of LEDs)
* Assembly side: top
* Bake components selected because the JLCPCB part #C5349954 needs to be baked.
* Board cleaning: yes
* Depanel boards & edge rail before delivery: yes
* Antenna shows up as a component in the BOM but it doesn't have to be selected as a component and is okay to be left as empty.
* The R10 and R12 parts that were mapped to the C11616 part were on shortfall so we replaced them with the C54920807 part.

Other options were left as defaults / not edited. If you want to check you have the same options you can see the [`/media/JLCPCB`](./media/JLCPCB/). Where there are screenshots of the order process.

## Ordering / printing the holder

Technically this project doesn't require a holder but if you want to hang this on the wall or having it stand on a desk you will need one. The wall mount is designed to be printed with minimal supports. It has slots for the PCB to slide into and it has a keyhole for mounting to the wall like a picture frame. You can either print this by yourself if you have a 3d printer or use a service like [JLC3DP](https://jlc3dp.com/). We had a printer we could use so we printed the holder ourselve. If you want more info about the design choices etc. for the holder check the [`/3dfiles`](./3dFiles/README.md) folder.

## Flashing the firmware

Connect your board via the USB Type-C port to a computer. It is recommended to use the Web Installer but building from source is also easy

### Web Installer

Go to [the Web Installer site](https://ltm.hekinav.dev/web_installer/) and follow the instructions on the site.

### Building from source

Use the guide in the [firmware repo](https://github.com/hekinav/ltm-firmware/tree/main/firmware#compiling-the-code-yourself)

