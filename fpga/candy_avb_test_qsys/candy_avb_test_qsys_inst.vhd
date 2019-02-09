	component candy_avb_test_qsys is
		port (
			clk_clk                           : in    std_logic                     := 'X';             -- clk
			codec_clk_clk                     : out   std_logic;                                        -- clk
			codec_reset_export                : out   std_logic;                                        -- export
			eth_clk_clk                       : out   std_logic;                                        -- clk
			eth_interrupt_export              : inout std_logic                     := 'X';             -- export
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
			wb_clk_o                          : out   std_logic;                                        -- clk_o
			wb_rst_o                          : out   std_logic;                                        -- rst_o
			wb_cyc_o                          : out   std_logic;                                        -- cyc_o
			wb_stb_o                          : out   std_logic;                                        -- stb_o
			wb_adr_o                          : out   std_logic_vector(31 downto 0);                    -- adr_o
			wb_sel_o                          : out   std_logic_vector(3 downto 0);                     -- sel_o
			wb_we_o                           : out   std_logic;                                        -- we_o
			wb_dat_o                          : out   std_logic_vector(31 downto 0);                    -- dat_o
			wb_dat_i                          : in    std_logic_vector(31 downto 0) := (others => 'X'); -- dat_i
			wb_ack_i                          : in    std_logic                     := 'X';             -- ack_i
			wb_err_i                          : in    std_logic                     := 'X';             -- err_i
			wb_rty_i                          : in    std_logic                     := 'X'              -- rty_i
		);
	end component candy_avb_test_qsys;

	u0 : component candy_avb_test_qsys
		port map (
			clk_clk                           => CONNECTED_TO_clk_clk,                           --                         clk.clk
			codec_clk_clk                     => CONNECTED_TO_codec_clk_clk,                     --                   codec_clk.clk
			codec_reset_export                => CONNECTED_TO_codec_reset_export,                --                 codec_reset.export
			eth_clk_clk                       => CONNECTED_TO_eth_clk_clk,                       --                     eth_clk.clk
			eth_interrupt_export              => CONNECTED_TO_eth_interrupt_export,              --               eth_interrupt.export
			new_sdram_controller_0_wire_addr  => CONNECTED_TO_new_sdram_controller_0_wire_addr,  -- new_sdram_controller_0_wire.addr
			new_sdram_controller_0_wire_ba    => CONNECTED_TO_new_sdram_controller_0_wire_ba,    --                            .ba
			new_sdram_controller_0_wire_cas_n => CONNECTED_TO_new_sdram_controller_0_wire_cas_n, --                            .cas_n
			new_sdram_controller_0_wire_cke   => CONNECTED_TO_new_sdram_controller_0_wire_cke,   --                            .cke
			new_sdram_controller_0_wire_cs_n  => CONNECTED_TO_new_sdram_controller_0_wire_cs_n,  --                            .cs_n
			new_sdram_controller_0_wire_dq    => CONNECTED_TO_new_sdram_controller_0_wire_dq,    --                            .dq
			new_sdram_controller_0_wire_dqm   => CONNECTED_TO_new_sdram_controller_0_wire_dqm,   --                            .dqm
			new_sdram_controller_0_wire_ras_n => CONNECTED_TO_new_sdram_controller_0_wire_ras_n, --                            .ras_n
			new_sdram_controller_0_wire_we_n  => CONNECTED_TO_new_sdram_controller_0_wire_we_n,  --                            .we_n
			reset_reset_n                     => CONNECTED_TO_reset_reset_n,                     --                       reset.reset_n
			sdclk_clk_clk                     => CONNECTED_TO_sdclk_clk_clk,                     --                   sdclk_clk.clk
			uart0_rxd                         => CONNECTED_TO_uart0_rxd,                         --                       uart0.rxd
			uart0_txd                         => CONNECTED_TO_uart0_txd,                         --                            .txd
			uart0_cts_n                       => CONNECTED_TO_uart0_cts_n,                       --                            .cts_n
			uart0_rts_n                       => CONNECTED_TO_uart0_rts_n,                       --                            .rts_n
			user_led_export                   => CONNECTED_TO_user_led_export,                   --                    user_led.export
			wb_clk_o                          => CONNECTED_TO_wb_clk_o,                          --                          wb.clk_o
			wb_rst_o                          => CONNECTED_TO_wb_rst_o,                          --                            .rst_o
			wb_cyc_o                          => CONNECTED_TO_wb_cyc_o,                          --                            .cyc_o
			wb_stb_o                          => CONNECTED_TO_wb_stb_o,                          --                            .stb_o
			wb_adr_o                          => CONNECTED_TO_wb_adr_o,                          --                            .adr_o
			wb_sel_o                          => CONNECTED_TO_wb_sel_o,                          --                            .sel_o
			wb_we_o                           => CONNECTED_TO_wb_we_o,                           --                            .we_o
			wb_dat_o                          => CONNECTED_TO_wb_dat_o,                          --                            .dat_o
			wb_dat_i                          => CONNECTED_TO_wb_dat_i,                          --                            .dat_i
			wb_ack_i                          => CONNECTED_TO_wb_ack_i,                          --                            .ack_i
			wb_err_i                          => CONNECTED_TO_wb_err_i,                          --                            .err_i
			wb_rty_i                          => CONNECTED_TO_wb_rty_i                           --                            .rty_i
		);

