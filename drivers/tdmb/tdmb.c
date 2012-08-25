/////////////////////////////////
#include "INC_INCLUDES.h"
#include <linux/irq.h>
#include <linux/wait.h>
#ifndef TRUE
	#define TRUE   1   
#endif 

#ifndef FALSE
	#define FALSE  0 
#endif

#ifndef NULL
  #define NULL  0
#endif


#define	TDMB_DEV_NAME       	"Tdmb"
#define TDMB_DEV_MAJOR      	251
#define TDMB_IRQ            	28

#define INT_OCCUR_SIG		0x0A		// [S5PV210_Kernel], 20101220, ASJ, 
#define DIRECT_OUT_SIG		0x01		// [MX100_iRiver_ws], 20110119, ASJ, 
extern struct spi_device *gInc_spi;

///////////////////////////////////////////////////////////////////////////////
// ioctl message
///////////////////////////////////////////////////////////////////////////////
#define TDMB_MAGIC 'T'
#define TDMB_IOCTL_MAX 7


#define TDMB_T_STOP 		_IOW(TDMB_MAGIC,  0, int)
#define TDMB_T_SCAN 		_IOWR(TDMB_MAGIC, 1, BB_CH_INFO)
#define TDMB_T_PLAY 		_IOW(TDMB_MAGIC,  2, BB_CH_PLAY)
#define TDMB_T_SIGNAL		_IOR(TDMB_MAGIC,  3, BB_CH_SIGNAL)
#define TDMB_T_TEST1		_IOR(TDMB_MAGIC,  4, int)
#define TDMB_T_TEST2		_IOW(TDMB_MAGIC,  5, int)


///////////////////////////////////////////////////////////////////////////////
// ioctl Data
///////////////////////////////////////////////////////////////////////////////
typedef struct
{
	unsigned int	nIndex;			//ASJ 20101116 add
	unsigned short	nBbpStatus;
	unsigned long	uiRFFreq;
	unsigned short	uiEnsembleID;
	unsigned char	ucSubChCnt;
	unsigned char	aucSubChID[MAX_SUBCHANNEL];
	unsigned char	aucTMID[MAX_SUBCHANNEL];
	unsigned char	aucEnsembleLabel[MAX_LABEL_CHAR];
	unsigned char	aucServiceLabel[MAX_SUBCHANNEL][MAX_LABEL_CHAR];
	unsigned short  uiBitRate[MAX_SUBCHANNEL];	// [S5PV210_Kernel], 20101213, ASJ, 
} BB_CH_INFO;

typedef struct
{
	unsigned short	nBbpStatus;
	unsigned int	uiRFFreq;
	unsigned char	ucSubChID;
	unsigned char	ucTMID;
} BB_CH_PLAY;

typedef struct
{
	unsigned short   uiRssi;
	unsigned char   uiSNR;
	unsigned int   ulPreBER;
	unsigned int   ulPostBER;
	unsigned short  uiCER;
} BB_CH_SIGNAL;


struct ST_TDMB_Interrupt{
	int tdmb_irq;
	struct completion comp;
};
static struct ST_TDMB_Interrupt g_stTdmb_Int;


///////////////////////////////////////////////////////////////////////////////
// thread
///////////////////////////////////////////////////////////////////////////////
static ST_SUBCH_INFO g_stPlayInfo;
struct task_struct* g_pTS;
static int g_TaskFlag = 0;
unsigned char g_StreamBuff[4096] = {0};

DECLARE_WAIT_QUEUE_HEAD( WaitQueue_Read );
static unsigned char ReadQ;

unsigned int  INC_TickCnt(void)
{
	struct timeval tick;
	do_gettimeofday(&tick);

	// return msec tick
	return (tick.tv_sec*1000 + tick.tv_usec/1000);
}

void INC_GPIO_DMBEnable(void)
{
#if defined(CONFIG_MX100_EVM) /* ktj */
	//power down output pin active low  so...set high
	s3c_gpio_cfgpin(S5PV210_GPJ1(3), S3C_GPIO_OUTPUT);
	s3c_gpio_setpin(S5PV210_GPJ1(3), 1);

	//DMB_enable pin active high so ...set high
	s3c_gpio_cfgpin(S5PV210_GPJ0(1), S3C_GPIO_OUTPUT);
	s3c_gpio_setpin(S5PV210_GPJ0(1), 1);
	msleep(10); /* ktj add */
#elif defined(CONFIG_MX100_WS)
  	s3c_gpio_cfgpin(S5PV210_GPH0(5), S3C_GPIO_OUTPUT);
    	s3c_gpio_setpin(S5PV210_GPH0(5), 1);
	msleep(10);
#endif
	//msleep(10);
}

