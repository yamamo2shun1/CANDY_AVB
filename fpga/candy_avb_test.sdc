create_clock -name CLK -period 20.833 [get_ports {CLK}]

create_clock -name ETH_RX_CLK -period 40.000 [get_ports {ETH_RX_CLK}]
create_clock -name ETH_TX_CLK -period 40.000 [get_ports {ETH_TX_CLK}]

#altpll
#sdclk_clk
#codec_clk
#eth_clk
#create_generated_clock -name ETH_CLK -source [get_ports {CLK}]

derive_pll_clocks
derive_clock_uncertainty

#set ETH_MII_tco_max 10.0
#set ETH_MII_tco_min 10.0
set ETH_MII_tco_max 24.0
set ETH_MII_tco_min 16.0
#set ETH_MII_tsu 12.0
#set ETH_MII_th 5.0
set ETH_MII_tsu 13.7
set ETH_MII_th 4.0

#set_input_delay  -clock {CLK} 1 [all_inputs]
#set_output_delay -clock {CLK} 1 [all_outputs]

set_input_delay -clock {ETH_RX_CLK} -max $ETH_MII_tco_max [get_ports {ETH_MII_RXD* ETH_MII_RX_DV ETH_MII_RX_ER}]
set_input_delay -clock {ETH_RX_CLK} -min $ETH_MII_tco_min [get_ports {ETH_MII_RXD* ETH_MII_RX_DV ETH_MII_RX_ER}] -add_delay
set_output_delay -clock {ETH_TX_CLK} -max $ETH_MII_tsu [get_ports {ETH_MII_TXD* ETH_MII_TX_EN}]
set_output_delay -clock {ETH_TX_CLK} -min $ETH_MII_th  [get_ports {ETH_MII_TXD* ETH_MII_TX_EN}] -add_delay