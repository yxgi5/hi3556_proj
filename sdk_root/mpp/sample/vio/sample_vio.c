
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "sample_comm.h"


/******************************************************************************
* function : show usage
******************************************************************************/
HI_VOID SAMPLE_VIO_VoInterface_Usage(HI_VOID)
{
    printf("intf:\n");
    printf("\t 0) vo HDMI output, default.\n");
    printf("\t 1) vo BT1120 output.\n");
}

static void delay_ms(int ms)
{
    usleep(ms * 1000);
}

HI_S32 SetStaticStatisticsCfg(VI_PIPE ViPipe)
{
    HI_S32 i, j = 0;
    HI_S32 s32Ret = HI_SUCCESS;

    ISP_STATISTICS_CFG_S stStatisticsCfg;

    s32Ret = HI_MPI_ISP_GetStatisticsConfig(ViPipe, &stStatisticsCfg);
    
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("GetStaticStatisticsCfg failed!\n");
        return s32Ret;
    }

    //if (HI_FALSE == bMetryFixed)
    {
        for (i = 0; i < AE_ZONE_ROW; i++)
        {
            for (j = 0; j < AE_ZONE_COLUMN; j++)
            {
                stStatisticsCfg.stAECfg.au8Weight[i][j] = 15;
            }
        }
    }

    stStatisticsCfg.unKey.bit1FEAeGloStat = 1;
    stStatisticsCfg.unKey.bit1FEAeLocStat = 1;
    stStatisticsCfg.unKey.bit1FEAeStiGloStat = 1;
    stStatisticsCfg.unKey.bit1FEAeStiLocStat = 1;
    stStatisticsCfg.unKey.bit1BEAeGloStat = 1;
    stStatisticsCfg.unKey.bit1BEAeLocStat = 1;
    stStatisticsCfg.unKey.bit1BEAeStiGloStat = 1;
    stStatisticsCfg.unKey.bit1BEAeStiLocStat = 1;
    stStatisticsCfg.unKey.bit1AwbStat1 = 1;
    stStatisticsCfg.unKey.bit1AwbStat2 = 1;
    stStatisticsCfg.unKey.bit2Rsv0 = 3;
    stStatisticsCfg.unKey.bit1FEAfStat = 1;
    stStatisticsCfg.unKey.bit1BEAfStat = 1;
    stStatisticsCfg.unKey.bit2Rsv1 = 3;
    stStatisticsCfg.unKey.bit1Dehaze = 1;
    stStatisticsCfg.unKey.bit1MgStat = 1;
    stStatisticsCfg.unKey.bit14Rsv = 30;
    stStatisticsCfg.unKey.bit32IsrAccess = 1;

    s32Ret = HI_MPI_ISP_SetStatisticsConfig(ViPipe, &stStatisticsCfg);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("GetStaticStatisticsCfg failed!\n");
        return s32Ret;
    }
    
    return HI_SUCCESS;
}

void SAMPLE_VIO_Usage(char* sPrgNm)
{
    printf("Usage : %s <index> <intf>\n", sPrgNm);
    printf("index:\n");
    

    //P42@HiMPP V4.0 媒体处理软件开发参考.pdf
    //VI和VPSS的工作模式
    
    //parallel mode: 并行模式
    //当对接大数据量的时序，例如8K@30fps时，需要VI_CAP与两个VI_PROC处于并行模式，
    //VI_CAP直接把一帧数据送给两个VI_PROC并行处理
    //再给VPSS并行处理
    printf("\t 0)parallel SDR8     VI - VPSS - VO - HDMI.      Embeded isp, phychn channel preview.\n");

    //online mode：在线模式
    //对VI而言VI_CAP与VI_PROC之间在线数据流传输，此模式下VI_CAP不会写出RAW数据到DDR，而是直接把数据流送给VI_PROC。
    //对VPSS而言VI_PROC与VPSS之间的在线数据流传输，在此模式下VI_PROC不会写出YUV数据到DDR，而是直接把数据流送给VPSS。
    printf("\t 1)online   SDR8     VI - VPSS - VO - HDMI.      2 pipe, Embeded isp, phychn channel preview.\n");

    //offline mode：离线模式
    //对VI而言VI_CAP写出RAW数据到DDR，然后VI_PROC从DDR读取RAW数据进行后处理。
    //对VPSS而言VI_PROC写出YUV数据到DDR，然后VPSS从DDR读取YUV数据进行后处理。
    printf("\t 2)offline  SDR8     VI - VPSS - VO - HDMI.      4 pipe, Embeded isp, phychn channel preview.\n");
    printf("\t 3)online  WDR+HDR10 VI - VPSS - VO - HDMI.      Embeded isp, phychn channel preview.\n");
    printf("\t 4)online   SDR8     VI - VO - HDMI.             Embeded isp, LDC+ROTATE.\n");
    printf("\t 5)online   SDR8     VI - VO - HDMI.             Embeded isp, FreeRotation.\n");
    printf("\t 6)online   SDR8     VI - VO - HDMI.             Embeded isp, LDC+SPREAD.\n");
    printf("\t 7)online   SDR8     VI - VO - HDMI.             Embeded isp, LDC+SPREAD.\n");
	printf("\t 8)online   SDR8     VI - VO - HDMI.             Embeded isp, LDC+SPREAD.\n");
    
    printf("intf:\n");
    printf("\t 0) vo HDMI output, default.\n");
    printf("\t 1) vo BT1120 output.\n");
    return;
}


/******************************************************************************
* function : to process abnormal case
******************************************************************************/
void SAMPLE_VIO_HandleSig(HI_S32 signo)
{
    if (SIGINT == signo || SIGTERM == signo)
    {
        SAMPLE_COMM_All_ISP_Stop();
        SAMPLE_COMM_SYS_Exit();
        printf("\033[0;31mprogram termination abnormally!\033[0;39m\n");
    }
    exit(-1);
}

HI_S32 SAMPLE_VIO_StartViVo(SAMPLE_VI_CONFIG_S* pstViConfig, SAMPLE_VO_CONFIG_S* pstVoConfig)
{
    HI_S32  s32Ret;
    //SAMPLE_VI_INFO_S*   pstViInfo = HI_NULL;
    //VI_PIPE             ViPipe;

    s32Ret = SAMPLE_COMM_VI_StartVi(pstViConfig);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start vi failed!\n");
        return s32Ret;
    }

    s32Ret = SAMPLE_COMM_VO_StartVO(pstVoConfig);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_VIO start VO failed with %#x!\n", s32Ret);
        goto EXIT;
    }

/*
	pstViInfo = &pstViConfig->astViInfo[0];
	ViPipe      = pstViInfo->stPipeInfo.aPipe[0];
	
    //s32Ret = ISP_SensorInit(ViPipe);
    delay_ms(100);
    ov9712_write_register(ViPipe, 0x2a, 0x9c);
    ov9712_write_register(ViPipe, 0x2b, 0x06);
    ov9712_write_register(ViPipe, 0x10, 0xf0);
    ov9712_write_register(ViPipe, 0x00, 0x3f);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start vi failed!\n");
        return s32Ret;
    }
*/
    return s32Ret;

EXIT:
    SAMPLE_COMM_VI_StopVi(pstViConfig);

    return s32Ret;
}

HI_S32 SAMPLE_VIO_StopViVo(SAMPLE_VI_CONFIG_S* pstViConfig, SAMPLE_VO_CONFIG_S* pstVoConfig)
{
    SAMPLE_COMM_VO_StopVO(pstVoConfig);

    SAMPLE_COMM_VI_StopVi(pstViConfig);

    return HI_SUCCESS;
}

//P43@HiMPP V4.0 媒体处理软件开发参考.pdf
//当对接大数据量的时序，例如8K@30fps时，
//需要VI_CAP与两个VI_PROC处于并行模式，
//VI_CAP直接把一帧数据送给两个VI_PROC并行处理。
HI_S32 SAMPLE_VIO_8K30_PARALLEL(VO_INTF_TYPE_E enVoIntfType)
{
    HI_S32                  s32Ret              = HI_SUCCESS;
    VI_DEV                  ViDev0              = 0;
    VI_PIPE                 ViPipe0             = 0;
    VI_CHN                  ViChn               = 0;
    HI_S32                  s32ViCnt            = 1;
    VPSS_GRP                VpssGrp0            = 0;
    VPSS_CHN                VpssChn[4]          = {VPSS_CHN0, VPSS_CHN1, VPSS_CHN2, VPSS_CHN3};
    VPSS_GRP_ATTR_S         stVpssGrpAttr       = {0};
    VPSS_CHN_ATTR_S         stVpssChnAttr[VPSS_MAX_PHY_CHN_NUM];
    HI_BOOL                 abChnEnable[VPSS_MAX_PHY_CHN_NUM] = {0};
    VO_DEV                  VoDev               = SAMPLE_VO_DEV_DHD0;
    VO_CHN                  VoChn               = 0;
    VO_INTF_SYNC_E          g_enIntfSync        = VO_OUTPUT_3840x2160_30;
    HI_U32                  g_u32DisBufLen      = 3;
    PIC_SIZE_E              enPicSize           = PIC_3840x2160;
    WDR_MODE_E              enWDRMode           = WDR_MODE_NONE;
    DYNAMIC_RANGE_E         enDynamicRange      = DYNAMIC_RANGE_SDR8;
    PIXEL_FORMAT_E          enPixFormat         = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    VIDEO_FORMAT_E          enVideoFormat       = VIDEO_FORMAT_LINEAR;
    COMPRESS_MODE_E         enCompressMode      = COMPRESS_MODE_NONE;
    VI_VPSS_MODE_E          enMastPipeMode      = VI_PARALLEL_VPSS_PARALLEL;
    SIZE_S                  stSize;
    HI_U32                  u32BlkSize;
    VB_CONFIG_S             stVbConf;
    SAMPLE_VI_CONFIG_S      stViConfig;
    SAMPLE_VO_CONFIG_S      stVoConfig;


    /************************************************
    step 1:  Get all sensors information, need one vi
        ,and need one mipi --
    *************************************************/
    SAMPLE_COMM_VI_GetSensorInfo(&stViConfig);
    stViConfig.s32WorkingViNum                           = s32ViCnt;

    stViConfig.as32WorkingViId[0]                        = 0;
    stViConfig.astViInfo[0].stSnsInfo.MipiDev            = SAMPLE_COMM_VI_GetComboDevBySensor(stViConfig.astViInfo[0].stSnsInfo.enSnsType, 0);
    stViConfig.astViInfo[0].stSnsInfo.s32BusId           = 0;

    stViConfig.astViInfo[0].stDevInfo.ViDev              = ViDev0;
    stViConfig.astViInfo[0].stDevInfo.enWDRMode          = enWDRMode;

    stViConfig.astViInfo[0].stPipeInfo.enMastPipeMode    = enMastPipeMode;
    stViConfig.astViInfo[0].stPipeInfo.aPipe[0]          = ViPipe0;
    stViConfig.astViInfo[0].stPipeInfo.aPipe[1]          = -1;
    stViConfig.astViInfo[0].stPipeInfo.aPipe[2]          = -1;
    stViConfig.astViInfo[0].stPipeInfo.aPipe[3]          = -1;

    stViConfig.astViInfo[0].stChnInfo.ViChn              = ViChn;
    stViConfig.astViInfo[0].stChnInfo.enPixFormat        = enPixFormat;
    stViConfig.astViInfo[0].stChnInfo.enDynamicRange     = enDynamicRange;
    stViConfig.astViInfo[0].stChnInfo.enVideoFormat      = enVideoFormat;
    stViConfig.astViInfo[0].stChnInfo.enCompressMode     = enCompressMode;

    /************************************************
    step 2:  Get  input size
    *************************************************/
    s32Ret = SAMPLE_COMM_VI_GetSizeBySensor(stViConfig.astViInfo[0].stSnsInfo.enSnsType, &enPicSize);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_GetSizeBySensor failed with %d!\n", s32Ret);
        return s32Ret;
    }

    s32Ret = SAMPLE_COMM_SYS_GetPicSize(enPicSize, &stSize);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed with %d!\n", s32Ret);
        return s32Ret;
    }

    /************************************************
    step3:  Init SYS and common VB
    *************************************************/
    hi_memset(&stVbConf, sizeof(VB_CONFIG_S), 0, sizeof(VB_CONFIG_S));
    stVbConf.u32MaxPoolCnt              = 2;

    u32BlkSize = COMMON_GetPicBufferSize(stSize.u32Width, stSize.u32Height, SAMPLE_PIXEL_FORMAT, DATA_BITWIDTH_10, COMPRESS_MODE_SEG, DEFAULT_ALIGN);
    stVbConf.astCommPool[0].u64BlkSize  = u32BlkSize;
    stVbConf.astCommPool[0].u32BlkCnt   = 10;

    u32BlkSize = VI_GetRawBufferSize(stSize.u32Width, stSize.u32Height, PIXEL_FORMAT_RGB_BAYER_16BPP, COMPRESS_MODE_NONE, DEFAULT_ALIGN);
    stVbConf.astCommPool[1].u64BlkSize  = u32BlkSize;
    stVbConf.astCommPool[1].u32BlkCnt   = 4;

    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("system init failed with %d!\n", s32Ret);
        goto EXIT;
    }

    s32Ret = SAMPLE_COMM_VI_SetParam(&stViConfig);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_SetParam failed with %d!\n", s32Ret);
        goto EXIT;
    }


    /************************************************
    step 4: start VI
    *************************************************/
    s32Ret = SAMPLE_COMM_VI_StartVi(&stViConfig);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_StartVi failed with %d!\n", s32Ret);
        goto EXIT3;
    }

    /************************************************
    step 5: start VPSS, need one grp
    *************************************************/
    stVpssGrpAttr.u32MaxW                        = stSize.u32Width;
    stVpssGrpAttr.u32MaxH                        = stSize.u32Height;
    stVpssGrpAttr.enPixelFormat                  = enPixFormat;
    stVpssGrpAttr.enDynamicRange                 = enDynamicRange;
    stVpssGrpAttr.stFrameRate.s32SrcFrameRate    = -1;
    stVpssGrpAttr.stFrameRate.s32DstFrameRate    = -1;

    abChnEnable[0]                               = HI_TRUE;
    stVpssChnAttr[0].u32Width                    = stSize.u32Width;
    stVpssChnAttr[0].u32Height                   = stSize.u32Height;
    stVpssChnAttr[0].enChnMode                   = VPSS_CHN_MODE_USER;
    stVpssChnAttr[0].enCompressMode              = enCompressMode;
    stVpssChnAttr[0].enDynamicRange              = enDynamicRange;
    stVpssChnAttr[0].enPixelFormat               = enPixFormat;
    stVpssChnAttr[0].enVideoFormat               = enVideoFormat;
    stVpssChnAttr[0].stFrameRate.s32SrcFrameRate = -1;
    stVpssChnAttr[0].stFrameRate.s32DstFrameRate = -1;
    stVpssChnAttr[0].u32Depth                    = 1;
    stVpssChnAttr[0].bMirror                     = HI_FALSE;
    stVpssChnAttr[0].bFlip                       = HI_FALSE;
    stVpssChnAttr[0].stAspectRatio.enMode        = ASPECT_RATIO_NONE;

    s32Ret = SAMPLE_COMM_VPSS_Start(VpssGrp0, abChnEnable, &stVpssGrpAttr, stVpssChnAttr);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VPSS_Start Grp0 failed with %d!\n", s32Ret);
        goto EXIT2;
    }

    /************************************************
    step 6:  VI bind VPSS, for total parallel, no need bind
    *************************************************/

    /************************************************
    step 7:  start V0
    *************************************************/
    SAMPLE_COMM_VO_GetDefConfig(&stVoConfig);
    stVoConfig.VoDev                                    = VoDev;
    stVoConfig.enVoIntfType                             = enVoIntfType;
    stVoConfig.enIntfSync                               = g_enIntfSync;
    stVoConfig.enPicSize                                = enPicSize;
    stVoConfig.u32DisBufLen                             = g_u32DisBufLen;
    stVoConfig.enDstDynamicRange                        = enDynamicRange;
    stVoConfig.enVoMode                                 = VO_MODE_1MUX;

    s32Ret = SAMPLE_COMM_VO_StartVO(&stVoConfig);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VO_StartVO failed with %d!\n", s32Ret);
        goto EXIT1;
    }
    /************************************************
    step 8:  VI bind VPSS bind VO
    *************************************************/
    s32Ret = SAMPLE_COMM_VI_Bind_VPSS(ViPipe0, ViChn, VpssGrp0);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_Bind_VPSS failed with %d!\n", s32Ret);
        goto EXIT1;
    }

    s32Ret = SAMPLE_COMM_VPSS_Bind_VO(VpssGrp0, VpssChn[0], stVoConfig.VoDev, VoChn);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VPSS_Bind_VO Grp0 failed with %d!\n", s32Ret);
        goto EXIT0;
    }

    PAUSE();

    SAMPLE_COMM_VPSS_UnBind_VO(VpssGrp0, VpssChn[0], stVoConfig.VoDev, VoChn);
EXIT0:
    SAMPLE_COMM_VI_UnBind_VPSS(ViPipe0, ViChn, VpssGrp0);
