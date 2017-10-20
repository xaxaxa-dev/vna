# PCB design files

##### Iteration 1 design files
* main.pcb - main board
* tx.pcb - signal generator
* coupler.pcb - directional coupler

##### Iteration 2 design files
* main.pcb - VNA board

##### Usage
To open the schematic files, first add the symbol library by editing ~/.gEDA/gafrc and adding the following line:
```
(component-library "/path/to/vna/pcb/sym")
```

##### Notes
* Inductors with value="fb" are power supply filtering ferrite beads; the HZ0603B112R-10 (0603) and MI0805J102R-10 (0805) were used in the prototype, but any ferrite bead with sufficient current handling will work.
* Inductors with value="130n" are all dc blocking inductors in the signal path or LO path; an inductor with sufficiently high |Z| (>= 100ohms) in the entire frequency range of operation should be used; Typical inductors with SRF high enough and also high enough inductance to cover the lower bands are difficult to find, but many cheap ferrite beads meet the requirement so can be used instead.
