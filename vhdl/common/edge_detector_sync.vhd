library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;
-- rising edge detector
entity edge_detector_sync is
	Port (clk, inp: in std_logic;
			outp: out std_logic);
end;
architecture a of edge_detector_sync is
	signal inp1, inp2, inp3, inp4: std_logic := '1';
	signal tmp: std_logic;
begin
	inp1 <= inp when rising_edge(clk);
	inp2 <= inp1 when rising_edge(clk);
	inp3 <= inp2 when rising_edge(clk);
	inp4 <= inp3 when rising_edge(clk);
	tmp <= '1' when inp4='0' and inp3='1' else '0';
	outp <= tmp when rising_edge(clk);
end a;

library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;
-- rising and falling edge detector
entity double_edge_detector_sync is
	Port (clk, inp: in std_logic;
			outp: out std_logic);
end;
architecture a of double_edge_detector_sync is
	signal inp1, inp2, inp3, inp4: std_logic := '0';
	signal tmp: std_logic;
begin
	inp1 <= inp when rising_edge(clk);
	inp2 <= inp1 when rising_edge(clk);
	inp3 <= inp2 when rising_edge(clk);
	inp4 <= inp3 when rising_edge(clk);
	tmp <= inp4 xor inp3;
	outp <= tmp when rising_edge(clk);
end a;
