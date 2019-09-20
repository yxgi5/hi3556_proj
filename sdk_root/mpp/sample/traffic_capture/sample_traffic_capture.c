
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
#include <sys/prctl.h>

#include "sample_comm.h"


/******************************************************************************
* function : show usage
******************************************************************************/
static void SAMPLE_Capture_Usage(char* sPrgNm)
{
    printf("Usage : %s <index> \n", sPrgNm);
    printf("index:\n");
    printf("\t 0)sample of traffic picture capture .\n");

    return;
}

#ifndef __HuaweiLite__
/******************************************************************************
* function : to process abnormal case
******************************************************************************/
static void SAMPLE_Capture_HandleSig(HI_S32 signo)
{
    if (SIGINT == signo || SIGTERM == signo)
    {
        SAMPLE_COMM_All_ISP_Stop();
        SAMPLE_COMM_SYS_Exit();
        printf("\033[0;31mprogram termination abnormally!\033[0;39m\n");
    }
    exit(-1);
}
#endif

extern HI_S32 SAMPLE_COMM_VI_GetChnAttrBySns(SAMPLE_SNS_TYPE_E enSnsType, VI_CHN_ATTR_S* pstChnAttr);
extern HI_S32 SAMPLE_COMM_VI_GetPipeAttrBySns(SAMPLE_SNS_TYPE_E enSnsType, VI_PIPE_ATTR_S* pstPipeAttr);
extern HI_S32 SAMPLE_COMM_VI_StopMIPI(SAMPLE_VI_CONFIG_S* pstViConfig);

#define USER_INFO_LINE_NUM 4

static HI_S32 SAMPLE_Capture_StartVi(SAMPLE_VI_CONFIG_S* pstViConfig)
{
    HI_S32 s32Ret = HI_SUCCESS;
    HI_S32 s32SnsId = 0;
    SAMPLE_SNS_TYPE_E enSnsType = pstViConfig->astViInfo[s32SnsId].stSnsInfo.enSnsType;
    VI_DEV ViDev                = pstViConfig->astViInfo[s32SnsId].stDevInfo.ViDev;
    VI_DEV_ATTR_S stViDevAttr;
    VI_PIPE_ATTR_S stPipeAttr;
    VI_CHN ViChn;
    VI_CHN_ATTR_S stChnAttr;
    VI_PIPE ViPipe;
    VI_DEV_BIND_PIPE_S  stDevBindPipe;
    HI_S32 i;
    VI_PIPE ViRawOutPipe = pstViConfig->astViInfo[s32SnsId].stPipeInfo.aPipe[0];
    VI_PIPE ViCapturePipe = pstViConfig->astViInfo[s32SnsId].stPipeInfo.aPipe[3];

    s32Ret = SAMPLE_COMM_VI_StartMIPI(pstViConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_StartMIPI failed with %#x\n", s32Ret);
        return s32Ret;
    }

    SAMPLE_COMM_VI_GetDevAttrBySns(enSnsType, &stViDevAttr);
    s32Ret = HI_MPI_VI_SetDevAttr(ViDev, &stViDevAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VI_SetDevAttr failed with %#x\n", s32Ret);
        return s32Ret;
    }

    s32Ret = HI_MPI_VI_EnableDev(ViDev);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VI_EnableDev failed with %#x\n", s32Ret);
        return s32Ret;
    }

    stDevBindPipe.u32Num = 4;
    stDevBindPipe.PipeId[0] = pstViConfig->astViInfo[s32SnsId].stPipeInfo.aPipe[0];
    stDevBindPipe.PipeId[1] = pstViConfig->astViInfo[s32SnsId].stPipeInfo.aPipe[1];
    stDevBindPipe.PipeId[2] = pstViConfig->astViInfo[s32SnsId].stPipeInfo.aPipe[2];
    stDevBindPipe.PipeId[3] = pstViConfig->astViInfo[s32SnsId].stPipeInfo.aPipe[3];
    s32Ret = HI_MPI_VI_SetDevBindPipe(ViDev, &stDevBindPipe);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VI_SetDevBindPipe failed with %#x\n", s32Ret);
        return s32Ret;
    }

    for(i=0; i<WDR_MAX_PIPE_NUM; i++)
    {
        ViPipe = pstViConfig->astViInfo[s32SnsId].stPipeInfo.aPipe[i];

        if(-1 == ViPipe)
        {
            continue;
        }

        SAMPLE_COMM_VI_GetPipeAttrBySns(enSnsType, &stPipeAttr);
        if(ViPipe != ViRawOutPipe)
        {
            stPipeAttr.u32MaxH -= USER_INFO_LINE_NUM;
        }
        s32Ret = HI_MPI_VI_CreatePipe(ViPipe, &stPipeAttr);
        if (HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("HI_MPI_VI_CreatePipe failed with %#x\n", s32Ret);
            return s32Ret;
        }

        s32Ret = HI_MPI_VI_StartPipe(ViPipe);
        if (HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("HI_MPI_VI_StartPipe failed with %#x\n", s32Ret);
            return s32Ret;
        }

        ViChn = pstViConfig->astViInfo[s32SnsId].stChnInfo.ViChn;
        SAMPLE_COMM_VI_GetChnAttrBySns(enSnsType, &stChnAttr);
        if(ViPipe != ViRawOutPipe)
        {
            stChnAttr.stSize.u32Height -= USER_INFO_LINE_NUM;
        }
        stChnAttr.enPixelFormat = pstViConfig->astViInfo[s32SnsId].stChnInfo.enPixFormat;
        stChnAttr.enDynamicRange = pstViConfig->astViInfo[s32SnsId].stChnInfo.enDynamicRange;
        stChnAttr.enVideoFormat = pstViConfig->astViInfo[s32SnsId].stChnInfo.enVideoFormat;
        stChnAttr.enCompressMode = pstViConfig->astViInfo[s32SnsId].stChnInfo.enCompressMode;
        if(ViPipe == ViCapturePipe)
        {
            stChnAttr.u32Depth = 1;
        }
        s32Ret = HI_MPI_VI_SetChnAttr(ViPipe, ViChn, &stChnAttr);
        if (HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("HI_MPI_VI_SetChnAttr failed with %#x\n", s32Ret);
            return s32Ret;
        }

        /* Disconnect data flow between FE and BE in driver */
        s32Ret = HI_MPI_VI_SetPipeFrameSource(ViPipe, VI_PIPE_FRAME_SOURCE_USER_BE);
        if (HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("HI_MPI_VI_SetPipeFrameSource failed with %#x\n", s32Ret);
            return s32Ret;
        }

        if(ViPipe != ViRawOutPipe)
        {
            s32Ret = HI_MPI_VI_EnableChn(ViPipe, ViChn);
            if (HI_SUCCESS != s32Ret)
            {
                SAMPLE_PRT("HI_MPI_VI_EnableChn failed with %#x\n", s32Ret);
                return s32Ret;
            }

            /* Close RAW data flow written by FE */
            s32Ret = HI_MPI_VI_DisablePipeInterrupt(ViPipe);
            if (HI_SUCCESS != s32Ret)
            {
                SAMPLE_PRT("HI_MPI_VI_DisablePipeInterrupt failed with %#x\n", s32Ret);
                return s32Ret;
            }
        }
    }

    return HI_SUCCESS;
}

