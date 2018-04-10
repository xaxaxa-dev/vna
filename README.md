
Table of Contents
=================
   * [vna](#vna)
   * [Building the software](#building-the-software)
   * [Block diagram](#block-diagram)
   * [Screenshots](#screenshots)
   * [Parts selection](#parts-selection)
   * [Pictures](#pictures)


# vna
A simple and cheap vector network analyzer, including support software.

As seen on kickstarter:
https://www.kickstarter.com/projects/1759352588/xavna-a-full-featured-low-cost-two-port-vna

__Directory layout__

* Root directory: software; compile and run on any Linux based system
* pcb: schematics, pcb layouts, and simulation files
* vhdl: circuitry implemented on the fpga

__Specifications__

* Frequency range: guaranteed **137MHz - 2500MHz**, typical **135MHz - 3500MHz**
* Measurement signal level (controlled using spi interface): **-20dBm to 10dBm, with 1dB increments**

__Interfacing__

The main board connects to a PC through usb and communicates via a virtual serial port device; the PC software sets the frequency and other parameters by sending two-byte register write commands, and the device sends averaged vector values representing magnitude and phase of measured wave.

# Building the software

__Building on linux__

Build libxavna (required for QT GUI):
```
sudo apt-get install automake libtool make g++ libeigen3-dev libfftw3-dev
cd /PATH/TO/vna
autoreconf --install
./configure
make
cd libxavna/xavna_mock_ui/
/PATH/TO/qmake
make
```

Build & run QT GUI:
```
cd /PATH/TO/vna
cd vna_qt
/PATH/TO/qmake
make
../run
```

__Building on mac os__
```
brew install automake libtool make eigen fftw
cd /PATH/TO/vna
./deploy_macos.sh
# result is in ./vna_qt/vna_qt.app
```

__Cross-compile for windows (from linux)__

Download and build MXE:
```
cd ~/
git clone https://github.com/mxe/mxe.git
cd mxe
export QT_MXE_ARCH=386
make qt5 qtcharts cc eigen fftw pthreads
```
Edit mxe/settings.mk and add i686-w64-mingw32.shared to MXE_TARGETS.

Build
```
cd /PATH/TO/vna
export PATH="/PATH/TO/MXE/usr/bin:$PATH"
./deploy_windows.sh
```

# Block diagram

##### Overall architecture
![block diagram](pictures/overall_diagram.png)

##### Receivers & interfacing
![block diagram](pictures/vna_main.png)

##### Signal generator
![block diagram](pictures/vna_tx.png)

##### FPGA logic
![block diagram](pictures/fpga_logic.png)





# Screenshots

##### Open circuited coax stub

![screenshot](pictures/screenshot_new_coax.png)

##### Antenna

![screenshot](pictures/screenshot_new_antenna.png)

##### Time to fault (measuring coax cable)

![screenshot](pictures/screenshot_new_ttf.png)


# Parts selection
* fpga: XC6SLX9-2TQG144C 
* adc: AD9200
* mixer: AD8342
* synthesizers: ADF4350
* rf switches: BGS14GA14
* programmable attenuator: PE4312

# Pictures
##### Main board (release 2)
![vna board 2](pictures/main2_top.jpg)

##### Calibration standards (Short, Open, Load)
![calibration standards](pictures/calibration_standards.jpg)


