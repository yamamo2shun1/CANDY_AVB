library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_arith.all;
use ieee.std_logic_unsigned.all;
use ieee.numeric_std.all;

entity CANDY_AVB is
	port( -- CLOCK & RESET
			CLK: in std_logic;
			RST: in std_logic;
			
			-- CLOCK OUT
			CODEC_CLKOUT: out std_logic;
			ETH_CLKOUT:   out std_logic;
			
			-- LED
			LED: out std_logic_vector(1 downto 0);
			
			-- UART - CP2102N
			UART_TX:  out std_logic;
			UART_RX:  in  std_logic;
			UART_RTS: out std_logic;
			UART_CTS: in  std_logic;
			
			-- A/D Converter
			ADC_IN: in std_logic_vector(8 downto 0);
			
			-- 3.3V <-> 5V Degital IO
			LS_OE: out   std_logic;
			LS_A:  inout std_logic_vector(7 downto 0);

			-- I2S - ADAU1761 x2
			CODEC_SCL:      inout std_logic;
			CODEC_SDA:      inout std_logic;
			CODEC_RESET:    out   std_logic;
			CODEC_BITCLOCK: inout std_logic_vector(1 downto 0);
			CODEC_LRCLOCK:  inout std_logic_vector(1 downto 0);
			CODEC_DATA_OUT: in    std_logic_vector(1 downto 0);
			CODEC_DATA_IN:  out   std_logic_vector(1 downto 0);
			
			-- Ethernet DP83848
			ETH_INTERRUPT: inout std_logic;
			ETH_MDC:       out   std_logic;
			ETH_MDIO:      inout std_logic;
			ETH_MII_CRS:   in    std_logic;
			ETH_MII_COL:   in    std_logic;
			ETH_RX_CLK:    in    std_logic;
			ETH_TX_CLK:    in    std_logic;
			ETH_MII_RX_DV: in    std_logic;
			ETH_MII_RX_ER: in    std_logic;
			ETH_MII_TX_EN: out   std_logic;
			ETH_MII_TXD:   out   std_logic_vector(3 downto 0);
			ETH_MII_RXD:   in    std_logic_vector(3 downto 0);
			
			-- SDRAM
			DRAM_CLK:  out   std_logic;
			DRAM_CKE:  out   std_logic;
			DRAM_ADDR: out   std_logic_vector(11 downto 0);
			DRAM_BA:   out   std_logic_vector(1 downto 0);
			DRAM_CAS:  out   std_logic;
			DRAM_RAS:  out   std_logic;
			DRAM_CS:   out   std_logic;
			DRAM_WE:   out   std_logic;
			DRAM_UDQM: out   std_logic;
			DRAM_LDQM: out   std_logic;
			DRAM_DQ:   inout std_logic_vector(15 downto 0)
	);
end CANDY_AVB;

architecture RTL of CANDY_AVB is
	component candy_avb_test_qsys is
		port (
			clk_clk                           : in    std_logic                     := 'X';             -- clk
			codec_clk_clk                     : out   std_logic;                                        -- clk
			codec_reset_export                : out   std_logic;                                        -- export
		-- codec_scl_export                  : inout std_logic                     := 'X';             -- export
		-- codec_sda_export                  : inout std_logic                     := 'X';             -- export
		-- eth_mii_rx_d                      : in    std_logic_vector(3 downto 0)  := (others => 'X'); -- mii_rx_d
		-- eth_mii_rx_dv                     : in    std_logic                     := 'X';             -- mii_rx_dv
		-- eth_mii_rx_err                    : in    std_logic                     := 'X';             -- mii_rx_err
		-- eth_mii_tx_d                      : out   std_logic_vector(3 downto 0);                     -- mii_tx_d
		-- eth_mii_tx_en                     : out   std_logic;                                        -- mii_tx_en
		-- eth_mii_tx_err                    : out   std_logic;                                        -- mii_tx_err
		-- eth_mii_crs                       : in    std_logic                     := 'X';             -- mii_crs
		-- eth_mii_col                       : in    std_logic                     := 'X';             -- mii_col
			eth_clk_clk                       : out   std_logic;                                        -- clk
		-- eth_interrupt_export              : inout std_logic                     := 'X';             -- export
		-- eth_mdio_mdc                      : out   std_logic;                                        -- mdc
		-- eth_mdio_mdio_in                  : in    std_logic                     := 'X';             -- mdio_in
		-- eth_mdio_mdio_out                 : out   std_logic;                                        -- mdio_out
		-- eth_mdio_mdio_oen                 : out   std_logic;                                        -- mdio_oen
		-- eth_misc_ff_tx_crc_fwd            : in    std_logic                     := 'X';             -- ff_tx_crc_fwd
		-- eth_misc_ff_tx_septy              : out   std_logic;                                        -- ff_tx_septy
		-- eth_misc_tx_ff_uflow              : out   std_logic;                                        -- tx_ff_uflow
		-- eth_misc_ff_tx_a_full             : out   std_logic;                                        -- ff_tx_a_full
		-- eth_misc_ff_tx_a_empty            : out   std_logic;                                        -- ff_tx_a_empty
		-- eth_misc_rx_err_stat              : out   std_logic_vector(17 downto 0);                    -- rx_err_stat
		-- eth_misc_rx_frm_type              : out   std_logic_vector(3 downto 0);                     -- rx_frm_type
		-- eth_misc_ff_rx_dsav               : out   std_logic;                                        -- ff_rx_dsav
		-- eth_misc_ff_rx_a_full             : out   std_logic;                                        -- ff_rx_a_full
		-- eth_misc_ff_rx_a_empty            : out   std_logic;                                        -- ff_rx_a_empty
		-- eth_rx_clk_clk                    : in    std_logic                     := 'X';             -- clk
		-- eth_status_set_10                 : in    std_logic                     := 'X';             -- set_10
		-- eth_status_set_1000               : in    std_logic                     := 'X';             -- set_1000
		-- eth_status_eth_mode               : out   std_logic;                                        -- eth_mode
		-- eth_status_ena_10                 : out   std_logic;                                        -- ena_10
		-- eth_tx_clk_clk                    : in    std_logic                     := 'X';             -- clk
			new_sdram_controller_0_wire_addr  : out   std_logic_vector(11 downto 0);                    -- addr
			new_sdram_controller_0_wire_ba    : out   std_logic_vector(1 downto 0);                     -- ba
			new_sdram_controller_0_wire_cas_n : out   std_logic;                                        -- cas_n
			new_sdram_controller_0_wire_cke   : out   std_logic;                                        -- cke
			new_sdram_controller_0_wire_cs_n  : out   std_logic;                                        -- cs_n
			new_sdram_controller_0_wire_dq    : inout std_logic_vector(15 downto 0) := (others => 'X'); -- dq
			new_sdram_controller_0_wire_dqm   : out   std_logic_vector(1 downto 0);                     -- dqm
			new_sdram_controller_0_wire_ras_n : out   std_logic;                                        -- ras_n
			new_sdram_controller_0_wire_we_n  : out   std_logic;                                        -- we_n
			reset_reset_n                     : in    std_logic                     := 'X';             -- reset_n
			sdclk_clk_clk                     : out   std_logic;                                        -- clk
			uart0_rxd                         : in    std_logic                     := 'X';             -- rxd
			uart0_txd                         : out   std_logic;                                        -- txd
			uart0_cts_n                       : in    std_logic                     := 'X';             -- cts_n
			uart0_rts_n                       : out   std_logic;                                        -- rts_n
			user_led_export                   : out   std_logic_vector(1 downto 0);                     -- export
			wb_clk_o                          : out   std_logic;                                        -- clk
			wb_rst_o                          : out   std_logic;                                        -- rst
			wb_cyc_o                          : out   std_logic;                                        -- cyc
			wb_stb_o                          : out   std_logic;                                        -- stb
			wb_adr_o                          : out   std_logic_vector(31 downto 0);                    -- adr
			wb_sel_o                          : out   std_logic_vector(3 downto 0);                     -- sel
			wb_we_o                           : out   std_logic                     := 'X';             -- we
			wb_dat_o                          : out   std_logic_vector(31 downto 0);                    -- dat_o
			wb_dat_i                          : in    std_logic_vector(31 downto 0) := (others => 'X'); -- dat_i
			wb_ack_i                          : in    std_logic                     := 'X';             -- ack
			wb_err_i                          : in    std_logic                     := 'X';             -- err
			wb_rty_i                          : in    std_logic                     := 'X'              -- rty
		);
	end component candy_avb_test_qsys;

	component i2c_master_top
		generic(
			ARST_LVL : std_logic := '0'                   -- asynchronous reset level
		);
		port(
         -- wishbone signals
         wb_clk_i      : in  std_logic;                    -- master clock input
         wb_rst_i      : in  std_logic := '0';             -- synchronous active high reset
         arst_i        : in  std_logic := not ARST_LVL;    -- asynchronous reset
         wb_adr_i      : in  std_logic_vector(2 downto 0); -- lower address bits
         wb_dat_i      : in  std_logic_vector(7 downto 0); -- Databus input
         wb_dat_o      : out std_logic_vector(7 downto 0); -- Databus output
         wb_we_i       : in  std_logic;                    -- Write enable input
         wb_stb_i      : in  std_logic;                    -- Strobe signals / core select signal
         wb_cyc_i      : in  std_logic;                    -- Valid bus cycle input
         wb_ack_o      : out std_logic;                    -- Bus cycle acknowledge output
         wb_inta_o     : out std_logic;                    -- interrupt request output signal

         -- i2c lines
         scl_pad_i     : in  std_logic;                    -- i2c clock line input
         scl_pad_o     : out std_logic;                    -- i2c clock line output
         scl_padoen_o  : out std_logic;                    -- i2c clock line output enable, active low
         sda_pad_i     : in  std_logic;                    -- i2c data line input
         sda_pad_o     : out std_logic;                    -- i2c data line output
         sda_padoen_o  : out std_logic                     -- i2c data line output enable, active low
		);
	end component;
	
	component i2s_to_codec
		generic(
			DATA_WIDTH: integer range 0 to 32 := 32
		);
		port(
			RESET:    in std_logic;
			LRCLK_I_MST:  in  std_logic;
			BITCLK_I_MST: in  std_logic;
			DATA_I_MST:   in  std_logic;
			DATA_O_MST:   out std_logic;
			LRCLK_O_SLV:  out std_logic;
			BITCLK_O_SLV: out std_logic;
			DATA_I_SLV:   in  std_logic;
			DATA_O_SLV:   out std_logic
		);
	end component;
	
	constant p_wb_offset_low: std_logic_vector(31 downto 0) := X"00000000";
	constant p_wb_offset_hi:  std_logic_vector(31 downto 0) := X"3FFFFFFF";
	constant p_wb_i2c_low:    std_logic_vector(31 downto 0) := p_wb_offset_low + X"00000040";
	constant p_wb_i2c_hi:     std_logic_vector(31 downto 0) := p_wb_offset_low + X"0000007F";
	
--	signal mdio_out: std_logic;
--	signal mdio_oen: std_logic;
--	signal mdio_in:  std_logic;
	
	signal wb_clk:     std_logic;
	signal wb_rst:     std_logic;
	signal wb_cyc:     std_logic;
	signal wb_stb:     std_logic;
	signal wb_adr:     std_logic_vector(31 downto 0);
	signal wb_sel:     std_logic_vector(3 downto 0);
	signal wb_we:      std_logic;
	signal wb_dati:    std_logic_vector(31 downto 0);
	signal wb_dato:    std_logic_vector(31 downto 0);
	signal wb_ack:     std_logic;
	signal wb_ack_dff: std_logic;
	signal wb_err:     std_logic;
	signal wb_rty:     std_logic;
	
	-- i2c
	signal cyc_i_i2c: std_logic;
	signal stb_i_i2c: std_logic;
	signal we_i_i2c:  std_logic;
	signal adr_i_i2c: std_logic_vector(2 downto 0);
	signal dat_o_i2c: std_logic_vector(7 downto 0);
	signal ack_o_i2c: std_logic;
	signal sel_i_i2c: std_logic;
	signal inta_i2c:  std_logic;
	
	signal rstn:    std_logic;
	signal scl_i:   std_logic;
	signal scl_o:   std_logic;
	signal scl_oen: std_logic;
	signal sda_i:   std_logic;
	signal sda_o:   std_logic;
	signal sda_oen: std_logic;
	
	begin
		rstn <= RST;
		
		u0 : candy_avb_test_qsys port map (
				clk_clk                           => CLK,                   --                         clk.clk
				codec_clk_clk                     => CODEC_CLKOUT,          --                   codec_clk.clk
				codec_reset_export                => CODEC_RESET,           --                 codec_reset.export
			-- codec_scl_export                  => CODEC_SCL,             --                   codec_scl.export
			-- codec_sda_export                  => CODEC_SDA,             --                   codec_sda.export
--				eth_mii_rx_d                      => ETH_MII_RXD,           --                         eth.mii_rx_d
--				eth_mii_rx_dv                     => ETH_MII_RX_DV,         --                            .mii_rx_dv
--				eth_mii_rx_err                    => ETH_MII_RX_ER,         --                            .mii_rx_err
--				eth_mii_tx_d                      => ETH_MII_TXD,           --                            .mii_tx_d
--				eth_mii_tx_en                     => ETH_MII_TX_EN,         --                            .mii_tx_en
--			-- eth_mii_tx_err                    => ETH_MII_TX_ER,         --                            .mii_tx_err*/
--				eth_mii_crs                       => ETH_MII_CRS,           --                            .mii_crs
--				eth_mii_col                       => ETH_MII_COL,           --                            .mii_col
				eth_clk_clk                       => ETH_CLKOUT,            --                     eth_clk.clk
--				eth_interrupt_export              => ETH_INTERRUPT,         --               eth_interrupt.export
--				eth_mdio_mdc                      => ETH_MDC,               --                    eth_mdio.mdc
--				eth_mdio_mdio_in                  => mdio_in,               --                            .mdio_in
--				eth_mdio_mdio_out                 => mdio_out,              --                            .mdio_out
--				eth_mdio_mdio_oen                 => mdio_oen,              --                            .mdio_oen
--				eth_rx_clk_clk                    => ETH_RX_CLK,            --                  eth_rx_clk.clk
--				eth_tx_clk_clk                    => ETH_TX_CLK,            --                  eth_tx_clk.clk
				new_sdram_controller_0_wire_addr  => DRAM_ADDR,             -- new_sdram_controller_0_wire.addr
				new_sdram_controller_0_wire_ba    => DRAM_BA,               --                            .ba
				new_sdram_controller_0_wire_cas_n => DRAM_CAS,              --                            .cas_n
				new_sdram_controller_0_wire_cke   => DRAM_CKE,              --                            .cke
				new_sdram_controller_0_wire_cs_n  => DRAM_CS,               --                            .cs_n
				new_sdram_controller_0_wire_dq    => DRAM_DQ,               --                            .dq
			 --new_sdram_controller_0_wire_dqm   => DRAM_UDQM & DRAM_LDQM, --                            .dqm
				new_sdram_controller_0_wire_dqm(1) => DRAM_UDQM,            --                            .dqm
				new_sdram_controller_0_wire_dqm(0) => DRAM_LDQM,            --                            .dqm
				new_sdram_controller_0_wire_ras_n => DRAM_RAS,              --                            .ras_n
				new_sdram_controller_0_wire_we_n  => DRAM_WE,               --                            .we_n
				reset_reset_n                     => RST,                   --                       reset.reset_n
				sdclk_clk_clk                     => DRAM_CLK,              --                   sdclk_clk.clk
				uart0_rxd                         => UART_RX,               --                       uart0.rxd
				uart0_txd                         => UART_TX,               --                            .txd
				uart0_cts_n                       => UART_CTS,              --                            .cts_n
				uart0_rts_n                       => UART_RTS,              --                            .rts_n
				user_led_export                   => LED,                   --                    user_led.export
				wb_clk_o                          => wb_clk,                --                          wb.clk
				wb_rst_o                          => wb_rst,                --                            .rst
				wb_cyc_o                          => wb_cyc,                --                            .cyc
				wb_stb_o                          => wb_stb,                --                            .stb
				wb_adr_o                          => wb_adr,                --                            .adr
				wb_sel_o                          => wb_sel,                --                            .sel
				wb_we_o                           => wb_we,                 --                            .we
				wb_dat_o                          => wb_dati,              --                            .dat_o
				wb_dat_i                          => wb_dato,              --                            .dat_i
				wb_ack_i                          => wb_ack,                --                            .ack
				wb_err_i                          => wb_err,                --                            .err
				wb_rty_i                          => wb_rty                 --                            .rty
			);
			
		u1 : i2c_master_top generic map (ARST_LVL => '0') port map (
				wb_clk_i  => wb_clk,
            wb_rst_i  => wb_rst,
            arst_i    => rstn,
            wb_adr_i  => adr_i_i2c,
				wb_dat_i  => wb_dati(7 downto 0),
				wb_dat_o  => dat_o_i2c,
            wb_we_i   => we_i_i2c,
            wb_stb_i  => stb_i_i2c,
            wb_cyc_i  => cyc_i_i2c,
            wb_ack_o  => ack_o_i2c,
            wb_inta_o => inta_i2c,

            -- i2c lines
            scl_pad_i    => scl_i,
            scl_pad_o    => scl_o,
            scl_padoen_o => scl_oen,
            sda_pad_i    => sda_i,
            sda_pad_o    => sda_o,
            sda_padoen_o => sda_oen
			);
	
		u2 : i2s_to_codec generic map (DATA_WIDTH => 32) port map (
				RESET        => RST,
				LRCLK_I_MST  => CODEC_LRCLOCK(0),
				BITCLK_I_MST => CODEC_BITCLOCK(0),
				DATA_I_MST   => CODEC_DATA_OUT(0),
				DATA_O_MST   => CODEC_DATA_IN(0),
				LRCLK_O_SLV  => CODEC_LRCLOCK(1),
				BITCLK_O_SLV => CODEC_BITCLOCK(1),
				DATA_I_SLV   => CODEC_DATA_OUT(1),
				DATA_O_SLV   => CODEC_DATA_IN(1)
			);
		
		-- I2C
		sel_i_i2c <= '1' when ((wb_adr >= p_wb_i2c_low) and (wb_adr <= p_wb_i2c_hi)) else '0';
		cyc_i_i2c <= wb_cyc when (sel_i_i2c = '1') else '0';
		stb_i_i2c <= wb_stb when (sel_i_i2c = '1') else '0';
		adr_i_i2c <= wb_adr(4 downto 2);
		we_i_i2c  <= wb_we when (sel_i_i2c = '1') else '0';
		
		wb_dato <= (dat_o_i2c & dat_o_i2c & dat_o_i2c & dat_o_i2c) when (sel_i_i2c = '1') else X"00000000";
		
		wb_ack <= '1' when (wb_ack_dff = '1') else
					 ack_o_i2c when (sel_i_i2c = '1') else
					 wb_stb;
		
		process(wb_clk, rstn)
		begin
			if rstn = '0' then
				wb_ack_dff <= '0';
			elsif (wb_clk'event and wb_clk = '1') then
				if ((sel_i_i2c = '1') and (ack_o_i2c = '1')) then
					wb_ack_dff <= '1';
				else
					if (wb_stb = '0') then
						wb_ack_dff <= '0';
					else
						wb_ack_dff <= wb_ack_dff;
					end if;
				end if;
			end if;
		end process;
					 
		wb_err <= '0';
		wb_rty <= '0';
	
		-- MDIO
--		mdio_in <= ETH_MDIO;
--		ETH_MDIO <= mdio_out when mdio_oen = '0' else 'Z';

		-- I2C
		CODEC_SCL <= scl_o when (scl_oen = '0') else 'Z';
		CODEC_SDA <= sda_o when (sda_oen = '0') else 'Z';
		scl_i <= CODEC_SCL;
		sda_i <= CODEC_SDA;
end RTL;