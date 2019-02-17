library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity i2s_to_codec is
	generic(
		DATA_WIDTH: integer range 16 to 32 := 32
	);
	port(
		RESET:    in std_logic;
		LRCLK_I:  in std_logic;
		BITCLK_I: in std_logic;
		DATA_I:   in std_logic;
		DATA_O:   out std_logic
	);
end i2s_to_codec;

architecture behavioral of i2s_to_codec is
	signal counter: integer range 0 to DATA_WIDTH;
	signal current_lr: std_logic;
	signal shift_reg: std_logic_vector(DATA_WIDTH - 1 downto 0);
	signal data_l: std_logic_vector(DATA_WIDTH - 1 downto 0);
	signal data_r: std_logic_vector(DATA_WIDTH - 1 downto 0);

	begin
		rd: process(BITCLK_I, LRCLK_I, RESET)
		begin
			if (RESET = '0') then
				counter <= DATA_WIDTH - 1;
				data_l <= (others => '0');
				data_r <= (others => '0');
				shift_reg <= (others => '0');
			elsif (rising_edge(BITCLK_I)) then
				if (LRCLK_I /= current_lr) then
					current_lr <= LRCLK_I;
					counter <= DATA_WIDTH - 1;
					
					if (current_lr = '1') then
						data_l <= shift_reg;
					else
						data_r <= shift_reg;
					end if;
					
					shift_reg <= (others => '0');
					
					shift_reg <= shift_reg(DATA_WIDTH - 2 downto 0) & DATA_I;
					counter <= counter - 1;
				elsif (counter >= 0) then
					shift_reg <= shift_reg(DATA_WIDTH - 2 downto 0) & DATA_I;
					counter <= counter - 1;
				end if;
			end if;
		end process;
		
		wr: process(BITCLK_I, LRCLK_I)
		begin
			if (BITCLK_I'event and BITCLK_I = '0') then
			--	DATA_O <= data_l(counter) when (LRCLK_I = '1') else data_r(counter);
				if (LRCLK_I = '1') then
					DATA_O <= data_l(counter);
				else
					DATA_O <= data_r(counter);
				end if;
			end if;
		end process;

end behavioral;