EXIT1:
    SAMPLE_COMM_VO_StopVO(&stVoConfig);
EXIT2:
    SAMPLE_COMM_VPSS_Stop(VpssGrp0, abChnEnable);
EXIT3:
    SAMPLE_COMM_VI_StopVi(&stViConfig);
EXIT:
    SAMPLE_COMM_SYS_Exit();

    return s32Ret;
}

HI_S32 SAMPLE_VIO_2X4K60_TotalOnline(VO_INTF_TYPE_E enVoIntfType)
{
    HI_S32                  s32Ret              = HI_SUCCESS;
    VI_DEV                  ViDev0              = 0;
    VI_DEV                  ViDev4              = 4;
    VI_PIPE                 ViPipe0             = 0;
    VI_PIPE                 ViPipe4             = 4;
    VI_CHN                  ViChn               = 0;
    HI_S32                  s32ViCnt            = 2;
    VPSS_GRP                VpssGrp0            = 0;
    VPSS_GRP                VpssGrp1            = 4;
    VPSS_CHN                VpssChn[4]          = {VPSS_CHN0, VPSS_CHN1, VPSS_CHN2, VPSS_CHN3};
    VPSS_GRP_ATTR_S         stVpssGrpAttr       = {0};
    VPSS_CHN_ATTR_S         stVpssChnAttr[VPSS_MAX_PHY_CHN_NUM];
    HI_BOOL                 abChnEnable[VPSS_MAX_PHY_CHN_NUM] = {0};
    VO_DEV                  VoDev               = SAMPLE_VO_DEV_DHD0;
    VO_CHN                  VoChn[2]            = {0, 1};
    VO_INTF_SYNC_E          g_enIntfSync        = VO_OUTPUT_1080P30;
    HI_U32                  g_u32DisBufLen      = 3;
    PIC_SIZE_E              enPicSize           = PIC_3840x2160;
    WDR_MODE_E              enWDRMode           = WDR_MODE_NONE;
    DYNAMIC_RANGE_E         enDynamicRange      = DYNAMIC_RANGE_SDR8;
    PIXEL_FORMAT_E          enPixFormat         = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    VIDEO_FORMAT_E          enVideoFormat       = VIDEO_FORMAT_LINEAR;
    COMPRESS_MODE_E         enCompressMode      = COMPRESS_MODE_NONE;
    VI_VPSS_MODE_E          enMastPipeMode      = VI_ONLINE_VPSS_ONLINE;
    SIZE_S                  stSize;
    HI_U32                  u32BlkSize;
    VB_CONFIG_S             stVbConf;
    SAMPLE_VI_CONFIG_S      stViConfig;
    SAMPLE_VO_CONFIG_S      stVoConfig;


    /************************************************
    step 1:  Get all sensors information, need two vi
        ,and need two mipi --
    *************************************************/
    SAMPLE_COMM_VI_GetSensorInfo(&stViConfig);
    stViConfig.s32WorkingViNum                           = s32ViCnt;

    stViConfig.as32WorkingViId[0]                        = 0;
    stViConfig.astViInfo[0].stSnsInfo.MipiDev            = SAMPLE_COMM_VI_GetComboDevBySensor(stViConfig.astViInfo[0].stSnsInfo.enSnsType, 0);
    stViConfig.astViInfo[0].stSnsInfo.s32BusId           = 0;

    stViConfig.astViInfo[0].stDevInfo.ViDev              = ViDev0;
    stViConfig.astViInfo[0].stDevInfo.enWDRMode          = enWDRMode;

    stViConfig.astViInfo[0].stPipeInfo.enMastPipeMode    = enMastPipeMode;
    stViConfig.astViInfo[0].stPipeInfo.aPipe[0]          = ViPipe0;
    stViConfig.astViInfo[0].stPipeInfo.aPipe[1]          = -1;
    stViConfig.astViInfo[0].stPipeInfo.aPipe[2]          = -1;
    stViConfig.astViInfo[0].stPipeInfo.aPipe[3]          = -1;

    stViConfig.astViInfo[0].stChnInfo.ViChn              = ViChn;
    stViConfig.astViInfo[0].stChnInfo.enPixFormat        = enPixFormat;
    stViConfig.astViInfo[0].stChnInfo.enDynamicRange     = enDynamicRange;
    stViConfig.astViInfo[0].stChnInfo.enVideoFormat      = enVideoFormat;
    stViConfig.astViInfo[0].stChnInfo.enCompressMode     = enCompressMode;

    stViConfig.as32WorkingViId[1]                        = 1;
    stViConfig.astViInfo[1].stSnsInfo.MipiDev            = SAMPLE_COMM_VI_GetComboDevBySensor(stViConfig.astViInfo[0].stSnsInfo.enSnsType, 2);
    stViConfig.astViInfo[1].stSnsInfo.s32BusId           = 2;

    stViConfig.astViInfo[1].stDevInfo.ViDev              = ViDev4;
    stViConfig.astViInfo[1].stDevInfo.enWDRMode          = enWDRMode;

    stViConfig.astViInfo[1].stPipeInfo.enMastPipeMode    = enMastPipeMode;
    stViConfig.astViInfo[1].stPipeInfo.aPipe[0]          = ViPipe4;
    stViConfig.astViInfo[1].stPipeInfo.aPipe[1]          = -1;
    stViConfig.astViInfo[1].stPipeInfo.aPipe[2]          = -1;
    stViConfig.astViInfo[1].stPipeInfo.aPipe[3]          = -1;

    stViConfig.astViInfo[1].stChnInfo.ViChn              = ViChn;
    stViConfig.astViInfo[1].stChnInfo.enPixFormat        = enPixFormat;
    stViConfig.astViInfo[1].stChnInfo.enDynamicRange     = enDynamicRange;
    stViConfig.astViInfo[1].stChnInfo.enVideoFormat      = enVideoFormat;
    stViConfig.astViInfo[1].stChnInfo.enCompressMode     = enCompressMode;
    /************************************************
    step 2:  Get  input size
    *************************************************/
    s32Ret = SAMPLE_COMM_VI_GetSizeBySensor(stViConfig.astViInfo[0].stSnsInfo.enSnsType, &enPicSize);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_GetSizeBySensor failed with %d!\n", s32Ret);
        return s32Ret;
    }

    s32Ret = SAMPLE_COMM_SYS_GetPicSize(enPicSize, &stSize);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed with %d!\n", s32Ret);
        return s32Ret;
    }

    /************************************************
    step3:  Init SYS and common VB
    *************************************************/
    hi_memset(&stVbConf, sizeof(VB_CONFIG_S), 0, sizeof(VB_CONFIG_S));
    stVbConf.u32MaxPoolCnt              = 2;

    u32BlkSize = COMMON_GetPicBufferSize(stSize.u32Width, stSize.u32Height, SAMPLE_PIXEL_FORMAT, DATA_BITWIDTH_10, COMPRESS_MODE_NONE, DEFAULT_ALIGN);
    stVbConf.astCommPool[0].u64BlkSize  = u32BlkSize;
    stVbConf.astCommPool[0].u32BlkCnt   = 10;

    u32BlkSize = VI_GetRawBufferSize(stSize.u32Width, stSize.u32Height, PIXEL_FORMAT_RGB_BAYER_16BPP, COMPRESS_MODE_NONE, DEFAULT_ALIGN);
    stVbConf.astCommPool[1].u64BlkSize  = u32BlkSize;
    stVbConf.astCommPool[1].u32BlkCnt   = 4;

    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("system init failed with %d!\n", s32Ret);
        goto EXIT;
    }

    s32Ret = SAMPLE_COMM_VI_SetParam(&stViConfig);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_SetParam failed with %d!\n", s32Ret);
        goto EXIT;
    }


    /************************************************
    step 4: start VI
    *************************************************/
    s32Ret = SAMPLE_COMM_VI_StartVi(&stViConfig);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_StartVi failed with %d!\n", s32Ret);
        goto EXIT5;
    }

    /************************************************
    step 5: start VPSS, need two grp
    *************************************************/
    stVpssGrpAttr.u32MaxW                        = stSize.u32Width;
    stVpssGrpAttr.u32MaxH                        = stSize.u32Height;
    stVpssGrpAttr.enPixelFormat                  = enPixFormat;
    stVpssGrpAttr.enDynamicRange                 = enDynamicRange;
    stVpssGrpAttr.stFrameRate.s32SrcFrameRate    = -1;
    stVpssGrpAttr.stFrameRate.s32DstFrameRate    = -1;

    abChnEnable[0]                               = HI_TRUE;
    stVpssChnAttr[0].u32Width                    = stSize.u32Width;
    stVpssChnAttr[0].u32Height                   = stSize.u32Height;
    stVpssChnAttr[0].enChnMode                   = VPSS_CHN_MODE_USER;
    stVpssChnAttr[0].enCompressMode              = enCompressMode;
    stVpssChnAttr[0].enDynamicRange              = enDynamicRange;
    stVpssChnAttr[0].enPixelFormat               = enPixFormat;
    stVpssChnAttr[0].enVideoFormat               = enVideoFormat;
    stVpssChnAttr[0].stFrameRate.s32SrcFrameRate = -1;
    stVpssChnAttr[0].stFrameRate.s32DstFrameRate = -1;
    stVpssChnAttr[0].u32Depth                    = 0;
    stVpssChnAttr[0].bMirror                     = HI_FALSE;
    stVpssChnAttr[0].bFlip                       = HI_FALSE;
    stVpssChnAttr[0].stAspectRatio.enMode        = ASPECT_RATIO_NONE;

    s32Ret = SAMPLE_COMM_VPSS_Start(VpssGrp0, abChnEnable, &stVpssGrpAttr, stVpssChnAttr);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VPSS_Start Grp0 failed with %d!\n", s32Ret);
        goto EXIT4;
    }

    s32Ret = SAMPLE_COMM_VPSS_Start(VpssGrp1, abChnEnable, &stVpssGrpAttr, stVpssChnAttr);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VPSS_Start Grp1 failed with %d!\n", s32Ret);
        goto EXIT3;
    }

    /************************************************
    step 6:  VI bind VPSS, for total online, no need bind
    *************************************************/

    /************************************************
    step 7:  start V0
    *************************************************/
    SAMPLE_COMM_VO_GetDefConfig(&stVoConfig);
    stVoConfig.VoDev                                    = VoDev;
    stVoConfig.enVoIntfType                             = enVoIntfType;
    stVoConfig.enIntfSync                               = g_enIntfSync;
    stVoConfig.enPicSize                                = enPicSize;
    stVoConfig.u32DisBufLen                             = g_u32DisBufLen;
    stVoConfig.enDstDynamicRange                        = enDynamicRange;
    stVoConfig.enVoMode                                 = VO_MODE_1MUX;

    s32Ret = SAMPLE_COMM_VO_StartVO(&stVoConfig);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VO_StartVO failed with %d!\n", s32Ret);
        goto EXIT2;
    }
    /************************************************
    step 8:  VO bind VPSS
    *************************************************/
    s32Ret = SAMPLE_COMM_VPSS_Bind_VO(VpssGrp0, VpssChn[0], stVoConfig.VoDev, VoChn[0]);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VPSS_Bind_VO Grp0 failed with %d!\n", s32Ret);
        goto EXIT2;
    }

//    s32Ret = SAMPLE_COMM_VPSS_Bind_VO(VpssGrp1, VpssChn[0], stVoConfig.VoDev, VoChn[0]);
//
//    if (HI_SUCCESS != s32Ret)
//    {
//        SAMPLE_PRT("SAMPLE_COMM_VPSS_Bind_VO Grp1 failed with %d!\n", s32Ret);
//        goto EXIT1;
//    }

    PAUSE();

//    SAMPLE_COMM_VPSS_UnBind_VO(VpssGrp1, VpssChn[0], stVoConfig.VoDev, VoChn[0]);
//EXIT1:
    SAMPLE_COMM_VPSS_UnBind_VO(VpssGrp0, VpssChn[0], stVoConfig.VoDev, VoChn[0]);
EXIT2:
    SAMPLE_COMM_VO_StopVO(&stVoConfig);
EXIT3:
    SAMPLE_COMM_VPSS_Stop(VpssGrp1, abChnEnable);
EXIT4:
    SAMPLE_COMM_VPSS_Stop(VpssGrp0, abChnEnable);
EXIT5:
    SAMPLE_COMM_VI_StopVi(&stViConfig);
EXIT:
    SAMPLE_COMM_SYS_Exit();

    return s32Ret;
}

