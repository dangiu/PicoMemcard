# PicoMemcard
PicoMemcard allows you to build your own supercharged PSX Memory Card that can be connected to your computer via USB in order to transfer saves directly to/from your PSX. You can use it to repurpose broken/counterfeit Memory Cards creating a better one using only a Raspberry Pi Pico.

## Features
* Able to faithfully simulate PSX Memory Card
* USB connection to import/export saves
* Allows to copy saves to/from any other memory card (using original PSX file manager)
* Allows to play burned CDs (thanks to [FreePSXBoot])
* Cheaper than an original memory card

## Bill of materials
* **Raspberry Pi Pico** (around $5)
* One of:
    * Custom [PicoMemcard PCB](#picomemcard-pcb)
    * Broken/Counterfeit/Original PSX Memory Card (counterfeit ones can be found on AliExpress for around $2-3)
    * PSX/PS2 Controller Cable
    * Nothing, if you are a mad man and feel like soldering cables directly to your PSX (would not recommend).

Basically anything that will allow you to interface with the memory card slot pins will do. If you have a broken contoller you can cut off the cable and use that since controllers and memory cards share the same bus. Of course, plugging your memory card into the controller slot will prevent you from using 2 controllers at the same time.

In total building a PicoMemcard wil cost you less than buying a used original Memory Card!

## Video
[![PicoMemcard](https://img.youtube.com/vi/Sie0kzmnJJw/0.jpg)](https://www.youtube.com/watch?v=Sie0kzmnJJw)

## PicoMemcard PCB
<img src="./docs/PCBs.jpg" alt="Custom PCBs" width="800">

This is the custom PCB designed and manufactured specifically for this application. It makes it much easier to build PicoMemcard since you don't need to cut up another memory card and all the soldering pads are easily accessible. Still, you will need a soldering iron to assemble the device.

The Raspberry Pi Pico must sit flush on top of the PCB, so use the soldering pads instead of the through-holes (see right side of the picture). Personally, I used a couple of instant-glue drops to fix it in place and then soldered the different pads.

I've designed the PCB with a bit of future-proofeness in mind. Although not yet implemented, it supports the installation of an SD SPI expansion board to allow the Pico to read/write data to a microSD card and store multiple memory card images (see [Future Development](#future-development) section).

Since I've already received so many requests and I'm starting to lose track of them, I created a Google Form where you can [request one]. It also allows me to understand how many PCBs I should manufacture. If you do request one, make sure to specify a way for me to contact you back in order to get shipping information and stuff.

Before filling the form, keep in mind that the cheapest way to build PicoMemcard is using a counterfeit memory card from Aliexpress. The custom PCB makes the building process much easier and gets you a more professional-looking product, but the features you get are the same.

<img src="./docs/PCB-Demo.jpg" alt="Custom PCB in use" width="800">

In the picture above you can see the PCB plugged into a PSOne. I'm already working on a 3d-printed enclosure instead of using paper sheets as a spacer.

## Wiring
The wiring diagram below shows how to wire the Pico and a counterfeit memory card. For the other cases (wiring directly to the PSX or using a controller cable) the pins on the Pico are the same, the pinout of the PSX/controller can be found on [psx-spx]. The image shows the bottom side of the memory card with the cover removed.

<img src="./docs/wiring_bg.svg" alt="Wiring Diagram" width="800">

The dashed line on the PCB of the memory card is where you should cut a groove deep enough to disconnect the original circuitry from the traces. The yellow squares above the line indicate where you should scrape away the protective film in order to expose the copper traces and solder the wires onto them.

Finally the area at the bottom of the memory card is where you can cut a hole to feed the wires through connecting them to the Pico.

## Installation
1. Download the latest [release].
2. While pressing the 'BOOTSEL' button on your Raspberry Pi Pico, plug it into your computer. 
3. Drag and drop the PicoMemcard release onto your Raspberry Pi Pico.
4. PicoMemcard should appear on your PC as a USB drive.
5. Upload a memory card image to your PicoMemcard.

## Transfering Data
As of the current release, uploading data to PicoMemcard requires some precise steps:
* The image of the memory card to upload must be called exactly `MEMCARD.MCR`. Uploading anything else will not result in any errors but PicoMemcard will not save the uploaded data to flash. After rebooting the device the old data will still be present.
* The image must be 128KB (131072 bytes) in size which is precisely the size of an original Memory Card.
* After the image has been uploaded, the device **must be safely ejected** in order for the data to be actually imported correctly. This is a limitation of the current implementation (see [Design](#design) section).

Inside `docs/images` you can find two memory card images. One has a couple of saves on it so you can test if everything works correctly, the other is completely empty.

**ATTENTION:**
I would recommend to never plug PicoMemcard both into the PC (via USB) and the PSX at the same time! Otherwise the 5V provided by USB would end up on the 3.3V rail of the PSX. I'm not really sure if this could cause actual damage but I would avoid risking it.

If you really need to have the Pico plugged into both the USB and PSX (e.g. for debugging purposes), disconnect the 3.3V line from the VBUS pin. In this way you can power on the Pico using a simple USB phone charger or by plugging it into your PC.

## Future Development
As of now PicoMemcard is still in very early development stages and only tested on a PSOne Pal model (SCPH-102, bios version 4.4). It **should** work on any PSX model. If you want to try it on your PSX any feedback would be much appreciated.

I've tried to make the project as accessible as possible by using the least amount of hardware but this comes with a few limitations, in particular regarding the flash storage of the Pico. In the future I want to add the possibility to use a microSD card by adding a microSD SPI module, this would improve on the following:
* The very limited storage preventing from having multiple memory card images at the same time and being able to switch them using a button on the Pico.
* The complex codebase using multiple filesystems (see [Design](#design) section).
* The brief downtime periods PicoMemcard may have while writing new data to flash memory, appearing as if the Memory Card was briefly disconnected.
* The need to perform safe ejection as a way to signal the Pico to transfer the data from the virtual FAT disk to the flash filesystem (see [Design](#design) section).

## Design
For people interested in understanding how PicoMemcard works I provide a more extensive explanation in [this post].

## Thanks To
* [psx-spx] and Martin "NO$PSX" Korth - PlayStation Specifications and documented Memory Card protocol and filesystem.
* [Andrew J. McCubbin] - Additional information about Memory Card and Controller communication with PSX.
* [littlefs] - Filesystem designed to work on NOR flash used by many microcontrollers, including the Raspberry Pi Pico.
* [ChaN FatFS] - FAT filesystem implementation for embedded devices.
* [Scoppy] - Use your Raspberry Pi Pico as an oscilloscope, very cheap and accurate. Without this I would have not been able to debug many issues.
* [PulseView] - Used to import, visualize and manipulate the data from Scoppy on a PC.

[FreePSXBoot]: https://github.com/brad-lin/FreePSXBoot
[psx-spx]: https://psx-spx.consoledev.net/pinouts/#controller-ports-and-memory-card-ports
[Andrew J. McCubbin]: http://www.emudocs.org/PlayStation/psxcont/
[littlefs]: https://github.com/littlefs-project/littlefs
[ChaN FatFS]: http://elm-chan.org/fsw/ff/00index_e.html
[Scoppy]: https://github.com/fhdm-dev/scoppy
[PulseView]: https://sigrok.org/wiki/PulseView
[release]: https://github.com/dangiu/PicoMemcard/releases/latest
[this post]: https://dangiu.github.io/2022/05/13/picomemcard.html
[request one]: https://forms.gle/f6XHtz6W5fn5qDZV7


<!-- 
TODO
Add FAQ? Is it necessary?
Add Changelog
-->
