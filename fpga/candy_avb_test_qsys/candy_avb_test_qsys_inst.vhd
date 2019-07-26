	component candy_avb_test_qsys is
		port (
			adc_pll_locked_export             : in    std_logic                     := 'X';             -- export
			altpll_locked_export              : out   std_logic;                                        -- export
			clk_clk                           : in    std_logic                     := 'X';             -- clk
			codec_clk_clk                     : out   std_logic;                                        -- clk
			codec_reset_export                : out   std_logic;                                        -- export
			eth_mii_rx_d                      : in    std_logic_vector(3 downto 0)  := (others => 'X'); -- mii_rx_d
			eth_mii_rx_dv                     : in    std_logic                     := 'X';             -- mii_rx_dv
			eth_mii_rx_err                    : in    std_logic                     := 'X';             -- mii_rx_err
			eth_mii_tx_d                      : out   std_logic_vector(3 downto 0);                     -- mii_tx_d
			eth_mii_tx_en                     : out   std_logic;                                        -- mii_tx_en
			eth_mii_tx_err                    : out   std_logic;                                        -- mii_tx_err
			eth_mii_crs                       : in    std_logic                     := 'X';             -- mii_crs
			eth_mii_col                       : in    std_logic                     := 'X';             -- mii_col
			eth_clk_clk                       : out   std_logic;                                        -- clk
			eth_gmii_gmii_rx_d                : in    std_logic_vector(7 downto 0)  := (others => 'X'); -- gmii_rx_d
			eth_gmii_gmii_rx_dv               : in    std_logic                     := 'X';             -- gmii_rx_dv
			eth_gmii_gmii_rx_err              : in    std_logic                     := 'X';             -- gmii_rx_err
			eth_gmii_gmii_tx_d                : out   std_logic_vector(7 downto 0);                     -- gmii_tx_d
			eth_gmii_gmii_tx_en               : out   std_logic;                                        -- gmii_tx_en
			eth_gmii_gmii_tx_err              : out   std_logic;                                        -- gmii_tx_err
			eth_interrupt_export              : inout std_logic                     := 'X';             -- export
			eth_mdio_mdc                      : out   std_logic;                                        -- mdc
			eth_mdio_mdio_in                  : in    std_logic                     := 'X';             -- mdio_in
			eth_mdio_mdio_out                 : out   std_logic;                                        -- mdio_out
			eth_mdio_mdio_oen                 : out   std_logic;                                        -- mdio_oen
			eth_misc_ff_tx_crc_fwd            : in    std_logic                     := 'X';             -- ff_tx_crc_fwd
			eth_misc_ff_tx_septy              : out   std_logic;                                        -- ff_tx_septy
			eth_misc_tx_ff_uflow              : out   std_logic;                                        -- tx_ff_uflow
			eth_misc_ff_tx_a_full             : out   std_logic;                                        -- ff_tx_a_full
			eth_misc_ff_tx_a_empty            : out   std_logic;                                        -- ff_tx_a_empty
			eth_misc_rx_err_stat              : out   std_logic_vector(17 downto 0);                    -- rx_err_stat
			eth_misc_rx_frm_type              : out   std_logic_vector(3 downto 0);                     -- rx_frm_type
			eth_misc_ff_rx_dsav               : out   std_logic;                                        -- ff_rx_dsav
			eth_misc_ff_rx_a_full             : out   std_logic;                                        -- ff_rx_a_full
			eth_misc_ff_rx_a_empty            : out   std_logic;                                        -- ff_rx_a_empty
			eth_rx_clk_clk                    : in    std_logic                     := 'X';             -- clk
			eth_status_set_10                 : in    std_logic                     := 'X';             -- set_10
			eth_status_set_1000               : in    std_logic                     := 'X';             -- set_1000
			eth_status_eth_mode               : out   std_logic;                                        -- eth_mode
			eth_status_ena_10                 : out   std_logic;                                        -- ena_10
			eth_tx_clk_clk                    : in    std_logic                     := 'X';             -- clk
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
			wb_rty_i                          : in    std_logic                     := 'X';             -- rty_i
			avmm_waitrequest                  : in    std_logic                     := 'X';             -- waitrequest
			avmm_readdata                     : in    std_logic_vector(31 downto 0) := (others => 'X'); -- readdata
			avmm_readdatavalid                : in    std_logic                     := 'X';             -- readdatavalid
			avmm_burstcount                   : out   std_logic_vector(0 downto 0);                     -- burstcount
			avmm_writedata                    : out   std_logic_vector(31 downto 0);                    -- writedata
			avmm_address                      : out   std_logic_vector(7 downto 0);                     -- address
			avmm_write                        : out   std_logic;                                        -- write
			avmm_read                         : out   std_logic;                                        -- read
			avmm_byteenable                   : out   std_logic_vector(3 downto 0);                     -- byteenable
			avmm_debugaccess                  : out   std_logic;                                        -- debugaccess
			avmm_clk_clk                      : out   std_logic                                         -- clk
		);
	end component candy_avb_test_qsys;

	u0 : component candy_avb_test_qsys
		port map (
			adc_pll_locked_export             => CONNECTED_TO_adc_pll_locked_export,             --              adc_pll_locked.export
			altpll_locked_export              => CONNECTED_TO_altpll_locked_export,              --               altpll_locked.export
			clk_clk                           => CONNECTED_TO_clk_clk,                           --                         clk.clk
			codec_clk_clk                     => CONNECTED_TO_codec_clk_clk,                     --                   codec_clk.clk
			codec_reset_export                => CONNECTED_TO_codec_reset_export,                --                 codec_reset.export
			eth_mii_rx_d                      => CONNECTED_TO_eth_mii_rx_d,                      --                         eth.mii_rx_d
			eth_mii_rx_dv                     => CONNECTED_TO_eth_mii_rx_dv,                     --                            .mii_rx_dv
			eth_mii_rx_err                    => CONNECTED_TO_eth_mii_rx_err,                    --                            .mii_rx_err
			eth_mii_tx_d                      => CONNECTED_TO_eth_mii_tx_d,                      --                            .mii_tx_d
			eth_mii_tx_en                     => CONNECTED_TO_eth_mii_tx_en,                     --                            .mii_tx_en
			eth_mii_tx_err                    => CONNECTED_TO_eth_mii_tx_err,                    --                            .mii_tx_err
			eth_mii_crs                       => CONNECTED_TO_eth_mii_crs,                       --                            .mii_crs
			eth_mii_col                       => CONNECTED_TO_eth_mii_col,                       --                            .mii_col
			eth_clk_clk                       => CONNECTED_TO_eth_clk_clk,                       --                     eth_clk.clk
			eth_gmii_gmii_rx_d                => CONNECTED_TO_eth_gmii_gmii_rx_d,                --                    eth_gmii.gmii_rx_d
			eth_gmii_gmii_rx_dv               => CONNECTED_TO_eth_gmii_gmii_rx_dv,               --                            .gmii_rx_dv
			eth_gmii_gmii_rx_err              => CONNECTED_TO_eth_gmii_gmii_rx_err,              --                            .gmii_rx_err
			eth_gmii_gmii_tx_d                => CONNECTED_TO_eth_gmii_gmii_tx_d,                --                            .gmii_tx_d
			eth_gmii_gmii_tx_en               => CONNECTED_TO_eth_gmii_gmii_tx_en,               --                            .gmii_tx_en
			eth_gmii_gmii_tx_err              => CONNECTED_TO_eth_gmii_gmii_tx_err,              --                            .gmii_tx_err
			eth_interrupt_export              => CONNECTED_TO_eth_interrupt_export,              --               eth_interrupt.export
			eth_mdio_mdc                      => CONNECTED_TO_eth_mdio_mdc,                      --                    eth_mdio.mdc
			eth_mdio_mdio_in                  => CONNECTED_TO_eth_mdio_mdio_in,                  --                            .mdio_in
			eth_mdio_mdio_out                 => CONNECTED_TO_eth_mdio_mdio_out,                 --                            .mdio_out
			eth_mdio_mdio_oen                 => CONNECTED_TO_eth_mdio_mdio_oen,                 --                            .mdio_oen
			eth_misc_ff_tx_crc_fwd            => CONNECTED_TO_eth_misc_ff_tx_crc_fwd,            --                    eth_misc.ff_tx_crc_fwd
			eth_misc_ff_tx_septy              => CONNECTED_TO_eth_misc_ff_tx_septy,              --                            .ff_tx_septy
			eth_misc_tx_ff_uflow              => CONNECTED_TO_eth_misc_tx_ff_uflow,              --                            .tx_ff_uflow
			eth_misc_ff_tx_a_full             => CONNECTED_TO_eth_misc_ff_tx_a_full,             --                            .ff_tx_a_full
			eth_misc_ff_tx_a_empty            => CONNECTED_TO_eth_misc_ff_tx_a_empty,            --                            .ff_tx_a_empty
			eth_misc_rx_err_stat              => CONNECTED_TO_eth_misc_rx_err_stat,              --                            .rx_err_stat
			eth_misc_rx_frm_type              => CONNECTED_TO_eth_misc_rx_frm_type,              --                            .rx_frm_type
			eth_misc_ff_rx_dsav               => CONNECTED_TO_eth_misc_ff_rx_dsav,               --                            .ff_rx_dsav
			eth_misc_ff_rx_a_full             => CONNECTED_TO_eth_misc_ff_rx_a_full,             --                            .ff_rx_a_full
			eth_misc_ff_rx_a_empty            => CONNECTED_TO_eth_misc_ff_rx_a_empty,            --                            .ff_rx_a_empty
			eth_rx_clk_clk                    => CONNECTED_TO_eth_rx_clk_clk,                    --                  eth_rx_clk.clk
			eth_status_set_10                 => CONNECTED_TO_eth_status_set_10,                 --                  eth_status.set_10
			eth_status_set_1000               => CONNECTED_TO_eth_status_set_1000,               --                            .set_1000
			eth_status_eth_mode               => CONNECTED_TO_eth_status_eth_mode,               --                            .eth_mode
			eth_status_ena_10                 => CONNECTED_TO_eth_status_ena_10,                 --                            .ena_10
			eth_tx_clk_clk                    => CONNECTED_TO_eth_tx_clk_clk,                    --                  eth_tx_clk.clk
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
			wb_rty_i                          => CONNECTED_TO_wb_rty_i,                          --                            .rty_i
			avmm_waitrequest                  => CONNECTED_TO_avmm_waitrequest,                  --                        avmm.waitrequest
			avmm_readdata                     => CONNECTED_TO_avmm_readdata,                     --                            .readdata
			avmm_readdatavalid                => CONNECTED_TO_avmm_readdatavalid,                --                            .readdatavalid
			avmm_burstcount                   => CONNECTED_TO_avmm_burstcount,                   --                            .burstcount
			avmm_writedata                    => CONNECTED_TO_avmm_writedata,                    --                            .writedata
			avmm_address                      => CONNECTED_TO_avmm_address,                      --                            .address
			avmm_write                        => CONNECTED_TO_avmm_write,                        --                            .write
			avmm_read                         => CONNECTED_TO_avmm_read,                         --                            .read
			avmm_byteenable                   => CONNECTED_TO_avmm_byteenable,                   --                            .byteenable
			avmm_debugaccess                  => CONNECTED_TO_avmm_debugaccess,                  --                            .debugaccess
			avmm_clk_clk                      => CONNECTED_TO_avmm_clk_clk                       --                    avmm_clk.clk
		);