HI_S32 SAMPLE_VIO_4x4K30_WBC(VO_INTF_TYPE_E enVoIntfType)
{
    HI_S32                  s32Ret              = HI_SUCCESS;
    HI_S32                  i                   = 0;
    VI_DEV                  ViDev[4]            = {0, 2, 4, 6};
    VI_PIPE                 ViPipe[4]           = {0, 2, 4, 6};
    VI_CHN                  ViChn               = 0;
    HI_S32                  s32ViCnt            = 4;
    HI_S32                  s32VpssChnCnt       = 4;
    VPSS_GRP                VpssGrp[4]          = {0, 2, 4, 6};
    VPSS_CHN                VpssChn[4]          = {VPSS_CHN0, VPSS_CHN1, VPSS_CHN2, VPSS_CHN3};
    VPSS_GRP_ATTR_S         stVpssGrpAttr       = {0};
    VPSS_CHN_ATTR_S         stVpssChnAttr[VPSS_MAX_PHY_CHN_NUM];
    HI_BOOL                 abChnEnable[VPSS_MAX_PHY_CHN_NUM] = {0};
    VO_DEV                  VoDev               = SAMPLE_VO_DEV_DHD0;
    VO_CHN                  VoChn[4]            = {0, 1, 2, 3};
    VO_INTF_SYNC_E          g_enIntfSync        = VO_OUTPUT_1080P30;
    HI_U32                  g_u32DisBufLen      = 3;
    PIC_SIZE_E              enPicSize           = PIC_3840x2160;
    WDR_MODE_E              enWDRMode           = WDR_MODE_NONE;
    DYNAMIC_RANGE_E         enDynamicRange      = DYNAMIC_RANGE_SDR8;
    PIXEL_FORMAT_E          enPixFormat         = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    VIDEO_FORMAT_E          enVideoFormat       = VIDEO_FORMAT_LINEAR;
    COMPRESS_MODE_E         enCompressMode      = COMPRESS_MODE_NONE;
    VI_VPSS_MODE_E          enMastPipeMode      = VI_OFFLINE_VPSS_ONLINE;
    SIZE_S                  stSize;
    HI_U32                  u32BlkSize;
    VB_CONFIG_S             stVbConf;
    SAMPLE_VI_CONFIG_S      stViConfig;
    SAMPLE_VO_CONFIG_S      stVoConfig;
    SAMPLE_VO_CONFIG_S      stVoConfig1;
    SAMPLE_VO_WBC_CONFIG    stWbcConfig;
    VI_VPSS_MODE_S          stVIVPSSMode;


    /************************************************
    step 1:  Get all sensors information
    *************************************************/
    SAMPLE_COMM_VI_GetSensorInfo(&stViConfig);
    stViConfig.s32WorkingViNum                           = s32ViCnt;

    stViConfig.as32WorkingViId[0]                        = 0;
    stViConfig.astViInfo[0].stSnsInfo.MipiDev            = SAMPLE_COMM_VI_GetComboDevBySensor(stViConfig.astViInfo[0].stSnsInfo.enSnsType, 0);
    stViConfig.astViInfo[0].stSnsInfo.s32BusId           = 0;

    stViConfig.astViInfo[0].stDevInfo.ViDev              = ViDev[0];
    stViConfig.astViInfo[0].stDevInfo.enWDRMode          = enWDRMode;

    stViConfig.astViInfo[0].stPipeInfo.enMastPipeMode    = enMastPipeMode;
    stViConfig.astViInfo[0].stPipeInfo.aPipe[0]          = ViPipe[0];
    stViConfig.astViInfo[0].stPipeInfo.aPipe[1]          = -1;
    stViConfig.astViInfo[0].stPipeInfo.aPipe[2]          = -1;
    stViConfig.astViInfo[0].stPipeInfo.aPipe[3]          = -1;

    stViConfig.astViInfo[0].stChnInfo.ViChn              = ViChn;
    stViConfig.astViInfo[0].stChnInfo.enPixFormat        = enPixFormat;
    stViConfig.astViInfo[0].stChnInfo.enDynamicRange     = enDynamicRange;
    stViConfig.astViInfo[0].stChnInfo.enVideoFormat      = enVideoFormat;
    stViConfig.astViInfo[0].stChnInfo.enCompressMode     = enCompressMode;

    stViConfig.as32WorkingViId[1]                        = 1;
    stViConfig.astViInfo[1].stSnsInfo.MipiDev            = SAMPLE_COMM_VI_GetComboDevBySensor(stViConfig.astViInfo[1].stSnsInfo.enSnsType, 1);
    stViConfig.astViInfo[1].stSnsInfo.s32BusId           = 1;

    stViConfig.astViInfo[1].stDevInfo.ViDev              = ViDev[1];
    stViConfig.astViInfo[1].stDevInfo.enWDRMode          = enWDRMode;

    stViConfig.astViInfo[1].stPipeInfo.enMastPipeMode    = enMastPipeMode;
    stViConfig.astViInfo[1].stPipeInfo.aPipe[0]          = ViPipe[1];
    stViConfig.astViInfo[1].stPipeInfo.aPipe[1]          = -1;
    stViConfig.astViInfo[1].stPipeInfo.aPipe[2]          = -1;
    stViConfig.astViInfo[1].stPipeInfo.aPipe[3]          = -1;

    stViConfig.astViInfo[1].stChnInfo.ViChn              = ViChn;
    stViConfig.astViInfo[1].stChnInfo.enPixFormat        = enPixFormat;
    stViConfig.astViInfo[1].stChnInfo.enDynamicRange     = enDynamicRange;
    stViConfig.astViInfo[1].stChnInfo.enVideoFormat      = enVideoFormat;
    stViConfig.astViInfo[1].stChnInfo.enCompressMode     = enCompressMode;

    stViConfig.as32WorkingViId[2]                        = 2;
    stViConfig.astViInfo[2].stSnsInfo.MipiDev            = SAMPLE_COMM_VI_GetComboDevBySensor(stViConfig.astViInfo[2].stSnsInfo.enSnsType, 2);
    stViConfig.astViInfo[2].stSnsInfo.s32BusId           = 2;

    stViConfig.astViInfo[2].stDevInfo.ViDev              = ViDev[2];
    stViConfig.astViInfo[2].stDevInfo.enWDRMode          = enWDRMode;

    stViConfig.astViInfo[2].stPipeInfo.enMastPipeMode    = enMastPipeMode;
    stViConfig.astViInfo[2].stPipeInfo.aPipe[0]          = ViPipe[2];
    stViConfig.astViInfo[2].stPipeInfo.aPipe[1]          = -1;
    stViConfig.astViInfo[2].stPipeInfo.aPipe[2]          = -1;
    stViConfig.astViInfo[2].stPipeInfo.aPipe[3]          = -1;

    stViConfig.astViInfo[2].stChnInfo.ViChn              = ViChn;
    stViConfig.astViInfo[2].stChnInfo.enPixFormat        = enPixFormat;
    stViConfig.astViInfo[2].stChnInfo.enDynamicRange     = enDynamicRange;
    stViConfig.astViInfo[2].stChnInfo.enVideoFormat      = enVideoFormat;
    stViConfig.astViInfo[2].stChnInfo.enCompressMode     = enCompressMode;

    stViConfig.as32WorkingViId[3]                        = 3;
    stViConfig.astViInfo[3].stSnsInfo.MipiDev            = SAMPLE_COMM_VI_GetComboDevBySensor(stViConfig.astViInfo[3].stSnsInfo.enSnsType, 3);
    stViConfig.astViInfo[3].stSnsInfo.s32BusId           = 3;

    stViConfig.astViInfo[3].stDevInfo.ViDev              = ViDev[3];
    stViConfig.astViInfo[3].stDevInfo.enWDRMode          = enWDRMode;

    stViConfig.astViInfo[3].stPipeInfo.enMastPipeMode    = enMastPipeMode;
    stViConfig.astViInfo[3].stPipeInfo.aPipe[0]          = ViPipe[3];
    stViConfig.astViInfo[3].stPipeInfo.aPipe[1]          = -1;
    stViConfig.astViInfo[3].stPipeInfo.aPipe[2]          = -1;
    stViConfig.astViInfo[3].stPipeInfo.aPipe[3]          = -1;

    stViConfig.astViInfo[3].stChnInfo.ViChn              = ViChn;
    stViConfig.astViInfo[3].stChnInfo.enPixFormat        = enPixFormat;
    stViConfig.astViInfo[3].stChnInfo.enDynamicRange     = enDynamicRange;
    stViConfig.astViInfo[3].stChnInfo.enVideoFormat      = enVideoFormat;
    stViConfig.astViInfo[3].stChnInfo.enCompressMode     = enCompressMode;


    /************************************************
    step 2:  Get  input size
    *************************************************/
    s32Ret = SAMPLE_COMM_VI_GetSizeBySensor(stViConfig.astViInfo[0].stSnsInfo.enSnsType, &enPicSize);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_GetSizeBySensor failed with %d!\n", s32Ret);
        return s32Ret;
    }

    s32Ret = SAMPLE_COMM_SYS_GetPicSize(enPicSize, &stSize);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed with %d!\n", s32Ret);
        return s32Ret;
    }

    /************************************************
    step3:  Init SYS and common VB
    *************************************************/
    hi_memset(&stVbConf, sizeof(VB_CONFIG_S), 0, sizeof(VB_CONFIG_S));
    stVbConf.u32MaxPoolCnt              = 2;

    u32BlkSize = COMMON_GetPicBufferSize(stSize.u32Width, stSize.u32Height, SAMPLE_PIXEL_FORMAT, DATA_BITWIDTH_10, COMPRESS_MODE_NONE, DEFAULT_ALIGN);
    stVbConf.astCommPool[0].u64BlkSize  = u32BlkSize;
    stVbConf.astCommPool[0].u32BlkCnt   = 30;

    u32BlkSize = VI_GetRawBufferSize(stSize.u32Width, stSize.u32Height, PIXEL_FORMAT_RGB_BAYER_16BPP, COMPRESS_MODE_NONE, DEFAULT_ALIGN);
    stVbConf.astCommPool[1].u64BlkSize  = u32BlkSize;
    stVbConf.astCommPool[1].u32BlkCnt   = 15;

    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("system init failed with %d!\n", s32Ret);
        goto EXIT;
    }

    for(i = 0;i < VI_MAX_PIPE_NUM;i++)
    {
        stVIVPSSMode.aenMode[i] = VI_OFFLINE_VPSS_ONLINE;
    }

    s32Ret = HI_MPI_SYS_SetVIVPSSMode(&stVIVPSSMode);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_SYS_SetVIVPSSMode failed with %d!\n", s32Ret);
        goto EXIT;
    }

    /************************************************
    step 4: start VI
    *************************************************/
    s32Ret = SAMPLE_COMM_VI_StartVi(&stViConfig);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_StartVi failed with %d!\n", s32Ret);
        goto EXIT4;
    }

    /************************************************
    step 5: start VPSS
    *************************************************/
    stVpssGrpAttr.u32MaxW                        = stSize.u32Width;
    stVpssGrpAttr.u32MaxH                        = stSize.u32Height;
    stVpssGrpAttr.enPixelFormat                  = enPixFormat;
    stVpssGrpAttr.enDynamicRange                 = enDynamicRange;
    stVpssGrpAttr.stFrameRate.s32SrcFrameRate    = -1;
    stVpssGrpAttr.stFrameRate.s32DstFrameRate    = -1;

    abChnEnable[0]                               = HI_TRUE;
    stVpssChnAttr[0].u32Width                    = stSize.u32Width;
    stVpssChnAttr[0].u32Height                   = stSize.u32Height;
    stVpssChnAttr[0].enChnMode                   = VPSS_CHN_MODE_USER;
    stVpssChnAttr[0].enCompressMode              = enCompressMode;
    stVpssChnAttr[0].enDynamicRange              = enDynamicRange;
    stVpssChnAttr[0].enPixelFormat               = enPixFormat;
    stVpssChnAttr[0].enVideoFormat               = enVideoFormat;
    stVpssChnAttr[0].stFrameRate.s32SrcFrameRate = -1;
    stVpssChnAttr[0].stFrameRate.s32DstFrameRate = -1;
    stVpssChnAttr[0].u32Depth                    = 1;
    stVpssChnAttr[0].bMirror                     = HI_FALSE;
    stVpssChnAttr[0].bFlip                       = HI_FALSE;
    stVpssChnAttr[0].stAspectRatio.enMode        = ASPECT_RATIO_NONE;

    for (i = 0; i < s32VpssChnCnt; i++)
    {
        s32Ret = SAMPLE_COMM_VPSS_Start(VpssGrp[i], abChnEnable, &stVpssGrpAttr, stVpssChnAttr);

        if (HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("SAMPLE_COMM_VPSS_Start failed with %d!\n", s32Ret);
            goto EXIT4;
        }
    }

    /************************************************
    step 6:  VI bind VPSS
    *************************************************/
    for (i = 0; i < s32VpssChnCnt; i++)
    {
        s32Ret = SAMPLE_COMM_VI_Bind_VPSS(ViPipe[i], ViChn, VpssGrp[i]);

        if (HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("SAMPLE_COMM_VI_Bind_VPSS failed with %d!\n", s32Ret);
            goto EXIT3;
        }
    }

    /************************************************
    step 7:  start V0
    *************************************************/
    SAMPLE_COMM_VO_GetDefConfig(&stVoConfig);
    stVoConfig.VoDev                                    = VoDev;
    stVoConfig.enVoIntfType                             = enVoIntfType;
    stVoConfig.enIntfSync                               = g_enIntfSync;
    stVoConfig.enPicSize                                = enPicSize;
    stVoConfig.u32DisBufLen                             = g_u32DisBufLen;
    stVoConfig.enDstDynamicRange                        = enDynamicRange;
    stVoConfig.enVoMode                                 = VO_MODE_1MUX;

    s32Ret = SAMPLE_COMM_VO_StartVO(&stVoConfig);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VO_StartVO failed with %d!\n", s32Ret);
        goto EXIT2;
    }
    /************************************************
    step 8:  VO bind VPSS
    *************************************************/
    //for (i = 0; i < s32VpssChnCnt; i++)
    for (i = 0; i < 1; i++)
    {
        s32Ret = SAMPLE_COMM_VPSS_Bind_VO(VpssGrp[i], VpssChn[0], stVoConfig.VoDev, VoChn[0]);

        if (HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("SAMPLE_COMM_VPSS_Bind_VO Grp%d failed with %d!\n", i, s32Ret);
            goto EXIT2;
        }
    }

    /************************************************
    step 9:  start V0 device 1
    *************************************************/
    SAMPLE_COMM_VO_GetDefConfig(&stVoConfig1);
    stVoConfig1.VoDev                                    = SAMPLE_VO_DEV_DHD1;
    stVoConfig1.enVoIntfType                             = VO_INTF_BT1120;
    stVoConfig1.u32BgColor                               = COLOR_RGB_CYN;

    s32Ret = SAMPLE_COMM_VO_StartVO(&stVoConfig1);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VO_StartVO failed with %d!\n", s32Ret);
        goto EXIT2;
    }
    /************************************************
    step 10:  start WBC device 0
    *************************************************/
    SAMPLE_COMM_VO_GetDefWBCConfig(&stWbcConfig);
    s32Ret = SAMPLE_COMM_VO_StartWBC(&stWbcConfig);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VO_StartWBC failed with %d!\n", s32Ret);
        goto EXIT1;
    }
    /************************************************
    step 11: WBC bind VO
    *************************************************/
    s32Ret = SAMPLE_COMM_VO_Bind_VO(stWbcConfig.VoWbc,0,stVoConfig1.VoDev,0);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VO_Bind_VO failed with %d!\n", s32Ret);
        goto EXIT0;
    }

    PAUSE();

    //for (i = 0; i < s32VpssChnCnt; i++)
    for (i = 0; i < 1; i++)
    {
        SAMPLE_COMM_VPSS_UnBind_VO(VpssGrp[i], VpssChn[0], stVoConfig.VoDev, VoChn[0]);
    }
EXIT0:
    SAMPLE_COMM_VO_StopWBC(&stWbcConfig);
EXIT1:
    SAMPLE_COMM_VO_StopVO(&stVoConfig1);
EXIT2:
    SAMPLE_COMM_VO_StopVO(&stVoConfig);
EXIT3:
    for (i = 0; i < s32VpssChnCnt; i++)
    {
        SAMPLE_COMM_VPSS_Stop(VpssGrp[i], abChnEnable);
    }
EXIT4:
    SAMPLE_COMM_VI_StopVi(&stViConfig);
EXIT:
    SAMPLE_COMM_SYS_Exit();

    return s32Ret;
}



HI_S32 SAMPLE_VIO_StartViVpssVo(SAMPLE_VI_CONFIG_S stViConfig, VPSS_GRP_ATTR_S stVpssGrpAttr, VPSS_CHN_ATTR_S *pastVpssChnAttr, SAMPLE_VO_CONFIG_S stVoConfig)
{
    HI_S32   s32Ret = HI_SUCCESS;

    VI_PIPE  ViPipe  = 0;
    VI_CHN   ViChn   = 0;

    VPSS_GRP VpssGrp = 0;
    VPSS_CHN VpssChn = VPSS_CHN0;
    HI_BOOL  abChnEnable[VPSS_MAX_PHY_CHN_NUM] = {0};

    VO_CHN   VoChn   = 0;

    /*set vi param*/
    s32Ret = SAMPLE_COMM_VI_SetParam(&stViConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("set vi param failed!\n");
        return s32Ret;
    }

    /*start vi*/
    s32Ret = SAMPLE_COMM_VI_StartVi(&stViConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start vi failed.s32Ret:0x%x !\n", s32Ret);
        return s32Ret;
    }

    /*start vpss*/
    abChnEnable[0] = HI_TRUE;

    s32Ret = SAMPLE_COMM_VPSS_Start(VpssGrp, abChnEnable, &stVpssGrpAttr, pastVpssChnAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start vpss group failed. s32Ret: 0x%x !\n", s32Ret);
        goto EXIT1;
    }

    /*vi bind vpss*/
    s32Ret = SAMPLE_COMM_VI_Bind_VPSS(ViPipe, ViChn, VpssGrp);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("vi bind vpss failed. s32Ret: 0x%x !\n", s32Ret);
        goto EXIT2;
    }

    /*start vo*/
    s32Ret = SAMPLE_COMM_VO_StartVO(&stVoConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start vo failed. s32Ret: 0x%x !\n", s32Ret);
        goto EXIT3;
    }

    /*vpss bind vo*/
    s32Ret = SAMPLE_COMM_VPSS_Bind_VO(VpssGrp, VpssChn, stVoConfig.VoDev, VoChn);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("vo bind vpss failed. s32Ret: 0x%x !\n", s32Ret);
        goto EXIT4;
    }

    goto EXIT;

    //SAMPLE_COMM_VPSS_UnBind_VO(VpssGrp, VpssChn, stVoConfig.VoDev, VoChn); /* unreachable */
EXIT4:
    SAMPLE_COMM_VO_StopVO(&stVoConfig);
EXIT3:
    SAMPLE_COMM_VI_UnBind_VPSS(ViPipe, ViChn, VpssGrp);
EXIT2:
    SAMPLE_COMM_VPSS_Stop(VpssGrp, abChnEnable);
EXIT1:
    SAMPLE_COMM_VI_StopVi(&stViConfig);
EXIT:
    return HI_SUCCESS;
}