static HI_S32 SAMPLE_Capture_StopVi(SAMPLE_VI_CONFIG_S* pstViConfig)
{
    HI_S32 s32Ret = HI_SUCCESS;
    HI_S32 s32SnsId = 0;
    VI_DEV ViDev = pstViConfig->astViInfo[s32SnsId].stDevInfo.ViDev;
    VI_CHN ViChn;
    VI_PIPE ViPipe;
    HI_S32 i;

    for(i=0; i<WDR_MAX_PIPE_NUM; i++)
    {
        ViPipe = pstViConfig->astViInfo[s32SnsId].stPipeInfo.aPipe[i];

        if(-1 == ViPipe)
        {
            continue;
        }

        ViChn = pstViConfig->astViInfo[s32SnsId].stChnInfo.ViChn;
        s32Ret = HI_MPI_VI_DisableChn(ViPipe, ViChn);
        if (HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("HI_MPI_VI_DisableChn failed with %#x\n", s32Ret);
        }

        s32Ret = HI_MPI_VI_StopPipe(ViPipe);
        if (HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("HI_MPI_VI_StopPipe failed with %#x\n", s32Ret);
        }

        s32Ret = HI_MPI_VI_DestroyPipe(ViPipe);
        if (HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("HI_MPI_VI_DestroyPipe failed with %#x\n", s32Ret);
        }
    }

    s32Ret = HI_MPI_VI_DisableDev(ViDev);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VI_DisableDev failed with %#x\n", s32Ret);
    }

    s32Ret = SAMPLE_COMM_VI_StopMIPI(pstViConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_StartMIPI failed with %#x\n", s32Ret);
    }

    return HI_SUCCESS;
}

