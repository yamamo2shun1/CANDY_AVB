
module candy_avb_test_qsys (
	clk_clk,
	codec_clk_clk,
	eth_mii_rx_d,
	eth_mii_rx_dv,
	eth_mii_rx_err,
	eth_mii_tx_d,
	eth_mii_tx_en,
	eth_mii_tx_err,
	eth_mii_crs,
	eth_mii_col,
	eth_clk_clk,
	eth_interrupt_export,
	eth_mdio_mdc,
	eth_mdio_mdio_in,
	eth_mdio_mdio_out,
	eth_mdio_mdio_oen,
	eth_misc_ff_tx_crc_fwd,
	eth_misc_ff_tx_septy,
	eth_misc_tx_ff_uflow,
	eth_misc_ff_tx_a_full,
	eth_misc_ff_tx_a_empty,
	eth_misc_rx_err_stat,
	eth_misc_rx_frm_type,
	eth_misc_ff_rx_dsav,
	eth_misc_ff_rx_a_full,
	eth_misc_ff_rx_a_empty,
	eth_status_set_10,
	eth_status_set_1000,
	eth_status_eth_mode,
	eth_status_ena_10,
	new_sdram_controller_0_wire_addr,
	new_sdram_controller_0_wire_ba,
	new_sdram_controller_0_wire_cas_n,
	new_sdram_controller_0_wire_cke,
	new_sdram_controller_0_wire_cs_n,
	new_sdram_controller_0_wire_dq,
	new_sdram_controller_0_wire_dqm,
	new_sdram_controller_0_wire_ras_n,
	new_sdram_controller_0_wire_we_n,
	reset_reset_n,
	sdclk_clk_clk,
	uart0_rxd,
	uart0_txd,
	uart0_cts_n,
	uart0_rts_n,
	user_led_export,
	eth_rx_clk_clk,
	eth_tx_clk_clk);	

	input		clk_clk;
	output		codec_clk_clk;
	input	[3:0]	eth_mii_rx_d;
	input		eth_mii_rx_dv;
	input		eth_mii_rx_err;
	output	[3:0]	eth_mii_tx_d;
	output		eth_mii_tx_en;
	output		eth_mii_tx_err;
	input		eth_mii_crs;
	input		eth_mii_col;
	output		eth_clk_clk;
	inout		eth_interrupt_export;
	output		eth_mdio_mdc;
	input		eth_mdio_mdio_in;
	output		eth_mdio_mdio_out;
	output		eth_mdio_mdio_oen;
	input		eth_misc_ff_tx_crc_fwd;
	output		eth_misc_ff_tx_septy;
	output		eth_misc_tx_ff_uflow;
	output		eth_misc_ff_tx_a_full;
	output		eth_misc_ff_tx_a_empty;
	output	[17:0]	eth_misc_rx_err_stat;
	output	[3:0]	eth_misc_rx_frm_type;
	output		eth_misc_ff_rx_dsav;
	output		eth_misc_ff_rx_a_full;
	output		eth_misc_ff_rx_a_empty;
	input		eth_status_set_10;
	input		eth_status_set_1000;
	output		eth_status_eth_mode;
	output		eth_status_ena_10;
	output	[11:0]	new_sdram_controller_0_wire_addr;
	output	[1:0]	new_sdram_controller_0_wire_ba;
	output		new_sdram_controller_0_wire_cas_n;
	output		new_sdram_controller_0_wire_cke;
	output		new_sdram_controller_0_wire_cs_n;
	inout	[15:0]	new_sdram_controller_0_wire_dq;
	output	[1:0]	new_sdram_controller_0_wire_dqm;
	output		new_sdram_controller_0_wire_ras_n;
	output		new_sdram_controller_0_wire_we_n;
	input		reset_reset_n;
	output		sdclk_clk_clk;
	input		uart0_rxd;
	output		uart0_txd;
	input		uart0_cts_n;
	output		uart0_rts_n;
	output	[1:0]	user_led_export;
	input		eth_rx_clk_clk;
	input		eth_tx_clk_clk;
endmodule
