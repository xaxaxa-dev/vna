library ieee;
library work;
use ieee.numeric_std.all;
use ieee.std_logic_1164.all;
-- combinational grey code encoder/decoder
entity greycodeEnc is
	generic(width: integer := 8);
	port(datain: in unsigned(width-1 downto 0);
			dataout: out std_logic_vector(width-1 downto 0));
end entity;
architecture a of greycodeEnc is
begin
	dataout(width-1) <= datain(width-1);
g:	for I in 0 to width-2 generate
		dataout(I) <= datain(I) xor datain(I+1);
	end generate;
end architecture;

library ieee;
library work;
use ieee.numeric_std.all;
use ieee.std_logic_1164.all;
-- combinational grey code encoder/decoder
entity greycodeDec is
	generic(width: integer := 8);
	port(datain: in std_logic_vector(width-1 downto 0);
			dataout: out unsigned(width-1 downto 0));
end entity;
architecture a of greycodeDec is
	signal tmp: unsigned(width-1 downto 0);
begin
	tmp(width-1) <= datain(width-1);
g:	for I in 0 to width-2 generate
		tmp(I) <= datain(I) xor tmp(I+1);
	end generate;
	dataout <= tmp;
end architecture;
