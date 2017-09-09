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

![main board](pictures/main_top.png)


##### Signal generator

![signal generator board](pictures/tx_top.png)


##### Directional coupler

![directional coupler board](pictures/coupler_top.png)


##### Complete assembly

![vna assembly](pictures/all.jpg)


# Interfacing
The main board connects to a PC through usb and communicates via a virtual serial port device; the PC software sets the frequency and other parameters by sending two-byte register write commands, and the device sends averaged vector values representing magnitude and phase of measured wave.

# Block diagram
##### Main board
![block diagram](pictures/vna_main.png)
##### Signal generator
![block diagram](pictures/vna_tx.png)

# Screenshots

##### Open circuited stub

![screenshot](pictures/screenshot_open_stub.png)


##### Short circuited stub

![screenshot](pictures/screenshot_shorted_stub.png)


##### Antenna

![screenshot](pictures/screenshot_antenna.png)


# Parts selection
* fpga: XC6SLX9-2TQG144C 
* adc: AD9200
* mixer: CMY210
* synthesizers: ADF4350
* rf switches: BGS14GA14
* programmable attenuator: PE4302