static HI_S32 SAMPLE_Capture_StartIsp_NoRun(SAMPLE_VI_CONFIG_S* pstViConfig)
{
    HI_S32 s32Ret = HI_SUCCESS;
    HI_U32 u32SnsId = 0;
    VI_PIPE ViPipe;
    ISP_PUB_ATTR_S stPubAttr;
    HI_S32 s32BusId = -1;
    HI_U32 i;
    VI_PIPE ViRawOutPipe = pstViConfig->astViInfo[u32SnsId].stPipeInfo.aPipe[0];
    VI_PIPE ViControlSensorPipe = pstViConfig->astViInfo[u32SnsId].stPipeInfo.aPipe[1];

    u32SnsId = pstViConfig->astViInfo[0].stSnsInfo.s32SnsId;

    for(i=0; i<WDR_MAX_PIPE_NUM; i++)
    {
        ViPipe = pstViConfig->astViInfo[u32SnsId].stPipeInfo.aPipe[i];

        if(-1 == ViPipe)
        {
            continue;
        }

        SAMPLE_COMM_ISP_GetIspAttrBySns(pstViConfig->astViInfo[u32SnsId].stSnsInfo.enSnsType, &stPubAttr);
        stPubAttr.enWDRMode = pstViConfig->astViInfo[u32SnsId].stDevInfo.enWDRMode;
        if(ViPipe != ViRawOutPipe)
        {
            stPubAttr.stWndRect.u32Height -= USER_INFO_LINE_NUM;
        }

        /* //Because the pipe0 of 9M@50FPS sensor do flip in cmos.c, the bayer array changed to BGGR*/
        if (SONY_IMX477_MIPI_9M_50FPS_10BIT == pstViConfig->astViInfo[u32SnsId].stSnsInfo.enSnsType)
        {
            stPubAttr.enBayer = BAYER_RGGB;
        }

        s32Ret = SAMPLE_COMM_ISP_Sensor_Regiter_callback(ViPipe, u32SnsId);
        if (HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("register sensor %d to ISP %d failed\n", u32SnsId, ViPipe);
            return HI_FAILURE;
        }

        if(ViPipe == ViControlSensorPipe)
        {
            s32BusId = pstViConfig->astViInfo[u32SnsId].stSnsInfo.s32BusId;
        }
        else
        {
            s32BusId= -1;
        }

        s32Ret = SAMPLE_COMM_ISP_BindSns(ViPipe, u32SnsId, pstViConfig->astViInfo[u32SnsId].stSnsInfo.enSnsType, s32BusId);
        if (HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("register sensor %d bus id %d failed\n", u32SnsId, s32BusId);
            return HI_FAILURE;
        }

        s32Ret = SAMPLE_COMM_ISP_Aelib_Callback(ViPipe);
        if (HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("SAMPLE_COMM_ISP_Aelib_Callback failed\n");
            return HI_FAILURE;
        }

        s32Ret = SAMPLE_COMM_ISP_Awblib_Callback(ViPipe);
        if (HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("SAMPLE_COMM_ISP_Awblib_Callback failed\n");
            return HI_FAILURE;
        }

        s32Ret = HI_MPI_ISP_MemInit(ViPipe);
        if (s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("Init Ext memory failed with %#x!\n", s32Ret);
            return HI_FAILURE;
        }

        s32Ret = HI_MPI_ISP_SetPubAttr(ViPipe, &stPubAttr);
        if (s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("SetPubAttr failed with %#x!\n", s32Ret);
            return HI_FAILURE;
        }

        s32Ret = HI_MPI_ISP_Init(ViPipe);
        if (s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("ISP Init failed with %#x!\n", s32Ret);
            return HI_FAILURE;
        }
    }

    return s32Ret;
}

static HI_S32 SAMPLE_Capture_StopIsp_NoRun(SAMPLE_VI_CONFIG_S* pstViConfig)
{
    HI_S32 s32Ret = HI_SUCCESS;
    HI_U32 u32SnsId;
    VI_PIPE ViPipe;
    HI_U32 i;

    u32SnsId = pstViConfig->astViInfo[0].stSnsInfo.s32SnsId;

    for(i=0; i<WDR_MAX_PIPE_NUM; i++)
    {
        ViPipe = pstViConfig->astViInfo[u32SnsId].stPipeInfo.aPipe[i];

        if(-1 == ViPipe)
        {
            continue;
        }

        s32Ret = HI_MPI_ISP_Exit(ViPipe);
        if (s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("HI_MPI_ISP_Exit failed with %#x!\n", s32Ret);
        }

        s32Ret = SAMPLE_COMM_ISP_Awblib_UnCallback(ViPipe);
        if (s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("SAMPLE_COMM_ISP_Awblib_UnCallback failed with %#x!\n", s32Ret);
        }

        s32Ret = SAMPLE_COMM_ISP_Aelib_UnCallback(ViPipe);
        if (s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("SAMPLE_COMM_ISP_Aelib_UnCallback failed with %#x!\n", s32Ret);
        }

        s32Ret = SAMPLE_COMM_ISP_Sensor_UnRegiter_callback(ViPipe);
        if (s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("SAMPLE_COMM_ISP_Sensor_UnRegiter_callback failed with %#x!\n", s32Ret);
        }

    }

    return s32Ret;
}

