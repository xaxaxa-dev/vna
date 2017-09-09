# vna
a simple and cheap vector network analyzer, including support software

# Directory layout
* Root directory: software; compile and run on any Linux based system
* pcb: schematics, pcb layouts, and simulation files
* vhdl: circuitry implemented on the fpga

# Specifications
* Frequency range: guaranteed **137MHz - 2500MHz**, typical **135MHz - 3500MHz**
* Measurement signal level (controlled using on-board switches): **-5dBm to 10dBm, with 2dB increments**
* Measurement signal level (controlled using spi interface, not yet implemented): **-20dBm to 10dBm, with 1dB increments**
* 3 receivers: forward coupled, reverse coupled, through; can measure S11 and S21 of a two port device. To measure S22 and S12 the DUT needs to be manually reversed.
# Physical assembly
The vna is composed of 3 separate boards:
* main: contains fpga, usb interface, receivers, clock generator, LO synthesizer
* tx: signal generator; contains synthesizer, amplifier, filter bank, and programmable attenuator
* coupler: resistive directional coupler, ~15dB coupling

##### Main board

![main board](https://github.com/xaxaxa/vna/blob/master/pictures/main_top.png?raw=true)


##### Signal generator

![signal generator board](https://github.com/xaxaxa/vna/blob/master/pictures/tx_top.png?raw=true)


##### Directional coupler

![directional coupler board](https://github.com/xaxaxa/vna/blob/master/pictures/coupler_top.png?raw=true)


##### Complete assembly

![vna assembly](https://raw.githubusercontent.com/xaxaxa/vna/master/pictures/all.jpg)


# Interfacing
The main board connects to a PC through usb and communicates via a virtual serial port device; the PC software sets the frequency and other parameters by sending two-byte register write commands, and the device sends averaged vector values representing magnitude and phase of measured wave.

# Screenshots

##### Open circuited stub

![screenshot](https://github.com/xaxaxa/vna/blob/master/pictures/screenshot_open_stub.png?raw=true)


##### Short circuited stub

![screenshot](https://github.com/xaxaxa/vna/blob/master/pictures/screenshot_shorted_stub.png?raw=true)


##### Antenna

![screenshot](https://github.com/xaxaxa/vna/blob/master/pictures/screenshot_antenna.png?raw=true)


# Parts selection
* fpga: XC6SLX9-2TQG144C 
* adc: AD9200
* mixer: CMY210
* synthesizers: ADF4350
* rf switches: BGS14GA14
* programmable attenuator: PE4302
