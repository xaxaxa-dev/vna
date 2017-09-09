library ieee;
library work;
use ieee.numeric_std.all;
use ieee.std_logic_1164.all;
-- slow cross clock domain sync; really just a chain of flip-flops
entity cdcSync is
	generic(width: integer := 8;
				stages: integer := 3);
	port(dstclk: in std_logic;
			datain: in std_logic_vector(width-1 downto 0);
			dataout: out std_logic_vector(width-1 downto 0));
end entity;
architecture a of cdcSync is
	type ffs_t is array(stages downto 0) of std_logic_vector(width-1 downto 0);
	signal ffs: ffs_t;	-- lowest numbered stage is input
begin
g:	for I in 0 to stages-1 generate
		ffs(I+1) <= ffs(I) when rising_edge(dstclk);
	end generate;
	ffs(0) <= datain;
	dataout <= ffs(stages);
end a;
