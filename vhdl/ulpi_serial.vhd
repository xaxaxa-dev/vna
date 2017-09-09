library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;
use work.ulpi_port;
use work.usb_serial;

-- simple usb-serial device
entity ulpi_serial is
	Port (
		ulpi_data  : inout  std_logic_vector(7 downto 0);
		ulpi_dir      : in  std_logic;
		ulpi_nxt      : in  std_logic;
		ulpi_stp      : out std_logic := '1';
		ulpi_reset    : out std_logic := '1';
		ulpi_clk60    : in  std_logic;
		
		rxval: out std_logic;
		rxrdy: in std_logic;
		txval: in std_logic;
		txrdy: out std_logic;
		rxdat: out std_logic_vector(7 downto 0);
		txdat: in std_logic_vector(7 downto 0);
		
		--optional signals
		txcork: in std_logic := '0';
		reset: in std_logic := '0';
		highspeed,suspend,online: out std_logic := 'X';
		LED: out std_logic := 'X';
		txroom: out unsigned(13 downto 0)
	);
end ulpi_serial;

architecture a of ulpi_serial is
	constant RXBUFSIZE_BITS : integer := 12;
	constant TXBUFSIZE_BITS : integer := 12;
	signal ulpi_data_in, ulpi_data_out: std_logic_vector(7 downto 0);
	signal PHY_DATABUS16_8 : std_logic;
	signal PHY_RESET :       std_logic;
	signal PHY_XCVRSELECT :  std_logic;
	signal PHY_TERMSELECT :  std_logic;
	signal PHY_OPMODE :      std_logic_vector(1 downto 0);
	signal PHY_LINESTATE :    std_logic_vector(1 downto 0);
	signal PHY_CLKOUT :       std_logic;
	signal PHY_TXVALID :     std_logic;
	signal PHY_TXREADY : std_logic;
	signal PHY_RXVALID :       std_logic;
	signal PHY_RXACTIVE :      std_logic;
	signal PHY_RXERROR :     std_logic;
	signal PHY_DATAIN :        std_logic_vector(7 downto 0);
	signal PHY_DATAOUT :     std_logic_vector(7 downto 0);
	
	signal usb_suspend,usb_online,usb_highspeed: std_logic;
	signal clkcnt : unsigned(24 downto 0);
	signal usb_txroom: std_logic_vector(TXBUFSIZE_BITS-1 downto 0);
begin
	adaptor: entity ulpi_port port map(ulpi_data_in,ulpi_data_out,ulpi_dir,
		ulpi_nxt,ulpi_stp,ulpi_reset,ulpi_clk60,
		
		PHY_RESET,PHY_XCVRSELECT,PHY_TERMSELECT,PHY_OPMODE,PHY_LINESTATE,PHY_CLKOUT,
		PHY_TXVALID, PHY_TXREADY,PHY_RXVALID, PHY_RXACTIVE, PHY_RXERROR,'0','0',
		PHY_DATAIN,PHY_DATAOUT);
	ulpi_data <= ulpi_data_out when ulpi_dir='0' else "ZZZZZZZZ";
	ulpi_data_in <= ulpi_data;
	
	usb_serial_inst : entity usb_serial
	generic map (
		VENDORID        => X"fb9a",
		PRODUCTID       => X"fb9a",
		VERSIONBCD      => X"0031",
		HSSUPPORT       => true,
		SELFPOWERED     => false,
		RXBUFSIZE_BITS  => RXBUFSIZE_BITS,
		TXBUFSIZE_BITS  => TXBUFSIZE_BITS )
	port map (
		CLK             => ulpi_clk60,
		RESET           => reset,
		USBRST          => open,
		HIGHSPEED       => usb_highspeed,
		SUSPEND         => usb_suspend,
		ONLINE          => usb_online,
		RXVAL           => rxval,
		RXDAT           => rxdat,
		RXRDY           => rxrdy,
		RXLEN           => open,
		TXVAL           => txval,
		TXDAT           => txdat,
		TXRDY           => txrdy,
		TXROOM          => usb_txroom,
		TXCORK          => txcork,
		PHY_DATAIN      => PHY_DATAIN,
		PHY_DATAOUT     => PHY_DATAOUT,
		PHY_TXVALID     => PHY_TXVALID,
		PHY_TXREADY     => PHY_TXREADY,
		PHY_RXACTIVE    => PHY_RXACTIVE,
		PHY_RXVALID     => PHY_RXVALID,
		PHY_RXERROR     => PHY_RXERROR,
		PHY_LINESTATE   => PHY_LINESTATE,
		PHY_OPMODE      => PHY_OPMODE,
		PHY_XCVRSELECT  => PHY_XCVRSELECT,
		PHY_TERMSELECT  => PHY_TERMSELECT,
		PHY_RESET       => PHY_RESET );
	
	highspeed <= usb_highspeed;
	suspend <= usb_suspend;
	online <= usb_online;
	txroom <= "00"&unsigned(usb_txroom);
	
	clkcnt <= clkcnt + 1 when rising_edge(ulpi_clk60);
	-- Show USB status on LED
	LED <= '0' when (
	  -- suspended -> on
	  (usb_suspend = '1') or
	  -- offline -> blink 8%
	  (usb_online = '0' and clkcnt(24 downto 22) = "000") or
	  -- highspeed -> fast blink 50%
	  (usb_online = '1' and usb_highspeed = '1' and clkcnt(22) = '0') or
	  -- fullspeed -> slow blink 50%
	  (usb_online = '1' and usb_highspeed = '0' and clkcnt(24) = '0') ) else '1';

end a;