HI_S32 SAMPLE_VIO_4K30_WDR_HDR10(VO_INTF_TYPE_E enVoIntfType)
{
    HI_S32             s32Ret;

    HI_S32             s32ViCnt       = 1;
    VI_DEV             ViDev          = 0;
    VI_PIPE            ViPipe0        = 0;
    VI_CHN             ViChn          = 0;
    HI_S32             s32WorkSnsId   = 0;
    SAMPLE_VI_CONFIG_S stViConfig;
    combo_dev_t        ComboDev;
    SAMPLE_PIPE_INFO_S stPipeInfo;

    SIZE_S             stSize;
    VB_CONFIG_S        stVbConf;
    PIC_SIZE_E         enPicSize      = PIC_3840x2160;
    HI_U32             u32BlkSize;

    VO_CHN             VoChn          = 0;
    SAMPLE_VO_CONFIG_S stVoConfig;

    WDR_MODE_E         enWDRMode      = WDR_MODE_NONE;
    DYNAMIC_RANGE_E    enDynamicRange = DYNAMIC_RANGE_HDR10;
    PIXEL_FORMAT_E     enPixFormat    = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    VIDEO_FORMAT_E     enVideoFormat  = VIDEO_FORMAT_LINEAR;
    COMPRESS_MODE_E    enCompressMode = COMPRESS_MODE_NONE;

    VPSS_GRP           VpssGrp        = 0;
    VPSS_GRP_ATTR_S    stVpssGrpAttr;
    VPSS_CHN           VpssChn        = VPSS_CHN0;
    HI_BOOL            abChnEnable[VPSS_MAX_PHY_CHN_NUM] = {0};
    VPSS_CHN_ATTR_S    astVpssChnAttr[VPSS_MAX_PHY_CHN_NUM];
    VPSS_MOD_PARAM_S   stModParam;

    /*config vi*/
    SAMPLE_COMM_VI_GetSensorInfo(&stViConfig);
    ComboDev = SAMPLE_COMM_VI_GetComboDevBySensor(stViConfig.astViInfo[s32WorkSnsId].stSnsInfo.enSnsType, s32WorkSnsId);

    stViConfig.s32WorkingViNum    = s32ViCnt;
    stViConfig.as32WorkingViId[0] = 0;

    stViConfig.astViInfo[s32WorkSnsId].stSnsInfo.MipiDev         = ComboDev;
    stViConfig.astViInfo[s32WorkSnsId].stSnsInfo.s32BusId        = 0;

    /*get WDR mode*/
    s32Ret = SAMPLE_COMM_VI_GetWDRModeBySensor(stViConfig.astViInfo[s32WorkSnsId].stSnsInfo.enSnsType, &enWDRMode);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("get WDR mode by sensor failed!\n");
        return s32Ret;
    }

    stViConfig.astViInfo[s32WorkSnsId].stDevInfo.ViDev           = ViDev;
    stViConfig.astViInfo[s32WorkSnsId].stDevInfo.enWDRMode       = enWDRMode;

    /*get pipe info*/
    s32Ret = SAMPLE_COMM_VI_GetPipeBySensor(stViConfig.astViInfo[s32WorkSnsId].stSnsInfo.enSnsType, &stPipeInfo);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("get pipe info by sensor failed!\n");
        return s32Ret;
    }

    stViConfig.astViInfo[s32WorkSnsId].stPipeInfo.enMastPipeMode = stPipeInfo.enMastPipeMode;
    stViConfig.astViInfo[s32WorkSnsId].stPipeInfo.aPipe[0]       = stPipeInfo.aPipe[0];
    stViConfig.astViInfo[s32WorkSnsId].stPipeInfo.aPipe[1]       = stPipeInfo.aPipe[1];
    stViConfig.astViInfo[s32WorkSnsId].stPipeInfo.aPipe[2]       = stPipeInfo.aPipe[2];
    stViConfig.astViInfo[s32WorkSnsId].stPipeInfo.aPipe[3]       = stPipeInfo.aPipe[3];
    ViPipe0 = stPipeInfo.aPipe[0];

    stViConfig.astViInfo[s32WorkSnsId].stChnInfo.ViChn           = ViChn;
    stViConfig.astViInfo[s32WorkSnsId].stChnInfo.enPixFormat     = enPixFormat;
    stViConfig.astViInfo[s32WorkSnsId].stChnInfo.enDynamicRange  = DYNAMIC_RANGE_XDR;
    stViConfig.astViInfo[s32WorkSnsId].stChnInfo.enVideoFormat   = enVideoFormat;
    stViConfig.astViInfo[s32WorkSnsId].stChnInfo.enCompressMode  = enCompressMode;

    /*get picture size*/
    s32Ret = SAMPLE_COMM_VI_GetSizeBySensor(stViConfig.astViInfo[s32WorkSnsId].stSnsInfo.enSnsType, &enPicSize);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("get picture size by sensor failed!\n");
        return s32Ret;
    }

    s32Ret = SAMPLE_COMM_SYS_GetPicSize(enPicSize, &stSize);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("get picture size failed!\n");
        return s32Ret;
    }

    /*config vb*/
    hi_memset(&stVbConf, sizeof(VB_CONFIG_S), 0, sizeof(VB_CONFIG_S));
    stVbConf.u32MaxPoolCnt              = 2;

    u32BlkSize = COMMON_GetPicBufferSize(stSize.u32Width, stSize.u32Height, SAMPLE_PIXEL_FORMAT, DATA_BITWIDTH_10, COMPRESS_MODE_SEG, DEFAULT_ALIGN);
    stVbConf.astCommPool[0].u64BlkSize  = u32BlkSize;
    stVbConf.astCommPool[0].u32BlkCnt   = 10;

    u32BlkSize = VI_GetRawBufferSize(stSize.u32Width, stSize.u32Height, PIXEL_FORMAT_RGB_BAYER_16BPP, COMPRESS_MODE_NONE, DEFAULT_ALIGN);
    stVbConf.astCommPool[1].u64BlkSize  = u32BlkSize;
    stVbConf.astCommPool[1].u32BlkCnt   = 4;

    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("system init failed with %d!\n", s32Ret);
        SAMPLE_COMM_SYS_Exit();
        return s32Ret;
    }

    /*config vpss*/
    stVpssGrpAttr.stFrameRate.s32SrcFrameRate    = -1;
    stVpssGrpAttr.stFrameRate.s32DstFrameRate    = -1;
    stVpssGrpAttr.enDynamicRange                 = DYNAMIC_RANGE_XDR;
    stVpssGrpAttr.enPixelFormat                  = enPixFormat;
    stVpssGrpAttr.u32MaxW                        = stSize.u32Width;
    stVpssGrpAttr.u32MaxH                        = stSize.u32Height;

    astVpssChnAttr[VpssChn].u32Width                    = stSize.u32Width;
    astVpssChnAttr[VpssChn].u32Height                   = stSize.u32Height;
    astVpssChnAttr[VpssChn].enChnMode                   = VPSS_CHN_MODE_USER;
    astVpssChnAttr[VpssChn].enCompressMode              = enCompressMode;
    astVpssChnAttr[VpssChn].enDynamicRange              = enDynamicRange;
    astVpssChnAttr[VpssChn].enVideoFormat               = enVideoFormat;
    astVpssChnAttr[VpssChn].enPixelFormat               = enPixFormat;
    astVpssChnAttr[VpssChn].stFrameRate.s32SrcFrameRate = 30;
    astVpssChnAttr[VpssChn].stFrameRate.s32DstFrameRate = 30;
    astVpssChnAttr[VpssChn].u32Depth                    = 0;
    astVpssChnAttr[VpssChn].bMirror                     = HI_FALSE;
    astVpssChnAttr[VpssChn].bFlip                       = HI_FALSE;
    astVpssChnAttr[VpssChn].stAspectRatio.enMode        = ASPECT_RATIO_NONE;

    /*config vo*/
    SAMPLE_COMM_VO_GetDefConfig(&stVoConfig);

    stVoConfig.enDstDynamicRange = enDynamicRange;
    stVoConfig.enIntfSync = VO_OUTPUT_1080P60;
    stVoConfig.enPicSize = PIC_3840x2160;
    stVoConfig.stImageSize.u32Width = 1920;
    stVoConfig.stImageSize.u32Height = 1080;
    stVoConfig.enVoIntfType = enVoIntfType;

    s32Ret = HI_MPI_VPSS_GetModParam(&stModParam);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("Get VPSS module param fali %#x!\n", s32Ret);
        SAMPLE_COMM_SYS_Exit();
        return s32Ret;
    }

    stModParam.bHdrSupport = HI_TRUE;

    s32Ret = HI_MPI_VPSS_SetModParam(&stModParam);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("Set VPSS module param fali %#x!\n", s32Ret);
        SAMPLE_COMM_SYS_Exit();
        return s32Ret;
    }


    s32Ret = SAMPLE_VIO_StartViVpssVo(stViConfig, stVpssGrpAttr, astVpssChnAttr, stVoConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_VIO_StartViVpssVo failed with %d!\n", s32Ret);
        SAMPLE_COMM_SYS_Exit();
        return s32Ret;
    }

#ifdef HDR2SDR
    printf("Press the Enter key to switch WDR mode to linear mode and switch HDR10 to SDR10.\n");
    getchar();

    SAMPLE_COMM_VPSS_UnBind_VO(VpssGrp, VpssChn, stVoConfig.VoDev, VoChn);
    SAMPLE_COMM_VO_StopVO(&stVoConfig);
    SAMPLE_COMM_VI_UnBind_VPSS(ViPipe0, ViChn, VpssGrp);
    SAMPLE_COMM_VPSS_Stop(VpssGrp, abChnEnable);
    SAMPLE_COMM_VI_StopVi(&stViConfig);

    /*change config*/
    stViConfig.astViInfo[s32WorkSnsId].stPipeInfo.aPipe[1]      = -1;
    stViConfig.astViInfo[s32WorkSnsId].stDevInfo.enWDRMode      = WDR_MODE_NONE;
    stViConfig.astViInfo[s32WorkSnsId].stChnInfo.enDynamicRange = DYNAMIC_RANGE_SDR10;

    stVpssGrpAttr.enDynamicRange = DYNAMIC_RANGE_SDR10;

    astVpssChnAttr[VpssChn].enDynamicRange = DYNAMIC_RANGE_SDR10;

    stVoConfig.enDstDynamicRange = DYNAMIC_RANGE_SDR10;

    s32Ret = SAMPLE_VIO_StartViVpssVo(stViConfig, stVpssGrpAttr, astVpssChnAttr, stVoConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_VIO_StartViVpssVo failed with %d!\n", s32Ret);
        SAMPLE_COMM_SYS_Exit();
        return s32Ret;
    }
#endif

    PAUSE();

    SAMPLE_COMM_VPSS_UnBind_VO(VpssGrp, VpssChn, stVoConfig.VoDev, VoChn);
    SAMPLE_COMM_VO_StopVO(&stVoConfig);
    SAMPLE_COMM_VI_UnBind_VPSS(ViPipe0, ViChn, VpssGrp);
    SAMPLE_COMM_VPSS_Stop(VpssGrp, abChnEnable);
    SAMPLE_COMM_VI_StopVi(&stViConfig);
    SAMPLE_COMM_SYS_Exit();

    return s32Ret;
}


HI_S32 SAMPLE_VIO_4K30_LDC_ROTATE(VO_INTF_TYPE_E enVoIntfType)
{
    HI_S32             s32Ret;
    VI_DEV             ViDev               = 0;
    VI_PIPE            ViPipe              = 0;
    VI_CHN             ViChn               = 0;
    HI_S32             s32WorkSnsId        = 0;
    VO_DEV             VoDev               = SAMPLE_VO_DEV_DHD0;
    VO_CHN             VoChn               = 0;
    SIZE_S             stSize;
    VB_CONFIG_S        stVbConf;
    PIC_SIZE_E         enPicSize           = PIC_3840x2160;
    HI_U32             u32BlkSize;
    VI_LDC_ATTR_S      stLDCAttr   = {0};
    SAMPLE_VI_CONFIG_S stViConfig  = {0};
    SAMPLE_VO_CONFIG_S stVoConfig  = {0};
    combo_dev_t          ComboDev;

    DYNAMIC_RANGE_E    enDynamicRange = DYNAMIC_RANGE_SDR8;
    PIXEL_FORMAT_E     enPixFormat    = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    VIDEO_FORMAT_E     enVideoFormat  = VIDEO_FORMAT_LINEAR;
    COMPRESS_MODE_E    enCompressMode = COMPRESS_MODE_NONE;
    VI_VPSS_MODE_E     enMastPipeMode = VI_ONLINE_VPSS_OFFLINE;

    /************************************************
    step1:  Get all sensors information
    *************************************************/
    SAMPLE_COMM_VI_GetSensorInfo(&stViConfig);
    ComboDev = SAMPLE_COMM_VI_GetComboDevBySensor(stViConfig.astViInfo[s32WorkSnsId].stSnsInfo.enSnsType, s32WorkSnsId);


    stViConfig.s32WorkingViNum                           = 1;

    stViConfig.as32WorkingViId[0]                        = 0;
    stViConfig.astViInfo[0].stSnsInfo.MipiDev            = ComboDev;
    stViConfig.astViInfo[0].stSnsInfo.s32BusId           = 0;

    stViConfig.astViInfo[0].stDevInfo.ViDev              = ViDev;
    stViConfig.astViInfo[0].stDevInfo.enWDRMode          = WDR_MODE_NONE;

    stViConfig.astViInfo[0].stPipeInfo.enMastPipeMode    = enMastPipeMode;
    stViConfig.astViInfo[0].stPipeInfo.aPipe[0]          = ViPipe;
    stViConfig.astViInfo[0].stPipeInfo.aPipe[1]          = -1;
    stViConfig.astViInfo[0].stPipeInfo.aPipe[2]          = -1;
    stViConfig.astViInfo[0].stPipeInfo.aPipe[3]          = -1;

    stViConfig.astViInfo[0].stChnInfo.ViChn              = ViChn;
    stViConfig.astViInfo[0].stChnInfo.enPixFormat        = enPixFormat;
    stViConfig.astViInfo[0].stChnInfo.enDynamicRange     = enDynamicRange;
    stViConfig.astViInfo[0].stChnInfo.enVideoFormat      = enVideoFormat;
    stViConfig.astViInfo[0].stChnInfo.enCompressMode     = enCompressMode;

    /************************************************
    step2:  Get  input size
    *************************************************/
    s32Ret = SAMPLE_COMM_VI_GetSizeBySensor(stViConfig.astViInfo[s32WorkSnsId].stSnsInfo.enSnsType, &enPicSize);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_GetSizeBySensor failed!\n");
        return s32Ret;
    }

    s32Ret = SAMPLE_COMM_SYS_GetPicSize(enPicSize, &stSize);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed!\n");
        return s32Ret;
    }

    /************************************************
    step3:  Init SYS and common VB
    *************************************************/

    hi_memset(&stVbConf, sizeof(VB_CONFIG_S), 0, sizeof(VB_CONFIG_S));
    stVbConf.u32MaxPoolCnt              = 2;

    u32BlkSize = COMMON_GetPicBufferSize(stSize.u32Width, stSize.u32Height, SAMPLE_PIXEL_FORMAT, DATA_BITWIDTH_10, COMPRESS_MODE_SEG, DEFAULT_ALIGN);
    stVbConf.astCommPool[0].u64BlkSize  = u32BlkSize;
    stVbConf.astCommPool[0].u32BlkCnt   = 10;

    u32BlkSize = VI_GetRawBufferSize(stSize.u32Width, stSize.u32Height, PIXEL_FORMAT_RGB_BAYER_16BPP, COMPRESS_MODE_NONE, DEFAULT_ALIGN);
    stVbConf.astCommPool[1].u64BlkSize  = u32BlkSize;
    stVbConf.astCommPool[1].u32BlkCnt   = 4;

    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("system init failed with %d!\n", s32Ret);
        SAMPLE_COMM_SYS_Exit();
        return s32Ret;
    }

    s32Ret = SAMPLE_COMM_VI_SetParam(&stViConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_COMM_SYS_Exit();
        return s32Ret;
    }


    /************************************************
    step4:  Init VI and VO
    *************************************************/
    SAMPLE_COMM_VO_GetDefConfig(&stVoConfig);
    s32Ret = SAMPLE_VIO_StartViVo(&stViConfig, &stVoConfig);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_VIO_StartViVo failed witfh %d\n", s32Ret);
        goto EXIT;
    }


    /************************************************
    step5:  Bind VI and VO
    *************************************************/
    s32Ret = SAMPLE_COMM_VI_Bind_VO(ViPipe, ViChn, VoDev, VoChn);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_Bind_VO failed with %#x!\n", s32Ret);
        goto EXIT1;
    }

    stLDCAttr.bEnable = HI_TRUE;
    stLDCAttr.stAttr.bAspect = 0;
    stLDCAttr.stAttr.s32XRatio = 100;
    stLDCAttr.stAttr.s32YRatio = 100;
    stLDCAttr.stAttr.s32XYRatio = 100;
    stLDCAttr.stAttr.s32CenterXOffset = 0;
    stLDCAttr.stAttr.s32CenterYOffset = 0;
    stLDCAttr.stAttr.s32DistortionRatio = 500;

    s32Ret = HI_MPI_VI_SetChnLDCAttr(ViPipe,ViChn,&stLDCAttr);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VI_SetChnLDCAttr failed witfh %d\n", s32Ret);
        goto EXIT;
    }


    printf("now LDC on, press any key to switch LDC Off!\n");
    getchar();

    stLDCAttr.bEnable = HI_FALSE;
    stLDCAttr.stAttr.bAspect = 0;
    stLDCAttr.stAttr.s32XRatio = 100;
    stLDCAttr.stAttr.s32YRatio = 100;
    stLDCAttr.stAttr.s32XYRatio = 100;
    stLDCAttr.stAttr.s32CenterXOffset = 0;
    stLDCAttr.stAttr.s32CenterYOffset = 0;
    stLDCAttr.stAttr.s32DistortionRatio = 300;

    s32Ret = HI_MPI_VI_SetChnLDCAttr(ViPipe,ViChn,&stLDCAttr);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VI_SetChnLDCAttr failed witfh %d\n", s32Ret);
        goto EXIT;
    }

    printf("now LDC off, press any key to switch LDC On!\n");
    getchar();

    stLDCAttr.bEnable = HI_TRUE;
    stLDCAttr.stAttr.bAspect = 0;
    stLDCAttr.stAttr.s32XRatio = 100;
    stLDCAttr.stAttr.s32YRatio = 100;
    stLDCAttr.stAttr.s32XYRatio = 100;
    stLDCAttr.stAttr.s32CenterXOffset = 0;
    stLDCAttr.stAttr.s32CenterYOffset = 0;
    stLDCAttr.stAttr.s32DistortionRatio = 300;

    s32Ret = HI_MPI_VI_SetChnLDCAttr(ViPipe,ViChn,&stLDCAttr);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VI_SetChnLDCAttr failed witfh %d\n", s32Ret);
        goto EXIT;
    }


    printf("now LDC on and Rotation 0, press any key to switch Rotation 90!\n");
    getchar();


    s32Ret = HI_MPI_VI_SetChnRotation(ViPipe,ViChn,ROTATION_90);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VI_SetChnLDCAttr failed witfh %d\n", s32Ret);
        goto EXIT;
    }

    printf("now LDC on and Rotation 90, press any key to switch Rotation 180!\n");
    getchar();

    s32Ret = HI_MPI_VI_SetChnRotation(ViPipe,ViChn,ROTATION_180);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VI_SetChnLDCAttr failed witfh %d\n", s32Ret);
        goto EXIT;
    }

    printf("now LDC on and Rotation 180, press any key to switch Rotation 270!\n");
    getchar();

    s32Ret = HI_MPI_VI_SetChnRotation(ViPipe,ViChn,ROTATION_270);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VI_SetChnLDCAttr failed witfh %d\n", s32Ret);
        goto EXIT;
    }

    printf("now LDC on and Rotation 270, press any key to switch Rotation 0!\n");
    getchar();

    s32Ret = HI_MPI_VI_SetChnRotation(ViPipe,ViChn,ROTATION_0);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VI_SetChnLDCAttr failed witfh %d\n", s32Ret);
        goto EXIT;
    }


    PAUSE();

    SAMPLE_COMM_VI_UnBind_VO(ViPipe, ViChn, VoDev, VoChn);

    EXIT1:
    SAMPLE_VIO_StopViVo(&stViConfig, &stVoConfig);

    EXIT:
    SAMPLE_COMM_SYS_Exit();

    return s32Ret;
}