static HI_S32 SAMPLE_Capture_SetIspParam(VI_PIPE ViPipe)
{
    HI_S32 s32Ret = HI_SUCCESS;
    VI_PIPE VideoPipe = 1;
    ISP_EXPOSURE_ATTR_S  stExpAttr;
    ISP_WB_ATTR_S  stWBAttr;

    /***************************************
    use your own isp param...
    ****************************************/
    /* isp ae param set */
    ISP_EXP_INFO_S  stExpInfo;
    s32Ret = HI_MPI_ISP_QueryExposureInfo(VideoPipe, &stExpInfo);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_ISP_QueryExposureInfo failed with %#x\n", s32Ret);
        return s32Ret;
    }

    s32Ret = HI_MPI_ISP_GetExposureAttr(ViPipe, &stExpAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_ISP_GetExposureAttr failed with %#x\n", s32Ret);
        return s32Ret;
    }

    stExpAttr.enOpType = OP_TYPE_MANUAL;
    stExpAttr.stManual.enExpTimeOpType = OP_TYPE_MANUAL;
    stExpAttr.stManual.enAGainOpType = OP_TYPE_MANUAL;
    stExpAttr.stManual.enDGainOpType = OP_TYPE_MANUAL;
    stExpAttr.stManual.enISPDGainOpType = OP_TYPE_MANUAL;
    stExpAttr.stManual.u32ExpTime = stExpInfo.u32ExpTime;
    stExpAttr.stManual.u32AGain = stExpInfo.u32AGain;
    stExpAttr.stManual.u32DGain = stExpInfo.u32DGain;
    stExpAttr.stManual.u32ISPDGain = stExpInfo.u32ISPDGain;
    s32Ret = HI_MPI_ISP_SetExposureAttr(ViPipe, &stExpAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_ISP_SetExposureAttr failed with %#x\n", s32Ret);
        return s32Ret;
    }

    /* isp awb param set */
    ISP_WB_INFO_S  stWBInfo;
    s32Ret = HI_MPI_ISP_QueryWBInfo(VideoPipe, &stWBInfo);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_ISP_QueryWBInfo failed with %#x\n", s32Ret);
        return s32Ret;
    }

    s32Ret = HI_MPI_ISP_GetWBAttr(ViPipe, &stWBAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_ISP_GetWBAttr failed with %#x\n", s32Ret);
        return s32Ret;
    }

    stWBAttr.enOpType = OP_TYPE_MANUAL;
    stWBAttr.stManual.u16Rgain = stWBInfo.u16Rgain;
    stWBAttr.stManual.u16Grgain = stWBInfo.u16Grgain;
    stWBAttr.stManual.u16Gbgain = stWBInfo.u16Gbgain;
    stWBAttr.stManual.u16Bgain = stWBInfo.u16Bgain;
    s32Ret = HI_MPI_ISP_SetWBAttr(ViPipe, &stWBAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_ISP_SetWBAttr failed with %#x\n", s32Ret);
        return s32Ret;
    }

    return HI_SUCCESS;
}

static HI_S32 SAMPLE_Capture_StartJpege(VENC_CHN VencChn, SIZE_S stSize)
{
    HI_S32 s32Ret = HI_SUCCESS;
    VENC_CHN_ATTR_S stAttr = {0};
    VENC_RECV_PIC_PARAM_S  stRecvParam;

    stAttr.stVencAttr.enType = PT_JPEG;
    stAttr.stVencAttr.u32MaxPicWidth = stSize.u32Width;
    stAttr.stVencAttr.u32MaxPicHeight = stSize.u32Height;
    stAttr.stVencAttr.u32BufSize = stSize.u32Width * stSize.u32Height * 2;
    stAttr.stVencAttr.u32Profile = 0;
    stAttr.stVencAttr.bByFrame = HI_TRUE;
    stAttr.stVencAttr.u32PicWidth = stSize.u32Width;
    stAttr.stVencAttr.u32PicHeight = stSize.u32Height;
    stAttr.stVencAttr.stAttrJpege.bSupportDCF = HI_FALSE;
    stAttr.stVencAttr.stAttrJpege.stMPFCfg.u8LargeThumbNailNum = 0;

    s32Ret = HI_MPI_VENC_CreateChn(VencChn, &stAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VENC_CreateChn failed with %#x\n", s32Ret);
        return s32Ret;
    }

    stRecvParam.s32RecvPicNum = -1;
    s32Ret = HI_MPI_VENC_StartRecvFrame(VencChn,&stRecvParam);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VENC_StartRecvPic faild with%#x! \n", s32Ret);
        return HI_FAILURE;
    }

    return HI_SUCCESS;
}

