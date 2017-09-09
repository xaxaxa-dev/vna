----------------------------------------------------------------------------------
-- Company: 
-- Engineer: 
-- 
-- Create Date:    19:12:03 04/30/2016 
-- Design Name: 
-- Module Name:    sr - Behavioral 
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
use IEEE.NUMERIC_STD.ALL;
-- shift register of len stages
entity sr is
	generic(bits: integer := 8;
			len: integer := 8);
	Port (clk : in  STD_LOGIC;
			din : in  STD_LOGIC_VECTOR (bits-1 downto 0);
			dout : out  STD_LOGIC_VECTOR (bits-1 downto 0));
end sr;

architecture a of sr is
	type arr_t is array(len-1 downto 0) of std_logic_vector(bits-1 downto 0);
	signal arr: arr_t;
begin
g:	for I in 0 to len-2 generate
		arr(I) <= arr(I+1) when rising_edge(clk);
	end generate;
	arr(len-1) <= din when rising_edge(clk);
	dout <= arr(0);
end a;


library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;
-- shift register of len stages
entity sr_unsigned is
	generic(bits: integer := 8;
			len: integer := 8);
	Port (clk : in  STD_LOGIC;
			din : in  unsigned (bits-1 downto 0);
			dout : out  unsigned (bits-1 downto 0));
end;

architecture a of sr_unsigned is
	type arr_t is array(len downto 0) of unsigned(bits-1 downto 0);
	signal arr: arr_t;
begin
g:	for I in 0 to len-1 generate
		arr(I) <= arr(I+1) when rising_edge(clk);
	end generate;
	arr(len) <= din;
	dout <= arr(0);
end a;


library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;
-- shift register of len stages
entity sr_signed is
	generic(bits: integer := 8;
			len: integer := 8);
	Port (clk : in  STD_LOGIC;
			din : in  signed (bits-1 downto 0);
			dout : out  signed (bits-1 downto 0));
end;

architecture a of sr_signed is
	type arr_t is array(len-1 downto 0) of signed(bits-1 downto 0);
	signal arr: arr_t;
begin
g:	for I in 0 to len-2 generate
		arr(I) <= arr(I+1) when rising_edge(clk);
	end generate;
	arr(len-1) <= din when rising_edge(clk);
	dout <= arr(0);
end a;


library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
-- shift register of len stages
entity sr_bit is
	generic(len: integer := 8);
	Port (clk : in  STD_LOGIC;
			din : in  std_logic;
			dout : out std_logic);
end;

architecture a of sr_bit is
	signal arr: std_logic_vector(len-1 downto 0);
begin
g:	for I in 0 to len-2 generate
		arr(I) <= arr(I+1) when rising_edge(clk);
	end generate;
	arr(len-1) <= din when rising_edge(clk);
	dout <= arr(0);
end a;
