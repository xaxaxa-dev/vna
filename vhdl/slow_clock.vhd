library ieee;
use ieee.numeric_std.all;
use ieee.std_logic_1164.all;
USE ieee.math_real.log2;
USE ieee.math_real.ceil;

-- output will start from 1 2 clock cycles after rst is pulsed
entity slow_clock is
	generic (divide: integer := 50000000;
				dutycycle: integer := 20000000);
	port (clk: in std_logic;
			o: out std_logic;
			rst: in std_logic := '0';
			phase: out unsigned(integer(ceil(log2(real(divide))))-1 downto 0)
					:= (others=>'X');
			skip: in std_logic := '0');
end;

architecture a of slow_clock is
	constant b: integer := integer(ceil(log2(real(divide))));
	signal cs,ns: unsigned(b-1 downto 0);
	signal next_out: std_logic;
begin
	cs <= ns when rising_edge(clk);
	ns <= to_unsigned(0,b) when rst='1' else
		cs+2 when cs<(divide-2) and skip='1' else
		to_unsigned(1,b) when skip='1' else
		cs+1 when cs<(divide-1) else
		to_unsigned(0,b);
	next_out <= '1' when cs<dutycycle else '0';
	o <= next_out when rising_edge(clk);
	phase <= cs when rising_edge(clk);
end architecture;

library ieee;
use ieee.numeric_std.all;
use ieee.std_logic_1164.all;
USE ieee.math_real.log2;
USE ieee.math_real.ceil;
entity slow_clock_odd is
	generic (divide: integer := 50000000;
				dutycycle: integer := 20000000);
	port (clk: in std_logic;
			o: out std_logic);
end;

architecture a of slow_clock_odd is
	constant b: integer := integer(ceil(log2(real(divide))));
	signal cs,ns: unsigned(b-1 downto 0);
	signal next_out,o1,o2: std_logic;
begin
	cs <= ns when rising_edge(clk);
	ns <= cs+1 when cs<(divide-1) else to_unsigned(0,b);
	next_out <= '1' when cs<dutycycle else '0';
	o1 <= next_out when rising_edge(clk);
	o2 <= o1 when falling_edge(clk);
	o <= o1 or o2;
end architecture;
