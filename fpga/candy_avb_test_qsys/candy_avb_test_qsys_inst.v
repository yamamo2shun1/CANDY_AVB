	candy_avb_test_qsys u0 (
		.clk_clk                           (<connected-to-clk_clk>),                           //                         clk.clk
		.codec_clk_clk                     (<connected-to-codec_clk_clk>),                     //                   codec_clk.clk
		.codec_reset_export                (<connected-to-codec_reset_export>),                //                 codec_reset.export
		.eth_clk_clk                       (<connected-to-eth_clk_clk>),                       //                     eth_clk.clk
		.eth_interrupt_export              (<connected-to-eth_interrupt_export>),              //               eth_interrupt.export
		.new_sdram_controller_0_wire_addr  (<connected-to-new_sdram_controller_0_wire_addr>),  // new_sdram_controller_0_wire.addr
		.new_sdram_controller_0_wire_ba    (<connected-to-new_sdram_controller_0_wire_ba>),    //                            .ba
		.new_sdram_controller_0_wire_cas_n (<connected-to-new_sdram_controller_0_wire_cas_n>), //                            .cas_n
		.new_sdram_controller_0_wire_cke   (<connected-to-new_sdram_controller_0_wire_cke>),   //                            .cke
		.new_sdram_controller_0_wire_cs_n  (<connected-to-new_sdram_controller_0_wire_cs_n>),  //                            .cs_n
		.new_sdram_controller_0_wire_dq    (<connected-to-new_sdram_controller_0_wire_dq>),    //                            .dq
		.new_sdram_controller_0_wire_dqm   (<connected-to-new_sdram_controller_0_wire_dqm>),   //                            .dqm
		.new_sdram_controller_0_wire_ras_n (<connected-to-new_sdram_controller_0_wire_ras_n>), //                            .ras_n
		.new_sdram_controller_0_wire_we_n  (<connected-to-new_sdram_controller_0_wire_we_n>),  //                            .we_n
		.reset_reset_n                     (<connected-to-reset_reset_n>),                     //                       reset.reset_n
		.sdclk_clk_clk                     (<connected-to-sdclk_clk_clk>),                     //                   sdclk_clk.clk
		.uart0_rxd                         (<connected-to-uart0_rxd>),                         //                       uart0.rxd
		.uart0_txd                         (<connected-to-uart0_txd>),                         //                            .txd
		.uart0_cts_n                       (<connected-to-uart0_cts_n>),                       //                            .cts_n
		.uart0_rts_n                       (<connected-to-uart0_rts_n>),                       //                            .rts_n
		.user_led_export                   (<connected-to-user_led_export>),                   //                    user_led.export
		.wb_clk_o                          (<connected-to-wb_clk_o>),                          //                          wb.clk_o
		.wb_rst_o                          (<connected-to-wb_rst_o>),                          //                            .rst_o
		.wb_cyc_o                          (<connected-to-wb_cyc_o>),                          //                            .cyc_o
		.wb_stb_o                          (<connected-to-wb_stb_o>),                          //                            .stb_o
		.wb_adr_o                          (<connected-to-wb_adr_o>),                          //                            .adr_o
		.wb_sel_o                          (<connected-to-wb_sel_o>),                          //                            .sel_o
		.wb_we_o                           (<connected-to-wb_we_o>),                           //                            .we_o
		.wb_dat_o                          (<connected-to-wb_dat_o>),                          //                            .dat_o
		.wb_dat_i                          (<connected-to-wb_dat_i>),                          //                            .dat_i
		.wb_ack_i                          (<connected-to-wb_ack_i>),                          //                            .ack_i
		.wb_err_i                          (<connected-to-wb_err_i>),                          //                            .err_i
		.wb_rty_i                          (<connected-to-wb_rty_i>)                           //                            .rty_i
	);

