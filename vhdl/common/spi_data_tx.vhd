library ieee;
library work;
use ieee.numeric_std.all;
use ieee.std_logic_1164.all;

entity spiDataTx is
	generic(words: integer;
			wordsize: integer);

	-- data is transmitted MSB first
	port(datarom: std_logic_vector(words*wordsize-1 downto 0);
		clk,doSend: in std_logic;
		scl,le,sdi: out std_logic);
end entity;

architecture a of spiDataTx is
	type rom_t is array(0 to words-1) of std_logic_vector(wordsize-1 downto 0);
	signal rom: rom_t;

	type states is (stop,load,send,done);
	signal state,stateNext: states;
	constant addrLen: integer := 4;
	signal addr,addrNext: unsigned(3 downto 0);
	signal cnt,cntNext: unsigned(7 downto 0);
	signal sr,srNext: unsigned(wordsize-1 downto 0);
	signal data: unsigned(wordsize-1 downto 0);
	signal clken,clkenNext: std_logic;
	
	signal outClk,outData,outLe,leNext: std_logic;
begin
	-- cast datarom to internal type
g:	for I in 0 to words-1 generate
		rom(I) <= datarom((I+1)*wordsize-1 downto I*wordsize);
	end generate;

	state <= stateNext when rising_edge(clk);
	addr <= addrNext when rising_edge(clk);
	cnt <= cntNext when rising_edge(clk);
	sr <= srNext when rising_edge(clk);
	stateNext <= load when state=stop and doSend='1' else
							send when state=load else
							done when state=send and cnt=wordsize-1 else
							stop when state=done and addr=words-1 else
							load when state=done else
							state;
	cntNext <= to_unsigned(0,8) when state=stop else
						cnt+1 when state=send else
						to_unsigned(0,8); --when state=done else
	addrNext <= addr+1 when state=done else
						to_unsigned(0,addrLen) when state=stop else
						addr;
	srNext <= sr(wordsize-2 downto 0)&"0" when state=send else
						data;

	data <= unsigned(rom(to_integer(unsigned(words-1+addr))));
	
	le <= leNext when rising_edge(clk);
	leNext <= '0' when state=send else
						'0' when state=done else
						'1';
	clken <= clkenNext when rising_edge(clk);
	clkenNext <= '1' when state=send else '0';
	scl <= clken and (not clk);
	sdi <= sr(wordsize-1) when rising_edge(clk);
end architecture;
