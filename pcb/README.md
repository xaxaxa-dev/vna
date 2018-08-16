# PCB design files

##### Full two port version design files
* main3_2.sch, main3_2.pcb - VNA board, 2nd revision __\[current production version\]__

##### T/R version 2 design files
* main2_2.sch, main2_2.pcb - VNA board, 2nd revision - switched to AD8342 mixer __\[current production version\]__
* main2.sch, main2.pcb - VNA board, initial revision

##### T/R version 1 design files
* main.sch, main.pcb - main board
* tx.sch, tx.pcb - signal generator
* coupler.pcb - directional coupler

##### Usage
PCB & schematic files can be opened with gEDA (pcb & gschem).

To open the schematic files, first add the symbol library by editing ~/.gEDA/gafrc and adding the following line:
```
(component-library "/path/to/vna/pcb/sym")
```
(Replace /path/to/vna with the location of the vna repository)

##### Notes (release 2)
* PCB stackup (4 layers; total thickness 1.5mm):
  * Top copper
  * __0.2mm FR4__
  * Inner copper 1
  * __1.0mm FR4__
  * Inner copper 2
  * __0.2mm FR4__
  * Bottom copper
* For all ferrite beads (value="fb") use FBMH1608HM601-T or any ferrite bead with high impedance (>100ohm) across the entire range of frequencies covered by the VNA
* The tcxo (footprint="tcxo3225") is ASVTX-11-121-19.200MHZ-T or any compatible 3.3V 19.2MHz TCXO with the same package and pinout
* The micro usb connector (footprint="custom_microusb1") used is 10118193-0001LF
* The SMA connectors used are CON-SMA-EDGE-S, but any edge mount SMA connector will work


##### Notes (release 1)
* 2 layer PCB
* PCB thickness: 1.0mm
* Inductors with value="fb" are power supply filtering ferrite beads; the HZ0603B112R-10 (0603) and MI0805J102R-10 (0805) were used in the prototype, but any ferrite bead with sufficient current handling will work.
* Inductors with value="130n" are all dc blocking inductors in the signal path or LO path; an inductor with sufficiently high |Z| (>= 100ohms) in the entire frequency range of operation should be used; Typical inductors with SRF high enough and also high enough inductance to cover the lower bands are difficult to find, but many cheap ferrite beads meet the requirement so can be used instead.


