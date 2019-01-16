# CANDY_AVB
## Description
This board is adopted Intel MAX10 series FPGA and Analog Devices SigmaDSP audio codec and then aimed at easier to prototyping and studying with a FPGA based on an audio signal and an ethernet.

A FPGA board in the market is too simple(it has only GPIO ports and
we have to make extended board by ourselves) or too high functionality
(DDR memory, HDMI, Gigabit ethernet...).

**I wanted simple FPGA board for sound and music.** So I started to develop this board.

## Specifications
* FPGA: Intel 10M16SAU(MAX10) x1
* SDRAM: Alliance Memory 64Mb(AS4C4M16SA-7BCN)
* ETH: TI DP83848 x1
* CODEC: Analog Devices ADAU1761 x2
* USB Serial: Silabs CP2102N x1
* CLOCK: 48MHz Crystal

## PCB
![CANBY_AVB PCB_TOP](https://github.com/tkrworks/CANDY_AVB/blob/master/candy_avb_top_pic.jpg "CANDY_AVB PCB TOP")
![CANBY_AVB PCB_BOTTOM](https://github.com/tkrworks/CANDY_AVB/blob/master/candy_avb_bottom_pic.jpg "CANDY_AVB PCB BOTTOM")

## Board
![CANBY_AVB Board](https://github.com/tkrworks/CANDY_AVB/blob/master/candy_avb_brd.png "CANDY_AVB Board")

## Block Diagram
![CANBY_AVB BLOCK_DIAGRAM](https://github.com/tkrworks/CANDY_AVB/blob/master/block_diagram.png "CANDY_AVB BLOCK DIAGRAM")

## License
<a rel="license" href="http://creativecommons.org/licenses/by/4.0/"><img alt="Creative Commons License" style="border-width:0" src="https://i.creativecommons.org/l/by/4.0/88x31.png" /></a><br /><span xmlns:dct="http://purl.org/dc/terms/" property="dct:title">CANDY_AVB</span> by <span xmlns:cc="http://creativecommons.org/ns#" property="cc:attributionName">Shunichi Yamamoto</span> is licensed under a <a rel="license" href="http://creativecommons.org/licenses/by/4.0/">Creative Commons Attribution 4.0 International License</a>.
