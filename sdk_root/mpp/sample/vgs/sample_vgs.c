
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
#include <sys/prctl.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "sample_comm.h"

pthread_t VgsThreadId;
HI_BOOL bVgsThreadExit = HI_FALSE;


/******************************************************************************
* function : show usage
******************************************************************************/
HI_VOID SAMPLE_VGS_VoInterface_Usage(HI_VOID)
{
    printf("intf:\n");
    printf("\t 0) vo HDMI output, default.\n");
    printf("\t 1) vo BT1120 output.\n");
}

void SAMPLE_VGS_Usage(char* sPrgNm)
{
    printf("Usage : %s <index> <intf>\n", sPrgNm);
    printf("index:\n");
    printf("\t 0) SDR10 COMPRESS, VI - VGS - VO - HDMI. \n");

    printf("intf:\n");
    printf("\t 0) vo HDMI output, default.\n");
    printf("\t 1) vo BT1120 output.\n");
    return;
}

/******************************************************************************
* function : to process abnormal case
******************************************************************************/
void SAMPLE_VGS_HandleSig(HI_S32 signo)
{
    if (SIGINT == signo || SIGTERM == signo)
    {
        SAMPLE_COMM_All_ISP_Stop();
        SAMPLE_COMM_SYS_Exit();
        printf("\033[0;31mprogram termination abnormally!\033[0;39m\n");
    }
    exit(-1);
}

typedef struct hiSAMPLE_VGS_MOD_INFO_S
{
    VI_PIPE                ViPipe;
    VI_CHN                 ViChn;
    VO_DEV                VoDev;
    VO_CHN                VoChn;
}SAMPLE_VGS_MOD_INFO_S;