HI_S32 SAMPLE_VIO_4K30_FreeRotation(VO_INTF_TYPE_E enVoIntfType)
{
    HI_S32             s32Ret;
    VI_DEV             ViDev               = 0;
    VI_PIPE            ViPipe              = 0;
    VI_CHN             ViChn               = 0;
    HI_S32             s32WorkSnsId        = 0;
    VO_DEV             VoDev               = SAMPLE_VO_DEV_DHD0;
    VO_CHN             VoChn               = 0;
    SIZE_S             stSize;
    VB_CONFIG_S        stVbConf;
    PIC_SIZE_E         enPicSize           = PIC_3840x2160;
    HI_U32             u32BlkSize;
    VI_ROTATION_EX_ATTR_S stViRotationExAttr  = {0};
    SAMPLE_VI_CONFIG_S stViConfig  = {0};
    SAMPLE_VO_CONFIG_S stVoConfig  = {0};
    combo_dev_t          ComboDev;

    DYNAMIC_RANGE_E    enDynamicRange = DYNAMIC_RANGE_SDR8;
    PIXEL_FORMAT_E     enPixFormat    = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    VIDEO_FORMAT_E     enVideoFormat  = VIDEO_FORMAT_LINEAR;
    COMPRESS_MODE_E    enCompressMode = COMPRESS_MODE_NONE;
    VI_VPSS_MODE_E     enMastPipeMode = VI_ONLINE_VPSS_OFFLINE;

    /************************************************
    step1:  Get all sensors information
    *************************************************/
    SAMPLE_COMM_VI_GetSensorInfo(&stViConfig);
    ComboDev = SAMPLE_COMM_VI_GetComboDevBySensor(stViConfig.astViInfo[s32WorkSnsId].stSnsInfo.enSnsType, s32WorkSnsId);


    stViConfig.s32WorkingViNum                           = 1;

    stViConfig.as32WorkingViId[0]                        = 0;
    stViConfig.astViInfo[0].stSnsInfo.MipiDev            = ComboDev;
    stViConfig.astViInfo[0].stSnsInfo.s32BusId           = 0;

    stViConfig.astViInfo[0].stDevInfo.ViDev              = ViDev;
    stViConfig.astViInfo[0].stDevInfo.enWDRMode          = WDR_MODE_NONE;

    stViConfig.astViInfo[0].stPipeInfo.enMastPipeMode    = enMastPipeMode;
    stViConfig.astViInfo[0].stPipeInfo.aPipe[0]          = ViPipe;
    stViConfig.astViInfo[0].stPipeInfo.aPipe[1]          = -1;
    stViConfig.astViInfo[0].stPipeInfo.aPipe[2]          = -1;
    stViConfig.astViInfo[0].stPipeInfo.aPipe[3]          = -1;

    stViConfig.astViInfo[0].stChnInfo.ViChn              = ViChn;
    stViConfig.astViInfo[0].stChnInfo.enPixFormat        = enPixFormat;
    stViConfig.astViInfo[0].stChnInfo.enDynamicRange     = enDynamicRange;
    stViConfig.astViInfo[0].stChnInfo.enVideoFormat      = enVideoFormat;
    stViConfig.astViInfo[0].stChnInfo.enCompressMode     = enCompressMode;

    /************************************************
    step2:  Get  input size
    *************************************************/
    s32Ret = SAMPLE_COMM_VI_GetSizeBySensor(stViConfig.astViInfo[s32WorkSnsId].stSnsInfo.enSnsType, &enPicSize);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_GetSizeBySensor failed!\n");
        return s32Ret;
    }

    s32Ret = SAMPLE_COMM_SYS_GetPicSize(enPicSize, &stSize);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed!\n");
        return s32Ret;
    }

    /************************************************
    step3:  Init SYS and common VB
    *************************************************/
    hi_memset(&stVbConf, sizeof(VB_CONFIG_S), 0, sizeof(VB_CONFIG_S));
    stVbConf.u32MaxPoolCnt              = 2;

    u32BlkSize = COMMON_GetPicBufferSize(stSize.u32Width, stSize.u32Height, SAMPLE_PIXEL_FORMAT, DATA_BITWIDTH_10, COMPRESS_MODE_SEG, DEFAULT_ALIGN);
    stVbConf.astCommPool[0].u64BlkSize  = u32BlkSize;
    stVbConf.astCommPool[0].u32BlkCnt   = 10;

    u32BlkSize = VI_GetRawBufferSize(stSize.u32Width, stSize.u32Height, PIXEL_FORMAT_RGB_BAYER_16BPP, COMPRESS_MODE_NONE, DEFAULT_ALIGN);
    stVbConf.astCommPool[1].u64BlkSize  = u32BlkSize;
    stVbConf.astCommPool[1].u32BlkCnt   = 4;

    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("system init failed with %d!\n", s32Ret);
        SAMPLE_COMM_SYS_Exit();
        return s32Ret;
    }

    s32Ret = SAMPLE_COMM_VI_SetParam(&stViConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_COMM_SYS_Exit();
        return s32Ret;
    }


    /************************************************
    step4:  Init VI and VO
    *************************************************/
    SAMPLE_COMM_VO_GetDefConfig(&stVoConfig);
    s32Ret = SAMPLE_VIO_StartViVo(&stViConfig, &stVoConfig);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_VIO_StartViVo failed witfh %d\n", s32Ret);
        goto EXIT;
    }


    /************************************************
    step5:  Bind VI and VO
    *************************************************/
    s32Ret = SAMPLE_COMM_VI_Bind_VO(ViPipe, ViChn, VoDev, VoChn);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_Bind_VO failed with %#x!\n", s32Ret);
        goto EXIT1;
    }

    stViRotationExAttr.bEnable                           = HI_TRUE;
    stViRotationExAttr.stRotationEx.enViewType           = ROTATION_VIEW_TYPE_ALL;
    stViRotationExAttr.stRotationEx.s32CenterXOffset     = 0;
    stViRotationExAttr.stRotationEx.s32CenterYOffset     = 0;
    stViRotationExAttr.stRotationEx.u32Angle             = 76;
    stViRotationExAttr.stRotationEx.stDestSize.u32Width  = stSize.u32Width;
    stViRotationExAttr.stRotationEx.stDestSize.u32Height = stSize.u32Height;

    s32Ret = HI_MPI_VI_SetChnRotationEx(ViPipe,ViChn,&stViRotationExAttr);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VI_SetChnRotationEx failed witfh %d\n", s32Ret);
        goto EXIT;
    }


    printf("now FreeAngle Rotation on and enViewType ALL, press any key to switch enViewType TYPICAL!\n");
    getchar();

    stViRotationExAttr.bEnable                           = HI_TRUE;
    stViRotationExAttr.stRotationEx.enViewType           = ROTATION_VIEW_TYPE_TYPICAL;
    stViRotationExAttr.stRotationEx.s32CenterXOffset     = 0;
    stViRotationExAttr.stRotationEx.s32CenterYOffset     = 0;
    stViRotationExAttr.stRotationEx.u32Angle             = 76;
    stViRotationExAttr.stRotationEx.stDestSize.u32Width  = stSize.u32Width;
    stViRotationExAttr.stRotationEx.stDestSize.u32Height = stSize.u32Height;

    s32Ret = HI_MPI_VI_SetChnRotationEx(ViPipe,ViChn,&stViRotationExAttr);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VI_SetChnRotationEx failed witfh %d\n", s32Ret);
        goto EXIT;
    }


    printf("now FreeAngle Rotation on, press any key to switch enViewType INSIDE!\n");
    getchar();

    stViRotationExAttr.bEnable                           = HI_TRUE;
    stViRotationExAttr.stRotationEx.enViewType           = ROTATION_VIEW_TYPE_INSIDE;
    stViRotationExAttr.stRotationEx.s32CenterXOffset     = 0;
    stViRotationExAttr.stRotationEx.s32CenterYOffset     = 0;
    stViRotationExAttr.stRotationEx.u32Angle             = 76;
    stViRotationExAttr.stRotationEx.stDestSize.u32Width  = stSize.u32Width;
    stViRotationExAttr.stRotationEx.stDestSize.u32Height = stSize.u32Height;

    s32Ret = HI_MPI_VI_SetChnRotationEx(ViPipe,ViChn,&stViRotationExAttr);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VI_SetChnRotationEx failed witfh %d\n", s32Ret);
        goto EXIT;
    }


    printf("now FreeAngle Rotation on, press any key to switch FreeAngle Rotation off!\n");
    getchar();

    stViRotationExAttr.bEnable                           = HI_FALSE;
    stViRotationExAttr.stRotationEx.enViewType           = ROTATION_VIEW_TYPE_INSIDE;
    stViRotationExAttr.stRotationEx.s32CenterXOffset     = 0;
    stViRotationExAttr.stRotationEx.s32CenterYOffset     = 0;
    stViRotationExAttr.stRotationEx.u32Angle             = 76;
    stViRotationExAttr.stRotationEx.stDestSize.u32Width  = stSize.u32Width;
    stViRotationExAttr.stRotationEx.stDestSize.u32Height = stSize.u32Height;

    s32Ret = HI_MPI_VI_SetChnRotationEx(ViPipe,ViChn,&stViRotationExAttr);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VI_SetChnRotationEx failed witfh %d\n", s32Ret);
        goto EXIT;
    }


    PAUSE();

    SAMPLE_COMM_VI_UnBind_VO(ViPipe, ViChn, VoDev, VoChn);

    EXIT1:
    SAMPLE_VIO_StopViVo(&stViConfig, &stVoConfig);

    EXIT:
    SAMPLE_COMM_SYS_Exit();

    return s32Ret;
}



HI_S32 SAMPLE_VIO_4K30_LDC_SPREAD(VO_INTF_TYPE_E enVoIntfType)
{
    HI_S32             s32Ret;
    VI_DEV             ViDev               = 0;
    VI_PIPE            ViPipe              = 0;
    VI_CHN             ViChn               = 0;
    HI_S32             s32WorkSnsId        = 0;
    VO_DEV             VoDev               = SAMPLE_VO_DEV_DHD0;
    VO_CHN             VoChn               = 0;
    SIZE_S             stSize;
    VB_CONFIG_S        stVbConf;
    PIC_SIZE_E         enPicSize           = PIC_3840x2160;
    HI_U32             u32BlkSize;
    VI_LDC_ATTR_S      stLDCAttr           = {0} ;
    SPREAD_ATTR_S      stSpreadAttr        = {0};
    SAMPLE_VI_CONFIG_S stViConfig  = {0};
    SAMPLE_VO_CONFIG_S stVoConfig  = {0};
    combo_dev_t          ComboDev;

    DYNAMIC_RANGE_E    enDynamicRange = DYNAMIC_RANGE_SDR8;
    PIXEL_FORMAT_E     enPixFormat    = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    VIDEO_FORMAT_E     enVideoFormat  = VIDEO_FORMAT_LINEAR;
    COMPRESS_MODE_E    enCompressMode = COMPRESS_MODE_NONE;
    VI_VPSS_MODE_E     enMastPipeMode = VI_ONLINE_VPSS_OFFLINE;

    /************************************************
    step1:  Get all sensors information
    *************************************************/
    SAMPLE_COMM_VI_GetSensorInfo(&stViConfig);
    ComboDev = SAMPLE_COMM_VI_GetComboDevBySensor(stViConfig.astViInfo[s32WorkSnsId].stSnsInfo.enSnsType, s32WorkSnsId);


    stViConfig.s32WorkingViNum                           = 1;

    stViConfig.as32WorkingViId[0]                        = 0;
    stViConfig.astViInfo[0].stSnsInfo.MipiDev            = ComboDev;
    stViConfig.astViInfo[0].stSnsInfo.s32BusId           = 0;

    stViConfig.astViInfo[0].stDevInfo.ViDev              = ViDev;
    stViConfig.astViInfo[0].stDevInfo.enWDRMode          = WDR_MODE_NONE;

    stViConfig.astViInfo[0].stPipeInfo.enMastPipeMode    = enMastPipeMode;
    stViConfig.astViInfo[0].stPipeInfo.aPipe[0]          = ViPipe;
    stViConfig.astViInfo[0].stPipeInfo.aPipe[1]          = -1;
    stViConfig.astViInfo[0].stPipeInfo.aPipe[2]          = -1;
    stViConfig.astViInfo[0].stPipeInfo.aPipe[3]          = -1;

    stViConfig.astViInfo[0].stChnInfo.ViChn              = ViChn;
    stViConfig.astViInfo[0].stChnInfo.enPixFormat        = enPixFormat;
    stViConfig.astViInfo[0].stChnInfo.enDynamicRange     = enDynamicRange;
    stViConfig.astViInfo[0].stChnInfo.enVideoFormat      = enVideoFormat;
    stViConfig.astViInfo[0].stChnInfo.enCompressMode     = enCompressMode;

    /************************************************
    step2:  Get  input size
    *************************************************/
    s32Ret = SAMPLE_COMM_VI_GetSizeBySensor(stViConfig.astViInfo[s32WorkSnsId].stSnsInfo.enSnsType, &enPicSize);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_GetSizeBySensor failed!\n");
        return s32Ret;
    }

    s32Ret = SAMPLE_COMM_SYS_GetPicSize(enPicSize, &stSize);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed!\n");
        return s32Ret;
    }

    /************************************************
    step3:  Init SYS and common VB
    *************************************************/
    hi_memset(&stVbConf, sizeof(VB_CONFIG_S), 0, sizeof(VB_CONFIG_S));
    stVbConf.u32MaxPoolCnt              = 2;

    u32BlkSize = COMMON_GetPicBufferSize(stSize.u32Width, stSize.u32Height, SAMPLE_PIXEL_FORMAT, DATA_BITWIDTH_10, COMPRESS_MODE_SEG, DEFAULT_ALIGN);
    stVbConf.astCommPool[0].u64BlkSize  = u32BlkSize;
    stVbConf.astCommPool[0].u32BlkCnt   = 10;

    u32BlkSize = VI_GetRawBufferSize(stSize.u32Width, stSize.u32Height, PIXEL_FORMAT_RGB_BAYER_16BPP, COMPRESS_MODE_NONE, DEFAULT_ALIGN);
    stVbConf.astCommPool[1].u64BlkSize  = u32BlkSize;
    stVbConf.astCommPool[1].u32BlkCnt   = 4;

    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("system init failed with %d!\n", s32Ret);
        SAMPLE_COMM_SYS_Exit();
        return s32Ret;
    }

    s32Ret = SAMPLE_COMM_VI_SetParam(&stViConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_COMM_SYS_Exit();
        return s32Ret;
    }

    /************************************************
    step4:  Init VI and VO
    *************************************************/
    SAMPLE_COMM_VO_GetDefConfig(&stVoConfig);
    s32Ret = SAMPLE_VIO_StartViVo(&stViConfig, &stVoConfig);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_VIO_StartViVo failed witfh %d\n", s32Ret);
        goto EXIT;
    }


    /************************************************
    step5:  Bind VI and VO
    *************************************************/
    s32Ret = SAMPLE_COMM_VI_Bind_VO(ViPipe, ViChn, VoDev, VoChn);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_Bind_VO failed with %#x!\n", s32Ret);
        goto EXIT1;
    }

    stLDCAttr.bEnable = HI_TRUE;
    stLDCAttr.stAttr.bAspect = 0;
    stLDCAttr.stAttr.s32XRatio = 100;
    stLDCAttr.stAttr.s32YRatio = 100;
    stLDCAttr.stAttr.s32XYRatio = 100;
    stLDCAttr.stAttr.s32CenterXOffset = 0;
    stLDCAttr.stAttr.s32CenterYOffset = 0;
    stLDCAttr.stAttr.s32DistortionRatio = 500;

    s32Ret = HI_MPI_VI_SetChnLDCAttr(ViPipe,ViChn,&stLDCAttr);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VI_SetChnLDCAttr failed witfh %d\n", s32Ret);
        goto EXIT;
    }


    stSpreadAttr.bEnable        = HI_TRUE;
    stSpreadAttr.u32SpreadCoef  = 16;
    stSpreadAttr.stDestSize.u32Width = 3840;
    stSpreadAttr.stDestSize.u32Height = 2160;

    s32Ret = HI_MPI_VI_SetChnSpreadAttr(ViPipe,ViChn,&stSpreadAttr);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VI_SetChnLDCAttr failed witfh %d\n", s32Ret);
        goto EXIT;
    }


    printf("now LDC on and spread on, press any key to switch spread Off!\n");
    getchar();

    stSpreadAttr.bEnable              = HI_FALSE;
    stSpreadAttr.u32SpreadCoef        = 16;
    stSpreadAttr.stDestSize.u32Width  = 3840;
    stSpreadAttr.stDestSize.u32Height = 2160;

    s32Ret = HI_MPI_VI_SetChnSpreadAttr(ViPipe,ViChn,&stSpreadAttr);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VI_SetChnSpreadAttr failed witfh %d\n", s32Ret);
        goto EXIT;
    }

    printf("now LDC on and spread off, press any key to switch spread On!\n");
    getchar();

    stSpreadAttr.bEnable             = HI_TRUE;
    stSpreadAttr.u32SpreadCoef       = 16;
    stSpreadAttr.stDestSize.u32Width = 3840;
    stSpreadAttr.stDestSize.u32Height= 2160;


    s32Ret = HI_MPI_VI_SetChnSpreadAttr(ViPipe,ViChn,&stSpreadAttr);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VI_SetChnSpreadAttr failed witfh %d\n", s32Ret);
        goto EXIT;
    }


    PAUSE();

    SAMPLE_COMM_VI_UnBind_VO(ViPipe, ViChn, VoDev, VoChn);

    EXIT1:
    SAMPLE_VIO_StopViVo(&stViConfig, &stVoConfig);

    EXIT:
    SAMPLE_COMM_SYS_Exit();

    return s32Ret;
}

