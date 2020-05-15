#include <stdio.h>
#include "platform.h"
#include "xil_types.h"
#include "xgpio.h"
#include "xtmrctr.h"
#include "xparameters.h"
#include "xgpiops.h"
#include "xaxidma.h"
#include "xil_io.h"
#include "xil_exception.h"
#include "xscugic.h"

XGpioPs gpio_0_ps;
XGpio gpio_0_pl;
XTmrCtr timer_0;
XScuGic intc;
XAxiDma dma_0_pl;

const int mio_gpio_led_pin = 47;
const int mio_gpio_pb_pin = 51;
const int emio_gpio_pb_pin = 54;

// DMA is just an experiment.
// The conclusion: use it under linux or spend a lot of time developing a framework that works.
// Some known issues here:
// 1.If DMA is working, can't do anything else in main loop (perhaps because of the physical memory
//   address handed to the DMA without malloc?)
// 2.AXI4 last signal - partial packet and the relationship between the size of the buffer and MAX_PKT_LEN
//   requires more thought and understanding.
const u32 MAX_PKT_LEN = 0x04;
const u32 RX_BUFFER_BASE = XPAR_PS7_DDR_0_S_AXI_BASEADDR + 0x00300000;

int interrupt_flag;

extern char inbyte(void);

void Timer_Interrupt_Handler(void *data, u8 timer_ctr_id) {
	XTmrCtr_Stop(data, timer_ctr_id);
	XGpioPs_WritePin(&gpio_0_ps, mio_gpio_led_pin, 1);
	XTmrCtr_Reset(data, timer_ctr_id);
	interrupt_flag = 1;
}

void Action_On_Press(const char pb_name[], XTmrCtr* timer) {
	printf("%s\n", pb_name);
	XGpioPs_WritePin(&gpio_0_ps, mio_gpio_led_pin, 0);
	XTmrCtr_Start(timer, 0);
	while(interrupt_flag != 1);
	interrupt_flag = 0;
}

int Setup_DMA_RX(XAxiDma* dma) {
	const u32 RX_BD_SPACE_BASE = XPAR_PS7_DDR_0_S_AXI_BASEADDR + 0x00001000;
	const u32 RX_BD_SPACE_HIGH = XPAR_PS7_DDR_0_S_AXI_BASEADDR + 0x00001FFF;

	XAxiDma_BdRing *RxRingPtr;
	int Delay = 0;
	int Coalesce = 1;
	int Status;
	XAxiDma_Bd BdTemplate;
	XAxiDma_Bd *BdPtr;
	XAxiDma_Bd *BdCurPtr;
	u32 BdCount;
	u32 FreeBdCount;
	UINTPTR RxBufferPtr;
	int Index;

	RxRingPtr = XAxiDma_GetRxRing(dma);

	/* Disable all RX interrupts before RxBD space setup */
	XAxiDma_BdRingIntDisable(RxRingPtr, XAXIDMA_IRQ_ALL_MASK);

	/* Set delay and coalescing */
	XAxiDma_BdRingSetCoalesce(RxRingPtr, Coalesce, Delay);

	/* Setup Rx BD space */
	BdCount = XAxiDma_BdRingCntCalc(XAXIDMA_BD_MINIMUM_ALIGNMENT, RX_BD_SPACE_HIGH - RX_BD_SPACE_BASE + 1);
	Status = XAxiDma_BdRingCreate(RxRingPtr, RX_BD_SPACE_BASE, RX_BD_SPACE_BASE, XAXIDMA_BD_MINIMUM_ALIGNMENT, BdCount);
	if (Status != XST_SUCCESS) {
		xil_printf("RX create BD ring failed %d\r\n", Status);
		return XST_FAILURE;
	}

	// Setup an all-zero BD as the template for the Rx channel.
	XAxiDma_BdClear(&BdTemplate);
	Status = XAxiDma_BdRingClone(RxRingPtr, &BdTemplate);
	if (Status != XST_SUCCESS) {
		xil_printf("RX clone BD failed %d\r\n", Status);
		return XST_FAILURE;
	}

	/* Attach buffers to RxBD ring so we are ready to receive packets */
	FreeBdCount = XAxiDma_BdRingGetFreeCnt(RxRingPtr);
	Status = XAxiDma_BdRingAlloc(RxRingPtr, FreeBdCount, &BdPtr);
	if (Status != XST_SUCCESS) {
		xil_printf("RX alloc BD failed %d\r\n", Status);
		return XST_FAILURE;
	}

	BdCurPtr = BdPtr;
	RxBufferPtr = RX_BUFFER_BASE;
	for (Index = 0; Index < FreeBdCount; Index++) {
		Status = XAxiDma_BdSetBufAddr(BdCurPtr, RxBufferPtr);
		if (Status != XST_SUCCESS) {
			xil_printf("Set buffer addr %x on BD %x failed %d\r\n", (unsigned int)RxBufferPtr, (UINTPTR)BdCurPtr, Status);
			return XST_FAILURE;
		}
		Status = XAxiDma_BdSetLength(BdCurPtr, MAX_PKT_LEN, RxRingPtr->MaxTransferLen);
		if (Status != XST_SUCCESS) {
			xil_printf("Rx set length %d on BD %x failed %d\r\n", MAX_PKT_LEN, (UINTPTR)BdCurPtr, Status);
			return XST_FAILURE;
		}

		// Receive BDs do not need to set anything for the control The hardware will set the SOF/EOF bits per stream status
		XAxiDma_BdSetCtrl(BdCurPtr, 0);
		XAxiDma_BdSetId(BdCurPtr, RxBufferPtr);
		RxBufferPtr += MAX_PKT_LEN;
		BdCurPtr = (XAxiDma_Bd *)XAxiDma_BdRingNext(RxRingPtr, BdCurPtr);
	}

	// Give to hardware.
	Status = XAxiDma_BdRingToHw(RxRingPtr, FreeBdCount, BdPtr);
	if (Status != XST_SUCCESS) {
		xil_printf("RX submit hw failed %d\r\n", Status);
		return XST_FAILURE;
	}

	/* Start RX DMA channel */
	Status = XAxiDma_BdRingStart(RxRingPtr);
	if (Status != XST_SUCCESS) {
		xil_printf("RX start hw failed %d\r\n", Status);
		return XST_FAILURE;
	}

	return FreeBdCount;
}

void DMA_RX(XAxiDma* dma, u32 num_bds) {
	XAxiDma_Bd *BdPtr;
	XAxiDma_BdRing *RxRingPtr = XAxiDma_GetRxRing(dma);
	int ProcessedBdCount = XAxiDma_BdRingFromHw(RxRingPtr, XAXIDMA_ALL_BDS, &BdPtr);

	// Do everytime for the entire buffer because of speculative prefetching.
	volatile u32* RxPacket;
	RxPacket = (u32*)RX_BUFFER_BASE;
	Xil_DCacheInvalidateRange((UINTPTR)RxPacket, MAX_PKT_LEN * num_bds);

	XAxiDma_Bd *bdHead = BdPtr;
	for (int i = 0; i < ProcessedBdCount; i++) {
		u32 *buf_addr = (u32 *)XAxiDma_BdGetBufAddr(BdPtr);
		xil_printf("RxPacket[%d] = 0x%x\r\n",  i, *buf_addr);
		BdPtr = (XAxiDma_Bd *)XAxiDma_BdRingNext(RxRingPtr, BdPtr);
	}

	/* Free all processed RX BDs for future transmission */
	int Status = XAxiDma_BdRingFree(RxRingPtr, ProcessedBdCount, bdHead);
	if (Status != XST_SUCCESS) {
		xil_printf("Failed to free %d rx BDs %d\r\n", ProcessedBdCount, Status);
	}

	/* Return processed BDs to RX channel so we are ready to receive new
	 * packets:
	 *    - Allocate all free RX BDs
	 *    - Pass the BDs to RX channel
	 */
	u32 FreeBdCount = XAxiDma_BdRingGetFreeCnt(RxRingPtr);
	Status = XAxiDma_BdRingAlloc(RxRingPtr, FreeBdCount, &BdPtr);
	if (Status != XST_SUCCESS) {
		xil_printf("bd alloc failed\r\n");
	}

	Status = XAxiDma_BdRingToHw(RxRingPtr, FreeBdCount, BdPtr);
	if (Status != XST_SUCCESS) {
		xil_printf("Submit %d rx BDs failed %d\r\n", FreeBdCount, Status);
	}
}

int main() {
	const u32 PS_MIO_GPIO_DIRECTION_EMIO = 0x0;
	const u32 PS_MIO_GPIO_DIRECTION_OUT = 0x1;
	const u32 PS_MIO_GPIO_DIRECTION_IN = 0x0;

	// Platform initialization.
	init_platform();

	// PL AXI DMA.
	XAxiDma_Config* dma_0_pl_config = XAxiDma_LookupConfig(XPAR_AXIDMA_0_DEVICE_ID);
	Xil_AssertNonvoid(!XAxiDma_CfgInitialize(&dma_0_pl, dma_0_pl_config));
	//u32 num_bds = Setup_DMA_RX(&dma_0_pl);

	// PL AXI GPIO.
	// bit 0 - input connected to btn3 on carrier.
	// bit 1 - output connected to led4 on carrier.
	Xil_AssertNonvoid(!XGpio_Initialize(&gpio_0_pl, XPAR_AXI_GPIO_0_DEVICE_ID));
	XGpio_SetDataDirection(&gpio_0_pl, 1, 0x1);

	// PL AXI timer_0.
	Xil_AssertNonvoid(!XTmrCtr_Initialize(&timer_0, XPAR_AXI_TIMER_0_DEVICE_ID));
	XTmrCtr_SetHandler(&timer_0, Timer_Interrupt_Handler, &timer_0);
	XTmrCtr_SetResetValue(&timer_0, 0, 0xf0000000);
	XTmrCtr_SetOptions(&timer_0, XPAR_AXI_TIMER_0_DEVICE_ID, (XTC_INT_MODE_OPTION | XTC_AUTO_RELOAD_OPTION));

	// PS GPIOs.
	XGpioPs_Config* gpio_0_ps_config = XGpioPs_LookupConfig(XPAR_PS7_GPIO_0_DEVICE_ID);
	Xil_AssertNonvoid(!XGpioPs_CfgInitialize(&gpio_0_ps, gpio_0_ps_config, gpio_0_ps_config->BaseAddr));

	// MIO LED.
	XGpioPs_SetDirectionPin(&gpio_0_ps, mio_gpio_led_pin, PS_MIO_GPIO_DIRECTION_OUT);
	XGpioPs_SetOutputEnablePin(&gpio_0_ps, mio_gpio_led_pin, 1);
	XGpioPs_WritePin(&gpio_0_ps, mio_gpio_led_pin, 1);

	// MIO PB.
	XGpioPs_SetDirectionPin(&gpio_0_ps, mio_gpio_pb_pin, PS_MIO_GPIO_DIRECTION_IN);
	XGpioPs_SetOutputEnablePin(&gpio_0_ps, mio_gpio_pb_pin, 1);

	// EMIO PB.
	XGpioPs_SetDirectionPin(&gpio_0_ps, emio_gpio_pb_pin, PS_MIO_GPIO_DIRECTION_EMIO);
	XGpioPs_SetOutputEnablePin(&gpio_0_ps, emio_gpio_pb_pin, 0);

	// Snoop Control Unit - Interrupt controller.
	XScuGic_Config* gic_config = XScuGic_LookupConfig(XPAR_PS7_SCUGIC_0_DEVICE_ID);
	Xil_AssertNonvoid(!XScuGic_CfgInitialize(&intc, gic_config, gic_config->CpuBaseAddress));
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT, (Xil_ExceptionHandler) XScuGic_InterruptHandler,	&intc);
	Xil_ExceptionEnable();

	// Connect & enable all interrupts.
	Xil_AssertNonvoid(!XScuGic_Connect(&intc, XPAR_FABRIC_AXI_TIMER_0_INTERRUPT_INTR, (Xil_ExceptionHandler)XTmrCtr_InterruptHandler, (void *)&timer_0));
	XScuGic_Enable(&intc, XPAR_FABRIC_AXI_TIMER_0_INTERRUPT_INTR);

	// Main loop
	u32 read_status = 0, prev_gpio_pb = 0, prev_emio_gpio_pb = 0, prev_mio_gpio_pb = 0;
	u8 carrier_gpio_led = 0;
	print("###################### Menu Starts ########################\r\n");
	print("Press BTN3 on carrier to use AXI GPIO as an input\r\n");
	print("Press BTN1 on carrier to use EMIO as an input\r\n");
	print("Press SW1 on microzed to use PS GPIO as an input\r\n");
	print(" ##################### Menu Ends #########################\r\n");
	while (1) {
		// GPIO experiments.
		read_status = XGpio_DiscreteRead(&gpio_0_pl, 1);
		if(((read_status & 0x1) == 0x1) && ((prev_gpio_pb & 0x1)== 0x0)) {
			XGpio_DiscreteWrite(&gpio_0_pl, 1, carrier_gpio_led<<1);
			carrier_gpio_led = !carrier_gpio_led;
			Action_On_Press("PL GPIO through AXI GP0", &timer_0);
		}
		prev_gpio_pb = read_status;
		read_status = XGpioPs_ReadPin(&gpio_0_ps, emio_gpio_pb_pin);
		if((read_status == 0x1) && (prev_emio_gpio_pb == 0x0)) {
			Action_On_Press("PS GPIO through EMIO", &timer_0);
		}
		prev_emio_gpio_pb = read_status;
		read_status = XGpioPs_ReadPin(&gpio_0_ps, mio_gpio_pb_pin);
		if((read_status == 0x1) && (prev_mio_gpio_pb == 0x0)) {
			Action_On_Press("PS GPIO through MIO", &timer_0);
		}
		prev_mio_gpio_pb = read_status;

		// DMA experiments
		//DMA_RX(&dma_0_pl, num_bds);
	}

	cleanup_platform();
	return 0;
}

