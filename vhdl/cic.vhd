----------------------------------------------------------------------------------
-- Company: 
-- Engineer: 
-- 
-- Create Date:    23:17:06 06/12/2016 
-- Design Name: 
-- Module Name:    cic_lpf_nd - a 
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
entity cic_integrator is
	generic(inbits,outbits: integer);
	port(clk: in std_logic;
			din: in signed(inbits-1 downto 0);	--unregistered
			dout: out signed(outbits-1 downto 0));	--registered
end entity;
architecture a of cic_integrator is
	signal val: signed(outbits-1 downto 0);
begin
	val <= val+resize(din,outbits) when rising_edge(clk);
	dout <= val;
end architecture;

library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;
use work.sr_signed;
entity cic_comb is
	generic(bits,len: integer);
	port(clk: in std_logic;
			din: in signed(bits-1 downto 0);	--unregistered
			dout: out signed(bits-1 downto 0));	--registered
end entity;
architecture a of cic_comb is
	signal delayed: signed(bits-1 downto 0);
begin
	sr1: entity sr_signed generic map(bits=>bits,len=>len)
		port map(clk=>clk,din=>din,dout=>delayed);
	dout <= delayed-din when rising_edge(clk);
end architecture;

library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;
use work.cic_integrator;
use work.cic_comb;

--cic lowpass filter (non-decimating)
entity cic_lpf_2_nd is
	generic(inbits: integer := 10;
			outbits: integer := 18;
			stages: integer := 5;
			bw_div: integer := 3	--the "differential delay" of the comb,
										--or the bandwidth division factor
			);
	Port (clk : in  STD_LOGIC;
			din: in signed(inbits-1 downto 0);
			dout: out signed(outbits-1 downto 0));
end cic_lpf_2_nd;
architecture a of cic_lpf_2_nd is
	constant N: integer := outbits-inbits;
	
	constant bitGrowth: integer := outbits-inbits;
	constant gain: integer := bw_div**stages;
	constant allowed_gain: integer := 2**bitGrowth;
	
	type tmp_t is array(0 to stages) of signed(outbits-1 downto 0);
	signal integrators: tmp_t;
	signal differentiators: tmp_t;
begin
	assert gain<=allowed_gain
		report "a bit growth of "&INTEGER'IMAGE(bitGrowth)
			&" will only allow a gain of"&INTEGER'IMAGE(allowed_gain)
			&", but gain (bw_div^stages) is "&INTEGER'IMAGE(gain)
			 severity error;

g1:	for I in 1 to stages generate
		integ: entity cic_integrator generic map(outbits,outbits)
			port map(clk,integrators(I-1),integrators(I));
	end generate;
	integrators(0) <= resize(din,outbits) when rising_edge(clk);
g2:	for I in 1 to stages generate
		diff: entity cic_comb generic map(outbits,bw_div)
			port map(clk,differentiators(I-1),differentiators(I));
	end generate;
	differentiators(0) <= integrators(stages);
	dout <= differentiators(stages);
end a;



library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;
use work.cic_integrator;
use work.cic_comb;

--cic lowpass filter (decimating)
entity cic_lpf_2_d is
	generic(inbits: integer := 10;
			outbits: integer := 18;
			decimation: integer := 2;	--should be an even number
			stages: integer := 5;
			
			--bw_div*decimation is the "differential delay" of the comb,
			--or the bandwidth division factor
			bw_div: integer := 3
			);
	Port(
		--clkd is the decimated clock; make sure flip-flops clocked on clk can
		--be fed into a flip-flop clocked on clkd
		clk,clkd : in  STD_LOGIC;
		din: in signed(inbits-1 downto 0);		--synchronous to clk
		dout: out signed(outbits-1 downto 0));	--synchronous to clkd
end cic_lpf_2_d;
architecture a of cic_lpf_2_d is
	constant N: integer := outbits-inbits;
	
	constant bitGrowth: integer := outbits-inbits;
	constant gain: integer := (bw_div*decimation)**stages;
	constant allowed_gain: integer := 2**bitGrowth;
	
	type tmp_t is array(0 to stages) of signed(outbits-1 downto 0);
	signal integrators: tmp_t;
	signal differentiators: tmp_t;
begin
	assert gain<=allowed_gain
		report "a bit growth of "&INTEGER'IMAGE(bitGrowth)
			&" will only allow a gain of"&INTEGER'IMAGE(allowed_gain)
			&", but gain (bw_div^stages) is "&INTEGER'IMAGE(gain)
			 severity error;

g1:	for I in 1 to stages generate
		integ: entity cic_integrator generic map(outbits,outbits)
			port map(clk,integrators(I-1),integrators(I));
	end generate;
	integrators(0) <= resize(din,outbits) when rising_edge(clk);
g2:	for I in 1 to stages generate
		diff: entity cic_comb generic map(outbits,bw_div)
			port map(clkd,differentiators(I-1),differentiators(I));
	end generate;
	differentiators(0) <= integrators(stages) when rising_edge(clkd);
	dout <= differentiators(stages);
end a;

