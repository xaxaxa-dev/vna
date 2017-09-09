library ieee;
library work;
use ieee.numeric_std.all;
use ieee.std_logic_1164.all;
-- AXI compliant register stages
entity axiDelay is
	generic(width: integer := 8;
				validDelay: integer := 1;	-- how much to delay "valid" line
				dataDelay: integer := 1);	-- how much to delay "data" line
	port(	clk: in std_logic;
			--input side
			inReady: out std_logic;
			inValid: in std_logic;
			inData: in std_logic_vector(width-1 downto 0);
			--output side
			outReady: in std_logic;
			outValid: out std_logic;
			outData: out std_logic_vector(width-1 downto 0));
end entity;
architecture a of axiDelay is
	type srValid_t is array(validDelay downto 0) of std_logic;
	type srData_t is array(dataDelay downto 0) of std_logic_vector(width-1 downto 0);
	signal srValid: srValid_t;
	signal srData: srData_t;
	
	signal srEnable: std_logic;
begin
	-- shift registers; LSB in, MSB out
g1:for I in 0 to validDelay-1 generate
		srValid(I+1) <= srValid(I) when srEnable='1' and rising_edge(clk);
	end generate;
	srValid(0) <= inValid;
g2:for I in 0 to dataDelay-1 generate
		srData(I+1) <= srData(I) when srEnable='1' and rising_edge(clk);
	end generate;
	srData(0) <= inData;
	
	outValid <= srValid(validDelay);
	outData <= srData(dataDelay);
	srEnable <= outReady or not srValid(validDelay);
	
	inReady <= srEnable;
end a;
