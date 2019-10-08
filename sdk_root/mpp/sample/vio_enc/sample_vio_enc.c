
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

HI_S32 SAMPLE_VIO_OV9712_PreView(VO_INTF_TYPE_E enVoIntfType)
{
    HI_S32             s32Ret = HI_SUCCESS;

    HI_S32             s32ViCnt       = 1;
    VI_DEV             ViDev          = 5;
    VI_PIPE            ViPipe         = 0;
    VI_CHN             ViChn          = 0;
    HI_S32             s32WorkSnsId   = 0;
    SAMPLE_VI_CONFIG_S stViConfig     = {0};

    SIZE_S             stSize;
    VB_CONFIG_S        stVbConf;
    PIC_SIZE_E         enPicSize      = PIC_720P;
    HI_U32             u32BlkSize;

    VO_DEV             VoDev          = SAMPLE_VO_DEV_DHD0;
    VO_CHN             VoChn          = 0;
    SAMPLE_VO_CONFIG_S stVoConfig     = {0};
    
    combo_dev_t        ComboDev;

    WDR_MODE_E         enWDRMode      = WDR_MODE_NONE;
    DYNAMIC_RANGE_E    enDynamicRange = DYNAMIC_RANGE_SDR8;
    PIXEL_FORMAT_E     enPixFormat    = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    VIDEO_FORMAT_E     enVideoFormat  = VIDEO_FORMAT_LINEAR;
    COMPRESS_MODE_E    enCompressMode = COMPRESS_MODE_NONE;
    VI_VPSS_MODE_E     enMastPipeMode = VI_ONLINE_VPSS_ONLINE;

    VPSS_GRP           VpssGrp        = 0;
    VPSS_GRP_ATTR_S    stVpssGrpAttr;
    VPSS_CHN           VpssChn        = VPSS_CHN0;
    HI_BOOL            abChnEnable[VPSS_MAX_PHY_CHN_NUM] = {0};
    VPSS_CHN_ATTR_S    astVpssChnAttr[VPSS_MAX_PHY_CHN_NUM];

    VENC_CHN           VencChn[1]  = {0};
    PAYLOAD_TYPE_E     enType      = PT_H265;
    SAMPLE_RC_E        enRcMode    = SAMPLE_RC_CBR;
    HI_U32             u32Profile  = 0;
    HI_BOOL            bRcnRefShareBuf = HI_FALSE;
    VENC_GOP_ATTR_S    stGopAttr;


    /************************************************
    step1:  Get all sensors information, config vi
    *************************************************/
    SAMPLE_COMM_VI_GetSensorInfo(&stViConfig);
    ComboDev = SAMPLE_COMM_VI_GetComboDevBySensor(stViConfig.astViInfo[s32WorkSnsId].stSnsInfo.enSnsType, s32WorkSnsId);
    //SAMPLE_PRT("stViConfig.astViInfo[s32WorkSnsId].stSnsInfo.enSnsType = %d\n", stViConfig.astViInfo[s32WorkSnsId].stSnsInfo.enSnsType); // by alex

    stViConfig.s32WorkingViNum                           = s32ViCnt;
    stViConfig.as32WorkingViId[0]                        = 0;
    stViConfig.astViInfo[s32WorkSnsId].stSnsInfo.MipiDev            = ComboDev; 
    stViConfig.astViInfo[s32WorkSnsId].stSnsInfo.s32BusId           = 0;
    stViConfig.astViInfo[s32WorkSnsId].stDevInfo.ViDev              = ViDev;
    stViConfig.astViInfo[s32WorkSnsId].stDevInfo.enWDRMode          = enWDRMode;
    stViConfig.astViInfo[s32WorkSnsId].stPipeInfo.enMastPipeMode    = enMastPipeMode;
    stViConfig.astViInfo[s32WorkSnsId].stPipeInfo.aPipe[0]          = ViPipe;
    stViConfig.astViInfo[s32WorkSnsId].stPipeInfo.aPipe[1]          = -1;
    stViConfig.astViInfo[s32WorkSnsId].stPipeInfo.aPipe[2]          = -1;
    stViConfig.astViInfo[s32WorkSnsId].stPipeInfo.aPipe[3]          = -1;
    stViConfig.astViInfo[s32WorkSnsId].stChnInfo.ViChn              = ViChn;
    stViConfig.astViInfo[s32WorkSnsId].stChnInfo.enPixFormat        = enPixFormat;
    stViConfig.astViInfo[s32WorkSnsId].stChnInfo.enDynamicRange     = enDynamicRange;
    stViConfig.astViInfo[s32WorkSnsId].stChnInfo.enVideoFormat      = enVideoFormat;
    stViConfig.astViInfo[s32WorkSnsId].stChnInfo.enCompressMode     = enCompressMode;

    /************************************************
    step2:  Get  input size
    *************************************************/
    s32Ret = SAMPLE_COMM_VI_GetSizeBySensor(stViConfig.astViInfo[s32WorkSnsId].stSnsInfo.enSnsType, &enPicSize);
    
    //SAMPLE_PRT("enPicSize = %d\n", enPicSize); // by alex

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_GetSizeBySensor failed!\n");
        return s32Ret;
    }

    s32Ret = SAMPLE_COMM_SYS_GetPicSize(enPicSize, &stSize);
    
    //SAMPLE_PRT("stSize.u32Width = %d, stSize.u32Height = %d\n", stSize.u32Width, stSize.u32Height); // by alex

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed!\n");
        return s32Ret;
    }

    /************************************************
    step3:  Init SYS and common VB, config vb
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
    step4:  start vi
    *************************************************/
    s32Ret = SAMPLE_COMM_VI_StartVi(&stViConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start vi failed.s32Ret:0x%x !\n", s32Ret);
        goto EXIT;
    }

    /************************************************
    step5:  config vpss
    *************************************************/
    hi_memset(&stVpssGrpAttr, sizeof(VPSS_GRP_ATTR_S), 0, sizeof(VPSS_GRP_ATTR_S));
    stVpssGrpAttr.stFrameRate.s32SrcFrameRate    = -1;
    stVpssGrpAttr.stFrameRate.s32DstFrameRate    = -1;
    stVpssGrpAttr.enDynamicRange                 = DYNAMIC_RANGE_SDR8;
    stVpssGrpAttr.enPixelFormat                  = enPixFormat;
    stVpssGrpAttr.u32MaxW                        = stSize.u32Width;
    stVpssGrpAttr.u32MaxH                        = stSize.u32Height;
    stVpssGrpAttr.bNrEn                          = HI_TRUE;
    stVpssGrpAttr.stNrAttr.enCompressMode        = COMPRESS_MODE_FRAME;
    stVpssGrpAttr.stNrAttr.enNrMotionMode        = NR_MOTION_MODE_NORMAL;

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

    /************************************************
    step6:  start vpss
    *************************************************/
    abChnEnable[0] = HI_TRUE;
    s32Ret = SAMPLE_COMM_VPSS_Start(VpssGrp, abChnEnable, &stVpssGrpAttr, astVpssChnAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start vpss group failed. s32Ret: 0x%x !\n", s32Ret);
        goto EXIT1;
    }

    /************************************************
    step7:  config venc
    *************************************************/
    stGopAttr.enGopMode  = VENC_GOPMODE_SMARTP;
    stGopAttr.stSmartP.s32BgQpDelta  = 7;
    stGopAttr.stSmartP.s32ViQpDelta  = 2;
    stGopAttr.stSmartP.u32BgInterval = 1200;
    s32Ret = SAMPLE_COMM_VENC_Start(VencChn[0], enType, enPicSize, enRcMode, u32Profile, &stGopAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start venc failed. s32Ret: 0x%x !\n", s32Ret);
        goto EXIT2;
    }

    s32Ret = SAMPLE_COMM_VPSS_Bind_VENC(VpssGrp, VpssChn, VencChn[0]);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Venc bind Vpss failed. s32Ret: 0x%x !n", s32Ret);
        goto EXIT3;
    }

    /************************************************
    step8:  config vo
    *************************************************/
    SAMPLE_COMM_VO_GetDefConfig(&stVoConfig);
    stVoConfig.enDstDynamicRange = enDynamicRange;
    stVoConfig.enVoIntfType = VO_INTF_HDMI;
    stVoConfig.enIntfSync = VO_OUTPUT_1080P60;
    stVoConfig.enPicSize = enPicSize;

    /************************************************
    step9:  start vo
    *************************************************/
    s32Ret = SAMPLE_COMM_VO_StartVO(&stVoConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start vo failed. s32Ret: 0x%x !\n", s32Ret);
        goto EXIT4;
    }

    /************************************************
    step10:  Bind vpss and VO
    *************************************************/
    s32Ret = SAMPLE_COMM_VPSS_Bind_VO(VpssGrp, VpssChn, stVoConfig.VoDev, VoChn);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("vo bind vpss failed. s32Ret: 0x%x !\n", s32Ret);
        goto EXIT5;
    }

    s32Ret = SAMPLE_COMM_VENC_StartGetStream(VencChn, sizeof(VencChn)/sizeof(VENC_CHN));
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Get venc stream failed!\n");
        goto EXIT6;
    }
 
    PAUSE();

    SAMPLE_COMM_VENC_StopGetStream();
    
    EXIT6:
        SAMPLE_COMM_VPSS_UnBind_VO(VpssGrp, VpssChn, stVoConfig.VoDev, VoChn);
    EXIT5:
        SAMPLE_COMM_VO_StopVO(&stVoConfig);
    EXIT4:
        SAMPLE_COMM_VPSS_UnBind_VENC(VpssGrp, VpssChn, VencChn[0]);
    EXIT3:
        SAMPLE_COMM_VENC_Stop(VencChn[0]);
    EXIT2:
        SAMPLE_COMM_VPSS_Stop(VpssGrp, abChnEnable);
    EXIT1:
        SAMPLE_COMM_VI_StopVi(&stViConfig);
    EXIT:
        SAMPLE_COMM_SYS_Exit();
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

#ifdef __HuaweiLite__
#else
    signal(SIGINT, SAMPLE_VIO_HandleSig);
    signal(SIGTERM, SAMPLE_VIO_HandleSig);
#endif

    s32Ret = SAMPLE_VIO_OV9712_PreView(enVoIntfType);

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

