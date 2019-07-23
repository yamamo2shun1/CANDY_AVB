library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity i2s_to_codec is
	generic(
		DATA_WIDTH: integer range 16 to 32 := 32
	);
	port(
		RESET:        in  std_logic;
		LRCLK_I_MST:  in  std_logic;
		BITCLK_I_MST: in  std_logic;
		DATA_I_MST:   in  std_logic;
		DATA_O_MST:   out std_logic;
		LRCLK_O_SLV:  out std_logic;
		BITCLK_O_SLV: out std_logic;
		DATA_I_SLV:   in  std_logic;
		DATA_O_SLV:   out std_logic
	);
end i2s_to_codec;

architecture behavioral of i2s_to_codec is
	signal counter: integer range 0 to DATA_WIDTH;
	signal current_lr: std_logic;
	signal shift_reg_mst: std_logic_vector(DATA_WIDTH - 1 downto 0);
	signal shift_reg_slv: std_logic_vector(DATA_WIDTH - 1 downto 0);
	signal data_l_mst: std_logic_vector(DATA_WIDTH - 1 downto 0);
	signal data_r_mst: std_logic_vector(DATA_WIDTH - 1 downto 0);
	signal data_l_slv: std_logic_vector(DATA_WIDTH - 1 downto 0);
	signal data_r_slv: std_logic_vector(DATA_WIDTH - 1 downto 0);

	begin
		BITCLK_O_SLV <= BITCLK_I_MST;
		LRCLK_O_SLV <= LRCLK_I_MST;
	
		rd: process(BITCLK_I_MST, LRCLK_I_MST, RESET)
		begin
			if (RESET = '0') then
				counter <= DATA_WIDTH - 1;
				
				shift_reg_mst <= (others => '0');
				shift_reg_slv <= (others => '0');
				
				data_l_mst <= (others => '0');
				data_r_mst <= (others => '0');
				data_l_slv <= (others => '0');
				data_r_slv <= (others => '0');
			elsif (rising_edge(BITCLK_I_MST)) then
				if (LRCLK_I_MST /= current_lr) then
					current_lr <= LRCLK_I_MST;
					counter <= DATA_WIDTH - 1;
					
					if (current_lr = '1') then
						data_l_mst <= shift_reg_mst;
						data_l_slv <= shift_reg_slv;
					else
						data_r_mst <= shift_reg_mst;
						data_r_slv <= shift_reg_slv;
					end if;
					
					shift_reg_mst <= (others => '0');
					shift_reg_slv <= (others => '0');
					
					shift_reg_mst <= shift_reg_mst(DATA_WIDTH - 2 downto 0) & DATA_I_MST;
					shift_reg_slv <= shift_reg_slv(DATA_WIDTH - 2 downto 0) & DATA_I_SLV;
					counter <= counter - 1;
				elsif (counter >= 0) then
					shift_reg_mst <= shift_reg_mst(DATA_WIDTH - 2 downto 0) & DATA_I_MST;
					shift_reg_slv <= shift_reg_slv(DATA_WIDTH - 2 downto 0) & DATA_I_SLV;
					counter <= counter - 1;
				end if;
			end if;
		end process;
		
		wr: process(BITCLK_I_MST, LRCLK_I_MST)
		begin
			if (BITCLK_I_MST'event and BITCLK_I_MST = '0') then
				if (LRCLK_I_MST = '1') then
					DATA_O_MST <= data_l_slv(counter);
					DATA_O_SLV <= data_l_mst(counter);
				else
					DATA_O_MST <= data_r_slv(counter);
					DATA_O_SLV <= data_r_mst(counter);
				end if;
			end if;
		end process;

end behavioral;