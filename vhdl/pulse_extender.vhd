----------------------------------------------------------------------------------
-- Company: 
-- Engineer: 
-- 
-- Create Date:    16:06:12 06/12/2016 
-- Design Name: 
-- Module Name:    pulse_extender - a 
-- Project Name: 
-- Target Devices: 
-- Tool versions: 
-- Description: 
--
-- Dependencies: 
--
-- Revision: 
-- Revision 0.01 - File Created
-- Additional Comments: 
--
----------------------------------------------------------------------------------
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
--use IEEE.NUMERIC_STD.ALL;
entity pulseExtender is
	generic(registered: boolean := true; --whether to register the output
				extend: integer := 1);
	Port (clk : in  STD_LOGIC;
			inp: in std_logic;
			outp: out std_logic);
end pulseExtender;

architecture a of pulseExtender is
	signal sr: std_logic_vector(extend downto 0);
	signal ors: std_logic_vector(extend downto 0);
begin
g:	for I in 1 to extend generate
		sr(I) <= sr(I-1) when rising_edge(clk);
		ors(I) <= ors(I-1) or sr(I);
	end generate;
	sr(0) <= inp;
	ors(0) <= inp;
g2:if registered generate
		outp <= ors(extend) when rising_edge(clk);
	end generate;
g3:if not registered generate
		outp <= ors(extend);
	end generate;
end a;

library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;
USE ieee.math_real.log2;
USE ieee.math_real.ceil;
entity pulseExtenderCounted is
	generic(extend: integer := 32);
	Port (clk : in  STD_LOGIC;
			inp: in std_logic;
			outp: out std_logic);
end pulseExtenderCounted;

architecture a of pulseExtenderCounted is
	constant bits: integer := integer(ceil(log2(real(extend+2))));
	signal state, stateNext: unsigned(bits-1 downto 0) := (others=>'0');
	signal outNext: std_logic;
begin
	stateNext <= to_unsigned(1, bits) when inp='1' else
				to_unsigned(0, bits) when state=0 else
				to_unsigned(0, bits) when state=extend else
				state+1;
	state <= stateNext when rising_edge(clk);
	outNext <= '0' when stateNext=0 else '1';
	outp <= outNext when rising_edge(clk);
end a;

