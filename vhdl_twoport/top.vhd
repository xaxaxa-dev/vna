
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

library UNISIM;
use UNISIM.VComponents.all;


use work.clk_wiz_v3_6;
use work.clk_wiz_v3_6_2;
use work.ulpi_serial;
use work.dcfifo;
use work.spiDataTx;
use work.slow_clock;
use work.slow_clock_odd;
use work.resetGenerator;
use work.configRegisterTypes.all;
use work.serialConfigRegister;
use work.vnaTxNew2;
use work.ejxGenerator;
use work.adf4350_calc;
use work.pulseExtenderCounted;
use work.cic_lpf_2_nd;
use work.cic_lpf_2_d;
use work.ili9341Out;
use work.bounce_sprite;
use work.graphics_types.all;
use work.edge_detector_sync;
use work.double_edge_detector_sync;
use work.sr_bit;

-- register map:
-- 00: pll_frequency [23..16]
-- 01: pll_frequency [15..8]
-- 02: pll_frequency [7..0]
-- 03: update_trigger; write 1 to update all plls
-- 04: attenuation, in 0.5dB increments
-- 05: 
--   [1..0]: signal generator output power
--   [3..2]: LO output power
--   [7..4]: output data mode:
--				0: normal
--				1: adc0 data (filtered and decimated to 19.2MSPS)
--				2: adc1 data (filtered and decimated to 19.2MSPS)
--				3: adc0 data unfiltered (downsampled to 19.2MSPS)
--				4: adc1 data unfiltered (downsampled to 19.2MSPS)
-- 06:
--   [0]: enable pll partial update (only update counters)
--	 [1]: measurement direction; 0: outgoing; 1: incoming
--   [2]: output signal on which port; 0: port 1; 1: port 2
-- 07: bb_offset [8..0]
-- 08: bb_frequency [31..24]
-- 09: bb_frequency [23..16]
-- 0a: bb_frequency [15..8]
-- 0b: bb_frequency [7..0]
-- note: the output signal frequency is pll_frequency * 10kHz

entity top is
	port (
		CLOCK_19_2 : in  STD_LOGIC;
		LED : out  STD_LOGIC_VECTOR (1 downto 0);
		  
		ADC0_DATA: in unsigned(9 downto 0);
		ADC0_STBY: out std_logic;

		ADC1_DATA: in unsigned(9 downto 0);
		ADC1_STBY: out std_logic;

		HEADER: inout std_logic_vector(9 downto 1);

		LCD_SCL,LCD_SDI,LCD_CS,LCD_DC,LCD_RST: out std_logic;

		USB_DIR: in std_logic;
		USB_NXT: in std_logic;
		USB_DATA: inout std_logic_vector(7 downto 0);
		USB_RESET_B: out std_logic;
		USB_STP: out std_logic;
		USB_REFCLK: out std_logic
	);
end top;