HI_S32 SAMPLE_VIO_OV426_PreView(VO_INTF_TYPE_E enVoIntfType)
{
    HI_S32             s32Ret;
    VI_DEV             ViDev               = 5; // 表3-2 Hi3559AV100 DEV与 MIPI/SLVS/BT.1120/BT.656/BT601/DC接口的绑定关系
    VI_PIPE            ViPipe              = 5; // 表3-2 Hi3559AV100 DEV与 MIPI/SLVS/BT.1120/BT.656/BT601/DC接口的绑定关系
    VI_CHN             ViChn               = 0;
    HI_S32             s32WorkSnsId        = 0;
    VO_DEV             VoDev               = SAMPLE_VO_DEV_DHD0;
    VO_CHN             VoChn               = 0;
    SIZE_S             stSize;
    VB_CONFIG_S        stVbConf;
    PIC_SIZE_E         enPicSize           = PIC_400x400;
    HI_U32             u32BlkSize;
    VI_LDC_ATTR_S      stLDCAttr           = {0} ;
    SPREAD_ATTR_S      stSpreadAttr        = {0};
    SAMPLE_VI_CONFIG_S stViConfig  = {0};
    SAMPLE_VO_CONFIG_S stVoConfig  = {0};
    combo_dev_t          ComboDev;

    DYNAMIC_RANGE_E    enDynamicRange = DYNAMIC_RANGE_SDR8;
    PIXEL_FORMAT_E     enPixFormat    = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    VIDEO_FORMAT_E     enVideoFormat  = VIDEO_FORMAT_LINEAR;
    COMPRESS_MODE_E    enCompressMode = COMPRESS_MODE_NONE;
    VI_VPSS_MODE_E     enMastPipeMode = VI_ONLINE_VPSS_OFFLINE;

    /************************************************
    step1:  Get all sensors information
    *************************************************/
    SAMPLE_COMM_VI_GetSensorInfo(&stViConfig);
    ComboDev = SAMPLE_COMM_VI_GetComboDevBySensor(stViConfig.astViInfo[s32WorkSnsId].stSnsInfo.enSnsType, s32WorkSnsId);


    stViConfig.s32WorkingViNum                           = 1;

    stViConfig.as32WorkingViId[0]                        = 0;
    
    // 目前是DC0, SAMPLE_COMM_VI_GetComboDevBySensorm()目前返回的ComboDev就是0，
    // 如果接DC1、DC2，就要修改SAMPLE_COMM_VI_GetComboDevBySensorm()了
    stViConfig.astViInfo[0].stSnsInfo.MipiDev            = ComboDev;
    stViConfig.astViInfo[0].stSnsInfo.s32BusId           = 0;

    stViConfig.astViInfo[0].stDevInfo.ViDev              = ViDev;
    stViConfig.astViInfo[0].stDevInfo.enWDRMode          = WDR_MODE_NONE;

    stViConfig.astViInfo[0].stPipeInfo.enMastPipeMode    = enMastPipeMode;
    stViConfig.astViInfo[0].stPipeInfo.aPipe[0]          = ViPipe;
    stViConfig.astViInfo[0].stPipeInfo.aPipe[1]          = -1;
    stViConfig.astViInfo[0].stPipeInfo.aPipe[2]          = -1;
    stViConfig.astViInfo[0].stPipeInfo.aPipe[3]          = -1;

    stViConfig.astViInfo[0].stChnInfo.ViChn              = ViChn;
    stViConfig.astViInfo[0].stChnInfo.enPixFormat        = enPixFormat;
    stViConfig.astViInfo[0].stChnInfo.enDynamicRange     = enDynamicRange;
    stViConfig.astViInfo[0].stChnInfo.enVideoFormat      = enVideoFormat;
    stViConfig.astViInfo[0].stChnInfo.enCompressMode     = enCompressMode;

    /************************************************
    step2:  Get  input size
    *************************************************/
    s32Ret = SAMPLE_COMM_VI_GetSizeBySensor(stViConfig.astViInfo[s32WorkSnsId].stSnsInfo.enSnsType, &enPicSize);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_GetSizeBySensor failed!\n");
        return s32Ret;
    }

    s32Ret = SAMPLE_COMM_SYS_GetPicSize(enPicSize, &stSize);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed!\n");
        return s32Ret;
    }

    /************************************************
    step3:  Init SYS and common VB
    *************************************************/
    hi_memset(&stVbConf, sizeof(VB_CONFIG_S), 0, sizeof(VB_CONFIG_S));
    stVbConf.u32MaxPoolCnt              = 2;

    u32BlkSize = COMMON_GetPicBufferSize(stSize.u32Width, stSize.u32Height, SAMPLE_PIXEL_FORMAT, DATA_BITWIDTH_10, COMPRESS_MODE_SEG, DEFAULT_ALIGN);
    stVbConf.astCommPool[0].u64BlkSize  = u32BlkSize;
    stVbConf.astCommPool[0].u32BlkCnt   = 10;

    u32BlkSize = VI_GetRawBufferSize(stSize.u32Width, stSize.u32Height, PIXEL_FORMAT_RGB_BAYER_16BPP, COMPRESS_MODE_NONE, DEFAULT_ALIGN);
    stVbConf.astCommPool[1].u64BlkSize  = u32BlkSize;
    stVbConf.astCommPool[1].u32BlkCnt   = 4;

    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("system init failed with %d!\n", s32Ret);
        SAMPLE_COMM_SYS_Exit();
        return s32Ret;
    }

    s32Ret = SAMPLE_COMM_VI_SetParam(&stViConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_COMM_SYS_Exit();
        return s32Ret;
    }

    /************************************************
    step4:  Init VI and VO
    *************************************************/
    SAMPLE_COMM_VO_GetDefConfig(&stVoConfig);
    s32Ret = SAMPLE_VIO_StartViVo(&stViConfig, &stVoConfig);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_VIO_StartViVo failed witfh %d\n", s32Ret);
        goto EXIT;
    }


    /************************************************
    step5:  Bind VI and VO
    *************************************************/
    s32Ret = SAMPLE_COMM_VI_Bind_VO(ViPipe, ViChn, VoDev, VoChn);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_Bind_VO failed with %#x!\n", s32Ret);
        goto EXIT1;
    }

    stLDCAttr.bEnable = HI_TRUE;
    stLDCAttr.stAttr.bAspect = 0;
    stLDCAttr.stAttr.s32XRatio = 100;
    stLDCAttr.stAttr.s32YRatio = 100;
    stLDCAttr.stAttr.s32XYRatio = 100;
    stLDCAttr.stAttr.s32CenterXOffset = 0;
    stLDCAttr.stAttr.s32CenterYOffset = 0;
    stLDCAttr.stAttr.s32DistortionRatio = 500;

    s32Ret = HI_MPI_VI_SetChnLDCAttr(ViPipe,ViChn,&stLDCAttr);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VI_SetChnLDCAttr failed witfh %d\n", s32Ret);
        goto EXIT;
    }


    stSpreadAttr.bEnable        = HI_TRUE;
    stSpreadAttr.u32SpreadCoef  = 16;
    stSpreadAttr.stDestSize.u32Width = 400;
    stSpreadAttr.stDestSize.u32Height = 400;

    s32Ret = HI_MPI_VI_SetChnSpreadAttr(ViPipe,ViChn,&stSpreadAttr);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VI_SetChnLDCAttr failed witfh %d\n", s32Ret);
        goto EXIT;
    }


    printf("now LDC on and spread on, press any key to switch spread Off!\n");
    getchar();

    stSpreadAttr.bEnable              = HI_FALSE;
    stSpreadAttr.u32SpreadCoef        = 16;
    stSpreadAttr.stDestSize.u32Width  = 400;
    stSpreadAttr.stDestSize.u32Height = 400;

    s32Ret = HI_MPI_VI_SetChnSpreadAttr(ViPipe,ViChn,&stSpreadAttr);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VI_SetChnSpreadAttr failed witfh %d\n", s32Ret);
        goto EXIT;
    }

    printf("now LDC on and spread off, press any key to switch spread On!\n");
    getchar();

    stSpreadAttr.bEnable             = HI_TRUE;
    stSpreadAttr.u32SpreadCoef       = 16;
    stSpreadAttr.stDestSize.u32Width = 400;
    stSpreadAttr.stDestSize.u32Height= 400;


    s32Ret = HI_MPI_VI_SetChnSpreadAttr(ViPipe,ViChn,&stSpreadAttr);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VI_SetChnSpreadAttr failed witfh %d\n", s32Ret);
        goto EXIT;
    }


    PAUSE();

    SAMPLE_COMM_VI_UnBind_VO(ViPipe, ViChn, VoDev, VoChn);

    EXIT1:
    SAMPLE_VIO_StopViVo(&stViConfig, &stVoConfig);

    EXIT:
    SAMPLE_COMM_SYS_Exit();

    return s32Ret;
}

HI_S32 SAMPLE_VIO_OV9712_PreView(VO_INTF_TYPE_E enVoIntfType)
{
    HI_S32             s32Ret;
    VI_DEV             ViDev               = 5; // 表3-2 Hi3559AV100 DEV与 MIPI/SLVS/BT.1120/BT.656/BT601/DC接口的绑定关系
    VI_PIPE            ViPipe              = 0; // 表3-2 Hi3559AV100 DEV与 MIPI/SLVS/BT.1120/BT.656/BT601/DC接口的绑定关系
    VI_CHN             ViChn               = 0;
    HI_S32             s32WorkSnsId        = 0;
    VO_DEV             VoDev               = SAMPLE_VO_DEV_DHD0;
    VO_CHN             VoChn               = 0;
    SIZE_S             stSize;
    VB_CONFIG_S        stVbConf;
    PIC_SIZE_E         enPicSize           = PIC_720P;
    HI_U32             u32BlkSize;
    VI_LDC_ATTR_S      stLDCAttr           = {0} ;
    SPREAD_ATTR_S      stSpreadAttr        = {0};
    SAMPLE_VI_CONFIG_S stViConfig  = {0};
    SAMPLE_VO_CONFIG_S stVoConfig  = {0};
    combo_dev_t          ComboDev;

    DYNAMIC_RANGE_E    enDynamicRange = DYNAMIC_RANGE_SDR8;
    PIXEL_FORMAT_E     enPixFormat    = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    //PIXEL_FORMAT_E     enPixFormat    = PIXEL_FORMAT_RGB_BAYER_12BPP;
    VIDEO_FORMAT_E     enVideoFormat  = VIDEO_FORMAT_LINEAR;
    COMPRESS_MODE_E    enCompressMode = COMPRESS_MODE_NONE;
    VI_VPSS_MODE_E     enMastPipeMode = VI_OFFLINE_VPSS_OFFLINE;

    /************************************************
    step1:  Get all sensors information
    *************************************************/
    SAMPLE_COMM_VI_GetSensorInfo(&stViConfig);
    ComboDev = SAMPLE_COMM_VI_GetComboDevBySensor(stViConfig.astViInfo[s32WorkSnsId].stSnsInfo.enSnsType, s32WorkSnsId);
    
    SAMPLE_PRT("stViConfig.astViInfo[s32WorkSnsId].stSnsInfo.enSnsType = %d\n", stViConfig.astViInfo[s32WorkSnsId].stSnsInfo.enSnsType); // by alex


    stViConfig.s32WorkingViNum                           = 1;

    stViConfig.as32WorkingViId[0]                        = 0;

    // 目前是DC0, SAMPLE_COMM_VI_GetComboDevBySensorm()目前返回的ComboDev就是0，
    // 如果接DC1、DC2，就要修改SAMPLE_COMM_VI_GetComboDevBySensorm()了
    stViConfig.astViInfo[0].stSnsInfo.MipiDev            = ComboDev; 
    stViConfig.astViInfo[0].stSnsInfo.s32BusId           = 0;

    stViConfig.astViInfo[0].stDevInfo.ViDev              = ViDev;
    stViConfig.astViInfo[0].stDevInfo.enWDRMode          = WDR_MODE_NONE;

    stViConfig.astViInfo[0].stPipeInfo.enMastPipeMode    = enMastPipeMode;
    stViConfig.astViInfo[0].stPipeInfo.aPipe[0]          = ViPipe;
    stViConfig.astViInfo[0].stPipeInfo.aPipe[1]          = -1;
    stViConfig.astViInfo[0].stPipeInfo.aPipe[2]          = -1;
    stViConfig.astViInfo[0].stPipeInfo.aPipe[3]          = -1;

    stViConfig.astViInfo[0].stChnInfo.ViChn              = ViChn;
    stViConfig.astViInfo[0].stChnInfo.enPixFormat        = enPixFormat;
    stViConfig.astViInfo[0].stChnInfo.enDynamicRange     = enDynamicRange;
    stViConfig.astViInfo[0].stChnInfo.enVideoFormat      = enVideoFormat;
    stViConfig.astViInfo[0].stChnInfo.enCompressMode     = enCompressMode;

    /************************************************
    step2:  Get  input size
    *************************************************/
    s32Ret = SAMPLE_COMM_VI_GetSizeBySensor(stViConfig.astViInfo[s32WorkSnsId].stSnsInfo.enSnsType, &enPicSize);
    
    SAMPLE_PRT("enPicSize = %d\n", enPicSize); // by alex

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_GetSizeBySensor failed!\n");
        return s32Ret;
    }

    s32Ret = SAMPLE_COMM_SYS_GetPicSize(enPicSize, &stSize);
    
    SAMPLE_PRT("stSize.u32Width = %d, stSize.u32Height = %d\n", stSize.u32Width, stSize.u32Height); // by alex

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed!\n");
        return s32Ret;
    }

    /************************************************
    step3:  Init SYS and common VB
    *************************************************/
    hi_memset(&stVbConf, sizeof(VB_CONFIG_S), 0, sizeof(VB_CONFIG_S));
    stVbConf.u32MaxPoolCnt              = 2;

    u32BlkSize = COMMON_GetPicBufferSize(stSize.u32Width, stSize.u32Height, SAMPLE_PIXEL_FORMAT, DATA_BITWIDTH_10, COMPRESS_MODE_SEG, DEFAULT_ALIGN);
    stVbConf.astCommPool[0].u64BlkSize  = u32BlkSize;
    stVbConf.astCommPool[0].u32BlkCnt   = 10;

    u32BlkSize = VI_GetRawBufferSize(stSize.u32Width, stSize.u32Height, PIXEL_FORMAT_RGB_BAYER_16BPP, COMPRESS_MODE_NONE, DEFAULT_ALIGN);
    stVbConf.astCommPool[1].u64BlkSize  = u32BlkSize;
    stVbConf.astCommPool[1].u32BlkCnt   = 4;

    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("system init failed with %d!\n", s32Ret);
        SAMPLE_COMM_SYS_Exit();
        return s32Ret;
    }

    s32Ret = SAMPLE_COMM_VI_SetParam(&stViConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_COMM_SYS_Exit();
        return s32Ret;
    }
    else
    {
    	asm("NOP");
    }


    /************************************************
    step4:  Init VI and VO
    *************************************************/
    SAMPLE_COMM_VO_GetDefConfig(&stVoConfig);
    
    stVoConfig.enIntfSync = VO_OUTPUT_1080P60;
    s32Ret = SAMPLE_VIO_StartViVo(&stViConfig, &stVoConfig);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_VIO_StartViVo failed witfh %d\n", s32Ret);
        goto EXIT;
    }
