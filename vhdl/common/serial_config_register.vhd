library ieee;
library work;
use ieee.numeric_std.all;
use ieee.std_logic_1164.all;

package configRegisterTypes is
	type array8 is array (natural range <>) of std_logic_vector(7 downto 0);
end package;

library ieee;
library work;
use ieee.numeric_std.all;
use ieee.std_logic_1164.all;
use work.configRegisterTypes.all;

-- command format: 1 byte cmd followed by 1 byte value; cmd is either 0
-- (for no-op) or addr+1 (to write to address addr)
entity serialConfigRegister is
	generic(bytes: integer;
			defaultValue: array8(0 to 254) := (others=>X"00"));
	port(clk: in std_logic;
		rxdat: in std_logic_vector(7 downto 0);
		rxval: in std_logic;
		
		registers: out array8(0 to bytes-1);
		write_indicator: out std_logic;
		write_indicator_addr: out unsigned(7 downto 0)
		);
end entity;
architecture a of serialConfigRegister is
	signal regs: array8(0 to bytes-1) := defaultValue(0 to bytes-1);
	
	signal state,stateNext: std_logic;
	signal waddr,waddrNext: unsigned(7 downto 0);
	
	signal wi: std_logic;
	signal wi_addr: unsigned(7 downto 0);
begin
	assert bytes<=254;
	registers <= regs;
	
	stateNext <= '0' when state='0' and rxdat="00000000" else
				'1' when state='0' else
				'0' when state='1';
	state <= stateNext when rxval='1' and rising_edge(clk);
	
	waddrNext <= unsigned(rxdat)-1 when state='0' else waddr;
	waddr <= waddrNext when rxval='1' and rising_edge(clk);
	
	wi <= '1' when state='1' and rxval='1' else '0';
	wi_addr <= waddr;
	
	-- delay write indicator so when it fires the register already
	-- has the new value
	write_indicator <= wi when rising_edge(clk);
	write_indicator_addr <= wi_addr when rising_edge(clk);
	
	process(clk)
	begin
		 if(rising_edge(clk)) then
			  if(state='1' and rxval='1') then
					regs(to_integer(waddr)) <= rxdat;
			  end if;
		 end if;
	end process;

end architecture;