static HI_VOID *SAMPLE_VGS_Get_Frame_Thread(HI_VOID *arg)
{
    SAMPLE_VGS_MOD_INFO_S *pstVgsModInfo = (SAMPLE_VGS_MOD_INFO_S *)arg;

    VI_PIPE                ViPipe = pstVgsModInfo->ViPipe;
    VI_CHN                 ViChn  = pstVgsModInfo->ViChn;
    VO_DEV                 VoDev  = pstVgsModInfo->VoDev;
    VO_CHN                 VoChn  = pstVgsModInfo->VoChn;

    VGS_HANDLE             hHandle;
    HI_U32                 u32OutWidth        = 1920;
    HI_U32                 u32OutHeight       = 1080;
    VGS_TASK_ATTR_S        stTask;

    HI_U32                 u32Align;
    HI_U32                 u32Width;
    HI_U32                 u32Height;
    PIXEL_FORMAT_E         enPixelFormat;
    DATA_BITWIDTH_E        enBitWidth;
    COMPRESS_MODE_E        enCmpMode;
    VB_CAL_CONFIG_S        stCalConfig;

    VB_BLK                 VbHandle = 0;

    HI_S32                 s32Ret = HI_FAILURE;

    HI_S32                 s32MilliSec        = -1;
    HI_U32                 u32OldDepth        = 2;
    HI_U32                 u32Depth           = 2;
    VI_CHN_ATTR_S          stChnAttr;
    HI_CHAR                acThreadName[15]   = {0};

    snprintf(acThreadName,sizeof(acThreadName),"VgsGetFrame");
    prctl(PR_SET_NAME,(unsigned long)acThreadName);

    /************************************************
    Set depth
    *************************************************/
    s32Ret = HI_MPI_VI_GetChnAttr(ViPipe,ViChn,&stChnAttr);

    if(HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VI_GetChnAttr failed, s32Ret:0x%x\n",s32Ret);
        goto EXIT;
    }

    u32OldDepth         = stChnAttr.u32Depth;
    stChnAttr.u32Depth  = u32Depth;

    s32Ret = HI_MPI_VI_SetChnAttr(ViPipe,ViChn,&stChnAttr);

    if(HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VI_SetChnAttr failed, s32Ret:0x%x\n",s32Ret);
        goto EXIT;
    }

    while(HI_FALSE == bVgsThreadExit)
    {
        /************************************************
        Get VI chn frame
        *************************************************/
        s32Ret = HI_MPI_VI_GetChnFrame(ViPipe, ViChn, &stTask.stImgIn, s32MilliSec);

        if(HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("HI_MPI_VI_GetChnFrame failed, s32Ret:0x%x\n",s32Ret);
            goto EXIT;
        }

        /************************************************
        Create VGS job
        *************************************************/
        u32Align      = 0;
        enCmpMode     = COMPRESS_MODE_SEG;
        enPixelFormat = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        enBitWidth    = DATA_BITWIDTH_10;
        u32Width      = u32OutWidth;
        u32Height     = u32OutHeight;

        COMMON_GetPicBufferConfig(u32Width, u32Height, enPixelFormat, enBitWidth, enCmpMode, u32Align, &stCalConfig);

        VbHandle = HI_MPI_VB_GetBlock(VB_INVALID_POOLID, stCalConfig.u32VBSize, HI_NULL);

        if (VB_INVALID_HANDLE == VbHandle)
        {
            SAMPLE_PRT("HI_MPI_VB_GetBlock failed!\n");
            goto EXIT;
        }

        stTask.stImgOut.u32PoolId                    = HI_MPI_VB_Handle2PoolId(VbHandle);
        stTask.stImgOut.enModId                      = HI_ID_VGS;
        stTask.stImgOut.stVFrame.u32Width            = u32Width;
        stTask.stImgOut.stVFrame.u32Height           = u32Height;
        stTask.stImgOut.stVFrame.enDynamicRange      = DYNAMIC_RANGE_SDR10;
        stTask.stImgOut.stVFrame.enCompressMode      = enCmpMode;
        stTask.stImgOut.stVFrame.enPixelFormat       = enPixelFormat;
        stTask.stImgOut.stVFrame.enVideoFormat       = VIDEO_FORMAT_LINEAR;
        stTask.stImgOut.stVFrame.enField             = VIDEO_FIELD_FRAME;
        stTask.stImgOut.stVFrame.enColorGamut        = COLOR_GAMUT_BT709;
        stTask.stImgOut.stVFrame.u32MaxLuminance     = 1200;
        stTask.stImgOut.stVFrame.u32MinLuminance     = 200;

        stTask.stImgOut.stVFrame.u64HeaderPhyAddr[0] = HI_MPI_VB_Handle2PhysAddr(VbHandle);
        stTask.stImgOut.stVFrame.u64HeaderPhyAddr[1] = stTask.stImgOut.stVFrame.u64HeaderPhyAddr[0] + stCalConfig.u32HeadYSize;
        stTask.stImgOut.stVFrame.u64HeaderPhyAddr[2] = stTask.stImgOut.stVFrame.u64HeaderPhyAddr[1];
        stTask.stImgOut.stVFrame.u64HeaderVirAddr[0] = (HI_U64)HI_MPI_SYS_Mmap(stTask.stImgOut.stVFrame.u64HeaderPhyAddr[0], stCalConfig.u32VBSize);
        stTask.stImgOut.stVFrame.u64HeaderVirAddr[1] = stTask.stImgOut.stVFrame.u64HeaderVirAddr[0] + stCalConfig.u32HeadYSize;
        stTask.stImgOut.stVFrame.u64HeaderVirAddr[2] = stTask.stImgOut.stVFrame.u64HeaderVirAddr[1];
        stTask.stImgOut.stVFrame.u32HeaderStride[0]  = stCalConfig.u32HeadStride;
        stTask.stImgOut.stVFrame.u32HeaderStride[1]  = stCalConfig.u32HeadStride;
        stTask.stImgOut.stVFrame.u32HeaderStride[2]  = stCalConfig.u32HeadStride;

        stTask.stImgOut.stVFrame.u64PhyAddr[0]       = stTask.stImgOut.stVFrame.u64HeaderPhyAddr[0] + stCalConfig.u32HeadSize;
        stTask.stImgOut.stVFrame.u64PhyAddr[1]       = stTask.stImgOut.stVFrame.u64PhyAddr[0] + stCalConfig.u32MainYSize;
        stTask.stImgOut.stVFrame.u64PhyAddr[2]       = stTask.stImgOut.stVFrame.u64PhyAddr[1];
        stTask.stImgOut.stVFrame.u64VirAddr[0]       = stTask.stImgOut.stVFrame.u64HeaderVirAddr[0] + stCalConfig.u32HeadSize;
        stTask.stImgOut.stVFrame.u64VirAddr[1]       = stTask.stImgOut.stVFrame.u64VirAddr[0] + stCalConfig.u32MainYSize;
        stTask.stImgOut.stVFrame.u64VirAddr[2]       = stTask.stImgOut.stVFrame.u64VirAddr[1];
        stTask.stImgOut.stVFrame.u32Stride[0]        = stCalConfig.u32MainStride;
        stTask.stImgOut.stVFrame.u32Stride[1]        = stCalConfig.u32MainStride;
        stTask.stImgOut.stVFrame.u32Stride[2]        = stCalConfig.u32MainStride;

        stTask.stImgOut.stVFrame.u64ExtPhyAddr[0]    = stTask.stImgOut.stVFrame.u64PhyAddr[0] + stCalConfig.u32MainSize;
        stTask.stImgOut.stVFrame.u64ExtPhyAddr[1]    = stTask.stImgOut.stVFrame.u64ExtPhyAddr[0] + stCalConfig.u32ExtYSize;
        stTask.stImgOut.stVFrame.u64ExtPhyAddr[2]    = stTask.stImgOut.stVFrame.u64ExtPhyAddr[1];
        stTask.stImgOut.stVFrame.u64ExtVirAddr[0]    = stTask.stImgOut.stVFrame.u64VirAddr[0] + stCalConfig.u32MainSize;
        stTask.stImgOut.stVFrame.u64ExtVirAddr[1]    = stTask.stImgOut.stVFrame.u64ExtVirAddr[0] + stCalConfig.u32ExtYSize;
        stTask.stImgOut.stVFrame.u64ExtVirAddr[2]    = stTask.stImgOut.stVFrame.u64ExtVirAddr[1];
        stTask.stImgOut.stVFrame.u32ExtStride[0]     = stCalConfig.u32ExtStride;
        stTask.stImgOut.stVFrame.u32ExtStride[1]     = stCalConfig.u32ExtStride;
        stTask.stImgOut.stVFrame.u32ExtStride[2]     = stCalConfig.u32ExtStride;

        s32Ret = HI_MPI_VGS_BeginJob(&hHandle);
        if(HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("HI_MPI_VI_GetChnFrame failed, s32Ret:0x%x\n",s32Ret);
            goto EXIT2;
        }

        s32Ret = HI_MPI_VGS_AddScaleTask(hHandle, &stTask, VGS_SCLCOEF_NORMAL);
        if(HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("HI_MPI_VGS_AddScaleTask failed, s32Ret:0x%x\n",s32Ret);
            goto EXIT2;
        }

        s32Ret = HI_MPI_VGS_EndJob(hHandle);
        if(HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("HI_MPI_VGS_EndJob failed, s32Ret:0x%x\n",s32Ret);
            goto EXIT2;
        }
        /************************************************
        Send frame to VO
        *************************************************/
        s32Ret = HI_MPI_VO_SendFrame(VoDev, VoChn, &stTask.stImgOut, s32MilliSec);

        if(HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("HI_MPI_VO_SendFrame failed, s32Ret:0x%x\n",s32Ret);
            goto EXIT2;
        }

        usleep(30000);

        /************************************************
        Munmap
        *************************************************/
        s32Ret = HI_MPI_SYS_Munmap((HI_VOID*)stTask.stImgOut.stVFrame.u64HeaderVirAddr[0],stCalConfig.u32VBSize);

        if(HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("HI_MPI_SYS_Munmap failed, s32Ret:0x%x\n",s32Ret);
            goto EXIT1;
        }

        /************************************************
        Release frame
        *************************************************/
        s32Ret = HI_MPI_VI_ReleaseChnFrame(ViPipe,ViChn,&stTask.stImgIn);

        if(HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("HI_MPI_VI_ReleaseChnFrame failed, s32Ret:0x%x\n",s32Ret);
            goto EXIT1;
        }

        /************************************************
        Release VB
        *************************************************/
        s32Ret = HI_MPI_VB_ReleaseBlock(VbHandle);

        if(HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("HI_MPI_VB_ReleaseBlock failed, s32Ret:0x%x\n",s32Ret);
            goto EXIT;
        }
    }

    /************************************************
    Set depth
    *************************************************/
    stChnAttr.u32Depth = u32OldDepth;
    s32Ret = HI_MPI_VI_SetChnAttr(ViPipe,ViChn,&stChnAttr);
    if(HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VI_SetChnAttr failed, s32Ret:0x%x\n",s32Ret);
        return HI_NULL;
    }

EXIT2:
    HI_MPI_SYS_Munmap((HI_VOID*)stTask.stImgOut.stVFrame.u64HeaderVirAddr[0],stCalConfig.u32VBSize);
EXIT1:
    HI_MPI_VB_ReleaseBlock(VbHandle);
EXIT:
    return HI_NULL;
}