#if 0
    {
        ISP_EXPOSURE_ATTR_S stExpAttr;
        HI_MPI_ISP_GetExposureAttr(ViPipe, &stExpAttr);

        stExpAttr.bByPass=HI_FALSE;
        stExpAttr.enOpType=OP_TYPE_AUTO;
        stExpAttr.u8AERunInterval=1;
        stExpAttr.bHistStatAdjust=HI_TRUE;
        stExpAttr.bAERouteExValid=HI_FALSE;
        stExpAttr.stAuto.stExpTimeRange.u32Min=0;
        stExpAttr.stAuto.stExpTimeRange.u32Max=150000;/*RW; Range:[0x0, 0xFFFFFFFF]; Format:32.0; sensor exposure time (unit: us ), it's related to the specific sensor */
        stExpAttr.stAuto.stAGainRange.u32Min=1024;
        stExpAttr.stAuto.stAGainRange.u32Max=31356;/*RW; Range:[0x400, 0xFFFFFFFF]; Format:22.10; sensor analog gain (unit: times, 10bit precision), it's related to the specific sensor */
        stExpAttr.stAuto.stDGainRange.u32Min=1024;
        stExpAttr.stAuto.stDGainRange.u32Max=128913;/*RW; Range:[0x400, 0xFFFFFFFF]; Format:22.10;  sensor digital gain (unit: times, 10bit precision), it's related to the specific sensor */
        stExpAttr.stAuto.stISPDGainRange.u32Min=1024;
        stExpAttr.stAuto.stISPDGainRange.u32Max=2048;/*RW; Range:[0x400, 0x40000]; Format:22.10;  ISP digital gain (unit: times, 10bit precision), it's related to the ISP digital gain range */
        stExpAttr.stAuto.stSysGainRange.u32Min=1024;
        stExpAttr.stAuto.stSysGainRange.u32Max=12288;/*RW; Range:[0x400, 0xFFFFFFFF]; Format:22.10;  system gain (unit: times, 10bit precision), it's related to the specific sensor and ISP Dgain range */
        stExpAttr.stAuto.u32GainThreshold=15789828;/*RW; Range:[0x400, 0xFFFFFFFF]; Format:22.10;  Gain threshold for slow shutter mode (unit: times, 10bit precision)*/
        stExpAttr.stAuto.u8Speed=128;/*RW; Range:[0x0, 0xFF]; Format:8.0; AE adjust step for dark scene to bright scene switch */
        stExpAttr.stAuto.u16BlackSpeedBias=144;
        stExpAttr.stAuto.u8Tolerance=2;
        stExpAttr.stAuto.u8Compensation=0x38;
        stExpAttr.stAuto.u16EVBias=1024;

        HI_MPI_ISP_SetExposureAttr(ViPipe, &stExpAttr);
    }
#endif
#if 0
    s32Ret = SetStaticStatisticsCfg(ViPipe);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SetStaticStatisticsCfg failed with %#x!\n", s32Ret);
        goto EXIT1;
    }
#endif

    /************************************************
    step5:  Bind VI and VO
    *************************************************/
    s32Ret = SAMPLE_COMM_VI_Bind_VO(ViPipe, ViChn, VoDev, VoChn);
    
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_Bind_VO failed with %#x!\n", s32Ret);
        goto EXIT1;
    }

    while(1)
    {
        char c;
        c=getchar();
        if(c=='q' || c=='Q')
            break;
        else if(c=='f')
        {
            int i;
            ISP_INNER_STATE_INFO_S stInnerStateInfo;
            s32Ret = HI_MPI_ISP_QueryInnerStateInfo(ViPipe, &stInnerStateInfo);
            if (HI_SUCCESS != s32Ret)
            {
                SAMPLE_PRT("HI_MPI_ISP_QueryInnerStateInfo failed with %#x!\n", s32Ret);
                goto EXIT1;
            }

            for(i=0;i<ISP_SHARPEN_GAIN_NUM;i++)
            {
                SAMPLE_PRT("stInnerStateInfo.au16TextureStr[%d] = %d\n", i, stInnerStateInfo.au16TextureStr[i]);
            }
            for(i=0;i<ISP_SHARPEN_GAIN_NUM;i++)
            {
                SAMPLE_PRT("stInnerStateInfo.au16EdgeStr[%d] = %d\n", i, stInnerStateInfo.au16EdgeStr[i]);
            }

            SAMPLE_PRT("stInnerStateInfo.u16TextureFreq = %d\n", stInnerStateInfo.u16TextureFreq);
            SAMPLE_PRT("stInnerStateInfo.u16EdgeFreq = %d\n", stInnerStateInfo.u16EdgeFreq);
            SAMPLE_PRT("stInnerStateInfo.u8OverShoot = %d\n", stInnerStateInfo.u8OverShoot);
            SAMPLE_PRT("stInnerStateInfo.u8UnderShoot = %d\n", stInnerStateInfo.u8UnderShoot);
            SAMPLE_PRT("stInnerStateInfo.u8ShootSupStr = %d\n", stInnerStateInfo.u8ShootSupStr);
            SAMPLE_PRT("stInnerStateInfo.u8NrLscRatio = %d\n", stInnerStateInfo.u8NrLscRatio);

            
            for(i=0;i<ISP_BAYER_CHN_NUM;i++)
            {
                SAMPLE_PRT("stInnerStateInfo.au16CoarseStr[%d] = %d\n", i, stInnerStateInfo.au16CoarseStr[i]);
            }
            for(i=0;i<WDR_MAX_FRAME_NUM;i++)
            {
                SAMPLE_PRT("stInnerStateInfo.au8WDRFrameStr[%d] = %d\n", i, stInnerStateInfo.au8WDRFrameStr[i]);
            }
            
            for(i=0;i<ISP_BAYER_CHN_NUM;i++)
            {
                SAMPLE_PRT("stInnerStateInfo.au8ChromaStr[%d] = %d\n", i, stInnerStateInfo.au8ChromaStr[i]);
            }
            
            SAMPLE_PRT("stInnerStateInfo.u8FineStr = %d\n", stInnerStateInfo.u8FineStr);
            SAMPLE_PRT("stInnerStateInfo.u16CoringWgt = %d\n", stInnerStateInfo.u16CoringWgt);
            SAMPLE_PRT("stInnerStateInfo.u16DeHazeStrengthActual = %d\n", stInnerStateInfo.u16DeHazeStrengthActual);
            SAMPLE_PRT("stInnerStateInfo.u16DrcStrengthActual = %d\n", stInnerStateInfo.u16DrcStrengthActual);

            for(i=0;i<3;i++)
            {
                SAMPLE_PRT("stInnerStateInfo.u32WDRExpRatioActual[%d] = %d\n", i, stInnerStateInfo.u32WDRExpRatioActual[i]);
            }

            SAMPLE_PRT("stInnerStateInfo.bWDRSwitchFinish = %d\n", stInnerStateInfo.bWDRSwitchFinish);
            SAMPLE_PRT("stInnerStateInfo.bResSwitchFinish = %d\n", stInnerStateInfo.bResSwitchFinish);
            SAMPLE_PRT("stInnerStateInfo.u8FineStr = %d\n", stInnerStateInfo.u8FineStr);
            for(i=0;i<ISP_BAYER_CHN_NUM;i++)
            {
                SAMPLE_PRT("stInnerStateInfo.au16BLActual[%d] = %d\n", i, stInnerStateInfo.au16BLActual[i]);
            }
        }
    }
//	        typedef struct hiISP_INNER_STATE_INFO_S
//	        {
//	
//	            HI_U16 au16TextureStr[ISP_SHARPEN_GAIN_NUM];     /* RW; range: [0, 4095]; Format:7.5;Undirectional sharpen strength for texture and detail enhancement*/
//	            HI_U16 au16EdgeStr[ISP_SHARPEN_GAIN_NUM];        /* RW; range: [0, 4095]; Format:7.5;Directional sharpen strength for edge enhancement*/
//	            HI_U16 u16TextureFreq;         /* RW; range: [0, 4095];Format:6.6; Texture frequency adjustment. Texture and detail will be finer when it increase*/
//	            HI_U16 u16EdgeFreq;            /* RW; range: [0, 4095];Format:6.6; Edge frequency adjustment. Edge will be narrower and thiner when it increase*/
//	            HI_U8  u8OverShoot;            /* RW; range: [0, 127]; Format:7.0;u8OvershootAmt*/
//	            HI_U8  u8UnderShoot;           /* RW; range: [0, 127]; Format:7.0;u8UndershootAmt*/
//	            HI_U8  u8ShootSupStr;          /* RW; range: [0, 255]; Format:8.0;overshoot and undershoot suppression strength, the amplitude and width of shoot will be decrease when shootSupSt increase*/
//	
//	            HI_U8   u8NrLscRatio;                       /*RW;Range:[0x0,0xff];Format:8.0; Strength of reserving the random noise according to luma*/
//	            HI_U16  au16CoarseStr[ISP_BAYER_CHN_NUM];   /*RW;Range:[0x0,0x3ff];Format:10.0; Coarse Strength of noise reduction */
//	            HI_U8   au8WDRFrameStr[WDR_MAX_FRAME_NUM];  /*RW;Range:[0x0,0x50];Format:7.0; Coarse strength of each frame in wdr mode*/
//	            HI_U8   au8ChromaStr[ISP_BAYER_CHN_NUM];    /*RW;Range:[0x0,0x3];Format:2.0;Strength of Chrmoa noise reduction for R/Gr/Gb/B channel*/
//	            HI_U8   u8FineStr;                 /*RW;Range:[0x0,0x80];Format:8.0;Strength of Luma noise reduction*/
//	            HI_U16  u16CoringWgt;              /*RW;Range:[0x0,0xC80];Format:12.0;Strength of reserving the random noise*/
//	
//	            HI_U16 u16DeHazeStrengthActual;      /* RW;Range:[0,0xFF];Format:8.0;actual dehze strength */
//	            HI_U16 u16DrcStrengthActual;          /* RW;Range: Hi3559AV100 = [0x0, 0xFF] | Hi3519AV100 = [0x0, 0x3FF];Strength of dynamic range compression.
//	                                                                   Higher values lead to higher differential gain between shadows and highlights. */
//	            HI_U32 u32WDRExpRatioActual[3];        /*RW; Range:[0x40, 0x4000]; Format:26.6; 0x40 means 1 times.
//	                                                                     When enExpRatioType is OP_TYPE_AUTO, u32ExpRatio is invalid.
//	                                                                     When enExpRatioType is OP_TYPE_MANUAL, u32ExpRatio is quotient of long exposure time / short exposure time. */
//	            HI_BOOL bWDRSwitchFinish;           /* RW; Range:[0, 1];Format:1.0;HI_TRUE: WDR switch is finished*/
//	            HI_BOOL bResSwitchFinish;           /* RW; Range:[0, 1];Format:1.0;HI_TRUE: Resolution switch is finished*/
//	            HI_U16 au16BLActual[ISP_BAYER_CHN_NUM]; /* RW; Range: [0x0, 0xFFF];Format:12.0;Actual Black level values that correspond to the black levels of the R,Gr, Gb, and B components respectively.*/
//	        } ISP_INNER_STATE_INFO_S;
    
/*
    stLDCAttr.bEnable = HI_TRUE;
    stLDCAttr.stAttr.bAspect = 0;
    stLDCAttr.stAttr.s32XRatio = 100;
    stLDCAttr.stAttr.s32YRatio = 100;
    stLDCAttr.stAttr.s32XYRatio = 100;
    stLDCAttr.stAttr.s32CenterXOffset = 0;
    stLDCAttr.stAttr.s32CenterYOffset = 0;
    stLDCAttr.stAttr.s32DistortionRatio = 500;

    s32Ret = HI_MPI_VI_SetChnLDCAttr(ViPipe,ViChn,&stLDCAttr);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VI_SetChnLDCAttr failed witfh %d\n", s32Ret);
        goto EXIT;
    }

    stSpreadAttr.bEnable        = HI_TRUE;
    stSpreadAttr.u32SpreadCoef  = 16;
    stSpreadAttr.stDestSize.u32Width = 1280;
    stSpreadAttr.stDestSize.u32Height = 720;

    s32Ret = HI_MPI_VI_SetChnSpreadAttr(ViPipe,ViChn,&stSpreadAttr);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VI_SetChnLDCAttr failed witfh %d\n", s32Ret);
        goto EXIT;
    }


    printf("now LDC on and spread on, press any key to switch spread Off!\n");
    getchar();

    stSpreadAttr.bEnable              = HI_FALSE;
    stSpreadAttr.u32SpreadCoef        = 16;
    stSpreadAttr.stDestSize.u32Width  = 1280;
    stSpreadAttr.stDestSize.u32Height = 720;

    s32Ret = HI_MPI_VI_SetChnSpreadAttr(ViPipe,ViChn,&stSpreadAttr);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VI_SetChnSpreadAttr failed witfh %d\n", s32Ret);
        goto EXIT;
    }

    printf("now LDC on and spread off, press any key to switch spread On!\n");
    getchar();

    stSpreadAttr.bEnable             = HI_TRUE;
    stSpreadAttr.u32SpreadCoef       = 16;
    stSpreadAttr.stDestSize.u32Width = 1280;
    stSpreadAttr.stDestSize.u32Height= 720;


    s32Ret = HI_MPI_VI_SetChnSpreadAttr(ViPipe,ViChn,&stSpreadAttr);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VI_SetChnSpreadAttr failed witfh %d\n", s32Ret);
        goto EXIT;
    }

*/

/*
	delay_ms(100);
	ov9712_write_register(ViPipe, 0x2a, 0x9c);
    ov9712_write_register(ViPipe, 0x2b, 0x06);
    ov9712_write_register(ViPipe, 0x10, 0xf0);
    ov9712_write_register(ViPipe, 0x00, 0x3f);
*/    
    //PAUSE();

    SAMPLE_COMM_VI_UnBind_VO(ViPipe, ViChn, VoDev, VoChn);

    EXIT1:
    SAMPLE_VIO_StopViVo(&stViConfig, &stVoConfig);

    EXIT:
    SAMPLE_COMM_SYS_Exit();

    return s32Ret;
}


