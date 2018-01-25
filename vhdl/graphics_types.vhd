library ieee;
use ieee.numeric_std.all;
use ieee.std_logic_1164.all;

package graphics_types is
	type color is array(0 to 2) of unsigned(7 downto 0);
	type position is array(0 to 1) of unsigned(11 downto 0);
	constant position_zero: position := ("000000000000","000000000000");
end package;