static HI_S32 SAMPLE_Capture_TriggerFrameProc(VI_PIPE ViPipe, VI_CHN ViChn, VENC_CHN VeChn, VIDEO_FRAME_INFO_S* pstRawInfo)
{
    HI_S32 s32Ret = HI_SUCCESS;
    VI_PIPE aPipeId[1];
    const VIDEO_FRAME_INFO_S* pastRawInfo[1];
    VIDEO_FRAME_INFO_S stYUVFrameInfo;
    HI_S32 s32MilliSec = 80;


    /**********************************
    1. first send raw
    ***********************************/
    s32Ret = SAMPLE_Capture_SetIspParam(ViPipe);
    if (HI_SUCCESS != s32Ret)
    {
        return s32Ret;
    }

    s32Ret = HI_MPI_ISP_RunOnce(ViPipe);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_ISP_RunOnce failed with %#x\n", s32Ret);
        return s32Ret;
    }

    aPipeId[0] = ViPipe;
    pastRawInfo[0] = pstRawInfo;
    s32Ret = HI_MPI_VI_SendPipeRaw(1, aPipeId, pastRawInfo, s32MilliSec);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VI_SendPipeRaw failed with %#x\n", s32Ret);
        return s32Ret;
    }

    #if 0
    ISP_VD_TYPE_E enIspVDType = ISP_VD_BE_END;
    s32Ret = HI_MPI_ISP_GetVDTimeOut(ViPipe, enIspVDType, s32MilliSec);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_ISP_GetVDTimeOut failed with %#x\n", s32Ret);
        return s32Ret;
    }
    #endif
    /* HI_MPI_VI_GetChnFrame purpose:
       1. wait for raw data processing to finish;
       2. discard the first yuv frame; */
    s32Ret = HI_MPI_VI_GetChnFrame(ViPipe, ViChn, &stYUVFrameInfo, s32MilliSec);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VI_GetChnFrame failed with %#x\n", s32Ret);
        return s32Ret;
    }

    s32Ret = HI_MPI_VI_ReleaseChnFrame(ViPipe, ViChn, &stYUVFrameInfo);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VI_ReleaseChnFrame failed with %#x\n", s32Ret);
        return s32Ret;
    }

    ISP_AE_STATISTICS_S  stAeStat;
    s32Ret = HI_MPI_ISP_GetAEStatistics(ViPipe, &stAeStat);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_ISP_GetAEStatistics failed with %#x\n", s32Ret);
        return s32Ret;
    }

    ISP_WB_STATISTICS_S stWBStat;
    s32Ret = HI_MPI_ISP_GetWBStatistics(ViPipe, &stWBStat);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_ISP_GetWBStatistics failed with %#x\n", s32Ret);
        return s32Ret;
    }

    int i;
    HI_S32 s32HistSum = 0;
    for(i=0; i<HIST_NUM; i++)
    {
        s32HistSum += stAeStat.au32BEHist1024Value[i];
    }
    printf("Get sum of histogram :%d\n", s32HistSum);

    HI_S32 s32WB_RSum = 0;
    HI_S32 s32WB_GSum = 0;
    HI_S32 s32WB_BSum = 0;
    for(i=0; i<AWB_ZONE_NUM; i++)
    {
        s32WB_RSum += stWBStat.au16ZoneAvgR[i];
        s32WB_GSum += stWBStat.au16ZoneAvgG[i];
        s32WB_BSum += stWBStat.au16ZoneAvgB[i];
    }
    printf("Get global WB:%d, %d, %d\n", stWBStat.u16GlobalR, stWBStat.u16GlobalG, stWBStat.u16GlobalB);
    printf("Get sum of WB:%d, %d, %d\n", s32WB_RSum, s32WB_GSum, s32WB_BSum);

    /* Calculate isp param from stAeStat and stWBStat */
    /* ... */

    /* Set isp param */
    s32Ret = SAMPLE_Capture_SetIspParam(ViPipe);
    if (HI_SUCCESS != s32Ret)
    {
        return s32Ret;
    }

    /*************************************
    2. second send raw
    **************************************/
    s32Ret = HI_MPI_ISP_RunOnce(ViPipe);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_ISP_RunOnce failed with %#x\n", s32Ret);
        return s32Ret;
    }

    s32Ret = HI_MPI_VI_SendPipeRaw(1, aPipeId, pastRawInfo, s32MilliSec);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VI_SendPipeRaw failed with %#x\n", s32Ret);
        return s32Ret;
    }

    s32Ret = HI_MPI_VI_GetChnFrame(ViPipe, ViChn, &stYUVFrameInfo, s32MilliSec);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VI_GetChnFrame failed with %#x\n", s32Ret);
        return s32Ret;
    }

    s32Ret = HI_MPI_VENC_SendFrame(VeChn, &stYUVFrameInfo, s32MilliSec);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VI_GetChnFrame failed with %#x\n", s32Ret);
        return s32Ret;
    }

    s32Ret = HI_MPI_VI_ReleaseChnFrame(ViPipe, ViChn, &stYUVFrameInfo);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VI_ReleaseChnFrame failed with %#x\n", s32Ret);
        return s32Ret;
    }

    return HI_SUCCESS;
}

