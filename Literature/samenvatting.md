# General overview of the project
## Goal of the project
design a RDC layer for contiki-ng, implement and test several RDC protocols 
using LoRa 2.4 GHz

## Questions
- USB to USB 
- masterthesis request form title

## TODO right now
- run through transmit and receive process and make a chart of it
- go through every file and explain everything in comments
 

## TODO bigger goals
- analyse driver for sx1280 transceiver
- implement RDC layer in contiki-ng
- compare RDC protocols: ContikiMAC, X-MAC, LPP, others
- Add driver to the contiki file structure

## improving the protocol
- test contikiMAC with different timings
- Test CAD with different amount of symbols
- Automatically change SF, BW based on data to send
- Timings will depend on the SF and BW selected
- A phase lock system can be added, as well as a "listen before talk" feature.

## Ideas
- create new pcb shield => required changes:
  - swap pins specified in report


# Literature
## LoRa
- 2.4 GHz
- LoRaWAN uses a Aloha like MAC protocol, 1 hop star topology, stay awake until ACK
- 1% duty cycle limit
- sub noise transmission
- LoRaWAN is bad for scalability, coverage, data rate, gateway outage => multihop
- uses orthogonal SF instead of orthogonal spreading codes
- SF between 7-12: higher SF more range, lower data rate, longer air time
- 2.4 GHz has no duty cycle restriction => more collisions => need good RDC
- 2.4 GHz has B increased to a max of 1600 kHz
- CAD: Clear Activity Detection. Can detect LoRa signals below the noise level
- USED SETTINGS: SF 7, BW 403 kHz, Preamble length 8, CR 4/5, CRC true

## Hardware
- zolertia firefly boards
  - used pinout: SCK B2, MISO B3, MOSI B1, CS A3, RESET A2, DIO1 A5, BUSY A4
- hardware shield with sx1280 LoRa transceiver

## Implementation of driver in contiki-ng
- place src folder inside arch/dev/radio/sx1280
- place examples in examples/sx1280
- changes in contiki files:
  - tsch timing needs an #ifndef somewhere
  - convert tsch timing to 32 bit instead of 16
  - Still need to figure out the rest of the things to do to get it working **correctly**



# Report
## Concepts to talk about
- LoRa
- IoT

## My work
- Integrate driver into contiki-ng
- change file structure to contain #region
- netstack.h: add configuration for RDC layer

testing ****
test
