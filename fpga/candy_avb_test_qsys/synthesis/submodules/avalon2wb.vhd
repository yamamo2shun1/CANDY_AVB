library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_arith.all;

entity AVALON2WB is
	port(
		csi_clk:     in std_logic; -- clock interface
		csi_reset_n: in std_logic; -- reset clock interface
		
		-- Avalon-MM
		avs_s1_address:       in  std_logic_vector(27 downto 0);-- := (others => '0'); -- s1 slave interface
		avs_s1_chipselect:    in  std_logic;
		avs_s1_byteenable:    in  std_logic_vector(3 downto 0);                     -- s1 slave interface
		avs_s1_read:          in  std_logic;-- := '0';                                 -- s1 slave interface
		avs_s1_write:         in  std_logic;-- := '0';                                 -- s1 slave interface
		avs_s1_writedata:     in  std_logic_vector(31 downto 0);-- := (others => '0'); -- s1 slave interface
		avs_s1_readdata:      out std_logic_vector(31 downto 0);                    -- s1 slave interface
		-- avs_s1_waitrequest_n: out std_logic;-- := '0';

		-- WISHBONE
		CLK_O: out std_logic;
		RST_O: out std_logic;
		CYC_O: out std_logic;
		STB_O: out std_logic;
		ADR_O: out std_logic_vector(31 downto 0);
		SEL_O: out std_logic_vector(3  downto 0);
		WE_O:  out std_logic;
		DAT_O: out std_logic_vector(31 downto 0);
		DAT_I: in  std_logic_vector(31 downto 0);
		ACK_I: in  std_logic;
		ERR_I: in  std_logic;
		RTY_I: in  std_logic 
	);
end AVALON2WB;

architecture structural of AVALON2WB is
	signal byte_adr_b01: std_logic_vector(1  downto 0);

	signal wb_term: std_logic; -- bus termination

	begin
		CLK_O <= csi_clk;
		RST_O <= not csi_reset_n;
			
		-- WishBone Bus termination
		wb_term <= '1' when ((ACK_I = '1') or (ERR_I = '1') or (RTY_I = '1')) else '0';
			
		byte_adr_b01 <= "00" when (avs_s1_byteenable(0) = '1') else                 
		 					 "01" when (avs_s1_byteenable(1) = '1') else
		 					 "10" when (avs_s1_byteenable(2) = '1') else
		 					 "11" when (avs_s1_byteenable(3) = '1') else "00";
	
		process(csi_clk, csi_reset_n)
		begin
			if (csi_reset_n = '0') then
				CYC_O <= '0';
			elsif (csi_clk'event and csi_clk = '1') then
				if (avs_s1_read = '1' or avs_s1_write = '1') then
					CYC_O <= '1';
				elsif (wb_term = '1') then
					CYC_O <= '0';
			-- else
			-- 	CYC_O <= CYC_O;
				end if;
			end if;
		end process;

		process(csi_clk, csi_reset_n)
		begin
			if (csi_reset_n = '0') then
				STB_O <= '0';
			elsif (csi_clk'event and csi_clk = '1') then
			-- if (avs_s1_read = '1' or avs_s1_write = '1') then
				if (avs_s1_chipselect = '1' and (avs_s1_read = '1' or avs_s1_write = '1')) then
					STB_O <= '1';
				elsif (wb_term = '1') then
					STB_O <= '0';
			-- else
			-- 	STB_O <= STB_O;
				end if;
			end if;
		end process;

		process(csi_clk, csi_reset_n)
		begin
			if (csi_reset_n = '0') then
				WE_O <= '0';
			elsif (csi_clk'event and csi_clk = '1') then
			-- if avs_s1_read = '1' or avs_s1_write = '1' then
				if (avs_s1_write = '1') then
					WE_O <= '1';
				elsif (wb_term = '1') then
					WE_O <= '0';
			-- else
			-- 	WE_O <= WE_O;
				end if;
			end if;
		end process;

		process(csi_clk, csi_reset_n)
		begin
			if (csi_reset_n = '0') then
				ADR_O <= X"00000000";
			elsif (csi_clk'event and csi_clk = '1') then
				if (avs_s1_read = '1' or avs_s1_write = '1') then
					ADR_O <= "00" & avs_s1_address(27 downto 0) & byte_adr_b01;
			-- else
			-- 	ADR_O <= ADR_O;
				end if;
			end if;
		end process;

		process(csi_clk, csi_reset_n)
		begin
			if (csi_reset_n = '0') then
				SEL_O <= "0000";
			elsif (csi_clk'event and csi_clk = '1') then
				if (avs_s1_read = '1' or avs_s1_write = '1') then
					SEL_O <= avs_s1_byteenable;
			-- else
			-- 	SEL_O <= SEL_O;
				end if;
			end if;
		end process;

		process(csi_clk, csi_reset_n)
		begin
			if (csi_reset_n = '0') then
				DAT_O <= X"00000000";
			elsif (csi_clk'event and csi_clk = '1') then
				if (avs_s1_write = '1') then
					DAT_O <= avs_s1_writedata;
				-- if (avs_s1_byteenable(0) = '1') then
				-- 	DAT_O <= avs_s1_writedata;
				-- elsif (avs_s1_byteenable(1) = '1') then  
				-- 	DAT_O <= X"00" & avs_s1_writedata(31 downto 8);
				-- elsif (avs_s1_byteenable(2) = '1') then
				-- 	DAT_O <= X"0000" & avs_s1_writedata(31 downto 16);
				-- elsif (avs_s1_byteenable(3) = '1') then
				-- 	DAT_O <= X"000000" & avs_s1_writedata(31 downto 24);
				-- end if;
			-- else
			-- 	DAT_O <= DAT_O;
				end if;
			end if;
		end process;
		
		process(csi_clk, csi_reset_n)
		begin
			if (csi_reset_n = '0') then
				avs_s1_readdata <= X"00000000";
			elsif (csi_clk'event and csi_clk = '1') then
				if ACK_I = '1' then
					avs_s1_readdata <= DAT_I;
				-- if (avs_s1_byteenable(0) = '1') then
				-- 	avs_s1_readdata <= DAT_I;
				-- elsif (avs_s1_byteenable(1) = '1') then  
				-- 	avs_s1_readdata <= DAT_I(31 downto 8) & X"00";
				-- elsif (avs_s1_byteenable(2) = '1') then
				-- 	avs_s1_readdata <= DAT_I(31 downto 16) & X"0000";
				-- elsif (avs_s1_byteenable(3) = '1') then
				-- 	avs_s1_readdata <= DAT_I(31 downto 24) & X"000000";
				-- end if;
			-- else
			-- 	avs_s1_readdata <= avs_s1_readdata;
				end if;
			end if;
		end process;
end architecture structural;