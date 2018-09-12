library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;
use work.vnaTxNew_misc.all;

entity vnaTxNew2 is
	generic(adcBits: integer := 10;
			sgBits: integer := 9
			);
	port(clk: in std_logic;
		adcData0, adcData1: in signed(adcBits-1 downto 0);
		sg_im,sg_re: in signed(sgBits-1 downto 0);
		
		-- switch control
		sw: out std_logic;
		
		-- serial data out
		txdat: out std_logic_vector(7 downto 0);
		txval: out std_logic
	);
end entity;

architecture a of vnaTxNew2 is
	constant resultBits: integer := adcBits+sgBits;
	constant accumPeriodOrder: integer := 15;
	constant accumBits: integer := resultBits+accumPeriodOrder-1;
	constant outBits: integer := 35;
	constant nInputs: integer := 2;
	constant nValues: integer := 4;

	type input_t is array(nInputs-1 downto 0) of signed(adcBits-1 downto 0);
	signal inputs: input_t;

	-- state machine
	signal accumPhase: unsigned(accumPeriodOrder-1 downto 0);
	signal accumPhaseLSB: unsigned(13 downto 0);
	signal swNext, sw0: std_logic;
	signal accumEnable,accumEnableNext: std_logic;

	-- mixer
	type mixer_t is array(nInputs-1 downto 0) of signed(adcBits+sgBits-1 downto 0);
	signal mixed_re,mixed_im: mixer_t;
	
	-- accumulator
	type accum_t is array(nValues-1 downto 0) of signed(accumBits-1 downto 0);
	signal accum_re,accum_im,accum_reNext,accum_imNext,accumIn_re,accumIn_im,accumIn_reNext,accumIn_imNext: accum_t;
	type out_t is array(nValues-1 downto 0) of signed(outBits-1 downto 0);
	signal out_re, out_im: out_t;
	
	-- serial tx
	type txSR_t is array(nValues*10-1 downto 0) of signed(7 downto 0);
	signal txSR, txSRNext, txValue: txSR_t;
	signal txvalNext: std_logic;
	
	
	-- internal signals, before the checksum unit
	signal txvalI: std_logic;
	signal txdatI: std_logic_vector(7 downto 0);
	
	-- checksum unit
	signal txvalPrev: std_logic;
	signal checksumReg,checksumRegNext: unsigned(7 downto 0);
begin
	inputs(0) <= adcData0 when rising_edge(clk);
	inputs(1) <= adcData1 when rising_edge(clk);
	
	-- state machine
	accumPhase <= accumPhase+1 when rising_edge(clk);
	accumPhaseLSB <= accumPhase(accumPhaseLSB'left downto 0);

	-- input mixers
g1:	for I in 0 to nInputs-1 generate
		-- quadrature mixer
		mixed_re(I) <= inputs(I)*sg_re when rising_edge(clk);
		mixed_im(I) <= inputs(I)*sg_im when rising_edge(clk);
	end generate;
	
	-- switch control: MSB of accumPhaseLSB determines switch position,
	-- and rest of accumPhaseLSB is used for timing
	swNext <= accumPhaseLSB(accumPhaseLSB'left);
	-- whenever switch position is changed, wait 10 periods before starting integration
	accumEnableNext <= '1' when accumPhaseLSB(accumPhaseLSB'left-1 downto 0) > 1024 else '0';
	sw0 <= swNext when rising_edge(clk);
	sw <= sw0;
	accumEnable <= accumEnableNext when rising_edge(clk);
	
	-- accumulator inputs
g1_1:
	for I in 0 to nInputs-1 generate
		accumIn_reNext(I*2) <= resize(mixed_re(I),accumBits) when sw0='0' and accumEnable='1' else to_signed(0,accumBits);
		accumIn_imNext(I*2) <= resize(mixed_im(I),accumBits) when sw0='0' and accumEnable='1' else to_signed(0,accumBits);
		accumIn_reNext(I*2+1) <= resize(mixed_re(I),accumBits) when sw0='1' and accumEnable='1' else to_signed(0,accumBits);
		accumIn_imNext(I*2+1) <= resize(mixed_im(I),accumBits) when sw0='1' and accumEnable='1' else to_signed(0,accumBits);
	end generate;

g1_2:
	for I in 0 to nValues-1 generate
		-- accumulator
		accumIn_re(I) <= accumIn_reNext(I) when rising_edge(clk);
		accumIn_im(I) <= accumIn_imNext(I) when rising_edge(clk);
		
		accum_reNext(I) <= accumIn_re(I) when accumPhase=0 else accum_re(I)+resize(accumIn_re(I),accumBits);
		accum_imNext(I) <= accumIn_im(I) when accumPhase=0 else accum_im(I)+resize(accumIn_im(I),accumBits);
		accum_re(I) <= accum_reNext(I) when rising_edge(clk);
		accum_im(I) <= accum_imNext(I) when rising_edge(clk);
		
	g1_2_1:
		if outBits >= accumBits generate
			out_re(I) <= resize(accum_re(I), outBits);
			out_im(I) <= resize(accum_im(I), outBits);
		end generate;
	g1_2_2:
		if outBits < accumBits generate
			out_re(I) <= accum_re(I)(accum_re(I)'left downto accum_re(I)'left-outBits+1);
			out_im(I) <= accum_im(I)(accum_im(I)'left downto accum_im(I)'left-outBits+1);
		end generate;
	end generate;
	
	-- tx shift register
g2:	for I in 0 to nValues-1 generate
		constant isContinuation: std_logic := to_std_logic(I/=0);
	begin
		txValue(I*10+9 downto I*10) <= ("1"&out_re(I)(34 downto 28), "1"&out_re(I)(27 downto 21), "1"&out_re(I)(20 downto 14), 
				"1"&out_re(I)(13 downto 7), "1"&out_re(I)(6 downto 0), 
				"1"&out_im(I)(34 downto 28), "1"&out_im(I)(27 downto 21), "1"&out_im(I)(20 downto 14), 
				"1"&out_im(I)(13 downto 7), isContinuation & out_im(I)(6 downto 0));
	end generate;
	
	txSRNext <= txValue when accumPhase=0 else ("00000000") & txSR(nValues*10-1 downto 1);
	txSR <= txSRNext when rising_edge(clk);
	
	txdatI <= std_logic_vector(txSR(0));
	txvalNext <= '1' when accumPhase<(nValues*10) else '0';
	txvalI <= txvalNext when rising_edge(clk);
	
	
	-- checksum unit
	checksumRegNext <= "01000110" when txvalI='0' else
					checksumReg xor (checksumReg(6 downto 0) & "1") xor unsigned(txdatI);
	checksumReg <= checksumRegNext when rising_edge(clk);
	txvalPrev <= txvalI when rising_edge(clk);
	txval <= txvalI or txvalPrev;
	txdat <= txdatI when txvalI='1' else "1" & std_logic_vector(checksumReg(6 downto 0));
end architecture;

