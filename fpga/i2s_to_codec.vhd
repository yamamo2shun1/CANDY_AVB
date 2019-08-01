library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity i2s_to_codec is
	generic(
		DATA_WIDTH: integer range 16 to 32 := 32
	);
	port(
		CLK:          in  std_logic;
		RESET:        in  std_logic;
		LRCLK_I_MST:  in  std_logic;
		BITCLK_I_MST: in  std_logic;
		DATA_I_MST:   in  std_logic;
		DATA_O_MST:   out std_logic;
		LRCLK_O_SLV:  out std_logic;
		BITCLK_O_SLV: out std_logic;
		DATA_I_SLV:   in  std_logic;
		DATA_O_SLV:   out std_logic;
		
		avs_s1_clk: in std_logic;
		
		-- Avalon-MM
		avs_s1_address:       in  std_logic_vector(7 downto 0);-- := (others => '0'); -- s1 slave interface
		avs_s1_read:          in  std_logic;-- := '0';                                 -- s1 slave interface
		avs_s1_write:         in  std_logic;-- := '0';                                 -- s1 slave interface
		avs_s1_writedata:     in  std_logic_vector(31 downto 0);-- := (others => '0'); -- s1 slave interface
		avs_s1_waitrequest:   out std_logic;
		avs_s1_readdata:      out std_logic_vector(31 downto 0)                        -- s1 slave interface
--		avs_s1_readdatavalid: out std_logic
	);
end i2s_to_codec;

architecture behavioral of i2s_to_codec is
	signal counter: integer range 0 to DATA_WIDTH := DATA_WIDTH - 1;
	signal counter_s: integer range 0 to DATA_WIDTH := DATA_WIDTH - 1;
	signal shift_reg_mst: std_logic_vector(DATA_WIDTH - 1 downto 0) := (others => '0');
	signal shift_reg_slv: std_logic_vector(DATA_WIDTH - 1 downto 0) := (others => '0');
	signal data_l_mst: std_logic_vector(DATA_WIDTH - 1 downto 0) := (others => '0');
	signal data_r_mst: std_logic_vector(DATA_WIDTH - 1 downto 0) := (others => '0');
	signal data_l_slv: std_logic_vector(DATA_WIDTH - 1 downto 0) := (others => '0');
	signal data_r_slv: std_logic_vector(DATA_WIDTH - 1 downto 0) := (others => '0');
	
	signal bclk_ff: std_logic_vector(1 downto 0) := (others => '0');
	signal lrclk_ff: std_logic_vector(1 downto 0) := (others => '0');
	signal data_mst_ff: std_logic_vector(1 downto 0) := (others => '0');
	signal data_slv_ff: std_logic_vector(1 downto 0) := (others => '0');
	
	signal bclk_last: std_logic := '0';
	signal lrclk_last: std_logic := '0';
	
	signal wait_counter: integer range 0 to 3;
--	signal valid_counter: integer range 0 to 1;
	signal adc_0: signed(31 downto 0) := (others => '0');
	signal adc_1: signed(31 downto 0) := (others => '0');

	begin
		process(CLK)
		begin
			if (rising_edge(CLK)) then
				bclk_ff <= bclk_ff(0) & BITCLK_I_MST;
				lrclk_ff <= lrclk_ff(0) & LRCLK_I_MST;
				data_mst_ff <= data_mst_ff(0) & DATA_I_MST;
				data_slv_ff <= data_slv_ff(0) & DATA_I_SLV;
				
				bclk_last <= bclk_ff(1);
			end if;
		end process;

		BITCLK_O_SLV <= BITCLK_I_MST;
		LRCLK_O_SLV <= LRCLK_I_MST;
	
		cnt: process(CLK, RESET)
		begin
			if (RESET = '0') then
				counter <= DATA_WIDTH - 1;
			elsif (rising_edge(CLK)) then
				if (bclk_last = '0' and bclk_ff(1) = '1') then
					if (lrclk_last /= lrclk_ff(1)) then
						counter <= DATA_WIDTH - 1;
						lrclk_last <= lrclk_ff(1);
					elsif (counter >= 0) then
						counter <= counter - 1;
						lrclk_last <= lrclk_ff(1);
					end if;
					
					if (counter = 1) then
						counter_s <= DATA_WIDTH - 1;
					else
						counter_s <= counter_s - 1;
					end if;
				end if;
			end if;
		end process;
		
		rd_mst: process(CLK, RESET)
			variable srm: std_logic_vector(DATA_WIDTH - 1 downto 0) := (others => '0');
			variable srm0: std_logic_vector(DATA_WIDTH - 1 downto 0) := (others => '0');

			variable vm0: signed(47 downto 0) := (others => '0');
			variable vm1: signed(23 downto 0) := (others => '0');
		begin
			if (RESET = '0') then
				data_l_mst <= (others => '0');
				data_r_mst <= (others => '0');
				
				shift_reg_mst <= (others => '0');
			elsif (rising_edge(CLK)) then
				if (bclk_last = '0' and bclk_ff(1) = '1') then
					if (counter = 1) then
						srm := shift_reg_mst(DATA_WIDTH - 2 downto 0) & '0';
						
						vm0 := signed(srm(30 downto 7)) * adc_0(23 downto 0);
						vm1 := resize(shift_right(vm0, 12), 24);
						srm0 := '0' & std_logic_vector(vm1) & "0000000";
						
						if (lrclk_last = '0') then
							data_l_mst <= srm0;
						else
							data_r_mst <= srm0;
						end if;
						
						shift_reg_mst <= shift_reg_mst(DATA_WIDTH - 2 downto 0) & data_mst_ff(1);
					else
						shift_reg_mst <= shift_reg_mst(DATA_WIDTH - 2 downto 0) & data_mst_ff(1);
					end if;
				end if;
			end if;
		end process;
		
		rd_slv: process(CLK, RESET)
			variable srs: std_logic_vector(DATA_WIDTH - 1 downto 0) := (others => '0');
			variable srs0: std_logic_vector(DATA_WIDTH - 1 downto 0) := (others => '0');

			variable vs0: signed(47 downto 0) := (others => '0');
			variable vs1: signed(23 downto 0) := (others => '0');
		begin
			if (RESET = '0') then
				data_l_slv <= (others => '0');
				data_r_slv <= (others => '0');
				
				shift_reg_slv <= (others => '0');
			elsif (rising_edge(CLK)) then
				if (bclk_last = '0' and bclk_ff(1) = '1') then
					if (counter = 1) then
						srs := shift_reg_slv(DATA_WIDTH - 2 downto 0) & '0';

						vs0 := signed(srs(30 downto 7)) * adc_1(23 downto 0);
						vs1 := resize(shift_right(vs0, 12), 24);
						srs0 := '0' & std_logic_vector(vs1) & "0000000";
						
						if (lrclk_last = '0') then
							data_l_slv <= srs0;
						else
							data_r_slv <= srs0;
						end if;
						
						shift_reg_slv <= shift_reg_slv(DATA_WIDTH - 2 downto 0) & data_slv_ff(1);
					else
						shift_reg_slv <= shift_reg_slv(DATA_WIDTH - 2 downto 0) & data_slv_ff(1);
					end if;
				end if;
			end if;
		end process;
		
--		wr_mst: process(CLK)
		wr_mst: process(BITCLK_I_MST)
		begin
			if (falling_edge(BITCLK_I_MST)) then
--				if (bclk_last = '0' and bclk_ff(1) = '1') then
--					if (lrclk_last = '1') then
					if (LRCLK_I_MST = '1') then
						DATA_O_MST <= data_l_slv(counter_s);
					else
						DATA_O_MST <= data_r_slv(counter_s);
					end if;
--				end if;
			end if;
		end process;
		
--		wr_slv: process(CLK)
		wr_slv: process(BITCLK_I_MST)
		begin
			if (falling_edge(BITCLK_I_MST)) then
--				if (bclk_last = '0' and bclk_ff(1) = '1') then
--					if (lrclk_last = '1') then
					if (LRCLK_I_MST = '1') then
						DATA_O_SLV <= data_l_mst(counter_s);
					else
						DATA_O_SLV <= data_r_mst(counter_s);
					end if;
--				end if;
			end if;
		end process;
		
		avs_rw: process(avs_s1_clk, RESET)
		begin
			if (RESET = '0') then
				adc_0 <= X"00000000";
			elsif(avs_s1_clk'event and avs_s1_clk = '1') then
--				if (avs_s1_read = '1') then
				if (avs_s1_read = '1' and wait_counter = 3) then
					avs_s1_readdata <= std_logic_vector(adc_0);
--					avs_s1_readdatavalid <= '1';
				elsif (avs_s1_write = '1') then
					if (avs_s1_address = "00000000") then
						adc_0 <= signed(avs_s1_writedata);
					elsif (avs_s1_address = "00000100") then
						adc_1 <= signed(avs_s1_writedata);
					end if;
				end if;
				
				if ((avs_s1_read = '1' or avs_s1_write = '1') and wait_counter < 3) then
					avs_s1_waitrequest <= '1';
					wait_counter <= wait_counter + 1;
				elsif ((avs_s1_read = '1' or avs_s1_write = '1') and wait_counter = 3) then
					avs_s1_waitrequest <= '0';
				elsif (avs_s1_read = '0' and avs_s1_write = '0') then
					wait_counter <= 0;
--				elsif (avs_s1_read = '0' and valid_counter = 0) then
--					avs_s1_readdatavalid <= '0';
				end if;
			end if;
		end process;		
end behavioral;