#if defined(CONFIG_MX100) /* ktj, implement dmb stop */
void INC_GPIO_DMBDisable(void)
{
#if defined(CONFIG_MX100_EVM) /* ktj */

    // reset set low
   	 s3c_gpio_cfgpin(S5PV210_GPJ1(4), S3C_GPIO_OUTPUT);
	s3c_gpio_setpin(S5PV210_GPJ1(4), 0);

	// DMB_enable low
	s3c_gpio_cfgpin(S5PV210_GPJ0(1), S3C_GPIO_OUTPUT);
	s3c_gpio_setpin(S5PV210_GPJ0(1), 0);

	// power_down low
	s3c_gpio_cfgpin(S5PV210_GPJ1(3), S3C_GPIO_OUTPUT);
	s3c_gpio_setpin(S5PV210_GPJ1(3), 0);

#elif defined(CONFIG_MX100_WS)

       INC_MSG_PRINTF(1, "[%s]   success  !!! \n", __func__);  
    // reset set low
	s3c_gpio_cfgpin(S5PV210_GPH0(6), S3C_GPIO_OUTPUT);
	s3c_gpio_setpin(S5PV210_GPH0(6), 0);

	//power down output pin active low
  	s3c_gpio_cfgpin(S5PV210_GPH0(5), S3C_GPIO_OUTPUT);
    	s3c_gpio_setpin(S5PV210_GPH0(5), 0);

#endif
}
#endif

void INC_GPIO_Reset(void)
{
#if 0 // Aesop Board : jsw
	s3c_gpio_cfgpin(S5PV210_GPG2(4), S3C_GPIO_OUTPUT);
	s3c_gpio_setpin(S5PV210_GPG2(4), 1);
	msleep(10);
	s3c_gpio_setpin(S5PV210_GPG2(4), 0);
	msleep(10);
	s3c_gpio_setpin(S5PV210_GPG2(4), 1);
#else 

    #if defined(CONFIG_MX100_EVM)
	s3c_gpio_cfgpin(S5PV210_GPJ1(4), S3C_GPIO_OUTPUT);
	s3c_gpio_setpin(S5PV210_GPJ1(4), 1);
	msleep(10);
	
	s3c_gpio_setpin(S5PV210_GPJ1(4), 0);
	msleep(10);
	s3c_gpio_setpin(S5PV210_GPJ1(4), 1);
    #elif defined(CONFIG_MX100_WS)

        s3c_gpio_cfgpin(S5PV210_GPH0(6), S3C_GPIO_OUTPUT);
	s3c_gpio_setpin(S5PV210_GPH0(6), 1);
	msleep(10);
	
	s3c_gpio_setpin(S5PV210_GPH0(6), 0);
	
	msleep(25); // 10->20
	
	s3c_gpio_setpin(S5PV210_GPH0(6), 1);

    #endif 
#endif
}
///////////////////////////////////////////////////////////////////////////////
// Interrupt
///////////////////////////////////////////////////////////////////////////////
static irqreturn_t INC_isr(int irq, void *handle)
{
	//INC_MSG_PRINTF(1, "[%s] ==> [%s] interrupt	!!! [0x%x] ", __FILE__, __func__, irq);
	if(g_TaskFlag)
		complete(&g_stTdmb_Int.comp);
	
	return IRQ_HANDLED;
}

int INC_GPIO_Interrupt(void)
{
#if 0 // Aesop Board : jsw
	s3c_gpio_cfgpin(S5PV210_GPH1(0), S3C_GPIO_SFN(0xf)); 
	g_stTdmb_Int.tdmb_irq = IRQ_EINT8;
#else
	s3c_gpio_cfgpin(S5PV210_GPH1(3), S3C_GPIO_SFN(0xf));
	g_stTdmb_Int.tdmb_irq = IRQ_EINT11 ; 
#endif	
	if(request_irq(g_stTdmb_Int.tdmb_irq, INC_isr, IRQF_TRIGGER_FALLING, "tdmb_int", NULL)){
		INC_MSG_PRINTF(1, "[%s] request_irq(0x%X) Fail	!!! \n", __func__, g_stTdmb_Int.tdmb_irq);
		return -1;
	}
	INC_MSG_PRINTF(1, "[%s] request_irq(0x%X) success  !!! \n", __func__, g_stTdmb_Int.tdmb_irq);
	return 0;
}

int INC_dump_thread(void *kthread)
{
	unsigned char  bFirstLoop = 1;
	unsigned int   nTickCnt = 0, nIdxChip = 0;
	unsigned short nDataLength = 0, nStatus = 0;
	long nTimeStamp = 0, tempStamp=0;
	ST_BBPINFO*		pInfo;				// [S5PV210_Kernel], 20101221, ASJ, 

	INC_MSG_PRINTF(1, "\n[%s] %s : INC_dump_thread start ================ \r\n",
		__FILE__, __func__);


	while (!kthread_should_stop())
	{
		if(!g_TaskFlag)
		{	
			ReadQ = DIRECT_OUT_SIG;
			wake_up_interruptible(&WaitQueue_Read);			// [MX100_iRiver_ws], 20110119, ASJ,  
			break;
		}

		if( (INC_TickCnt() - nTimeStamp ) > INC_CER_PERIOD_TIME )
		{
			nStatus = INTERFACE_STATUS_CHECK(TDMB_I2C_ID80);
			if( nStatus == 0xFF )
			{
				////////////////////////////////////
				// Reset!!!!! 
				////////////////////////////////////
				INC_GPIO_Reset();
				INC_MSG_PRINTF(1, "[%s] %s : SPI Reset !!!!\r\n", __FILE__, __func__);
			
				if(INC_SUCCESS == INC_CHIP_STATUS(TDMB_I2C_ID80))
				{
					INC_INIT(TDMB_I2C_ID80);
					INC_READY(TDMB_I2C_ID80, g_stPlayInfo.astSubChInfo[0].ulRFFreq);
					INC_START(TDMB_I2C_ID80, &g_stPlayInfo, 0); 
				}

				pInfo = INC_GET_STRINFO(TDMB_I2C_ID80);
				pInfo->ulFreq = g_stPlayInfo.astSubChInfo[0].ulRFFreq;
				bFirstLoop = TRUE;
			}
			else if(nStatus == INC_ERROR)
			{
				INC_MSG_PRINTF(1, "[%s][%d] ReSynced.....[%d] !!!!\r\n", __FILE__, __LINE__, nIdxChip);
				bFirstLoop = TRUE;
			}

			nTimeStamp = INC_TickCnt();
					
		}
		
		if(!g_TaskFlag)
		{	
			ReadQ = DIRECT_OUT_SIG;
			wake_up_interruptible(&WaitQueue_Read);			// [MX100_iRiver_ws], 20110119, ASJ,  
			break;
		}
		
		if(bFirstLoop)
		{
			nTickCnt = 0;
			bFirstLoop = FALSE;
			tempStamp = INC_TickCnt();
			init_completion(&g_stTdmb_Int.comp);
			INTERFACE_INT_ENABLE(TDMB_I2C_ID80,INC_MPI_INTERRUPT_ENABLE); 
			INTERFACE_INT_CLEAR(TDMB_I2C_ID80, INC_MPI_INTERRUPT_ENABLE);
			INC_INIT_MPI(TDMB_I2C_ID80);	
			INC_MSG_PRINTF(1, "[%s] %s : nTickCnt(%d) INC_INIT_MPI!!!!!! \r\n", 
				__FILE__, __func__, nTickCnt);
		}
			
		
		if(!g_TaskFlag)
		{	
			ReadQ = DIRECT_OUT_SIG;
			wake_up_interruptible(&WaitQueue_Read);			// [MX100_iRiver_ws], 20110119, ASJ,  
			break;
		}
#ifdef INTERRUPT_METHOD
	
		if(!wait_for_completion_timeout(&g_stTdmb_Int.comp, 1000*HZ/1000)){ // 1000msec
			INC_MSG_PRINTF(1, "[%s] INTR TimeOut : nTickCnt[%d]\r\n", __FILE__, nTickCnt);
			bFirstLoop = TRUE;
			continue;
		}

		/////////////////////////
		// ????? 해야 하나???
		/////////////////////////
		//init_completion(&g_stTdmb_Int.comp);
		//INC_MSG_PRINTF(1, "==>   Interrupt elapased Time : %d msec !\n", INC_TickCnt()-tempStamp);
		//tempStamp = INC_TickCnt();
#else
		if(!INTERFACE_INT_CHECK(TDMB_I2C_ID80)){
			INC_DELAY(5);
			continue;
		}
#endif

		////////////////////////////////////////////////////////////////
		// Read the dump size
		////////////////////////////////////////////////////////////////
		if(!g_TaskFlag)
		{	
			ReadQ = DIRECT_OUT_SIG;
			wake_up_interruptible(&WaitQueue_Read);			// [MX100_iRiver_ws], 20110119, ASJ,  
			break;
		}
		nDataLength = INC_CMD_READ(TDMB_I2C_ID80, APB_MPI_BASE+ 0x06);
	
		if( nDataLength & 0x4000 ){
			bFirstLoop = 1;
			INC_MSG_PRINTF(1, "[%s]==> FIFO FULL   : 0x%X nTickCnt(%d)\r\n", __FILE__, nDataLength, nTickCnt);
			continue;
		}
		else if( !(nDataLength & 0x3FFF ))	{
			nDataLength = 0;
			INTERFACE_INT_CLEAR(TDMB_I2C_ID80, INC_MPI_INTERRUPT_ENABLE);
			INC_MSG_PRINTF(1, "[%s]==> FIFO Empty   : 0x%X nTickCnt(%d)\r\n", __FILE__, nDataLength, nTickCnt);
			continue;

		}
		else{
			nDataLength &= 0x3FFF;
			nDataLength = INC_INTERRUPT_SIZE;
		}
		
		
		////////////////////////////////////////////////////////////////
		// dump the stream
		////////////////////////////////////////////////////////////////
		if(!g_TaskFlag)
		{	
			ReadQ = DIRECT_OUT_SIG;
			wake_up_interruptible(&WaitQueue_Read);			// [MX100_iRiver_ws], 20110119, ASJ,  
			break;
		}

		if(nDataLength >= INC_INTERRUPT_SIZE){
			INC_CMD_READ_BURST(TDMB_I2C_ID80, APB_STREAM_BASE, g_StreamBuff, INC_INTERRUPT_SIZE);
			ReadQ = INT_OCCUR_SIG; 
		    wake_up_interruptible(&WaitQueue_Read);
		}else
		{
			INC_MSG_PRINTF(1, " [%s] Read Burst   Error!! : len[%d], nTickCnt(%d)\r\n", 
				__FILE__, nDataLength, nTickCnt);
		}
		
		INTERFACE_INT_CLEAR(TDMB_I2C_ID80, INC_MPI_INTERRUPT_ENABLE);
		nTickCnt++;
	} 
	
	INC_MSG_PRINTF(1, "[%s] %s : INC_dump_thread end ================== \r\n\r\n",
					__FILE__, __func__);

	return 0;
}


int INC_scan(BB_CH_INFO* pChInfo)
{
	INC_UINT16 	nLoop = 0, nStatus = 0;
	INC_UINT32 	uiFreq;
	ST_FICDB_LIST*	pstFicDb;
	INC_UINT16	nSvrCnt = 0;
	
	INC_MSG_PRINTF(1, "[%s] %s : freq[%d] start \r\n", __FILE__, __func__, pChInfo->uiRFFreq);

	
	uiFreq = pChInfo->uiRFFreq;

		
	if(uiFreq == 0xffff || uiFreq == 0){
		INC_MSG_PRINTF(1, "[%s] %s : freq[%d] Error!!! \r\n", __FILE__, __func__, uiFreq);
		return -1;
	}

	
	////////////////////////////////////////////////
	// Start Single Scan
	////////////////////////////////////////////////
	if(!INTERFACE_SCAN(TDMB_I2C_ID80, uiFreq)){
		nStatus = (INC_UINT16)INTERFACE_ERROR_STATUS(TDMB_I2C_ID80);
		INC_MSG_PRINTF(1, "[%s] %s : [Scan Fail] Freq : %d, ErrorCode : 0x%X\r\n", 
			__FILE__, __func__, uiFreq, nStatus);
		if( nStatus == ERROR_STOP || nStatus == ERROR_INIT)
		{
			////////////////////////////////////
			// Reset!!!!! 
			////////////////////////////////////
			INC_GPIO_Reset();
			INC_MSG_PRINTF(1, "[%s] %s : SPI Reset !!!!\r\n", __FILE__, __func__);
			INTERFACE_INIT(TDMB_I2C_ID80);
		}		
		return -1;
	}else{
		INC_MSG_PRINTF(1, "[%s] %s : [Scan Success] Freq : %d \r\n", __FILE__, __func__, uiFreq);
		
	}
	pstFicDb = INC_GET_FICDB_LIST();
	
	pChInfo->nBbpStatus = nStatus;
	pChInfo->ucSubChCnt = pstFicDb->nSubChannelCnt;
	pChInfo->uiEnsembleID = pstFicDb->unEnsembleID;
	
	memcpy(pChInfo->aucEnsembleLabel, pstFicDb->aucEnsembleName,MAX_LABEL_CHAR);

	for(nLoop=0; nLoop<pstFicDb->stDMB.nPrimaryCnt; nLoop++)
	{
		pChInfo->aucSubChID[nSvrCnt] = pstFicDb->stDMB.astPrimary[nLoop].ucSubChid;
		pChInfo->aucTMID[nSvrCnt] = pstFicDb->stDMB.astPrimary[nLoop].ucTmID;
		pChInfo->uiBitRate[nSvrCnt]= pstFicDb->stDMB.astPrimary[nLoop].uiBitRate;		// [S5PV210_Kernel], 20101213, ASJ, 
		memcpy(pChInfo->aucServiceLabel[nSvrCnt], pstFicDb->stDMB.astPrimary[nLoop].aucLabels, MAX_LABEL_CHAR);	
		nSvrCnt++;
	}

	for(nLoop=0; nLoop<pstFicDb->stDAB.nPrimaryCnt; nLoop++)
	{
		pChInfo->aucSubChID[nSvrCnt] = pstFicDb->stDAB.astPrimary[nLoop].ucSubChid;
		pChInfo->aucTMID[nSvrCnt] = pstFicDb->stDAB.astPrimary[nLoop].ucTmID;
		pChInfo->uiBitRate[nSvrCnt]= pstFicDb->stDAB.astPrimary[nLoop].uiBitRate;
		memcpy(pChInfo->aucServiceLabel[nSvrCnt], pstFicDb->stDAB.astPrimary[nLoop].aucLabels, MAX_LABEL_CHAR);	

		nSvrCnt++;
	}


	for(nLoop=0; nLoop<pstFicDb->stDATA.nPrimaryCnt; nLoop++)
	{
		pChInfo->aucSubChID[nSvrCnt] = pstFicDb->stDATA.astPrimary[nLoop].ucSubChid;
		pChInfo->aucTMID[nSvrCnt] = pstFicDb->stDATA.astPrimary[nLoop].ucTmID;
		pChInfo->uiBitRate[nSvrCnt]= pstFicDb->stDATA.astPrimary[nLoop].uiBitRate;
		memcpy(pChInfo->aucServiceLabel[nSvrCnt], pstFicDb->stDATA.astPrimary[nLoop].aucLabels, MAX_LABEL_CHAR);	

		nSvrCnt++;
	}

	return 0;
}


int INC_start(BB_CH_PLAY* pstSetChInfo)
{
	
	g_TaskFlag = 0;
	
	INC_MSG_PRINTF(1, "[%s] %s : freq[%d] start\r\n", 	__FILE__, __func__, pstSetChInfo->uiRFFreq);
	////////////////////////////////////////////////
	// setting the channel
	////////////////////////////////////////////////
	memset(&g_stPlayInfo, 0, sizeof(ST_SUBCH_INFO));
	g_stPlayInfo.astSubChInfo[0].ulRFFreq = pstSetChInfo->uiRFFreq;
	g_stPlayInfo.astSubChInfo[0].ucSubChID = pstSetChInfo->ucSubChID;
	g_stPlayInfo.astSubChInfo[0].uiTmID = pstSetChInfo->ucTMID;
	pstSetChInfo->nBbpStatus = ERROR_NON;
	g_stPlayInfo.nSetCnt = 1;
	
	////////////////////////////////////////////////
	// Start Single Scan
	////////////////////////////////////////////////

	if(!INTERFACE_START(TDMB_I2C_ID80, &g_stPlayInfo))
	{
		pstSetChInfo->nBbpStatus = (INC_UINT16)INTERFACE_ERROR_STATUS(TDMB_I2C_ID80);
		INC_MSG_PRINTF(1, "[%s] %s : INTERFACE_START() Error : 0x%X \r\n", 
			__FILE__, __func__, pstSetChInfo->nBbpStatus);

		if( pstSetChInfo->nBbpStatus == ERROR_STOP || pstSetChInfo->nBbpStatus == ERROR_INIT)
		{
			////////////////////////////////////
			// Reset!!!!! 
			////////////////////////////////////
			INC_GPIO_Reset();
			INC_MSG_PRINTF(1, "[%s] %s : SPI Reset !!!!\r\n", __FILE__, __func__);
			INTERFACE_INIT(TDMB_I2C_ID80);
		}
		return 0;
	}else{
		INC_MSG_PRINTF(1, "[%s] %s : INTERFACE_START() Success !!!!\r\n", __FILE__, __func__);
	}
	
	g_TaskFlag = 1;
	g_pTS = kthread_run(INC_dump_thread, NULL, "kidle_timeout");
	if(IS_ERR(g_pTS) ) {// no error	
		INC_MSG_PRINTF(1, "[%s] %s : cann't create the INC_dump_thread !!!! \n", __FILE__, __func__);
		return -1;	
	}
	INC_MSG_PRINTF(1, "[%s] %s : created the INC_dump_thread !!!! \n", __FILE__, __func__);		
	return 0;
}

int INC_stop(void)
{
     INTERFACE_USER_STOP(TDMB_I2C_ID80);// [S5PV210_Kernel], 20101215, ASJ, 
     return 0;
}

int INC_INTERFACE_TEST(void)
{
	unsigned short nLoop = 0, nIndex = 0;
	unsigned short nData = 0;
	unsigned int nTick = 0;

	/////////////////////////////////////////////
	// SPI interface Test
	/////////////////////////////////////////////
	nTick = INC_TickCnt();
	while(nIndex < 50)
	{
		for(nLoop=0; nLoop<8; nLoop++)
		{
			INC_CMD_WRITE(TDMB_I2C_ID80, APB_RF_BASE+ nLoop, nLoop) ;
			nData = INC_CMD_READ(TDMB_I2C_ID80, APB_RF_BASE+ nLoop);

			if(nLoop != nData){
				INC_MSG_PRINTF(1, " [Interface Test : %02d]: WriteData[0x%X], ReadData[0x%X] \r\n", 
					(nIndex*8)+nLoop, nLoop, nData);
				return 0;
			}
		}
		nIndex++;
	}
	INC_MSG_PRINTF(1, " [Interface Test]: %d msec \r\n", INC_TickCnt()-nTick);
	return 1;
}

int tdmb_open (struct inode *inode, struct file *filp)
{
	INC_MSG_PRINTF(1,"######## [%s] %s Start!! ##################\n", __FILE__, __func__);

#if defined(CONFIG_MX100) /* ktj move frmo tdmb_init */
	INC_GPIO_DMBEnable();
#endif

	///////////////////////////////////////
	// by jin 10/11/04 : RESET PIN Setting
	///////////////////////////////////////
	INC_GPIO_Reset();
	
	if(INC_ERROR == INC_DRIVER_OPEN()){
		INC_MSG_PRINTF(1, "[%s] INC_DRIVER_OPEN() FAIL !!!\n", __func__);
		return -1;
	}
	INC_MSG_PRINTF(1, "[%s] INC_DRIVER_OPEN() SUCCESS \n", __func__);
	
	// Test the spi communication
	if(!INC_INTERFACE_TEST()){
		INC_MSG_PRINTF(1, "[%s] INC_INTERFACE_TEST() FAIL !!!\n", __func__);
		return -1;
	}
	INC_MSG_PRINTF(1, "[%s]   INC_INTERFACE_TEST() SUCCESS \n", __func__);


	if(!INTERFACE_INIT(TDMB_I2C_ID80)){
		INC_MSG_PRINTF(1, "[%s] INTERFACE_INIT() FAIL !!!\n", __func__);
		return -1;
	}
	INC_MSG_PRINTF(1, "[%s] INTERFACE_INIT() SUCCESS \n", __func__);

	///////////////////////////////////////
	// Interrupt setting
	///////////////////////////////////////
	if(INC_GPIO_Interrupt()){
		return -1;
	}
	g_pTS = NULL;
	INC_MSG_PRINTF(1,"######## [%s] %s Success!! ################\n", __FILE__, __func__);
	return 0;
}

ssize_t tdmb_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
	int res = -1;
	if(count > INC_INTERRUPT_SIZE)
	return -EMSGSIZE;
	res = copy_to_user(buf, g_StreamBuff, INC_INTERRUPT_SIZE);
	if (res > 0) {
		res = -EFAULT;
		INC_MSG_PRINTF(1, "[%s] %s : Error!! \n", __FILE__, __func__);
		return -1;
	}
	
	return INC_INTERRUPT_SIZE;
}

ssize_t tdmb_write (struct file *filp, const char *buf, size_t count, loff_t *f_pos)
{
	INC_MSG_PRINTF(1, "[%s] %s \n", __FILE__, __func__);
	return 0;
}

ssize_t tdmb_poll( struct file *filp, poll_table *wait )
{
	int mask = 0;
	poll_wait( filp, &WaitQueue_Read, wait );

	if(ReadQ == INT_OCCUR_SIG){
	  mask |= (POLLIN);
	}else if(ReadQ != 0){
	  mask |= POLLERR;
	}

	ReadQ = 0x00;
	return mask;
	
}	

int tdmb_ioctl(struct inode *inode,struct file *filp, unsigned int cmd, unsigned long arg)
{
	int error;
	int nIndex = 0;
	
	if(_IOC_TYPE(cmd) != TDMB_MAGIC)
		return -EINVAL;
	if(_IOC_NR(cmd) >= TDMB_IOCTL_MAX)
		return -EINVAL;

	switch(cmd)
	{

	case TDMB_T_STOP:
		{
			INC_MSG_PRINTF(1, "[%s] %s : TDMB_STOP_CH \n", __FILE__, __func__);
			INTERFACE_USER_STOP(TDMB_I2C_ID80);
			if(g_pTS != NULL){
				g_TaskFlag = 0;
				complete(&g_stTdmb_Int.comp);
				msleep(30);
				g_pTS = NULL;
			}

			if(copy_from_user((void*)&nIndex, (const void*)arg, sizeof(int)))
			 INC_MSG_PRINTF(1, "[%s] %s : TDMB_STOP_CH from : data[0x%x]4\n", __FILE__, __func__, nIndex);

		}
		break;	
	
	case TDMB_T_SCAN:
		{
			BB_CH_INFO ChInfo;
			if(copy_from_user((void*)&ChInfo, (const void*)arg, sizeof(BB_CH_INFO)))
			INC_MSG_PRINTF(1, "[%s] %s : TDMB_SCAN_FREQ_INDEX from : Freq [%d]\n", __FILE__, __func__, ChInfo.uiRFFreq);


			INC_MSG_PRINTF(1, "[%s] %s : TDMB_SCAN_FREQ_INDEX   : Freq [%d]\n", __FILE__, __func__, ChInfo.uiRFFreq);
			error = INC_scan(&ChInfo);
			if(error == 0)
			{
			 	if(copy_to_user((void*)arg, (const void*)&ChInfo, sizeof(BB_CH_INFO)))
			 	INC_MSG_PRINTF(1,"[%s] %s : TDMB_SCAN_FREQ_INDEX to: Freq [%ld]\n", __FILE__, __func__, ChInfo.uiRFFreq);


			  	 INC_MSG_PRINTF(1,"[%s] %s : TDMB_SCAN_FREQ_INDEX complete copy to jni : Freq [%ld]\n", __FILE__, __func__, ChInfo.uiRFFreq);
			   return 0;
			}
			else
			  return -1;
		}			
		break;
	case TDMB_T_PLAY:
		{
			BB_CH_PLAY stSetChInfo;
			if(copy_from_user((void*)&stSetChInfo, (const void*)arg, sizeof(BB_CH_PLAY)))
			 INC_MSG_PRINTF(1, "[%s] %s : TDMB_SET_CH from : Freq [%d]\n", __FILE__, __func__, stSetChInfo.uiRFFreq);
			 
			INC_MSG_PRINTF(1, "[%s] %s : TDMB_SET_CH : Freq [%d]\n", __FILE__, __func__, stSetChInfo.uiRFFreq);

			error = INC_start(&stSetChInfo);
			if(error < 0)
			{
		          return -1;
			}
			else
			{

			  if(copy_to_user((void*)arg, (const void*)&stSetChInfo, sizeof(BB_CH_PLAY)))
				INC_MSG_PRINTF(1, "[%s] %s : TDMB_SET_CH   to : Freq [%d]\n", __FILE__, __func__, stSetChInfo.uiRFFreq);				
			   return 0;
			}
		}
		break;
	case TDMB_T_SIGNAL:
		{
			BB_CH_SIGNAL ChRFinfo;		// [S5PV210_Kernel], 20101125, ASJ,
			ST_BBPINFO* pInfo;
			pInfo = INC_GET_STRINFO(TDMB_I2C_ID80);

			
			if(copy_from_user((void*)&ChRFinfo, (const void*)arg, sizeof(BB_CH_SIGNAL)))
			INC_MSG_PRINTF(1, "[%s] %s : TDMB_GET_CER from \n", __FILE__, __func__);

			ChRFinfo.uiRssi = pInfo->wRssi;
			ChRFinfo.uiSNR = pInfo->ucSnr;
			ChRFinfo.ulPreBER= pInfo->uiPreBER;
			ChRFinfo.ulPostBER = pInfo->uiPostBER;
			ChRFinfo.uiCER = pInfo->uiCER;
		
			if(copy_to_user((void*)arg, (const void*)&ChRFinfo, sizeof(BB_CH_SIGNAL)))
			INC_MSG_PRINTF(1, "[%s] %s : TDMB_GET_CER to: [%d]\n", __FILE__, __func__, ChRFinfo.uiCER);	
			
		}
		break;
		case TDMB_T_TEST1:
		{
			if(copy_from_user((void*)&nIndex, (const void*)arg, sizeof(int)))
			INC_MSG_PRINTF(1, "[%s] %s : TDMB_INTERFACE_TEST : data[0x%x]\n", __FILE__, __func__, nIndex);

			break;	
		}
	}
	return 0;
}

int tdmb_release (struct inode *inode, struct file *filp)
{
	INC_MSG_PRINTF(1, "[%s] %s : irq[%d] start\n", __FILE__, __func__, g_stTdmb_Int.tdmb_irq);
	INTERFACE_USER_STOP(TDMB_I2C_ID80);
	if(g_TaskFlag){
		g_TaskFlag = 0;
		complete(&g_stTdmb_Int.comp);
		msleep(30);
	}
	free_irq(g_stTdmb_Int.tdmb_irq, NULL);
	INC_MSG_PRINTF(1, "[%s] %s : irq[%d] 33 \n", __FILE__, __func__, g_stTdmb_Int.tdmb_irq);
	g_pTS = NULL;

#if defined(CONFIG_MX100) /* ktj */
    INC_GPIO_DMBDisable();
#endif

	return 0;
}


static int __init tdmb_probe(struct spi_device *spi)
{
	INC_MSG_PRINTF(1,"[%s] %s [%s] : spi.cs[%d], mod[%d], hz[%d] \n", 
		__FILE__,__func__, spi->modalias, spi->chip_select, spi->mode, spi->max_speed_hz);

	gInc_spi = spi;
	return 0;
}


/*-------------------------------------------------------------------------*/
/* The main reason to have this class is to make mdev/udev create the
 * /dev/spidevB.C character device nodes exposing our userspace API.
 * It also simplifies memory management.
 */
static struct class *tdmbdev_class;
/*-------------------------------------------------------------------------*/

static struct spi_driver tdmb_spi = {
	.driver = {
		.name = 	"tdmb",
		.owner =	THIS_MODULE,
	},
	.probe =	tdmb_probe,
	.remove = NULL,
	.suspend = NULL,
	.resume = NULL,

	/* NOTE:  suspend/resume methods are not necessary here.
	 * We don't do anything except pass the requests to/from
	 * the underlying controller.  The refrigerator handles
	 * most issues; the controller driver handles the rest.
	 */
};

struct file_operations tdmb_fops =
{
	.owner	  = THIS_MODULE,
	.ioctl	  = tdmb_ioctl,
	.read	  = tdmb_read,
	.write	  = tdmb_write,
	.poll	  = tdmb_poll,
	.open	  = tdmb_open,
	.release  = tdmb_release,
	
};

int tdmb_init(void)
{
	int result;
	struct device *dev;

	INC_MSG_PRINTF(1,"######## [%s] %s Start!! ##################\n", __FILE__, __func__);
	result = register_chrdev( TDMB_DEV_MAJOR, TDMB_DEV_NAME, &tdmb_fops);
	if (result < 0) {
		INC_MSG_PRINTF(1, "[%s] unable to get major %d for fb devs\n", __func__, TDMB_DEV_MAJOR);
		return result;
	}

	tdmbdev_class = class_create(THIS_MODULE, TDMB_DEV_NAME);
	if (IS_ERR(tdmbdev_class)) {
		INC_MSG_PRINTF(1, "[%s] Unable to create tdmbdev_class; errno = %ld\n", 
			__func__, PTR_ERR(tdmbdev_class));
		unregister_chrdev(TDMB_DEV_MAJOR, TDMB_DEV_NAME);
		return PTR_ERR(tdmbdev_class);
	}

	result = spi_register_driver(&tdmb_spi);
	if (result < 0) {
		INC_MSG_PRINTF(1, "[%s] Unable spi_register_driver; result = %ld\n", 
			__func__, result);
		class_destroy(tdmbdev_class);
		unregister_chrdev(TDMB_DEV_MAJOR, TDMB_DEV_NAME);
	}

	dev = device_create(tdmbdev_class, NULL, MKDEV(TDMB_DEV_MAJOR, TDMB_IRQ),
						NULL, "tdmb%d.%d", 0, 1);

	if (IS_ERR(dev)) {
		INC_MSG_PRINTF(1, "[%s] Unable to create device for framebuffer ; errno = %ld\n",
			__func__, PTR_ERR(dev));
		unregister_chrdev(TDMB_DEV_MAJOR, TDMB_DEV_NAME);
		return PTR_ERR(dev);
	}
	INC_MSG_PRINTF(1,"######## [%s] %s Success!! ################\n", __FILE__, __func__);

#if !defined(CONFIG_MX100) /* ktj move to tdmb_open */
	INC_GPIO_DMBEnable();
#endif
	init_waitqueue_head(&WaitQueue_Read);		// [S5PV210_Kernel], 20101221, ASJ, 
	return 0;
}
	
void tdmb_exit(void)
{
	INC_MSG_PRINTF(1, "[%s] %s \n", __FILE__, __func__);
	unregister_chrdev( TDMB_DEV_MAJOR, TDMB_DEV_NAME );
	//free_irq(g_stTdmb_Int.tdmb_irq, NULL);		// [S5PV210_Kernel], 20101220, ASJ, 
}

module_init(tdmb_init);
module_exit(tdmb_exit);

MODULE_LICENSE("Dual BSD/GPL");

