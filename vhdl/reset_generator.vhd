library ieee;
library work;
use ieee.numeric_std.all;
use ieee.std_logic_1164.all;
entity resetGenerator is
	generic(cnt: integer := 100);
	port(clk: in std_logic;
		rst: out std_logic);
end entity;
architecture a of resetGenerator is
	signal counter,counterNext: unsigned(31 downto 0) := to_unsigned(0,32);
	signal tmp: std_logic;
begin
	counterNext <= to_unsigned(cnt,32) when counter=cnt else counter+1;
	counter <= counterNext when rising_edge(clk);
	tmp <= '0' when counter=cnt else '1';
	rst <= tmp when rising_edge(clk);
end architecture;
