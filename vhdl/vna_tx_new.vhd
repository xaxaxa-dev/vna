library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

package vnaTxNew_misc is
	function to_std_logic(b: BOOLEAN) return std_logic is 
	begin 
		if (b = TRUE) then 
			return '1'; 
		else 
			return '0'; 
		end if; 
	end function to_std_logic;
end package;


library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;
use work.vnaTxNew_misc.all;

entity vnaTxNew is
	generic(adcBits: integer := 10;
			sgBits: integer := 9;
			disableInput0: boolean := false
			);
	port(clk: in std_logic;
		adcData0: in signed(adcBits-1 downto 0);
		adcData1: in signed(adcBits-1 downto 0);
		adcData2: in signed(adcBits-1 downto 0);
		sg_im,sg_re: in signed(sgBits-1 downto 0);
		
		-- serial data out
		txdat: out std_logic_vector(7 downto 0);
		txval: out std_logic
	);
end entity;

architecture a of vnaTxNew is
	constant resultBits: integer := adcBits+sgBits;
	constant accumPeriodOrder: integer := 12;
	constant accumBits: integer := resultBits+accumPeriodOrder;
	constant outBits: integer := 35;
	constant nInputs: integer := 3;

	type input_t is array(nInputs-1 downto 0) of signed(adcBits-1 downto 0);
	signal inputs: input_t;

	-- state machine
	signal accumPhase: unsigned(accumPeriodOrder-1 downto 0);

	-- mixer
	type mixer_t is array(nInputs-1 downto 0) of signed(adcBits+sgBits-1 downto 0);
	signal mixed_re,mixed_im: mixer_t;
	
	-- accumulator
	type accum_t is array(nInputs-1 downto 0) of signed(accumBits-1 downto 0);
	signal accum_re,accum_im,accum_reNext,accum_imNext: accum_t;
	type out_t is array(nInputs-1 downto 0) of signed(outBits-1 downto 0);
	signal out_re, out_im: out_t;
	
	-- serial tx
	type txSR_t is array(nInputs*10-1 downto 0) of signed(7 downto 0);
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
	inputs(2) <= adcData2 when rising_edge(clk);
	
	-- state machine
	accumPhase <= accumPhase+1 when rising_edge(clk);

g1:	for I in 0 to nInputs-1 generate
		-- quadrature mixer
		mixed_re(I) <= inputs(I)*sg_re when rising_edge(clk);
		mixed_im(I) <= inputs(I)*sg_im when rising_edge(clk);
		
		-- accumulator
		accum_reNext(I) <= resize(mixed_re(I),accumBits) when accumPhase=0 else accum_re(I)+resize(mixed_re(I),accumBits);
		accum_imNext(I) <= resize(mixed_im(I),accumBits) when accumPhase=0 else accum_im(I)+resize(mixed_im(I),accumBits);
		accum_re(I) <= accum_reNext(I) when rising_edge(clk);
		accum_im(I) <= accum_imNext(I) when rising_edge(clk);
		
		out_re(I) <= accum_re(I)(accum_re(I)'left downto accum_re(I)'left-outBits+1)
			when I/=0 or not disableInput0 else (others=>'1');
		out_im(I) <= accum_im(I)(accum_im(I)'left downto accum_im(I)'left-outBits+1)
			when I/=0 or not disableInput0 else (others=>'1');
	end generate;
	
	-- tx shift register
g2:	for I in 0 to nInputs-1 generate
		constant isContinuation: std_logic := to_std_logic(I/=0);
	begin
		txValue(I*10+9 downto I*10) <= ("1"&out_re(I)(34 downto 28), "1"&out_re(I)(27 downto 21), "1"&out_re(I)(20 downto 14), 
				"1"&out_re(I)(13 downto 7), "1"&out_re(I)(6 downto 0), 
				"1"&out_im(I)(34 downto 28), "1"&out_im(I)(27 downto 21), "1"&out_im(I)(20 downto 14), 
				"1"&out_im(I)(13 downto 7), isContinuation & out_im(I)(6 downto 0));
	end generate;
	
	txSRNext <= txValue when accumPhase=0 else ("00000000") & txSR(nInputs*10-1 downto 1);
	txSR <= txSRNext when rising_edge(clk);
	
	txdatI <= std_logic_vector(txSR(0));
	txvalNext <= '1' when accumPhase<(nInputs*10) else '0';
	txvalI <= txvalNext when rising_edge(clk);
	
	
	-- checksum unit
	checksumRegNext <= "01000110" when txvalI='0' else
					checksumReg xor (checksumReg(6 downto 0) & "1") xor unsigned(txdatI);
	checksumReg <= checksumRegNext when rising_edge(clk);
	txvalPrev <= txvalI when rising_edge(clk);
	txval <= txvalI or txvalPrev;
	txdat <= txdatI when txvalI='1' else "1" & std_logic_vector(checksumReg(6 downto 0));
end architecture;

