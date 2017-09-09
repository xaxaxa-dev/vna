# vna
a simple and cheap vector network analyzer, including support software
# Specifications
* Frequency range: guaranteed **137MHz - 2500MHz**, typical **135MHz - 3500MHz**
* Measurement signal level (controlled using on-board switches): **-5dBm to 10dBm, with 2dB increments**
* Measurement signal level (controlled using spi interface, not yet implemented): **-20dBm to 10dBm, with 1dB increments**
# Physical assembly
The vna is composed of 3 separate boards:
* main: contains fpga, usb interface, receivers, clock generator, LO synthesizer
* tx: signal generator; contains synthesizer, amplifier, filter bank, and programmable attenuator
* coupler: resistive directional coupler, ~15dB coupling
# Parts selection
* fpga: XC6SLX9-2TQG144C 
* adc: AD9200
* mixer: CMY210
* synthesizers: ADF4350
* rf switches: BGS14GA14
* programmable attenuator: PE4302
