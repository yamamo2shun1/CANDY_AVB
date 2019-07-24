#ifdef ALT_INICHE
    #include "ipport.h"
#endif

#include "system.h"
#include "altera_avalon_tse.h"
#include "altera_avalon_tse_system_info.h"

#define ADDITIONAL_PHY_CFG  0

alt_tse_system_info tse_mac_device[MAXNETS] = {
			TSE_SYSTEM_EXT_MEM_NO_SHARED_FIFO(ETH_TSE_0, 0, MSGDMA_TX, MSGDMA_RX, TSE_PHY_AUTO_ADDRESS, ADDITIONAL_PHY_CFG, DESCRIPTOR_MEMORY)
};