HI_S32 SAMPLE_xxx_PreView(VO_INTF_TYPE_E enVoIntfType)
{
    HI_S32             s32Ret;
    VI_DEV             ViDev               = 5; // ??????3-2 Hi3559AV100 DEV?????? MIPI/SLVS/BT.1120/BT.656/BT601/DC???ó?úμ?°ó?¨1????μ
    VI_PIPE            ViPipe              = 0; // ??????3-2 Hi3559AV100 DEV?????? MIPI/SLVS/BT.1120/BT.656/BT601/DC???ó?úμ?°ó?¨1????μ
    VI_CHN             ViChn               = 0;
    MIPI_DEV           MipiDev 	           = 0;
    VI_DEV_ATTR_S      stDevAttr;
    VI_PIPE_ATTR_S     stPipeAttr;
    VI_CHN_ATTR_S      stChnAttr;
    VI_DEV_BIND_PIPE_S stDevBindPipe;
    
    VB_CONFIG_S        stVbConf;
    HI_U32             u32BlkSize;
    SIZE_S             stSize;
    
    VI_VPSS_MODE_S      stVIVPSSMode;
    
    HI_S32              fd;
    combo_dev_t           devno = 0;
    sns_clk_source_t       SnsDev = 0;
    combo_dev_attr_t    stcomboDevAttr;
    
    VO_DEV             VoDev               = SAMPLE_VO_DEV_DHD0;
    VO_CHN             VoChn               = 0;
    SAMPLE_VO_CONFIG_S stVoConfig  = {0};
    
    
    HI_U32              u32SnsId = 0;
    ISP_PUB_ATTR_S      stPubAttr;
    HI_S32              s32BusId = 0;
    HI_S32              s32SnsId = 0;
    
    VI_PIPE         aPipe = 0;
    
    stSize.u32Width = 1280;
    stSize.u32Height = 720;
    
    stDevAttr.enIntfMode = VI_MODE_DIGITAL_CAMERA;
    stDevAttr.enWorkMode = VI_WORK_MODE_1Multiplex;
    stDevAttr.au32ComponentMask [0] = 0xFFF00000;
    stDevAttr.au32ComponentMask [1] = 0x0;
    stDevAttr.enScanMode = VI_SCAN_PROGRESSIVE;
    stDevAttr.as32AdChnId[0] = -1;
    stDevAttr.as32AdChnId[1] = -1;
    stDevAttr.as32AdChnId[2] = -1;
    stDevAttr.as32AdChnId[3] = -1;
    stDevAttr.enDataSeq = VI_DATA_SEQ_YUYV;
    stDevAttr.stSize.u32Width = 1280;
    stDevAttr.stSize.u32Height = 720;
    stDevAttr.stSynCfg.enVsync = VI_VSYNC_FIELD;
    stDevAttr.stSynCfg.enVsyncNeg = VI_VSYNC_NEG_HIGH;
    stDevAttr.stSynCfg.enHsync = VI_HSYNC_VALID_SINGNAL;
    stDevAttr.stSynCfg.enHsyncNeg = VI_HSYNC_NEG_HIGH;
    stDevAttr.stSynCfg.enVsyncValid = VI_VSYNC_VALID_SINGAL;
    stDevAttr.stSynCfg.enVsyncValidNeg = VI_VSYNC_VALID_NEG_HIGH;
    stDevAttr.stSynCfg.stTimingBlank.u32HsyncHfb = 370;
    stDevAttr.stSynCfg.stTimingBlank.u32HsyncAct = 1280;
    stDevAttr.stSynCfg.stTimingBlank.u32HsyncHbb = 0;
    stDevAttr.stSynCfg.stTimingBlank.u32VsyncVfb = 6;
    stDevAttr.stSynCfg.stTimingBlank.u32VsyncVact = 720;
    stDevAttr.stSynCfg.stTimingBlank.u32VsyncVbb = 6;
    stDevAttr.stSynCfg.stTimingBlank.u32VsyncVbfb = 0;
    stDevAttr.stSynCfg.stTimingBlank.u32VsyncVbact = 0;
    stDevAttr.stSynCfg.stTimingBlank.u32VsyncVbbb = 0;
    stDevAttr.enInputDataType = VI_DATA_TYPE_RGB;
    stDevAttr.bDataReverse = HI_FALSE;
    stDevAttr.stSize.u32Width = 1280;
    stDevAttr.stSize.u32Height = 720;
    stDevAttr.stBasAttr.stSacleAttr.stBasSize.u32Width = 1280;
    stDevAttr.stBasAttr.stSacleAttr.stBasSize.u32Height = 720;
    stDevAttr.stBasAttr.stRephaseAttr.enHRephaseMode = VI_REPHASE_MODE_NONE;
    stDevAttr.stBasAttr.stRephaseAttr.enVRephaseMode = VI_REPHASE_MODE_NONE;
    stDevAttr.stWDRAttr.enWDRMode = WDR_MODE_NONE;
    stDevAttr.stWDRAttr.u32CacheLine = 720;
    stDevAttr.enDataRate = DATA_RATE_X1;
    

    hi_memset(&stVbConf, sizeof(VB_CONFIG_S), 0, sizeof(VB_CONFIG_S));
    stVbConf.u32MaxPoolCnt              = 2;

    u32BlkSize = COMMON_GetPicBufferSize(stSize.u32Width, stSize.u32Height, SAMPLE_PIXEL_FORMAT, DATA_BITWIDTH_10, COMPRESS_MODE_SEG, DEFAULT_ALIGN);
    stVbConf.astCommPool[0].u64BlkSize  = u32BlkSize;
    stVbConf.astCommPool[0].u32BlkCnt   = 10;

    u32BlkSize = VI_GetRawBufferSize(stSize.u32Width, stSize.u32Height, PIXEL_FORMAT_RGB_BAYER_16BPP, COMPRESS_MODE_NONE, DEFAULT_ALIGN);
    stVbConf.astCommPool[1].u64BlkSize  = u32BlkSize;
    stVbConf.astCommPool[1].u32BlkCnt   = 4;

    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("system init failed with %d!\n", s32Ret);
        SAMPLE_COMM_SYS_Exit();
        return s32Ret;
    }
    
    s32Ret = SAMPLE_COMM_VI_SetMipiHsMode(LANE_DIVIDE_MODE_7);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_SetMipiHsMode failed!\n");
        return HI_FAILURE;
    }
    
    fd = open("/dev/hi_mipi", O_RDWR);
    if (fd < 0)
    {
        SAMPLE_PRT("open hi_mipi dev failed\n");
        return HI_FAILURE;
    }

    s32Ret = ioctl(fd, HI_MIPI_RESET_MIPI, &devno);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("RESET_MIPI %d failed\n", devno);
        return HI_FAILURE;
    }
    
    s32Ret = ioctl(fd, HI_MIPI_ENABLE_SENSOR_CLOCK, &SnsDev);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MIPI_ENABLE_SENSOR_CLOCK failed\n");
        return HI_FAILURE;
    }
    
    s32Ret = ioctl(fd, HI_MIPI_RESET_SENSOR, &SnsDev);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MIPI_RESET_SENSOR failed\n");
        return HI_FAILURE;
    }
    
    stcomboDevAttr.devno = 0;
    stcomboDevAttr.input_mode = INPUT_MODE_CMOS;
    stcomboDevAttr.data_rate = DATA_RATE_X1;
    stcomboDevAttr.img_rect.x = 0;
    stcomboDevAttr.img_rect.y = 0;
    stcomboDevAttr.img_rect.width = 1280;
    stcomboDevAttr.img_rect.height = 720;
    s32Ret = ioctl(fd, HI_MIPI_SET_DEV_ATTR, &stcomboDevAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("MIPI_SET_DEV_ATTR failed\n");
        return HI_FAILURE;
    }

    s32Ret = ioctl(fd, HI_MIPI_UNRESET_MIPI, &devno);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("UNRESET_MIPI %d failed\n", devno);
        return HI_FAILURE;
    }
    
    s32Ret = ioctl(fd, HI_MIPI_UNRESET_SENSOR, &SnsDev);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MIPI_UNRESET_SENSOR failed\n");
        return HI_FAILURE;
    }
    
    s32Ret = HI_MPI_SYS_GetVIVPSSMode(&stVIVPSSMode);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Get VI-VPSS mode Param failed with %#x!\n", s32Ret);

        return HI_FAILURE;
    }
    
    stVIVPSSMode.aenMode[0] = VI_OFFLINE_VPSS_OFFLINE;
    s32Ret = HI_MPI_SYS_SetVIVPSSMode(&stVIVPSSMode);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Set VI-VPSS mode Param failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    s32Ret = HI_MPI_VI_SetDevAttr(ViDev, &stDevAttr);
    if (s32Ret != HI_SUCCESS)
    {
        printf("Set dev attributes failed with error code %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    s32Ret = HI_MPI_VI_EnableDev(ViDev);
    if (s32Ret != HI_SUCCESS)
    {
        printf("Enable dev failed with error code %#x!\n", s32Ret);
        return HI_FAILURE;
    }

//    s32Ret = HI_MPI_VI_SetMipiBindDev(ViDev, MipiDev);
//    if (s32Ret != HI_SUCCESS)
//    {
//        printf("Mipi bind dev failed with error code %#x!\n", s32Ret);
//        return HI_FAILURE;
//    }

	stDevBindPipe.u32Num = 1;
    stDevBindPipe.PipeId[0] = ViPipe;
    
    s32Ret = HI_MPI_VI_SetDevBindPipe(ViDev, &stDevBindPipe);
    if (s32Ret != HI_SUCCESS)
    {
        printf("Dev bind pipe failed with error code %#x!\n", s32Ret);
        return HI_FAILURE;
    }

	stPipeAttr.enPipeBypassMode = VI_PIPE_BYPASS_NONE;
    stPipeAttr.bIspBypass = HI_FALSE;
    stPipeAttr.bYuvSkip = HI_FALSE;
    stPipeAttr.u32MaxW = 1280;
    stPipeAttr.u32MaxH = 720;
    stPipeAttr.enPixFmt = PIXEL_FORMAT_RGB_BAYER_12BPP;
    stPipeAttr.enCompressMode = COMPRESS_MODE_LINE;
    stPipeAttr.enBitWidth = DATA_BITWIDTH_10;
    stPipeAttr.bNrEn = HI_TRUE;
    stPipeAttr.stNrAttr.enPixFmt = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    stPipeAttr.stNrAttr.enBitWidth = DATA_BITWIDTH_8;
    //stPipeAttr.stNrAttr.enBitWidth = DATA_BITWIDTH_10;
    stPipeAttr.stNrAttr.enNrRefSource = VI_NR_REF_FROM_RFR;
    //stPipeAttr.stNrAttr.enCompressMode = COMPRESS_MODE_FRAME;
    stPipeAttr.stNrAttr.enCompressMode = COMPRESS_MODE_NONE;
    stPipeAttr.bSharpenEn = HI_FALSE;
    stPipeAttr.stFrameRate.s32SrcFrameRate = -1;
    stPipeAttr.stFrameRate.s32DstFrameRate = -1;  
    stPipeAttr.bDiscardProPic = 0;
     
    s32Ret = HI_MPI_VI_CreatePipe(ViPipe, &stPipeAttr);
    if (s32Ret != HI_SUCCESS)
    {
        printf("Creat pipe failed with error code %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    stChnAttr.stSize.u32Width = 1280;
    stChnAttr.stSize.u32Height = 720;
    stChnAttr.enDynamicRange = DYNAMIC_RANGE_SDR8;
    stChnAttr.enPixelFormat = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    stChnAttr.enVideoFormat= VIDEO_FORMAT_LINEAR;
    stChnAttr.enCompressMode = COMPRESS_MODE_NONE;
    stChnAttr.bMirror = HI_FALSE;
    stChnAttr.bFlip = HI_FALSE;
    stChnAttr.u32Depth = 0;
    stChnAttr.stFrameRate.s32SrcFrameRate = -1;
    stChnAttr.stFrameRate.s32DstFrameRate = -1;
    //stChnAttr.stFrameRate.s32SrcFrameRate = 30;
    //stChnAttr.stFrameRate.s32DstFrameRate = 30;

    s32Ret = HI_MPI_VI_StartPipe (ViPipe);
    if (s32Ret != HI_SUCCESS)
    {
        printf("Start pipe failed with error code %#x!\n", s32Ret);
        return HI_FAILURE;
    }
    s32Ret = HI_MPI_VI_SetChnAttr(ViPipe,ViChn,&stChnAttr);
    if (s32Ret != HI_SUCCESS)
    {
        printf("Set chn attributes failed with error code %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    s32Ret = HI_MPI_VI_EnableChn(ViPipe,ViChn);
    if (s32Ret != HI_SUCCESS)
    {
        printf("Enable chn failed with error code %#x!\n", s32Ret);
        return HI_FAILURE;
    }
    
    /* now, vi is capturing images, you can do something else ... */
    
    SAMPLE_COMM_ISP_GetIspAttrBySns(OMNIVISION_OV9712_DC_720P_30FPS_12BIT, &stPubAttr);

	stPubAttr.stWndRect.s32X = 0;
	stPubAttr.stWndRect.s32Y = 0;
	stPubAttr.stWndRect.u32Width = 1280;
	stPubAttr.stWndRect.u32Height = 720;
	//stPubAttr.stSnsSize.u32Width = 1280;
	//stPubAttr.stSnsSize.u32Height = 720;
	stPubAttr.stSnsSize.u32Width = 1296;
	stPubAttr.stSnsSize.u32Height = 818;
	stPubAttr.f32FrameRate = 30;
	stPubAttr.enBayer = BAYER_BGGR;
    stPubAttr.enWDRMode = WDR_MODE_NONE;
    stPubAttr.u8SnsMode = 0;
    
    
    s32Ret = SAMPLE_COMM_ISP_Sensor_Regiter_callback(ViPipe, u32SnsId);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("register sensor %d to ISP %d failed\n", u32SnsId, ViPipe);
        SAMPLE_COMM_ISP_Stop(ViPipe);
        return HI_FAILURE;
    }
    
    s32Ret = SAMPLE_COMM_ISP_BindSns(ViPipe, u32SnsId, OMNIVISION_OV9712_DC_720P_30FPS_12BIT, s32BusId);
    if (HI_SUCCESS != s32Ret)
	{
		SAMPLE_PRT("register sensor %d bus id %d failed\n", u32SnsId, s32BusId);
		SAMPLE_COMM_ISP_Stop(ViPipe);
		return HI_FAILURE;
	}
/*    
    s32Ret = SAMPLE_COMM_ISP_Aelib_Callback(ViPipe);

	if (HI_SUCCESS != s32Ret)
	{
		SAMPLE_PRT("SAMPLE_COMM_ISP_Aelib_Callback failed\n");
		SAMPLE_COMM_ISP_Stop(ViPipe);
		return HI_FAILURE;
	}

	s32Ret = SAMPLE_COMM_ISP_Awblib_Callback(ViPipe);

	if (HI_SUCCESS != s32Ret)
	{
		SAMPLE_PRT("SAMPLE_COMM_ISP_Awblib_Callback failed\n");
		SAMPLE_COMM_ISP_Stop(ViPipe);
		return HI_FAILURE;
	}
*/
	s32Ret = HI_MPI_ISP_MemInit(ViPipe);

	if (s32Ret != HI_SUCCESS)
	{
		SAMPLE_PRT("Init Ext memory failed with %#x!\n", s32Ret);
		SAMPLE_COMM_ISP_Stop(ViPipe);
		return HI_FAILURE;
	}

	s32Ret = HI_MPI_ISP_SetPubAttr(ViPipe, &stPubAttr);

	if (s32Ret != HI_SUCCESS)
	{
		SAMPLE_PRT("SetPubAttr failed with %#x!\n", s32Ret);
		SAMPLE_COMM_ISP_Stop(ViPipe);
		return HI_FAILURE;
	}

	s32Ret = HI_MPI_ISP_Init(ViPipe);

	if (s32Ret != HI_SUCCESS)
	{
		SAMPLE_PRT("ISP Init failed with %#x!\n", s32Ret);
		SAMPLE_COMM_ISP_Stop(ViPipe);
		return HI_FAILURE;
	}

	s32Ret = SAMPLE_COMM_ISP_Run(&aPipe);

	if (s32Ret != HI_SUCCESS)
	{
		SAMPLE_PRT("ISP Run failed with %#x!\n", s32Ret);
		SAMPLE_COMM_ISP_Stop(ViPipe);
		return HI_FAILURE;
	}
    
    
    SAMPLE_COMM_VO_GetDefConfig(&stVoConfig);
    stVoConfig.enIntfSync = VO_OUTPUT_1080P60;
    
    s32Ret = SAMPLE_COMM_VO_StartVO(&stVoConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_VIO start VO failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }
    
    s32Ret = SAMPLE_COMM_VI_Bind_VO(ViPipe, ViChn, VoDev, VoChn);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_Bind_VO failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }
    
    PAUSE();
    
    s32Ret = HI_MPI_VI_DisableChn(ViPipe,ViChn);
    if (s32Ret != HI_SUCCESS)
    {
        printf("Disable chn failed with error code %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    s32Ret = HI_MPI_VI_StopPipe (ViPipe);
    if (s32Ret != HI_SUCCESS)
    {
        printf("Stop pipe failed with error code %#x!\n", s32Ret);
        return HI_FAILURE;
    }
    s32Ret = HI_MPI_VI_DestroyPipe(ViPipe);
    if (s32Ret != HI_SUCCESS)
    {
        printf("Destroy pipe failed with error code %#x!\n", s32Ret);
        return HI_FAILURE;
    }
    s32Ret = HI_MPI_VI_DisableDev(ViDev);
    if (s32Ret != HI_SUCCESS)
    {
        printf("Disable dev failed with error code %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    return s32Ret;
}

/******************************************************************************
* function    : main()
* Description : main
******************************************************************************/
#ifdef __HuaweiLite__
int app_main(int argc, char *argv[])
#else
int main(int argc, char* argv[])
#endif
{
    HI_S32 s32Ret = HI_FAILURE;
    HI_S32 s32Index;
    VO_INTF_TYPE_E enVoIntfType = VO_INTF_HDMI;

    if (argc < 2)
    {
        SAMPLE_VIO_Usage(argv[0]);
        return HI_FAILURE;
    }

#ifdef __HuaweiLite__
#else
    signal(SIGINT, SAMPLE_VIO_HandleSig);
    signal(SIGTERM, SAMPLE_VIO_HandleSig);
#endif

    if ((argc > 2) && (1 == atoi(argv[2])))
    {
        enVoIntfType = VO_INTF_BT1120;
    }

    s32Index = atoi(argv[1]);
    switch (s32Index)
    {
        case 0:
            s32Ret = SAMPLE_VIO_8K30_PARALLEL(enVoIntfType);
            break;

        case 1:
            s32Ret = SAMPLE_VIO_2X4K60_TotalOnline(enVoIntfType);
            break;

        case 2:
            s32Ret = SAMPLE_VIO_4x4K30_WBC(enVoIntfType);
            break;

        case 3:
            s32Ret = SAMPLE_VIO_4K30_WDR_HDR10(enVoIntfType);
            break;

        case 4:
            s32Ret = SAMPLE_VIO_4K30_LDC_ROTATE(enVoIntfType);
            break;

        case 5:
            s32Ret = SAMPLE_VIO_4K30_FreeRotation(enVoIntfType);
            break;

        case 6:
            s32Ret = SAMPLE_VIO_4K30_LDC_SPREAD(enVoIntfType);
            break;

        case 7:
            s32Ret = SAMPLE_VIO_OV426_PreView(enVoIntfType);
            break;
			
		case 8:
            s32Ret = SAMPLE_VIO_OV9712_PreView(enVoIntfType);
            break;
            
        case 9:
            s32Ret = SAMPLE_xxx_PreView(enVoIntfType);
            break;
            
        default:
            SAMPLE_PRT("the index %d is invaild!\n",s32Index);
            SAMPLE_VIO_Usage(argv[0]);
            return HI_FAILURE;
    }

    if (HI_SUCCESS == s32Ret)
    {
        SAMPLE_PRT("sample_vio exit success!\n");
    }
    else
    {
        SAMPLE_PRT("sample_vio exit abnormally!\n");
    }
    return s32Ret;
}


#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */

