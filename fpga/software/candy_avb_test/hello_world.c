/*
 * "Hello World" example.
 *
 * This example prints 'Hello from Nios II' to the STDOUT stream. It runs on
 * the Nios II 'standard', 'full_featured', 'fast', and 'low_cost' example
 * designs. It runs with or without the MicroC/OS-II RTOS and requires a STDOUT
 * device in your system's hardware.
 * The memory footprint of this hosted application is ~69 kbytes by default
 * using the standard reference design.
 *
 * For a reduced footprint version of this template, and an explanation of how
 * to reduce the memory footprint for a given application, see the
 * "small_hello_world" template.
 *
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>

#include "system.h"
#include "altera_avalon_pio_regs.h"
#include "altera_modular_adc.h"

#include "sys/alt_irq.h"
#include "sys/alt_cache.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "SigmaStudioFW.h"

#include "candy_avb_IC_1.h"
#include "candy_avb_IC_1_PARAM.h"
#include "candy_avb_IC_2.h"
#include "candy_avb_IC_2_PARAM.h"

#include "eth_ocm_regs.h"
#include "eth_ocm_desc.h"

#define mainTASK1_PERIOD (500)
#define mainTASK2_PERIOD (1000)

#define mainTASK1_PRIORITY (tskIDLE_PRIORITY + 2)
#define mainTASK2_PRIORITY (tskIDLE_PRIORITY + 1)

#define EMAC_BASEADDR ETH_OCM_0_BASE
//#define EMAC_BASEADDR (AVALON_WB_BASE + 0x1000)
//#define BUFMEM_BASEADDR DESCRIPTOR_MEMORY_BASE

#define ETH_OCM_PHY_ADDR_PHY_ID1    0x2
#define ETH_OCM_PHY_ADDR_PHY_ID2    0x3

//#define PHY_ADR 0x1F//0x01
#define ETH_MACADDR0 0x70
#define ETH_MACADDR1 0xB3
#define ETH_MACADDR2 0xD5
#define ETH_MACADDR3 0xDF
#define ETH_MACADDR4 0xB0
#define ETH_MACADDR5 0x00

#define ETHHDR_BIAS 0

volatile char pkt[1562];
volatile char test[1024] = {0x0};

char start = 0;
int phy_adr = 0x0;

static void eth_ocm_isr();
static int eth_ocm_read_init();
static int eth_ocm_rx_isr();
static int eth_ocm_wait(int base);

//void testEthernet(void);
static void prvPrintTask1(void *pvParameters);
//static void prvPrintTask2(void *pvParameters);

int init_MAC();

int led_toggle = 0;

int itest = 0;
float ftest = 0.0f;
double dtest = 0.0;

uint32_t adc[2] = {0};
volatile uint32_t adc_busy = 0;

void adc_callback(void *context)
{
	alt_adc_word_read(MODULAR_ADC_0_SAMPLE_STORE_CSR_BASE, adc, 2);
	adc_busy = 0;
}

void adc_init(void)
{
	adc_stop(MODULAR_ADC_0_SEQUENCER_CSR_BASE);
	adc_set_mode_run_once(MODULAR_ADC_0_SEQUENCER_CSR_BASE);
	alt_adc_register_callback
	(
		altera_modular_adc_open(MODULAR_ADC_0_SEQUENCER_CSR_NAME),
		(alt_adc_callback) adc_callback, NULL,
		MODULAR_ADC_0_SAMPLE_STORE_CSR_BASE
	);
}

int main()
{
	//I2C SCL
	//IOWR_ALTERA_AVALON_PIO_DIRECTION(PIO_2_BASE, 1);

	//CODEC RESET
	IOWR_ALTERA_AVALON_PIO_DIRECTION(PIO_4_BASE, 1);
	IOWR_ALTERA_AVALON_PIO_DATA(PIO_4_BASE, 0);
	usleep(1000 * 20);
	IOWR_ALTERA_AVALON_PIO_DATA(PIO_4_BASE, 1);

	//i2c_setup(0x00, 0xC7);
	i2c_setup(0x00, 0xB3);
	//i2c_setup(0x00, 0x31);

	//status = i2c_start(0x70, 0x00);
	default_download_IC_1();
	default_download_IC_2();

	usleep(1000);
	IOWR_ALTERA_AVALON_PIO_DIRECTION(PIO_4_BASE, 0);

	adc_init();
	adc_busy = 1;
	adc_start(MODULAR_ADC_0_SEQUENCER_CSR_BASE);

	itest++;
	ftest = 3.14f;
	dtest = 3.141594;

	printf("Hello from Nios II! %d %f %lf\n", itest, ftest, dtest);

#if 1
	init_MAC();

    xTaskCreate(prvPrintTask1, "Task1", configMINIMAL_STACK_SIZE, NULL, mainTASK1_PRIORITY, NULL);

  	vTaskStartScheduler();
#endif

  	while (true)
  	{
  		if (adc_busy == 0)
  		{
  			printf("adc = {%lu, %lu}", adc[0], adc[1]);

  			adc_busy = 1;
  			adc_start(MODULAR_ADC_0_SEQUENCER_CSR_BASE);
  		}

#if 1
  		for (int i = 0; i < 1000; i++)
  		{
  			usleep(1000);
  		}
#endif
  	}

  	return 0;
}

static void eth_ocm_isr()
{
	int result;

	result = IORD_ETH_OCM_INT_SOURCE(EMAC_BASEADDR);

	IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, 0x00);

	while (result)
	{
		IOWR_ETH_OCM_INT_SOURCE(EMAC_BASEADDR, result);

		if (result & (ETH_OCM_INT_MASK_RXB_MSK | ETH_OCM_INT_MASK_RXE_MSK))
		{
			eth_ocm_rx_isr();
		}

		if (result & (ETH_OCM_INT_MASK_TXB_MSK | ETH_OCM_INT_MASK_TXE_MSK))
		{
		}

		result = IORD_ETH_OCM_INT_SOURCE(EMAC_BASEADDR);
	}
}

static int eth_ocm_read_init()
{
	alt_u8 *buf_ptr;

	buf_ptr = (alt_u8*)alt_remap_cached((volatile void*)pkt, 4);
	buf_ptr = (alt_u8*)(((unsigned int)buf_ptr) + ETHHDR_BIAS);

	if (!(IORD_ETH_OCM_DESC_CTRL(EMAC_BASEADDR, 1) & ETH_OCM_RXDESC_EMPTY_MSK))
	{
#if 1
		IOWR_ETH_OCM_DESC_PTR(EMAC_BASEADDR, 1, (alt_u32)buf_ptr);
		IOWR_ETH_OCM_DESC_CTRL(EMAC_BASEADDR, 1, ETH_OCM_RXDESC_EMPTY_MSK |
												 ETH_OCM_RXDESC_IRQ_MSK |
												 ETH_OCM_RXDESC_WRAP_MSK);
#else
		IOWR_ETH_OCM_DESC_PTR(EMAC_BASEADDR, 1, BUFMEM_BASEADDR + 0x100);
		IOWR_ETH_OCM_DESC_CTRL(EMAC_BASEADDR, 1, ETH_OCM_RXDESC_EMPTY_MSK |
												  ETH_OCM_RXDESC_IRQ_MSK |
												  ETH_OCM_RXDESC_WRAP_MSK);
#endif
	}

	return 0;
}

static int eth_ocm_rx_isr()
{
	alt_u32 stat;
	alt_u8 *buf_ptr;
	int pklen;

	stat = IORD_ETH_OCM_DESC_CTRL(EMAC_BASEADDR, 1);
	while (!(stat & ETH_OCM_RXDESC_EMPTY_MSK))
	{
		pklen = stat & ETH_OCM_RXDESC_LEN_MSK;
		pklen = pklen >> ETH_OCM_RXDESC_LEN_OFST;

		if (pklen < 1550)
		{
			if (!(stat & ETH_OCM_RXDESC_ERROR_MSK))
			{
				start = 1;
			}
		}

		buf_ptr = (alt_u8*)alt_remap_cached((volatile void*)pkt, 4);
		buf_ptr = (alt_u8*)(((unsigned int)buf_ptr) + ETHHDR_BIAS);
		IOWR_ETH_OCM_DESC_PTR(EMAC_BASEADDR, 1, (alt_u32)buf_ptr);
		IOWR_ETH_OCM_DESC_CTRL(EMAC_BASEADDR, 1, ETH_OCM_RXDESC_EMPTY_MSK |
												  ETH_OCM_RXDESC_IRQ_MSK |
												  ETH_OCM_RXDESC_WRAP_MSK);
		stat = IORD_ETH_OCM_DESC_CTRL(EMAC_BASEADDR, 1);
	}

	return 0;
}

void eth_ocm_set_phy_addr(int base, int phyad, int reg)
{
	phyad = (phyad & ETH_OCM_MIIADDRESS_FIAD_MSK) | ((reg << ETH_OCM_MIIADDRESS_RGAD_OFST) & ETH_OCM_MIIADDRESS_RGAD_MSK);
#if 0
	phyad &= ETH_OCM_MIIADDRESS_FIAD_MSK;
	reg = reg << ETH_OCM_MIIADDRESS_RGAD_OFST;
	reg &= ETH_OCM_MIIADDRESS_RGAD_MSK;
	phyad |= reg;
#endif
	printf("phyad = %x", phyad);
	IOWR_ETH_OCM_MIIADDRESS(base, phyad);
}

void eth_ocm_write_phy_reg(int base, int phyad, int reg, int data)
{
	eth_ocm_set_phy_addr(base, phyad, reg);
	IOWR_ETH_OCM_MIITX_DATA(base, data);
	IOWR_ETH_OCM_MIICOMMAND(base, ETH_OCM_MIICOMMAND_WCTRLDATA_MSK);
	eth_ocm_wait(base);
}

int eth_ocm_read_phy_reg(int base, int phyad, int reg)
{
	int result;
	int err = 1;

	eth_ocm_set_phy_addr(base, phyad, reg);
	IOWR_ETH_OCM_MIICOMMAND(base, ETH_OCM_MIICOMMAND_RSTAT_MSK);
	err = eth_ocm_wait(base);
	result = IORD_ETH_OCM_MIIRX_DATA(base);

	return result;
}

static int eth_ocm_wait(int base){
    int temp = 1;
    int i = 0;

    while (temp && i < 1000)
    {
        temp = IORD_ETH_OCM_MIISTATUS(base) & ETH_OCM_MIISTATUS_BUSY_MSK;
        i++;
    }

    return temp;
}

int init_MAC()
{
	uint32_t tmp0 = 0x0;
	uint32_t tmp1 = 0x0;

	int phyid = 0;
	int phyid2 = 0;

	//IOWR(BUFMEM_BASEADDR, 0x0, 0x12345678);
	//uint32_t test = IORD(BUFMEM_BASEADDR, 0x0);

#if 0
	while ((eth_ocm_read_phy_reg(ETH_OCM_0_BASE, PHY_ADR, 0x01) & 0x04) == 0)
	{
		eth_ocm_write_phy_reg(ETH_OCM_0_BASE, PHY_ADR, 0x0, 0xB300);
		usleep(10000);
	}
#endif

	IOWR_ETH_OCM_MIIMODER(EMAC_BASEADDR, 0x200);
	IOWR_ETH_OCM_MIIMODER(EMAC_BASEADDR, 0x000);
	IOWR_ETH_OCM_MIIMODER(EMAC_BASEADDR, 0x064);

	do {
		tmp0 = IORD_ETH_OCM_MIIMODER(EMAC_BASEADDR);
	} while (tmp0 == 0);

	phyid = eth_ocm_read_phy_reg(EMAC_BASEADDR, 0, ETH_OCM_PHY_ADDR_PHY_ID1);
	for (int dat = 0; dat < 0xFF; dat++)
	{
		phyid = eth_ocm_read_phy_reg(EMAC_BASEADDR, dat, ETH_OCM_PHY_ADDR_PHY_ID1);
		phyid2 = eth_ocm_read_phy_reg(EMAC_BASEADDR, dat, ETH_OCM_PHY_ADDR_PHY_ID2);

		if (phyid != phyid2 && (phyid2 != 0xffff)){
			phy_adr = dat;

			printf("phyid1 = %x %d\n", phyid, phyid);
			printf("phyid2 = %x %d\n", phyid2, phyid2);
			printf("\n");

			int oui = phyid << 6;
			oui |= ((phyid2 >> 10) & 0x003F);
			int mdl = ((phyid2 >> 4) & 0x03F);
			int rev = (phyid2 & 0x00F);

			printf("[eth_ocm_phy_init] Found PHY:\n"
					"  Address: 0x0%X\n"
					"  OUI: 0x%X\n"
					"  Model: 0x%X\n"
					"  Rev: 0x%X\n", dat, oui, mdl, rev);

			break;
		}
	}
	printf("search completed.");

	tmp0 = eth_ocm_read_phy_reg(EMAC_BASEADDR, phy_adr, 0x01);
	//while ((tmp0 & 0x04) == 0)
	//{
		eth_ocm_write_phy_reg(EMAC_BASEADDR, phy_adr, 0x00, 0xB300);

	//	tmp0 = eth_ocm_read_phy_reg(ETH_OCM_0_BASE, phy_adr, 0x01);
	//	usleep(1000000);
	//}

	do {
		tmp0 = IORD_ETH_OCM_MIISTATUS(EMAC_BASEADDR) & 0x2;
		printf("writing... %lx\n", tmp0);
	} while (tmp0 != 0);
	printf("MDIO write\n");
	printf("\n");

	IOWR_ETH_OCM_MIIADDRESS(EMAC_BASEADDR, phy_adr | 0x100);
	IOWR_ETH_OCM_MIICOMMAND(EMAC_BASEADDR, 0x2);

	do {
		tmp0 = IORD_ETH_OCM_MIISTATUS(EMAC_BASEADDR) & 0x2;
	} while (tmp0 != 0);

	printf("MDIO read status = ");
	tmp0 = IORD_ETH_OCM_MIIRX_DATA(EMAC_BASEADDR);
	printf("%lx\n", tmp0);
	printf("\n");

	for (int i = ETH_OCM_DESC_START; i < ETH_OCM_DESC_END; i++) {
		IOWR(EMAC_BASEADDR, i, 0);
	}
	IOWR_ETH_OCM_MODER(EMAC_BASEADDR, 0);
	//IOWR_ETH_OCM_TX_BD_NUM(ETH_OCM_0_BASE, 0x40);
	IOWR_ETH_OCM_TX_BD_NUM(EMAC_BASEADDR, 1);

	IOWR_ETH_OCM_INT_MASK(EMAC_BASEADDR, 0x0000007F);
	tmp1 = IORD_ETH_OCM_INT_MASK(EMAC_BASEADDR);
	IOWR_ETH_OCM_INT_SOURCE(EMAC_BASEADDR, 0x0000007F);
	tmp1 = IORD_ETH_OCM_INT_SOURCE(EMAC_BASEADDR);
	IOWR_ETH_OCM_IPGT(EMAC_BASEADDR, 0x15); // 0x12
	IOWR_ETH_OCM_IPGR1(EMAC_BASEADDR, 0x0000000C);
	IOWR_ETH_OCM_IPGR2(EMAC_BASEADDR, 0x00000012);
	IOWR_ETH_OCM_PACKETLEN(EMAC_BASEADDR, 0x00400600);
	IOWR_ETH_OCM_COLLCONF(EMAC_BASEADDR, 0x000F003F);
	IOWR_ETH_OCM_CTRLMODER(EMAC_BASEADDR, 0x0);

	IOWR_ETH_OCM_MAC_ADDR1(EMAC_BASEADDR, ETH_MACADDR0 << 8 | ETH_MACADDR1);
	IOWR_ETH_OCM_MAC_ADDR0(EMAC_BASEADDR, ETH_MACADDR2 << 24 | ETH_MACADDR3 << 16 | ETH_MACADDR4 << 8 | ETH_MACADDR5);

	IOWR_ETH_OCM_MODER(EMAC_BASEADDR, ETH_OCM_MODER_PAD_MSK |
									  ETH_OCM_MODER_PRO_MSK |
									  ETH_OCM_MODER_CRCEN_MSK |
									  ETH_OCM_MODER_RXEN_MSK |
									  ETH_OCM_MODER_TXEN_MSK |
									  ETH_OCM_MODER_FULLD_MSK);

	tmp1 = alt_irq_register(ETH_OCM_0_IRQ, (void *)ETH_OCM_0_BASE, eth_ocm_isr);
	//xTaskCreate(eth_ocm_isr, "Task2", configMINIMAL_STACK_SIZE, NULL, mainTASK2_PRIORITY, NULL);

	eth_ocm_read_init();

#if 0
	do {
		tmp0 = IORD_ETH_OCM_MIISTATUS(EMAC_BASEADDR) & 0x2;
		printf("writing... %lx\n", tmp0);
	} while (tmp0 != 0);
	printf("MDIO write\n");
	printf("\n");

	IOWR_ETH_OCM_MIIADDRESS(EMAC_BASEADDR, phy_adr | 0x100);
	IOWR_ETH_OCM_MIICOMMAND(EMAC_BASEADDR, 0x2);

	do {
		tmp0 = IORD_ETH_OCM_MIISTATUS(EMAC_BASEADDR) & 0x2;
	} while (tmp0 != 0);

	printf("MDIO read status = ");
	tmp0 = IORD_ETH_OCM_MIIRX_DATA(EMAC_BASEADDR);
	printf("%lx\n", tmp0);
	printf("\n");
#endif
}

static void prvPrintTask1(void *pvParameters)
{
	uint32_t tmp0 = 0x0;
	uint32_t tmp1 = 0x0;

	// tx frame ARP
#if 0
	test[0]  = 0xffffffff;
	test[1]  = 0xffff70B3;
	test[2]  = 0xD5DFB000;
	test[3]  = 0x08060001;
	test[4]  = 0x08000604;
	test[5]  = 0x000170B3;
	test[6]  = 0xD5DFB000;
	test[7]  = 0xc0a80001;
	test[8]  = 0x00000000;
	test[9]  = 0x0000c0a8;
	test[10] = 0x00030000;
	test[11] = 0x00000000;
	test[12] = 0x00000000;
	test[13] = 0x00000000;
	test[14] = 0x00000000;
#endif

#if 1
	//Dest Address: ARP => FF-FF-FF-FF-FF-FF
	test[0]  = 0xff;
	test[1]  = 0xff;
	test[2]  = 0xff;
	test[3]  = 0xff;
	test[4]  = 0xff;
	test[5]  = 0xff;

	//Source Address
	test[6]  = 0x70;
	test[7]  = 0xB3;
	test[8]  = 0xD5;
	test[9]  = 0xDF;
	test[10] = 0xB0;
	test[11] = 0x00;

	//Type
	test[12] = 0x08;
	test[13] = 0x06;

	//Hardware Type: Ethernet => 1(fixed)
	test[14] = 0x00;
	test[15] = 0x01;

	//Protocol Type: TCP/IP => 0x0800(fixed)
	test[16] = 0x08;
	test[17] = 0x00;

	//Hardware Length: MAC Address Length => 6(fixed)
	test[18] = 0x06;

	//Protocol Length: IP Address Length => 4(fixed)
	test[19] = 0x04;

	//Operation: ARP Request(1) / ARP Reply(2)
	test[20] = 0x00;
	test[21] = 0x01;

	//Source Hardware Address
	test[22] = 0x70;
	test[23] = 0xB3;
	test[24] = 0xD5;
	test[25] = 0xDF;
	test[26] = 0xB0;
	test[27] = 0x00;

	//Source Protocol Address
	test[28] = 0xc0;
	test[29] = 0xa8;
	test[30] = 0x00;
	test[31] = 0x01;

	//Dest Hardware Address
	test[32] = 0x00;
	test[33] = 0x00;
	test[34] = 0x00;
	test[35] = 0x00;
	test[36] = 0x00;
	test[37] = 0x00;

	//Dest Protocol Address
	test[38] = 0xc0;
	test[39] = 0xa8;
	test[40] = 0x00;
	test[41] = 0x03;

	test[42] = 0x00;
	test[43] = 0x00;
	test[44] = 0x00;
	test[45] = 0x00;
	test[46] = 0x00;
	test[47] = 0x00;
	test[48] = 0x00;
	test[49] = 0x00;
	test[50] = 0x00;
	test[51] = 0x00;
	test[52] = 0x00;
	test[53] = 0x00;
	test[54] = 0x00;
	test[55] = 0x00;
	test[56] = 0x00;
	test[57] = 0x00;
	test[58] = 0x00;
	test[59] = 0x00;
#else
	//Dest Address: ARP => FF-FF-FF-FF-FF-FF
	IOWR(BUFMEM_BASEADDR, 0,  0xFFFFFFFF);
	IOWR(BUFMEM_BASEADDR, 4,  0xFFFF70B3);
	IOWR(BUFMEM_BASEADDR, 8,  0xD5DFB000);
	IOWR(BUFMEM_BASEADDR, 12, 0x08060001);
	IOWR(BUFMEM_BASEADDR, 16, 0x08000604);
	IOWR(BUFMEM_BASEADDR, 20, 0x000170B3);
	IOWR(BUFMEM_BASEADDR, 24, 0xD5DFB000);
	IOWR(BUFMEM_BASEADDR, 28, 0xC0A80001);
	IOWR(BUFMEM_BASEADDR, 32, 0x00000000);
	IOWR(BUFMEM_BASEADDR, 36, 0x0000C0A8);
	IOWR(BUFMEM_BASEADDR, 40, 0x00030000);
	IOWR(BUFMEM_BASEADDR, 44, 0x00000000);
	IOWR(BUFMEM_BASEADDR, 48, 0x00000000);
	IOWR(BUFMEM_BASEADDR, 52, 0x00000000);
	IOWR(BUFMEM_BASEADDR, 56, 0x00000000);
#endif

	//IOWR_ETH_OCM_DESC_CTRL(ETH_OCM_0_BASE, 0, 0x405800); //len
	//tmp0 = IORD_ETH_OCM_DESC_CTRL(ETH_OCM_0_BASE, 0);
	//printf("TXBD1=%08x \n",tmp0);

	//tmp1 = IORD_ETH_OCM_DESC_CTRL(ETH_OCM_0_BASE, 0x40);
	//printf("RXBD1=%08x \n",tmp1);

	//IOWR_ETH_OCM_DESC_CTRL(ETH_OCM_0_BASE, 0, 0x40d800); //tx strat

#if 0
	while (tmp0 != 0)
	{
		tmp0 =  IORD_ETH_OCM_DESC_CTRL(EMAC_BASEADDR, 1) & 0x8000;
	}

	tmp0 = IORD_ETH_OCM_DESC_CTRL(EMAC_BASEADDR, 1);
	printf("RX end   RXBD1=%08x \n",tmp0);
#endif

#if 0
	printf("tx data\n");
	for(int lp = 0; lp < 5; lp++){
		printf("%08x ", test[lp]);
		printf("%08x ", test[lp + 1]);
		printf("%08x ", test[lp + 2]);
		printf("%08x \n", test[lp + 3]);
	}
	printf("rx data\n");
#endif
#if 0
	for(int lp=0; lp < 5; lp++){
		printf("%08x ", IORD(BUFMEM_BASEADDR, 0x1000 + lp * 0x10));
		printf("%08x ", IORD(BUFMEM_BASEADDR, 0x1000 + lp * 0x10 + 0x4));
		printf("%08x ", IORD(BUFMEM_BASEADDR, 0x1000 + lp * 0x10 + 0x8));
		printf("%08x \n", IORD(BUFMEM_BASEADDR, 0x1000 + lp * 0x10 + 0xc));
	}
#endif

#if 1
	for (;;)
	{
		vTaskDelay(mainTASK1_PERIOD / portTICK_PERIOD_MS);

		switch (led_toggle)
		{
		case 0:
			IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, 0x01);
			led_toggle = 1;

			//tmp1 = IORD_ETH_OCM_INT_SOURCE(EMAC_BASEADDR);
			tmp0 = IORD_ETH_OCM_DESC_CTRL(EMAC_BASEADDR, 0);
			tmp1 = IORD_ETH_OCM_MODER(EMAC_BASEADDR);

			if (!(tmp0 & ETH_OCM_TXDESC_READY_MSK))
			{
				printf("address 0 = %lx\n", test);
				alt_u8 *buf = (alt_u8 *)alt_remap_cached((volatile void *)test, 4);
				printf("address 1 = %lx\n", buf);
				printf("size = %d\n", sizeof(test));
				printf("buf = %x %x %x %x\n", buf[0], buf[4], buf[8], buf[12]);

				IOWR_ETH_OCM_DESC_PTR(EMAC_BASEADDR, 0, buf);
				//IOWR_ETH_OCM_DESC_PTR(EMAC_BASEADDR, 0, BUFMEM_BASEADDR);
				IOWR_ETH_OCM_DESC_CTRL(EMAC_BASEADDR, 0, (sizeof(test) << ETH_OCM_TXDESC_LEN_OFST) |
																		  ETH_OCM_TXDESC_READY_MSK |
																		  ETH_OCM_TXDESC_IRQ_MSK |
																		  ETH_OCM_TXDESC_WRAP_MSK
																		  //ETH_OCM_TXDESC_PAD_MSK |
																		  //ETH_OCM_TXDESC_CRC_MSK
													  	  );

				tmp0 = IORD_ETH_OCM_DESC_CTRL(EMAC_BASEADDR, 0);
				printf("TX start TXBD1=%08lx \n",tmp0);

				//tmp1 = IORD_ETH_OCM_DESC_CTRL(EMAC_BASEADDR, 1);

				long timeout_count = 0;
				do {
					tmp0 = IORD_ETH_OCM_DESC_CTRL(EMAC_BASEADDR, 0);
					tmp1 = IORD_ETH_OCM_INT_SOURCE(EMAC_BASEADDR);
					timeout_count++;
				} while ((tmp0 & ETH_OCM_TXDESC_READY_MSK) && timeout_count < 1000000);

				if (timeout_count < 1000000)
				{
					printf("TX succeeded!");
				}
				else
				{
					printf("TX timeout...");
				}

				tmp0 =  IORD_ETH_OCM_DESC_CTRL(EMAC_BASEADDR, 0);
				printf("TX end   TXBD1=%08lx \n",tmp0);
			}
			break;
		case 1:
			IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, 0x02);
			led_toggle = 0;
			break;
		}
	}
#endif
}

#if 0
static void prvPrintTask2(void *pvParameters)
{
	for (;;)
	{
		vTaskDelay(mainTASK2_PERIOD / portTICK_PERIOD_MS);
	}
}
#endif