static HI_S32 SAMPLE_Capture_VideoFrameProc(VI_PIPE ViPipe, VIDEO_FRAME_INFO_S* pstRawInfo)
{
    HI_S32 s32Ret = HI_SUCCESS;
    VI_PIPE aPipeId[1];
    const VIDEO_FRAME_INFO_S* pastRawInfo[1];
    HI_S32 s32MilliSec = 80;

    s32Ret = HI_MPI_ISP_RunOnce(ViPipe);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_ISP_RunOnce failed with %#x\n", s32Ret);
        return s32Ret;
    }

    aPipeId[0] = ViPipe;
    pastRawInfo[0] = pstRawInfo;
    s32Ret = HI_MPI_VI_SendPipeRaw(1, aPipeId, pastRawInfo, s32MilliSec);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VI_SendPipeRaw failed with %#x\n", s32Ret);
        return s32Ret;
    }

    return HI_SUCCESS;
}


HI_S32 SAMPLE_TrafficCapture_Offline(HI_VOID)
{
    SAMPLE_SNS_TYPE_E  enSnsType;
    VI_DEV             ViDev                = 5;
    VI_PIPE            ViRawOutPipe         = 0;
    VI_PIPE            ViLongExpPipe        = 1;
    VI_PIPE            ViShortExpPipe       = 2;
    VI_PIPE            ViCapturePipe        = 3;
    VI_CHN             ViChn                = 0;
    HI_S32             s32SnsId             = 0;
    VO_DEV             VoDev                = SAMPLE_VO_DEV_DHD0;
    VO_CHN             VoChn                = 0;
    WDR_MODE_E         enWDRMode            = WDR_MODE_NONE;
    DYNAMIC_RANGE_E    enDynamicRange       = DYNAMIC_RANGE_SDR8;
    PIXEL_FORMAT_E     enPixFormat          = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    VIDEO_FORMAT_E     enVideoFormat        = VIDEO_FORMAT_LINEAR;
    COMPRESS_MODE_E    enCompressMode       = COMPRESS_MODE_NONE;
    VI_VPSS_MODE_E     enVideoPipeMode      = VI_OFFLINE_VPSS_OFFLINE;
    VB_CONFIG_S        stVbConf;
    PIC_SIZE_E         enPicSize;
    HI_U32             u32BlkSize;
    SAMPLE_VI_CONFIG_S stViConfig;
    SAMPLE_VO_CONFIG_S stVoConfig;
    SIZE_S stSize;
    VIDEO_FRAME_INFO_S  stRawInfo;
    VENC_CHN VencChn = 0;
    HI_U32 i = 0;
    HI_S32 s32Ret = HI_SUCCESS;
    combo_dev_t          ComboDev;

    ComboDev = SAMPLE_COMM_VI_GetComboDevBySensor(stViConfig.astViInfo[s32SnsId].stSnsInfo.enSnsType, s32SnsId);

    /************************************************
    step1:  Get all sensors information
    *************************************************/
    SAMPLE_COMM_VI_GetSensorInfo(&stViConfig);
    stViConfig.s32WorkingViNum                           = 1;
    stViConfig.as32WorkingViId[0]                        = 0;
    enSnsType = stViConfig.astViInfo[0].stSnsInfo.enSnsType;
    stViConfig.astViInfo[0].stSnsInfo.MipiDev            = ComboDev;
    stViConfig.astViInfo[0].stSnsInfo.s32BusId           = 0;
    stViConfig.astViInfo[0].stSnsInfo.s32SnsId           = s32SnsId;

    stViConfig.astViInfo[0].stDevInfo.ViDev              = ViDev;
    stViConfig.astViInfo[0].stDevInfo.enWDRMode          = enWDRMode;

    stViConfig.astViInfo[0].stPipeInfo.enMastPipeMode    = enVideoPipeMode;
    stViConfig.astViInfo[0].stPipeInfo.aPipe[0]          = ViRawOutPipe;
    stViConfig.astViInfo[0].stPipeInfo.aPipe[1]          = ViLongExpPipe;
    stViConfig.astViInfo[0].stPipeInfo.aPipe[2]          = ViShortExpPipe;
    stViConfig.astViInfo[0].stPipeInfo.aPipe[3]          = ViCapturePipe;

    stViConfig.astViInfo[0].stChnInfo.ViChn              = ViChn;
    stViConfig.astViInfo[0].stChnInfo.enPixFormat        = enPixFormat;
    stViConfig.astViInfo[0].stChnInfo.enDynamicRange     = enDynamicRange;
    stViConfig.astViInfo[0].stChnInfo.enVideoFormat      = enVideoFormat;
    stViConfig.astViInfo[0].stChnInfo.enCompressMode     = enCompressMode;


    /************************************************
    step2:  Get  input size
    *************************************************/
    s32Ret = SAMPLE_COMM_VI_GetSizeBySensor(enSnsType, &enPicSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_GetSizeBySensor failed with %#x\n", s32Ret);
        return s32Ret;
    }

    s32Ret = SAMPLE_COMM_SYS_GetPicSize(enPicSize, &stSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed with %#x\n", s32Ret);
        return s32Ret;
    }

    /************************************************
    step3:  Init SYS and common VB
    *************************************************/
    memset(&stVbConf, 0, sizeof(VB_CONFIG_S));
    stVbConf.u32MaxPoolCnt              = 2;

    u32BlkSize = COMMON_GetPicBufferSize(stSize.u32Width, stSize.u32Height, SAMPLE_PIXEL_FORMAT, DATA_BITWIDTH_8, enCompressMode, DEFAULT_ALIGN);
    stVbConf.astCommPool[0].u64BlkSize  = u32BlkSize;
    stVbConf.astCommPool[0].u32BlkCnt   = 18;

    u32BlkSize = VI_GetRawBufferSize(stSize.u32Width, stSize.u32Height, PIXEL_FORMAT_RGB_BAYER_12BPP, COMPRESS_MODE_NONE, DEFAULT_ALIGN);
    stVbConf.astCommPool[1].u64BlkSize  = u32BlkSize;
    stVbConf.astCommPool[1].u32BlkCnt   = 8;

    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("system init failed with %#x\n", s32Ret);
        goto EXIT;
    }

    VI_VPSS_MODE_S  stVIVPSSMode;
    HI_MPI_SYS_GetVIVPSSMode(&stVIVPSSMode);
    stVIVPSSMode.aenMode[0] = VI_OFFLINE_VPSS_OFFLINE;
    stVIVPSSMode.aenMode[1] = VI_OFFLINE_VPSS_OFFLINE;
    stVIVPSSMode.aenMode[2] = VI_OFFLINE_VPSS_OFFLINE;
    stVIVPSSMode.aenMode[3] = VI_OFFLINE_VPSS_OFFLINE;
    s32Ret = HI_MPI_SYS_SetVIVPSSMode(&stVIVPSSMode);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_SYS_SetVIVPSSMode failed with %#x\n", s32Ret);
        goto EXIT;
    }

    /************************************************
    step4:  Init VI ISP
    *************************************************/
    s32Ret = SAMPLE_Capture_StartVi(&stViConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_Capture_StartIsp_NoRun failed with %#x\n", s32Ret);
        goto EXIT;
    }

    s32Ret = SAMPLE_Capture_StartIsp_NoRun(&stViConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_Capture_StartIsp_NoRun failed with %#x\n", s32Ret);
        goto EXIT1;
    }

    /************************************************
    step5:  Init VO
    *************************************************/
    s32Ret = SAMPLE_COMM_VO_GetDefConfig(&stVoConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VO_StartVO failed with %#x\n", s32Ret);
        goto EXIT2;
    }

    stVoConfig.enVoMode = VO_MODE_2MUX;
    //stVoConfig.enVoMode = 0;
    stVoConfig.enVoIntfType   = VO_INTF_HDMI;
    stVoConfig.enIntfSync     = VO_OUTPUT_1080P60;
    s32Ret = SAMPLE_COMM_VO_StartVO(&stVoConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VO_StartVO failed with %#x\n", s32Ret);
        goto EXIT2;
    }

    s32Ret = SAMPLE_COMM_VI_Bind_VO(ViLongExpPipe, ViChn, stVoConfig.VoDev, VoChn);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_Bind_VO failed with %#x\n", s32Ret);
        goto EXIT3;
    }

    VoChn = 1;
    s32Ret = SAMPLE_COMM_VI_Bind_VO(ViShortExpPipe, ViChn, stVoConfig.VoDev, VoChn);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_Bind_VO failed with %#x\n", s32Ret);
        goto EXIT3;
    }

    /************************************************
    step6:  Init jpege
    *************************************************/
    stSize.u32Height -= USER_INFO_LINE_NUM;
    s32Ret = SAMPLE_Capture_StartJpege(VencChn, stSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VENC_SnapStart failed with %#x\n", s32Ret);
        goto EXIT3;
    }

    /************************************************
    step7:  main capture process
    *************************************************/
    VI_DUMP_ATTR_S stDumpAttr;
    stDumpAttr.bEnable = HI_TRUE;
    stDumpAttr.u32Depth = 3;
    s32Ret = HI_MPI_VI_SetPipeDumpAttr(ViRawOutPipe, &stDumpAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VI_SetPipeDumpAttr failed with %#x\n", s32Ret);
        goto EXIT4;
    }

    for (i = 0; i < 2000; i++)
    {
        HI_S32 s32MilliSec = 10000;
        HI_BOOL bLongExpFrame;
        s32Ret = HI_MPI_VI_GetPipeFrame(ViRawOutPipe, &stRawInfo, s32MilliSec);
        if (HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("HI_MPI_VI_GetPipeFrame failed with %#x\n", s32Ret);
            continue;
        }

        /* Use u32TimeRef or u64PTS to judge frame which is long exposure or short exposure. */
        bLongExpFrame = (stRawInfo.stVFrame.u32TimeRef % 4) ? HI_TRUE : HI_FALSE;

        /* crop user info data and use user info to do what you want to do. */
        stRawInfo.stVFrame.u32Height -= USER_INFO_LINE_NUM;

        /* find the capture raw frame, this is just a demo. */
        if (i == 99)
        {
            s32Ret = SAMPLE_Capture_TriggerFrameProc(ViCapturePipe, ViChn, VencChn, &stRawInfo);
            if (HI_SUCCESS != s32Ret)
            {
                break;
            }

            /* get jpg */
            s32Ret = SAMPLE_COMM_VENC_SaveJpeg(VencChn, 1);
            if (HI_SUCCESS != s32Ret)
            {
                SAMPLE_PRT("SAMPLE_COMM_VENC_SaveJpeg failed with %#x\n", s32Ret);
                break;
            }
            printf("Save captured picture successfully.\n");
        }
        else
        {
            if(bLongExpFrame)
            {
                s32Ret = SAMPLE_Capture_VideoFrameProc(ViLongExpPipe, &stRawInfo);
                if (HI_SUCCESS != s32Ret)
                {
                    break;
                }

                /* record a video from this yuv, or something else you want... */
            }
            else
            {
                s32Ret = SAMPLE_Capture_VideoFrameProc(ViShortExpPipe, &stRawInfo);
                if (HI_SUCCESS != s32Ret)
                {
                    break;
                }

                /* do some intelligent processing from this yuv ... */
            }
        }

        s32Ret = HI_MPI_VI_ReleasePipeFrame(ViRawOutPipe, &stRawInfo);
        if (HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("HI_MPI_VI_ReleasePipeFrame failed with %#x\n", s32Ret);
            goto EXIT5;
        }
    }
    if (HI_SUCCESS != s32Ret)
    {
         s32Ret = HI_MPI_VI_ReleasePipeFrame(ViRawOutPipe, &stRawInfo);
        if (HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("HI_MPI_VI_ReleasePipeFrame failed with %#x\n", s32Ret);
        }
    }

EXIT5:
    stDumpAttr.bEnable = HI_FALSE;
    stDumpAttr.u32Depth = 0;
    HI_MPI_VI_SetPipeDumpAttr(ViRawOutPipe, &stDumpAttr);
EXIT4:
    SAMPLE_COMM_VENC_Stop(VencChn);
EXIT3:
    SAMPLE_COMM_VI_UnBind_VO(ViShortExpPipe, ViChn, VoDev, VoChn);
    VoChn = 0;
    SAMPLE_COMM_VI_UnBind_VO(ViLongExpPipe, ViChn, VoDev, VoChn);
    SAMPLE_COMM_VO_StopVO(&stVoConfig);
EXIT2:
    SAMPLE_Capture_StopIsp_NoRun(&stViConfig);
EXIT1:
    SAMPLE_Capture_StopVi(&stViConfig);
EXIT:
    SAMPLE_COMM_SYS_Exit();

    return HI_SUCCESS;
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

    if (argc < 2)
    {
        SAMPLE_Capture_Usage(argv[0]);
        return HI_FAILURE;
    }

#ifndef __HuaweiLite__
    signal(SIGINT, SAMPLE_Capture_HandleSig);
    signal(SIGTERM, SAMPLE_Capture_HandleSig);
#endif

    s32Index = atoi(argv[1]);
    switch (s32Index)
    {
        case 0:
            s32Ret = SAMPLE_TrafficCapture_Offline();
            break;

        default:
            SAMPLE_PRT("the index %d is invaild!\n",s32Index);
            SAMPLE_Capture_Usage(argv[0]);
            return HI_FAILURE;
    }

    if (HI_SUCCESS == s32Ret)
    {
        SAMPLE_PRT("program exit normally!\n");
    }
    else
    {
        SAMPLE_PRT("program exit abnormally!\n");
    }

    return (s32Ret);
}


#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */

