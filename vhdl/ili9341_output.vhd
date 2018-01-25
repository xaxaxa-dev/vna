library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;
use work.graphics_types.all;
entity ili9341Out is
	generic(pixeldelay: integer := 3);
    port(clk: in std_logic;
		p: out position;	--registered
		pixel: in color;	--unregistered
		
		--device signals
		lcd_scl,lcd_sdi,lcd_cs,lcd_dc,lcd_rst: out std_logic);
end entity;
architecture a of ili9341Out is
	constant lcdCmdCount: integer := 8;
	constant lcdAddrMax: integer := lcdCmdCount+240*320-1;
	
	--the lcdAddr and lcdPhase at which the x and y counter will be reset to 0
	-- (at the next clock cycle)
	constant startAddr: integer := lcdCmdCount-1;
	constant startPhase: integer := 7-pixeldelay;
	
	--control signals
	signal lcdclk,lcdIsReplayingRom: std_logic;
	signal lcdAddr,lcdAddrNext,lcdDelayedAddr: unsigned(17 downto 0);
	signal lcdPhase,lcdPhaseNext,lcdDelayedPhase,lcdMaxPhase: unsigned(3 downto 0);
	
	signal curX,curY,nextX,nextY: unsigned(8 downto 0);
	
	--roms
	constant lcdRomsAddrLen: integer := 3;
	signal lcdRomsAddr: unsigned(lcdRomsAddrLen-1 downto 0);
	signal lcdRomSDIAddr: unsigned(lcdRomsAddrLen+3-1 downto 0);
	signal lcdRomSDI: std_logic_vector(63 downto 0);
	signal lcdRomDC: std_logic_vector(7 downto 0);
	signal lcdRomSDIData,lcdRomDCData,lcdData,lcdUseRom: std_logic;
	
	signal color,colorNext: unsigned(15 downto 0);
begin
	lcdclk <= clk;
	lcdPhaseNext <= to_unsigned(0,4) when lcdPhase=lcdMaxPhase
		else lcdPhase+1;
	lcdPhase <= lcdPhaseNext when rising_edge(lcdclk);
	lcdAddrNext <= to_unsigned(0,18) when lcdAddr=lcdAddrMax
		else lcdAddr+1;
	lcdAddr <= lcdAddrNext when lcdPhase=lcdMaxPhase and rising_edge(lcdclk);
	lcdMaxPhase <= to_unsigned(9,4) when lcdAddr<lcdCmdCount
		else to_unsigned(15,4);
	
	-- command roms
	lcdRomsAddr <= lcdAddr(lcdRomsAddrLen-1 downto 0);
	lcdRomSDIAddr <= not (lcdRomsAddr&lcdPhase(2 downto 0));
	lcdRomSDIData <= lcdRomSDI(to_integer(lcdRomSDIAddr)) when rising_edge(lcdclk);
	lcdRomDCData <= lcdRomDC(to_integer(not lcdRomsAddr)) when rising_edge(lcdclk);
	
	-- synchronized to lcdRom*Data
	lcdDelayedAddr <= lcdAddr when rising_edge(lcdclk);
	lcdDelayedPhase <= lcdPhase when rising_edge(lcdclk);
	lcdIsReplayingRom <= '1' when lcdDelayedAddr<lcdCmdCount else '0';
	
	--color <= "0000011111111111";
	--colorbit <= color(to_integer(not lcdPhase)) when rising_edge(lcdclk);
	
	nextX <= to_unsigned(0,9) when lcdAddr=startAddr
		else to_unsigned(0,9) when curX=239 else curX+1;
	nextY <= to_unsigned(0,9) when lcdAddr=startAddr
		else curY+1 when curX=239 else curY;
	curX <= nextX when lcdPhase=0 and rising_edge(lcdclk);
	curY <= nextY when lcdPhase=0 and rising_edge(lcdclk);
	p <= ("000"&curX, "000"&curY);
	
	colorNext <= pixel(2)(4 downto 0) & pixel(1)(5 downto 0)
		& pixel(0)(4 downto 0) when lcdPhase=0 else color(14 downto 0)&"0";
	color <= colorNext when rising_edge(lcdclk);
	
	lcd_scl <= lcdclk;
	lcd_dc <= lcdRomDCData when lcdIsReplayingRom='1' else '1';
	lcdData <= lcdRomSDIData when rising_edge(lcdclk);
	lcd_sdi <= lcdData when lcdIsReplayingRom='1' else color(15);
	lcd_cs <= '0' when (lcdDelayedPhase<=8 and lcdDelayedPhase/=0)
		else '0' when lcdDelayedAddr>=lcdCmdCount
		else '1';
	lcd_rst <= '1';
	
	--rom contents
	lcdRomSDI <= 
		X"11" &
		X"29" &
		X"3A" &
		"01010101" &
		X"13" &
		X"36" &
		"00000000" &
		X"2C";
	lcdRomDC <= "00010010";
end a;
