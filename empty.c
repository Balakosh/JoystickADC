#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include <xdc/std.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/System.h>

#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>

/* TI-RTOS Header files */
#include <ti/drivers/EMAC.h>
#include <ti/drivers/GPIO.h>
// #include <ti/drivers/I2C.h>
// #include <ti/drivers/SDSPI.h>
// #include <ti/drivers/SPI.h>
// #include <ti/drivers/UART.h>
// #include <ti/drivers/USBMSCHFatFs.h>
// #include <ti/drivers/Watchdog.h>
// #include <ti/drivers/WiFi.h>
#include <ti/drivers/UART.h>
#include <driverlib/adc.h>
#include <driverlib/ssi.h>
#include <inc/hw_memmap.h>
#include <sys/socket.h>

#include "Board.h"
#include "wlModule/wlmodule.h"

#define TASKSTACKSIZE 1024
#define ADC_TASK_STACKSIZE 2048
#define WLMODULE_TASK_STACKSIZE 2048

#define ADC0_BASEADDRESS 0x40038000
#define ADC1_BASEADDRESS 0x40039000

#define TCPPACKETSIZE 256
#define NUMTCPWORKERS 3

Task_Struct task0Struct;
Char task0Stack[TASKSTACKSIZE];

Task_Struct adcTaskStruct;
Char adcTaskStack[ADC_TASK_STACKSIZE];

Task_Struct wlmoduleTaskStruct;
Char wlmoduleTaskStack[WLMODULE_TASK_STACKSIZE];

uint32_t adcValueX;
uint32_t adcValueY;
int tcpClientHandle = 0;

typedef struct __attribute__((packed))
{
    uint16_t syncWord;
    uint16_t adcValueX;
    uint16_t adcValueY;
} tcpPaketStruct;

Void tcpWorker(UArg arg0, UArg arg1)
{
    int  clientfd = (int)arg0;
    int  bytesSent;
    tcpPaketStruct tcpPaket;

    tcpPaket.syncWord = 0xab;

    System_printf("tcpWorker: start clientfd = 0x%x\n", clientfd);

    while (1)
    {
//        int size = snprintf(buffer, TCPPACKETSIZE, "x=%d y=%d\n", adcValueX, adcValueY);
//        int stringBufferLen = strlen(buffer);

        tcpPaket.adcValueX = adcValueX;
        tcpPaket.adcValueY = adcValueY;
        bytesSent = send(clientfd, (void*)&tcpPaket, sizeof(tcpPaketStruct), 0);

        if (bytesSent < 0)
        {
            System_printf("Error: send failed.\n");
            break;
        }

        Task_sleep(10);
    }

    System_printf("tcpWorker stop clientfd = 0x%x\n", clientfd);

    close(clientfd);
}

Void tcpHandler(UArg arg0, UArg arg1)
{
    int                status;
    int                clientfd;
    int                server;
    struct sockaddr_in localAddr;
    struct sockaddr_in clientAddr;
    int                optval;
    int                optlen = sizeof(optval);
    socklen_t          addrlen = sizeof(clientAddr);
    Task_Handle        taskHandle;
    Task_Params        taskParams;
    Error_Block        eb;

    server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server == -1) {
        System_printf("Error: socket not created.\n");
        goto shutdown;
    }

    memset(&localAddr, 0, sizeof(localAddr));
    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    localAddr.sin_port = htons(arg0);

    status = bind(server, (struct sockaddr *)&localAddr, sizeof(localAddr));
    if (status == -1) {
        System_printf("Error: bind failed.\n");
        goto shutdown;
    }

    status = listen(server, NUMTCPWORKERS);
    if (status == -1) {
        System_printf("Error: listen failed.\n");
        goto shutdown;
    }

    optval = 1;
    if (setsockopt(server, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen) < 0) {
        System_printf("Error: setsockopt failed\n");
        goto shutdown;
    }

    while ((clientfd =
            accept(server, (struct sockaddr *)&clientAddr, &addrlen)) != -1) {

        System_printf("tcpHandler: Creating thread clientfd = %d\n", clientfd);

        tcpClientHandle = clientfd;

        Error_init(&eb);
        Task_Params_init(&taskParams);
        taskParams.arg0 = (UArg)clientfd;
        taskParams.stackSize = 1280;
        taskHandle = Task_create((Task_FuncPtr)tcpWorker, &taskParams, &eb);
        if (taskHandle == NULL) {
            System_printf("Error: Failed to create new Task\n");
            close(clientfd);
        }

        /* addrlen is a value-result param, must reset for next accept call */
        addrlen = sizeof(clientAddr);
    }

    System_printf("Error: accept failed.\n");

shutdown:
    if (server > 0) {
        close(server);
    }
}

void heartBeatFxn(UArg arg0, UArg arg1)
{
    while (1)
    {
        Task_sleep((unsigned int)arg0);
        GPIO_toggle(Board_LED0);
    }
}

void adcTaskFxn(UArg arg0, UArg arg1)
{
    while (1)
    {
        ADCProcessorTrigger(ADC0_BASE, 0);

        while(!ADCIntStatus(ADC0_BASE, 0, false))
        {
        }

        ADCSequenceDataGet(ADC0_BASE, 0, &adcValueX);

        ADCProcessorTrigger(ADC0_BASE, 1);

        while(!ADCIntStatus(ADC0_BASE, 1, false))
        {
        }

        ADCSequenceDataGet(ADC0_BASE, 1, &adcValueY);

        Task_sleep(1);
    }
}

void initADC(void)
{
    ADCSequenceConfigure(ADC0_BASE, 0, ADC_TRIGGER_PROCESSOR, 0);
    ADCSequenceConfigure(ADC0_BASE, 1, ADC_TRIGGER_PROCESSOR, 0);
    ADCSequenceStepConfigure(ADC0_BASE, 0, 0, ADC_CTL_IE | ADC_CTL_CH0 | ADC_CTL_END);
    ADCSequenceStepConfigure(ADC0_BASE, 1, 0, ADC_CTL_IE | ADC_CTL_CH9 | ADC_CTL_END);
    ADCSequenceEnable(ADC0_BASE, 0);
    ADCSequenceEnable(ADC0_BASE, 1);
}

void configSPI(void)
{
    SSIConfigSetExpClk(WLMODULE_SPI_BASE, 120000000, SSI_FRF_MOTO_MODE_0, SSI_MODE_MASTER, 2000000, 8);
    SSIEnable(WLMODULE_SPI_BASE);
}

void constructTasks(void)
{
    Task_Params taskParams;
    Task_Params adcTaskParams;
    Task_Params wlModuleTaskParams;
    Error_Block eb;

    Error_init(&eb);

    /* Construct heartBeat Task  thread */
    Task_Params_init(&taskParams);
    taskParams.arg0 = 1000;
    taskParams.stackSize = TASKSTACKSIZE;
    taskParams.stack = &task0Stack;
    Task_construct(&task0Struct, (Task_FuncPtr)heartBeatFxn, &taskParams, &eb);

    Error_init(&eb);

    Task_Params_init(&adcTaskParams);
    adcTaskParams.arg0 = 1000;
    adcTaskParams.stackSize = ADC_TASK_STACKSIZE;
    adcTaskParams.stack = &adcTaskStack;
    Task_construct(&adcTaskStruct, (Task_FuncPtr)adcTaskFxn, &adcTaskParams, &eb);

    Error_init(&eb);

    Task_Params_init(&wlModuleTaskParams);
    wlModuleTaskParams.arg0 = 1000;
    wlModuleTaskParams.stackSize = WLMODULE_TASK_STACKSIZE;
    wlModuleTaskParams.stack = &wlmoduleTaskStack;
    Task_construct(&wlmoduleTaskStruct, (Task_FuncPtr)wlModuleTaskFnx, &wlModuleTaskParams, &eb);
}

int main(void)
{
    /* Call board init functions */
    Board_initGeneral();
    Board_initEMAC();
    Board_initGPIO();
    // Board_initI2C();
    // Board_initSDSPI();
    Board_initSPI();
    // Board_initUART();
    // Board_initUSB(Board_USBDEVICE);
    // Board_initUSBMSCHFatFs();
    // Board_initWatchdog();
    // Board_initWiFi();

    initADC();
    configSPI();

    constructTasks();

    GPIO_write(Board_LED0, Board_LED_ON);

    BIOS_start();

    return (0);
}