HI_S32 SAMPLE_VGS_SDR10_COMPRESS(VO_INTF_TYPE_E enVoIntfType)
{
    SAMPLE_VGS_MOD_INFO_S stVgsModInfo;

    HI_S32                s32WorkSnsId         = 0;
    combo_dev_t           ComboDev;
    SAMPLE_VI_CONFIG_S       stViConfig;

    SAMPLE_VO_CONFIG_S       stVoConfig;

    PIC_SIZE_E               enPicSize            = PIC_3840x2160;
    SIZE_S                stSize;
    HI_U32                u32BlkSize;
    VB_CONFIG_S              stVbConf;

    HI_S32                  s32Ret;

    VI_DEV                   ViDev = 0;
    VI_PIPE                  ViPipe;
    VI_CHN                   ViChn;

    stVgsModInfo.ViPipe = 0;
    stVgsModInfo.ViChn  = 0;
    stVgsModInfo.VoDev  = SAMPLE_VO_DEV_DHD0;
    stVgsModInfo.VoChn  = 0;

    ViPipe = stVgsModInfo.ViPipe;
    ViChn  = stVgsModInfo.ViChn;

    /************************************************
    step1:  Get all sensors information
    *************************************************/
    SAMPLE_COMM_VI_GetSensorInfo(&stViConfig);
    ComboDev = SAMPLE_COMM_VI_GetComboDevBySensor(stViConfig.astViInfo[s32WorkSnsId].stSnsInfo.enSnsType, s32WorkSnsId);

    stViConfig.s32WorkingViNum    = 1;
    stViConfig.as32WorkingViId[0] = 0;

    stViConfig.astViInfo[s32WorkSnsId].stSnsInfo.MipiDev         = ComboDev;
    stViConfig.astViInfo[s32WorkSnsId].stSnsInfo.s32BusId        = 0;

    stViConfig.astViInfo[s32WorkSnsId].stDevInfo.ViDev           = ViDev;
    stViConfig.astViInfo[s32WorkSnsId].stDevInfo.enWDRMode       = WDR_MODE_NONE;

    stViConfig.astViInfo[s32WorkSnsId].stPipeInfo.enMastPipeMode = VI_OFFLINE_VPSS_OFFLINE;
    stViConfig.astViInfo[s32WorkSnsId].stPipeInfo.aPipe[0]       = ViPipe;
    stViConfig.astViInfo[s32WorkSnsId].stPipeInfo.aPipe[1]       = -1;
    stViConfig.astViInfo[s32WorkSnsId].stPipeInfo.aPipe[2]       = -1;
    stViConfig.astViInfo[s32WorkSnsId].stPipeInfo.aPipe[3]       = -1;

    stViConfig.astViInfo[s32WorkSnsId].stChnInfo.ViChn           = ViChn;
    stViConfig.astViInfo[s32WorkSnsId].stChnInfo.enPixFormat     = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    stViConfig.astViInfo[s32WorkSnsId].stChnInfo.enDynamicRange  = DYNAMIC_RANGE_SDR10;
    stViConfig.astViInfo[s32WorkSnsId].stChnInfo.enVideoFormat   = VIDEO_FORMAT_LINEAR;
    stViConfig.astViInfo[s32WorkSnsId].stChnInfo.enCompressMode  = COMPRESS_MODE_SEG;

    /************************************************
    step2:  Get input size
    *************************************************/
    s32Ret = SAMPLE_COMM_VI_GetSizeBySensor(stViConfig.astViInfo[s32WorkSnsId].stSnsInfo.enSnsType, &enPicSize);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_GetSizeBySensor failed, s32Ret:0x%x\n",s32Ret);
        goto EXIT;
    }

    s32Ret = SAMPLE_COMM_SYS_GetPicSize(enPicSize, &stSize);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed, s32Ret:0x%x\n",s32Ret);
        goto EXIT;
    }

    /************************************************
    step3:  Init SYS and common VB
    *************************************************/
    memset(&stVbConf, 0, sizeof(VB_CONFIG_S));
    stVbConf.u32MaxPoolCnt              = 2;

    u32BlkSize = COMMON_GetPicBufferSize(stSize.u32Width, stSize.u32Height, SAMPLE_PIXEL_FORMAT, DATA_BITWIDTH_10, COMPRESS_MODE_LINE, DEFAULT_ALIGN);
    stVbConf.astCommPool[0].u64BlkSize  = u32BlkSize;
    stVbConf.astCommPool[0].u32BlkCnt   = 10;

    u32BlkSize = VI_GetRawBufferSize(stSize.u32Width, stSize.u32Height, PIXEL_FORMAT_RGB_BAYER_16BPP, COMPRESS_MODE_NONE, DEFAULT_ALIGN);
    stVbConf.astCommPool[1].u64BlkSize  = u32BlkSize;
    stVbConf.astCommPool[1].u32BlkCnt   = 10;

    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_SYS_Init failed, s32Ret:0x%x\n",s32Ret);
        goto EXIT;
    }

    s32Ret = SAMPLE_COMM_VI_SetParam(&stViConfig);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_SetParam failed, s32Ret:0x%x\n",s32Ret);
        goto EXIT;
    }

     /************************************************
    step 4: start VI
    *************************************************/
//	    s32Ret = SAMPLE_COMM_VI_StartVi(&stViConfig);
//	
//	    if (HI_SUCCESS != s32Ret)
//	    {
//	        SAMPLE_PRT("SAMPLE_COMM_VI_StartVi failed, s32Ret:0x%x\n", s32Ret);
//	        goto EXIT;
//	    }

    /************************************************
    step4:  Start VO
    *************************************************/
    SAMPLE_COMM_VO_GetDefConfig(&stVoConfig);

    stVoConfig.enDstDynamicRange = DYNAMIC_RANGE_SDR10;
    stVoConfig.enIntfSync     = VO_OUTPUT_1080P60;

    s32Ret = SAMPLE_COMM_VO_StartVO(&stVoConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VO_StartVO failed, s32Ret:0x%x\n", s32Ret);
        goto EXIT1;
    }

    /************************************************
    step6:  Start VGS process
    *************************************************/
    bVgsThreadExit = HI_FALSE;

    pthread_create(&VgsThreadId, HI_NULL, SAMPLE_VGS_Get_Frame_Thread, &stVgsModInfo);

    PAUSE();

    bVgsThreadExit = HI_TRUE;

    pthread_join(VgsThreadId, HI_NULL);

    /************************************************
    step7:  Exit
    *************************************************/

    SAMPLE_COMM_VO_StopVO(&stVoConfig);
EXIT1:
    SAMPLE_COMM_VI_StopVi(&stViConfig);
EXIT:
    SAMPLE_COMM_SYS_Exit();

    return HI_SUCCESS;

}


/******************************************************************************
* function    : main()
* Description : main
******************************************************************************/
#ifdef __HuaweiLite__
    int app_main(int argc, char* argv[])
#else
    int main(int argc, char* argv[])
#endif
{
    HI_S32 s32Ret = HI_FAILURE;
    HI_S32 s32Index;
    VO_INTF_TYPE_E enVoIntfType = VO_INTF_HDMI;

    if (argc < 2)
    {
        SAMPLE_VGS_Usage(argv[0]);
        return HI_FAILURE;
    }

#ifndef __HuaweiLite__
    signal(SIGINT, SAMPLE_VGS_HandleSig);
    signal(SIGTERM, SAMPLE_VGS_HandleSig);
#endif

    if ((argc > 2) && (1 == atoi(argv[2])))
    {
        enVoIntfType = VO_INTF_BT1120;
    }

    s32Index = atoi(argv[1]);
    switch (s32Index)
    {
        case 0:
            s32Ret = SAMPLE_VGS_SDR10_COMPRESS(enVoIntfType);
            break;

        default:
            SAMPLE_PRT("the index %d is invaild!\n",s32Index);
            SAMPLE_VGS_Usage(argv[0]);
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
    return(s32Ret);
}


#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */

