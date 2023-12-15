# General overview of the project

## notes presentation
- chapter about CAD
- CAD vs CCA
- contikiMAC/RDC protocols for LoRa even possible?
- 
analyse timing requirements: time between 2 CCA may not be long enough to switch radios
especially since LoRa probably takes longer
=> research paper jaque

LoRa chips for all frequencies in 1 exist, check if seperate exists

CAD for LoRa 2.4 not found by student, read by student

## TODO 
- make sure the 2.4 GHz LoRa radio works with the netstack
- Literature study: related work chapter => search for papers over RDC for LoRa
  and include the previous work on this topic as a start for my work. Go over things like state of the art (new) advancements around LoRa 2.4 GHz, ...
- add an explanation of the current state of research into LoRa 2.4 GHz. no research into MAC protocols, only in limits in different scenarios. Research if there are papers over MAC protocols for sub GHz LoRa.

## LIST OF WORKING TESTS
- CSMA CC2538 -> CSMA CC2538
- CSMA CC2538 -> CSMA NULLRDC_NOFRAMER CC2538 
  

## LIST OF FAILED TESTS
- CSMA NULLRDC_NOFRAMER CC2538 -> CSMA NULLRDC_NOFRAMER CC2538 

## PROBLEMS
# 1 LoRa 2.4 GHz radio with udp root/sender nodes
- trying to build a RPL tree with SX128X driver doesn't work
- the exact same code using the internal radio driver does work
- new discovery: i got it working once but after adding a print statement
  the code stopped working again. After removing this print statement the code
  still didn't work. I also committed the code when it was working and after
  reverting to this commit the code still didn't work.

  - New method of testing: call every layer directly: netstack.radio already works,
  now move up to MAC, then network and so on
  OR maybe broken because the correct libraries are not included in the .c file

  - THE ISSUE IS MOST LIKELY THE DRIVER NOT HAVING A TRIGGER FOR WHEN A PACKET IS RECEIVED MEANING THE PACKETS IN ​THE BUFFER ARE NEVER READ UNLESS MANUALLY CALLED BY THE USER(LIKE IN THE DRIVER EXAMPLE)
  
# 2 RDC layer (nullrdc_noframer) with internal radio
- the firefly keeps on crashing and restarting after sending a packet
- the first packet being send is a DIS, test if it also happens for another packet
- code compiles and runs but no packets are being received, THEORY: there is a problem with the header and all our packets are being dropped before being read. The NULLRDC-NOFRAMER input function works because it can be called with normal CSMA, just not with RDC CSMA.


## workplan
- create nullRDC and use it to test if the RDC layer works
- create a simple rdc protocol like xmac
- create a more complicated rdc protocol like contikimac
- compare protocols performance
- compare final code against a clean version of contiki-ng to see what was 
actually changed



## improving the protocol
- test contikiMAC with different timings
- Test CAD with different amount of symbols
- Automatically change SF, BW based on data to send
- Timings will depend on the SF and BW selected
- A phase lock system can be added, as well as a "listen before talk" feature.


## INTERESTING SOURCES
https://lora.readthedocs.io/en/latest/
https://wireless-solutions.de/blog/2020/07/24/im282a-high-range-with-lora-on-worldwide-2-4-ghz-band/
https://www.semtech.com/products/wireless-rf/lora-connect
https://www.tme.eu/Document/1042f35a88b6ee421559d19923804032/SX128x.pdf
https://hal.science/hal-03868942/file/mobiquitous-22.pdf
https://jwcn-eurasipjournals.springeropen.com/articles/10.1186/s13638-019-1502-5#Sec3
https://www.ncbi.nlm.nih.gov/pmc/articles/PMC7472251/
https://blog.ttulka.com/lora-spreading-factor-explained/

