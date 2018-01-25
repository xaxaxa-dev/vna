library ieee;
use ieee.numeric_std.all;
use ieee.std_logic_1164.all;
use work.graphics_types.all;
entity bounce_sprite is
	generic(W,H,spriteW,spriteH: integer;
		initX,initY: integer := 0);
	port(clk, en: in std_logic;
			sprite_p: out position;
			initvX,initvY: in unsigned(14 downto 0));
end entity;
architecture a of bounce_sprite is
	--4 bits after decimal point
	type position_f is array(0 to 1) of unsigned(15 downto 0);
	--there are only 4 possible velocities
	signal cv,nv: std_logic_vector(1 downto 0);
	signal cp,np: position_f := (X"0000",X"0000");
	signal vx,vy: signed(15 downto 0);
begin
	cv <= nv when en='1' and rising_edge(clk);
	nv(0) <= '1' when cp(0)+spriteW*16+initvX*2>W*16 and cv(0)='0' else
			'0' when cp(0)<initvX*2 and cv(0)='1' else cv(0);
	nv(1) <= '1' when cp(1)+spriteH*16+initvY*2>H*16 and cv(1)='0' else
			'0' when cp(1)<initvY*2 and cv(1)='1' else cv(1);
	cp <= np when en='1' and rising_edge(clk);
	vx <= "0"&signed(initvX) when cv(0)='0' else -("0"&signed(initvX));
	vy <= "0"&signed(initvY) when cv(1)='0' else -("0"&signed(initvY));
	np <= (cp(0)+unsigned(vx),cp(1)+unsigned(vy));
	sprite_p <= (cp(0)(15 downto 4),cp(1)(15 downto 4));
end architecture;
