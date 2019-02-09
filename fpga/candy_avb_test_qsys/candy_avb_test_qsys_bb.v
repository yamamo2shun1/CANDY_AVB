
module candy_avb_test_qsys (
	clk_clk,
	codec_clk_clk,
	codec_reset_export,
	eth_clk_clk,
	eth_interrupt_export,
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
	wb_clk_o,
	wb_rst_o,
	wb_cyc_o,
	wb_stb_o,
	wb_adr_o,
	wb_sel_o,
	wb_we_o,
	wb_dat_o,
	wb_dat_i,
	wb_ack_i,
	wb_err_i,
	wb_rty_i);	

	input		clk_clk;
	output		codec_clk_clk;
	output		codec_reset_export;
	output		eth_clk_clk;
	inout		eth_interrupt_export;
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
	output		wb_clk_o;
	output		wb_rst_o;
	output		wb_cyc_o;
	output		wb_stb_o;
	output	[31:0]	wb_adr_o;
	output	[3:0]	wb_sel_o;
	output		wb_we_o;
	output	[31:0]	wb_dat_o;
	input	[31:0]	wb_dat_i;
	input		wb_ack_i;
	input		wb_err_i;
	input		wb_rty_i;
endmodule
