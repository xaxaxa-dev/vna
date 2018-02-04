# PCB design files

##### Release 2 design files
* main2_2.sch/main2_2.pcb - VNA board, 2nd revision - switched to AD8342 mixer
* main2.sch/main2.pcb - VNA board, initial revision

##### Release 1 design files
* main.sch/main.pcb - main board
* tx.sch/tx.pcb - signal generator
* coupler.pcb - directional coupler

##### Usage
To open the schematic files, first add the symbol library by editing ~/.gEDA/gafrc and adding the following line:
```
(component-library "/path/to/vna/pcb/sym")
```

##### Notes (iteration 2)
* For all ferrite beads (value="fb") use FBMH1608HM601-T or any ferrite bead with high impedance (>100ohm) across the entire range of frequencies covered by the VNA

##### Notes (iteration 1)
* Inductors with value="fb" are power supply filtering ferrite beads; the HZ0603B112R-10 (0603) and MI0805J102R-10 (0805) were used in the prototype, but any ferrite bead with sufficient current handling will work.
* Inductors with value="130n" are all dc blocking inductors in the signal path or LO path; an inductor with sufficiently high |Z| (>= 100ohms) in the entire frequency range of operation should be used; Typical inductors with SRF high enough and also high enough inductance to cover the lower bands are difficult to find, but many cheap ferrite beads meet the requirement so can be used instead.