architecture a of top is
	signal internalclk,CLOCK_19_2_b,CLOCK_240,CLOCK_120,CLOCK_60,CLOCK_19_2_i,CLOCK_19_2_i2,
		CLOCK_76_8,i2cclk,spiclk,adcclk,adcFiltClk: std_logic;
	signal pll1_locked,pll2_locked: std_logic;
	--reset
	signal reset: std_logic;
	signal clkgen_en: std_logic := '0';
	signal usbclk: std_logic;
	--signal reset: std_logic;
	
	
	--usb serial data
	signal rxval,rxrdy,realrxval,rxclk,txval,txrdy,txclk,txval1: std_logic;
	signal rxdat,txdat,txdat1: std_logic_vector(7 downto 0);
	signal usbtxval,usbtxrdy,usbrxval,usbrxrdy: std_logic;
	signal usbtxdat,usbrxdat: std_logic_vector(7 downto 0);
	signal txroom: unsigned(13 downto 0);
	signal tmp: unsigned(7 downto 0);
	signal led_usbserial: std_logic;
	signal usb_txcork: std_logic;
	
	signal vna_txdat: std_logic_vector(7 downto 0);
	signal vna_txval: std_logic;
	
	signal txdat_sel: unsigned(3 downto 0);
	
	signal fifo1empty,fifo1full: std_logic;
	
	-- adc
	signal adc0_data1, adc1_data1: unsigned(9 downto 0);
	signal data0, data1: signed(9 downto 0);
	signal filtered0, filtered1, filtered2: signed(20 downto 0);
	signal adc_overflows: std_logic_vector(1 downto 0);
	signal adc_overflow, adc_overflow_ignore: std_logic;
	signal pll_update1_adcclk, pll_update2_adcclk, pll_update3_adcclk, pll_do_update_adcclk: std_logic;
	
	-- sine generator
	signal sg_freq: unsigned(31 downto 0);
	signal sg_re,sg_im: signed(8 downto 0);
	signal sg_reset: std_logic;
	
	-- adf4350 config
	signal config_freq: unsigned(19 downto 0);				-- configured sgen frequency (10kHz increments)
	signal lo_freq, lo_offset: unsigned(19 downto 0);
	signal odiv: unsigned(5 downto 0);						-- rf divide factor
	
	signal pll1_R, pll2_R: std_logic_vector(9 downto 0);
	signal pll1_mod, pll2_mod: std_logic_vector(11 downto 0);
	signal pll1_N, pll2_N: std_logic_vector(15 downto 0);
	signal pll1_frac, pll2_frac: std_logic_vector(11 downto 0);
	signal pll1_O, pll2_O: std_logic_vector(2 downto 0);
	signal pll1_pwr, pll2_pwr: std_logic_vector(1 downto 0);
	--signal adf4350_clk,adf4350_le,adf4350_data: std_logic;
	signal pll_update_usbclk, pll_update1, pll_update2, pll_update3, pll_do_update, pll2_do_update: std_logic;
	signal pll_spi_partial_update: std_logic;
	signal pll_spi_startAddr: unsigned(2 downto 0);
	
	-- pe4312 config
	signal pe4312_attenuation: unsigned(5 downto 0);
	signal pe4312_do_update: std_logic;
	
	-- spi signals
	signal ADF4350_LO_CLK, ADF4350_LO_DATA, ADF4350_LO_LE,
           ADF4350_SG_CLK, ADF4350_SG_DATA, ADF4350_SG_LE,
           PE4312_CLK, PE4312_DATA, PE4312_LE: std_logic;
	
	-- header pins to slave card
	signal SPI_CLK, SPI_DATA: std_logic;
	signal RFSW_SG, RFSW_DIR: std_logic;
	
	-- config registers
	constant cfgCount: integer := 15;
	signal cfg: array8(0 to cfgCount-1);
	signal cfg_default: array8(0 to 254) :=
		(X"00", 	-- pll_frequency
		X"00",		-- pll_frequency
		std_logic_vector(to_unsigned(190, 8)),	-- pll_frequency
		X"00",		-- update trigger
		X"00",		-- attenuation
		"00001100", -- mode
		X"00", 		-- dir
		std_logic_vector(to_unsigned(70, 8)),	-- bb_offset
		X"09", X"55", X"55", X"50",				-- bb_frequency
		others=>X"00");
	signal cfg_wrIndicate: std_logic;
	signal cfg_wrAddr: unsigned(7 downto 0);
	-- how long to ignore adc overflow for after a pll reconfig; in adcclk cycles
	constant adc_ignore_overflow_duration: integer := 19200;
	
	-- ui
	signal use_vna_txdat: std_logic;
	signal led_adc_overflow: std_logic;
	
	-- lcd user side interface signals
	signal lcdClk: std_logic;
	signal lcd_scl1,lcd_sdi1,lcd_cs1,lcd_dc1: std_logic;
	signal curPos: position;
	signal pixel,pixelOut: color;
begin
	--############# CLOCKS ##############
	bufg1: IBUFG port map(I=>CLOCK_19_2,O=>CLOCK_19_2_b);
	pll: entity clk_wiz_v3_6 port map(
		CLK_IN1=>CLOCK_19_2_b,
		CLK_OUT1=>CLOCK_60,
		CLK_OUT2=>CLOCK_120,
		CLK_OUT3=>CLOCK_240,
		CLK_OUT4=>CLOCK_19_2_i,
		LOCKED=>pll1_locked);
	pll2: entity clk_wiz_v3_6_2 port map(
		CLK_IN1=>CLOCK_19_2_b,
		CLK_OUT1=>CLOCK_19_2_i2,
		CLK_OUT2=>CLOCK_76_8,
		LOCKED=>pll2_locked);
	
	INST_STARTUP: STARTUP_SPARTAN6
        port map(
         CFGCLK => open,
         CFGMCLK => internalclk,
         CLK => '0',
         EOS => open,
         GSR => '0',
         GTS => '0',
         KEYCLEARB => '0');
	usbclk <= CLOCK_60;
	adcclk <= CLOCK_76_8;
	adcFiltClk <= CLOCK_19_2_i2;
	-- 250kHz state machine clock => 62.5kHz i2c clock
	i2cc: entity slow_clock generic map(200,100) port map(internalclk,i2cclk);
	-- 2MHz spi clock
	spic: entity slow_clock generic map(26,13) port map(internalclk,spiclk);
	
	rg: entity resetGenerator generic map(25000) port map(spiclk,reset);
	
	
	--############# usb serial port device ##############
	usbdev: entity ulpi_serial generic map(minTxSize=>150) port map(USB_DATA, USB_DIR, USB_NXT,
		USB_STP, open, usbclk, usbrxval,usbrxrdy,usbtxval,usbtxrdy, usbrxdat,usbtxdat,
		LED=>led_usbserial, txroom=>txroom, txcork=>'0');
	USB_RESET_B <= '1';
	outbuf: ODDR2 generic map(DDR_ALIGNMENT=>"NONE",SRTYPE=>"SYNC")
		port map(C0=>usbclk, C1=>not usbclk,CE=>'1',D0=>'1',D1=>'0',Q=>USB_REFCLK);
	
	-- fifos
	fifo1: entity dcfifo generic map(8,13) port map(usbclk,txclk,
		usbtxval,usbtxrdy,usbtxdat,open,
		txval,txrdy,txdat,open);
	
	--txval <= '1';
	--txclk <= adcFiltClk;
	txdat1 <= std_logic_vector(filtered0(17 downto 10)) when txdat_sel=1 else
			std_logic_vector(filtered1(17 downto 10)) when txdat_sel=2 else
			std_logic_vector(data0(9 downto 2)) when txdat_sel=3 else
			std_logic_vector(data1(9 downto 2)) when txdat_sel=4 else
			vna_txdat;
	txval1 <= '1' when txdat_sel>0 and txdat_sel<5 else
			vna_txval;
	txdat <= txdat1 when rising_edge(txclk);
	txval <= txval1 when rising_edge(txclk);
	
	-- adcs
	adc0_data1 <= ADC0_DATA when rising_edge(adcclk);
	adc1_data1 <= ADC1_DATA when rising_edge(adcclk);
	data0 <= signed(adc0_data1+"1000000000") when rising_edge(adcclk);
	data1 <= signed(adc1_data1+"1000000000") when rising_edge(adcclk);
	
	adc_overflows(0) <= '1' when adc0_data1=(9 downto 0=>'1') or adc0_data1=0 else '0';
	adc_overflows(1) <= '1' when adc1_data1=(9 downto 0=>'1') or adc1_data1=0 else '0';
	adc_overflow <= adc_overflows(0) or adc_overflows(1) when rising_edge(adcclk);

	-- overflow detection
	pe2: entity pulseExtenderCounted generic map(192000) port map(adcclk, adc_overflow, led_adc_overflow);
	
	-- filters
	filt0: entity cic_lpf_2_d generic map(inbits=>10, outbits=>21, decimation=>4, stages=>3, bw_div=>3)
		port map(adcclk, adcFiltClk, data0, filtered0);
	filt1: entity cic_lpf_2_d generic map(inbits=>10, outbits=>21, decimation=>4, stages=>3, bw_div=>3)
		port map(adcclk, adcFiltClk, data1, filtered1);
	
	-- leds
	LED <= led_usbserial & led_adc_overflow;
	
	
	-- adf4350
	lo_freq <= config_freq+lo_offset;
	pll1_pwr <= cfg(5)(3 downto 2);
	pll2_pwr <= cfg(5)(1 downto 0);
	
	
	calc1: entity adf4350_calc generic map(1920, false) port map(usbclk, lo_freq, pll1_N, pll1_frac, pll1_O);
	calc2: entity adf4350_calc generic map(1920, false) port map(usbclk, config_freq, pll2_N, pll2_frac, pll2_O);
	
	pll1_R <= std_logic_vector(to_unsigned(1,10));
	pll1_mod <= std_logic_vector(to_unsigned(1920,12));
	--pll1_N <= std_logic_vector(to_unsigned(180, 16));
	--pll1_frac <= std_logic_vector(to_unsigned(0, 12));
	--pll1_O <= "011";
	--pll1_pwr <= "00";
	
	pll2_R <= pll1_R;
	pll2_mod <= pll1_mod;
	--pll2_N <= std_logic_vector(to_unsigned(180, 16));
	--pll2_frac <= std_logic_vector(to_unsigned(300, 12));
	--pll2_O <= pll1_O;
	--pll2_pwr <= "10";
	
	ed: entity double_edge_detector_sync port map(spiclk, pll_update_usbclk, pll_do_update);
	-- spi
	pll_spi_startAddr <= to_unsigned(0,3) when pll_spi_partial_update='0' else to_unsigned(4,3);
	spi1: entity spiDataTx generic map(words=>6,wordsize=>32) port map(
	--	 XXXXXXXXLLXXXXXXXXXXXXXXXXXXX101
		"00000000010000000000000000000101" &
	--	 XXXXXXXXF    OOO   BBBBBBBBVMAA    AA          R    OO          100
		"000000001"&pll1_O&"111111110001" & pll1_pwr & "1" & pll1_pwr & "100" &
		"00000000000000000" & std_logic_vector(to_unsigned(80,12)) & "011" &
		"01100100" & pll1_R & "01111101000010" &
		"00001000000000001" & pll1_mod & "001" &
		"0" & pll1_N & pll1_frac & "000",
		spiclk, pll_do_update, ADF4350_LO_CLK, ADF4350_LO_LE, ADF4350_LO_DATA, pll_spi_startAddr);
	
	spi2: entity spiDataTx generic map(words=>6,wordsize=>32) port map(
	--	 XXXXXXXXLLXXXXXXXXXXXXXXXXXXX101
		"00000000010000000000000000000101" &
	--	 XXXXXXXXF    OOO   BBBBBBBBVMAAAAR    OO          100
		"000000001"&pll2_O&"111111110000001" & pll2_pwr & "100" &
		"00000000000000000" & std_logic_vector(to_unsigned(80,12)) & "011" &
		"01100100" & pll2_R & "01111101000010" &
		"00001000000000001" & pll2_mod & "001" &
		"0" & pll2_N & pll2_frac & "000",
		spiclk, pll2_do_update, ADF4350_SG_CLK, ADF4350_SG_LE, ADF4350_SG_DATA, pll_spi_startAddr);
	
	spi3: entity spiDataTx generic map(words=>1,wordsize=>6) port map(
		std_logic_vector(pe4312_attenuation),
		spiclk, pe4312_do_update, PE4312_CLK, PE4312_LE, PE4312_DATA);
	
	sr1: entity sr_bit generic map(6*32*2) port map(spiclk, pll_do_update, pll2_do_update);
	sr2: entity sr_bit generic map(6*32*2) port map(spiclk, pll2_do_update, pe4312_do_update);
	
	SPI_CLK <= ADF4350_LO_CLK or ADF4350_SG_CLK or PE4312_CLK;
	SPI_DATA <= ADF4350_LO_DATA or ADF4350_SG_DATA or PE4312_DATA;
	
	-- slave card header
	HEADER(3) <= RFSW_DIR;
	HEADER(4) <= PE4312_LE;
	HEADER(5) <= RFSW_SG;
	HEADER(6) <= ADF4350_SG_LE;
	HEADER(7) <= ADF4350_LO_LE;
	HEADER(8) <= SPI_DATA;
	HEADER(9) <= SPI_CLK;
	
	
	-- config registers
	cfgInst: entity serialConfigRegister generic map(bytes=>cfgCount, defaultValue => cfg_default)
		port map(usbclk, usbrxdat, usbrxval, cfg, cfg_wrIndicate, cfg_wrAddr);
	usbrxrdy <= '1';
	config_freq <= unsigned(cfg(0)(3 downto 0)) & unsigned(cfg(1)) & unsigned(cfg(2));
	pll_update_usbclk <= not pll_update_usbclk when cfg_wrIndicate='1' and cfg_wrAddr=X"03" and rising_edge(usbclk);
	pe4312_attenuation <= unsigned(cfg(4)(5 downto 0));
	txdat_sel <= unsigned(cfg(5)(7 downto 4));
	pll_spi_partial_update <= cfg(6)(0) when rising_edge(spiclk);
	--RFSW_DIR <= not cfg(6)(1);
	RFSW_SG <= cfg(6)(2);
	sg_freq(31 downto 24) <= unsigned(cfg(8));
	sg_freq(23 downto 16) <= unsigned(cfg(9));
	sg_freq(15 downto 8) <= unsigned(cfg(10));
	sg_freq(7 downto 0) <= unsigned(cfg(11));
	lo_offset <= resize(unsigned(cfg(7)), 20);
	
	-- vna data
	sg_reset1: entity slow_clock generic map(192,1) port map(adcFiltClk,sg_reset);
	sg: entity ejxGenerator port map(adcFiltClk,sg_freq(31 downto 4),sg_re,sg_im,sg_reset);
	vnaTx1: entity vnaTxNew2 generic map(adcBits=>18) port map(adcFiltClk,
		filtered1(20 downto 3),filtered0(20 downto 3), sg_im,sg_re, RFSW_DIR, vna_txdat, vna_txval);
	txclk <= adcFiltClk;
	
	
	-- lcd controller
	lcdOut: entity ili9341Out generic map(2) port map(lcdClk,curPos,
		pixelOut, lcd_scl1,lcd_sdi1,lcd_cs1,lcd_dc1,open);
	
	lcdBuf: ODDR2 generic map(DDR_ALIGNMENT=>"NONE",SRTYPE=>"SYNC")
		port map(C0=>lcd_scl1, C1=>not lcd_scl1,CE=>'1',D0=>'1',D1=>'0',Q=>LCD_SCL);
	--LCD_SCL <= lcd_scl1;
	LCD_SDI <= lcd_sdi1 when falling_edge(lcdclk);
	LCD_CS <= lcd_cs1 when falling_edge(lcdclk);
	LCD_DC <= lcd_dc1 when falling_edge(lcdclk);
	pixelOut <= (X"ff",X"00",X"00");
	

	ADC0_STBY <= '0';
	ADC1_STBY <= '0';
	
	
end a;

