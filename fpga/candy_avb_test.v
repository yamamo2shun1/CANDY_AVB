module CANDY_AVB (
  /* CLOCK & RESET */
  input  CLK, RST,

  /* CLKOUT */
  output CODEC_CLKOUT,
  output ETH_CLKOUT,
  
  /* LED */
  output [1:0] LED,

  /* UART - CP2102N */
  output UART_TX,
  input  UART_RX,
  output UART_RTS,
  input  UART_CTS,
  
  /* A/D Converter */
  input [8:0] ADC_IN,
  
  /* 3.3V <-> 5V GPIO */
  output LS_OE,
  inout [7:0] LS_A,
  
  /* 3.3V GPIO */
  inout GPIO_M1,
  inout GPIO_M2,
  inout GPIO_L3,
  inout GPIO_N2,
  
  /* I2S - ADAU1761 x2 */
  inout        CODEC_SCL,
  inout        CODEC_SDA,
  output       CODEC_RESET,
  output [1:0] CODEC_BITCLOCK,
  output [1:0] CODEC_LRCLOCK,
  input  [1:0] CODEC_DATA_OUT,
  output [1:0] CODEC_DATA_IN,

  /* Ethernet DP83848 */
  inout        ETH_INTRRUPT,
  output       ETH_MDC,
  inout        ETH_MDIO,
  input        ETH_MII_CRS,
  input        ETH_MII_COL,
  input        ETH_RX_CLK,
  input        ETH_TX_CLK,
  input        ETH_MII_RX_DV,
  input        ETH_MII_RX_ER,
  output       ETH_MII_TX_EN,
  output [3:0] ETH_MII_TXD,
  input  [3:0] ETH_MII_RXD,
  
  /* SDRAM */
  output DRAM_CLK, DRAM_CKE,
  output [11:0] DRAM_ADDR,
  output [1:0] DRAM_BA,
  output DRAM_CAS, DRAM_RAS,
  output DRAM_CS, DRAM_WE,
  output DRAM_UDQM, DRAM_LDQM,
  inout [15:0] DRAM_DQ
);

wire mdio_out;
wire mdio_oen;

assign mdio_in = ETH_MDIO;
assign ETH_MDIO = (!mdio_oen) ? mdio_out : 1'bZ;

/*
always @* begin
   LED0 = USER_SW;
   LED1 = 1;
end
*/

candy_avb_test_qsys u0 (
  .clk_clk                           (CLK),
  .reset_reset_n                     (RST),
  .sdclk_clk_clk                     (DRAM_CLK),
  .new_sdram_controller_0_wire_addr  (DRAM_ADDR),
  .new_sdram_controller_0_wire_ba    (DRAM_BA),
  .new_sdram_controller_0_wire_cas_n (DRAM_CAS),
  .new_sdram_controller_0_wire_cke   (DRAM_CKE),
  .new_sdram_controller_0_wire_cs_n  (DRAM_CS),
  .new_sdram_controller_0_wire_dq    (DRAM_DQ),
  .new_sdram_controller_0_wire_dqm   ({DRAM_UDQM, DRAM_LDQM}),
  .new_sdram_controller_0_wire_ras_n (DRAM_RAS),
  .new_sdram_controller_0_wire_we_n  (DRAM_WE),
  .user_led_export                   (LED),
  .codec_scl_export                  (CODEC_SCL),
  .codec_sda_export                  (CODEC_SDA),
  .codec_clk_clk                     (CODEC_CLKOUT),
  .codec_reset_export                (CODEC_RESET),
  .eth_clk_clk                       (ETH_CLKOUT),
  .eth_tx_clk_clk                    (ETH_TX_CLK),
  .eth_rx_clk_clk                    (ETH_RX_CLK),
  .uart0_txd                         (UART_TX),
  .uart0_rxd                         (UART_RX),
  .uart0_rts_n                       (UART_RTS),
  .uart0_cts_n                       (UART_CTS),
  .eth_interrupt_export              (ETH_INTERRUPT),
  .eth_mdio_mdc                      (ETH_MDC),
  .eth_mdio_mdio_in                  (mdio_in),
  .eth_mdio_mdio_out                 (mdio_out),
  .eth_mdio_mdio_oen                 (mdio_oen),
  .eth_mii_rx_d                      (ETH_MII_RXD),
  .eth_mii_rx_dv                     (ETH_MII_RX_DV),
  .eth_mii_rx_err                    (ETH_MII_RX_ER),
  .eth_mii_tx_d                      (ETH_MII_TXD),
  .eth_mii_tx_en                     (ETH_MII_TX_EN),
  //.eth_mii_tx_err                    (),
  .eth_mii_crs                       (ETH_MII_CRS), 
  .eth_mii_col                       (ETH_MII_COL)
);

endmodule
