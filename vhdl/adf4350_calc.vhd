library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;
package adf4350_calc_misc is
	function  divide  (a : UNSIGNED; b : UNSIGNED) return UNSIGNED is
		variable a1 : unsigned(a'length-1 downto 0):=a;
		variable b1 : unsigned(b'length-1 downto 0):=b;
		variable p1 : unsigned(b'length downto 0):= (others => '0');
		variable i : integer:=0;
	begin
		for i in 0 to b'length-1 loop
			p1(b'length-1 downto 1) := p1(b'length-2 downto 0);
			p1(0) := a1(a'length-1);
			a1(a'length-1 downto 1) := a1(a'length-2 downto 0);
			p1 := p1-b1;
			if(p1(b'length-1) ='1') then
				a1(0) :='0';
				p1 := p1+b1;
			else
				a1(0) :='1';
			end if;
		end loop;
		return a1;
	end divide;
	function  modulo  (a : UNSIGNED; b : UNSIGNED) return UNSIGNED is
		variable a1 : unsigned(a'length-1 downto 0):=a;
		variable b1 : unsigned(b'length-1 downto 0):=b;
		variable p1 : unsigned(b'length downto 0):= (others => '0');
		variable i : integer:=0;
	begin
		for i in 0 to b'length-1 loop
			p1(b'length-1 downto 1) := p1(b'length-2 downto 0);
			p1(0) := a1(a'length-1);
			a1(a'length-1 downto 1) := a1(a'length-2 downto 0);
			p1 := p1-b1;
			if(p1(b'length-1) ='1') then
				a1(0) :='0';
				p1 := p1+b1;
			else
				a1(0) :='1';
			end if;
		end loop;
		return p1;
	end modulo;
end package;

library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;
use work.adf4350_calc_misc.all;

-- modulus must be chosen so that channel spacing is 10kHz;
-- freq is in steps of 10kHz.
entity adf4350_calc is
	generic(modulus: integer := 1920;
			fb_divided: boolean := false);
	port(clk: in std_logic;
		freq: in unsigned(19 downto 0);
		N: out std_logic_vector(15 downto 0);
		frac: out std_logic_vector(11 downto 0);
		O: out std_logic_vector(2 downto 0));
end entity;
architecture a of adf4350_calc is
	signal vcoFreq,vcoFreq1: unsigned(19 downto 0);
	signal odiv,odiv1: unsigned(2 downto 0);
	signal N0: unsigned(15 downto 0);
	signal frac0: unsigned(11 downto 0);
begin
	vcoFreq <= freq when freq>=220000 or fb_divided else
				shift_left(freq,1) when freq>=110000 else
				shift_left(freq,2) when freq>=55000 else
				shift_left(freq,3) when freq>=27500 else
				shift_left(freq,4) when freq>=13750 else
				shift_left(freq,5) when freq>=6875 else
				shift_left(freq,6);
	
	odiv <= to_unsigned(0,3) when freq>=220000 else
			to_unsigned(1,3) when freq>=110000 else
			to_unsigned(2,3) when freq>=55000 else
			to_unsigned(3,3) when freq>=27500 else
			to_unsigned(4,3) when freq>=13750 else
			to_unsigned(5,3) when freq>=6875 else
			to_unsigned(6,3);
	vcoFreq1 <= vcoFreq when rising_edge(clk);
	odiv1 <= odiv when rising_edge(clk);
	
	N0 <= divide(vcoFreq1,to_unsigned(modulus,20))(15 downto 0) when rising_edge(clk);
	frac0 <= modulo(vcoFreq1,to_unsigned(modulus,20))(11 downto 0) when rising_edge(clk);
	N <= std_logic_vector(N0);
	frac <= std_logic_vector(frac0);
	O <= std_logic_vector(odiv1) when rising_edge(clk);
end architecture;
