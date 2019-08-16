/******************************************************************************

  Copyright (C), 2019, Xinhai Medic. Co., Ltd.

 ******************************************************************************
  File Name     : ov426_cmos.c

  Version       : Initial Draft
  Author        : Andreas Zhang
  Created       : 2019/07/08
  Description   :
  History       :
  1.Date        : 2019/07/08
    Author      : Andreas Zhang
    Modification: Created file

******************************************************************************/
#if !defined(__OV426_CMOS_H_)
#define __OV426_CMOS_H_

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "hi_comm_sns.h"
#include "hi_comm_video.h"
#include "hi_sns_ctrl.h"
#include "mpi_isp.h"
#include "mpi_ae.h"
#include "mpi_awb.h"

#include "ov426_cmos_ex.h"
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */


#define OV426_ID 426

#define HIGHER_4BITS(x) (((x) & 0xf0000) >> 16)
#define HIGH_8BITS(x) (((x) & 0xff00) >> 8)
#define LOW_8BITS(x)  ((x) & 0x00ff)

#ifndef MAX
#define MAX(a, b) (((a) < (b)) ?  (b) : (a))
#endif

#ifndef MIN
#define MIN(a, b) (((a) > (b)) ?  (b) : (a))
#endif

ISP_SNS_STATE_S *g_pastOv426[ISP_MAX_PIPE_NUM] = {HI_NULL};

#define OV426_SENSOR_GET_CTX(dev, pstCtx)   (pstCtx = g_pastOv426[dev])
#define OV426_SENSOR_SET_CTX(dev, pstCtx)   (g_pastOv426[dev] = pstCtx)
#define OV426_SENSOR_RESET_CTX(dev)         (g_pastOv426[dev] = HI_NULL)

static HI_U32 g_au32InitExposure[ISP_MAX_PIPE_NUM]  = {0};
static HI_U32 g_au32LinesPer500ms[ISP_MAX_PIPE_NUM]   = {0};
static HI_U16 g_au16InitWBGain[ISP_MAX_PIPE_NUM][3] = {{0}};
static HI_U16 g_au16SampleRgain[ISP_MAX_PIPE_NUM] = {0};
static HI_U16 g_au16SampleBgain[ISP_MAX_PIPE_NUM] = {0};

ISP_SNS_COMMBUS_U g_aunOv426BusInfo[ISP_MAX_PIPE_NUM] =
{
    [0] = { .s8I2cDev = 0},
    [1 ... ISP_MAX_PIPE_NUM - 1] = { .s8I2cDev = -1}
};

static ISP_FSWDR_MODE_E genFSWDRMode[ISP_MAX_PIPE_NUM] =
{
    [0 ... ISP_MAX_PIPE_NUM - 1] = ISP_FSWDR_NORMAL_MODE
};

static HI_U32 gu32MaxTimeGetCnt[ISP_MAX_PIPE_NUM] = {0};

typedef struct hiOV426_STATE_S
{
    HI_U32      u32BRL;
    HI_U32      u32RHS1_MAX;
    HI_U32      u32deltaRHS1;
} OV426_STATE_S;

OV426_STATE_S g_astov426State[ISP_MAX_PIPE_NUM] = {{0}};


/****************************************************************************
 * extern                                                                   *
 ****************************************************************************/
extern unsigned char ov426_i2c_addr;
extern unsigned int  ov426_addr_byte;
extern unsigned int  ov426_data_byte;

extern void ov426_init(VI_PIPE ViPipe);
extern void ov426_exit(VI_PIPE ViPipe);
extern void ov426_standby(VI_PIPE ViPipe);
extern void ov426_restart(VI_PIPE ViPipe);
extern int  ov426_write_register(VI_PIPE ViPipe, int addr, int data);
extern int  ov426_read_register(VI_PIPE ViPipe, int addr);

/****************************************************************************
 * local variables                                                            *
 ****************************************************************************/
#define OV426_FULL_LINES_MAX  (0xFFFF)


// modified by andreas
// vts related regs maybe these two!?
#define VMAX_ADDR_H             (0x350C)    // VTS[15:8] Total vertical timing size
#define VMAX_ADDR_L             (0x350D)    // VTS[7:0] Total vertical timing size

// added by andreas
// ref to ov9732, these regs are simular to ov426
#define AEC_PK_EXPO_H           (0x3501)  /*  SHR0[19:16]  */
#define AEC_PK_EXPO_M           (0x3502)  /*  SHR0[15:8] */
#define AEC_PK_EXPO_L           (0x3503)  /*  SHR0[7:0] */
#define GAIN_ADDR_H             (0x350A)  // maybe no use based on 3.2.4@P34
#define GAIN_ADDR_L             (0x350B)

#define OV426_INCREASE_LINES (0) /* make real fps less than stand fps because NVR require*/

#define OV426_VMAX_12bit_LINEAR  (0x190+OV426_INCREASE_LINES)


//sensor fps mode
#define Ov426_400x400_12bit_30fps      (1)

static HI_S32 cmos_get_ae_default(VI_PIPE ViPipe, AE_SENSOR_DEFAULT_S *pstAeSnsDft)
{
    ISP_SNS_STATE_S *pstSnsState = HI_NULL;

    CMOS_CHECK_POINTER(pstAeSnsDft);
    OV426_SENSOR_GET_CTX(ViPipe, pstSnsState);
    CMOS_CHECK_POINTER(pstSnsState);

    memset(&pstAeSnsDft->stAERouteAttr, 0, sizeof(ISP_AE_ROUTE_S));

    pstAeSnsDft->u32FullLinesStd = pstSnsState->u32FLStd;
    pstAeSnsDft->u32FlickerFreq = 0;
    pstAeSnsDft->u32FullLinesMax = OV426_VMAX_12bit_LINEAR;

    pstAeSnsDft->stIntTimeAccu.enAccuType = AE_ACCURACY_LINEAR;
    pstAeSnsDft->stIntTimeAccu.f32Accuracy = 1;
    pstAeSnsDft->stIntTimeAccu.f32Offset = 0;
//	    pstAeSnsDft->u32MaxIntTime = (pstSnsState->u32FLStd >= 4) ? (pstSnsState->u32FLStd - 4) : (0);//pstSnsState->u32FLStd - 4;
//	    pstAeSnsDft->u32MinIntTime = 4;
//	    pstAeSnsDft->u32MaxIntTimeTarget = 65535;
//	    pstAeSnsDft->u32MinIntTimeTarget = 2;

    pstAeSnsDft->stAgainAccu.enAccuType = AE_ACCURACY_LINEAR; // by andreas
    pstAeSnsDft->stAgainAccu.f32Accuracy = 0.0625; // 1/16, by andreas
//	    pstAeSnsDft->u32MaxAgain = 16;// by andreas
//	    pstAeSnsDft->u32MinAgain = 0;// by andreas
//	    pstAeSnsDft->u32MaxAgainTarget = pstAeSnsDft->u32MaxAgain;// by andreas
//	    pstAeSnsDft->u32MinAgainTarget = pstAeSnsDft->u32MinAgain;// by andreas

    pstAeSnsDft->stDgainAccu.enAccuType = AE_ACCURACY_LINEAR;// by andreas
    pstAeSnsDft->stDgainAccu.f32Accuracy = 0;// by andreas

    pstAeSnsDft->u32ISPDgainShift = 8;
    pstAeSnsDft->u32MinISPDgainTarget = 1 << pstAeSnsDft->u32ISPDgainShift;
    pstAeSnsDft->u32MaxISPDgainTarget = 4 << pstAeSnsDft->u32ISPDgainShift;

    memcpy(&pstAeSnsDft->stPirisAttr, &gstPirisAttr, sizeof(ISP_PIRIS_ATTR_S));
    pstAeSnsDft->enMaxIrisFNO = ISP_IRIS_F_NO_1_4;
    pstAeSnsDft->enMinIrisFNO = ISP_IRIS_F_NO_5_6;

    pstAeSnsDft->bAERouteExValid = HI_FALSE;
    pstAeSnsDft->stAERouteAttr.u32TotalNum = 0;
    pstAeSnsDft->stAERouteAttrEx.u32TotalNum = 0;

    if (g_au32InitExposure[ViPipe] == 0)
    {
        pstAeSnsDft->u32InitExposure = 1000000;
    }
    //else
    //{
    //pstAeSnsDft->u32InitExposure = pstAeSnsDft->u32InitExposure;
    //}

    if (g_au32LinesPer500ms[ViPipe] == 0)
    {
        pstAeSnsDft->u32LinesPer500ms = pstSnsState->u32FLStd * 30 / 2;
    }
    else
    {
        pstAeSnsDft->u32LinesPer500ms = g_au32LinesPer500ms[ViPipe];
    }

    switch (pstSnsState->enWDRMode)
    {
        default:
        case WDR_MODE_NONE:
            pstAeSnsDft->au8HistThresh[0] = 0xd;
            pstAeSnsDft->au8HistThresh[1] = 0x28;
            pstAeSnsDft->au8HistThresh[2] = 0x60;
            pstAeSnsDft->au8HistThresh[3] = 0x80;

            pstAeSnsDft->u8AeCompensation = 0x38;
            pstAeSnsDft->enAeExpMode = AE_EXP_HIGHLIGHT_PRIOR;

            pstAeSnsDft->u32MaxIntTime = pstSnsState->u32FLStd - 5;
            pstAeSnsDft->u32MinIntTime = 2;
            pstAeSnsDft->u32MaxIntTimeTarget = 65535;
            pstAeSnsDft->u32MinIntTimeTarget = pstAeSnsDft->u32MinIntTime;
            pstAeSnsDft->stIntTimeAccu.f32Offset = 0.156;

            pstAeSnsDft->u32MaxAgain = 16;// by andreas
            pstAeSnsDft->u32MinAgain = 0;// by andreas
            pstAeSnsDft->u32MaxAgainTarget = pstAeSnsDft->u32MaxAgain;
            pstAeSnsDft->u32MinAgainTarget = pstAeSnsDft->u32MinAgain;

            pstAeSnsDft->u32MaxDgain = 0;  /* if Dgain enable,please set ispdgain bigger than 1*/
            pstAeSnsDft->u32MinDgain = 0;
            pstAeSnsDft->u32MaxDgainTarget = pstAeSnsDft->u32MaxDgain;
            pstAeSnsDft->u32MinDgainTarget = pstAeSnsDft->u32MinDgain;
            pstAeSnsDft->u32HmaxTimes = (1000000 * 1000) / (OV426_VMAX_12bit_LINEAR * 30);
            break;
//	        case WDR_MODE_2To1_LINE:
//	            pstAeSnsDft->au8HistThresh[0] = 0xC;
//	            pstAeSnsDft->au8HistThresh[1] = 0x18;
//	            pstAeSnsDft->au8HistThresh[2] = 0x60;
//	            pstAeSnsDft->au8HistThresh[3] = 0x80;
//	
//	
//	            pstAeSnsDft->u32MaxIntTime = pstSnsState->u32FLStd - 2;
//	
//	            pstAeSnsDft->stIntTimeAccu.enAccuType = AE_ACCURACY_LINEAR;
//	            pstAeSnsDft->stIntTimeAccu.f32Accuracy = 2;
//	            pstAeSnsDft->u32MinIntTime = 2;
//	            pstAeSnsDft->stIntTimeAccu.f32Offset = 0.311;
//	
//	            pstAeSnsDft->u32MaxIntTimeTarget = 65535;
//	            pstAeSnsDft->u32MinIntTimeTarget = pstAeSnsDft->u32MinIntTime;
//	
//	            pstAeSnsDft->u32MaxAgain = 31356;
//	            pstAeSnsDft->u32MinAgain = 1024;
//	            pstAeSnsDft->u32MaxAgainTarget = pstAeSnsDft->u32MaxAgain;
//	            pstAeSnsDft->u32MinAgainTarget = pstAeSnsDft->u32MinAgain;
//	
//	            pstAeSnsDft->u32MaxDgain = 128913;
//	            pstAeSnsDft->u32MinDgain = 1024;
//	            pstAeSnsDft->u32MaxDgainTarget = pstAeSnsDft->u32MaxDgain;
//	            pstAeSnsDft->u32MinDgainTarget = pstAeSnsDft->u32MinDgain;
//	
//	            pstAeSnsDft->u32InitExposure = g_au32InitExposure[ViPipe] ? g_au32InitExposure[ViPipe] : 16462;
//	            pstAeSnsDft->u32HmaxTimes = (1000000 * 1000) / (OV426_VMAX_12bit_LINEAR * 30);
//	            if (ISP_FSWDR_LONG_FRAME_MODE == genFSWDRMode[ViPipe])
//	            {
//	                pstAeSnsDft->u8AeCompensation = 56;
//	                pstAeSnsDft->enAeExpMode = AE_EXP_HIGHLIGHT_PRIOR;
//	            }
//	            else
//	            {
//	                pstAeSnsDft->u32MaxDgainTarget = 1024;
//	                pstAeSnsDft->u32MaxISPDgainTarget = 4096;
//	                pstAeSnsDft->u8AeCompensation = 24;
//	                pstAeSnsDft->enAeExpMode = AE_EXP_LOWLIGHT_PRIOR;
//	                pstAeSnsDft->u16ManRatioEnable = HI_FALSE;
//	                pstAeSnsDft->au32Ratio[0] = 0x400;
//	                pstAeSnsDft->au32Ratio[1] = 0x40;
//	                pstAeSnsDft->au32Ratio[2] = 0x40;
//	            }
//	            break;
    }

    return HI_SUCCESS;
}


/* the function of sensor set fps */
static HI_VOID cmos_fps_set(VI_PIPE ViPipe, HI_FLOAT f32Fps, AE_SENSOR_DEFAULT_S *pstAeSnsDft)
{
    HI_U32 u32MaxFps;
    HI_U32 u32Lines;
    ISP_SNS_STATE_S *pstSnsState = HI_NULL;

    CMOS_CHECK_POINTER_VOID(pstAeSnsDft);
    OV426_SENSOR_GET_CTX(ViPipe, pstSnsState);
    CMOS_CHECK_POINTER_VOID(pstSnsState);

    switch (pstSnsState->u8ImgMode)
    {
        case Ov426_400x400_12bit_30fps:
            u32MaxFps = 30;
            if ((f32Fps <= 30) && (f32Fps >= 0.07))
            {
                u32Lines = OV426_VMAX_12bit_LINEAR * u32MaxFps / DIV_0_TO_1_FLOAT(f32Fps);
            }
            else
            {
                ISP_TRACE(HI_DBG_ERR, "Not support Fps: %f\n", f32Fps);
                return;
            }
            u32Lines = (u32Lines > OV426_FULL_LINES_MAX) ? OV426_FULL_LINES_MAX : u32Lines;
            break;

        default:
            ISP_TRACE(HI_DBG_ERR, "Not support ImgMode: %d\n", pstSnsState->u8ImgMode);
            return;
    }

    if (WDR_MODE_NONE == pstSnsState->enWDRMode)
    {
        pstSnsState->astRegsInfo[0].astI2cData[0].u32Data = LOW_8BITS(u32Lines);
        pstSnsState->astRegsInfo[0].astI2cData[1].u32Data = HIGH_8BITS(u32Lines);
//	        pstSnsState->astRegsInfo[0].astI2cData[7].u32Data = ((u32Lines & 0xF0000) >> 16);
    }
//	    else
//	    {
//	        pstSnsState->astRegsInfo[0].astI2cData[8].u32Data = (u32Lines & 0xFF);
//	        pstSnsState->astRegsInfo[0].astI2cData[9].u32Data = ((u32Lines & 0xFF00) >> 8);
//	        pstSnsState->astRegsInfo[0].astI2cData[10].u32Data = ((u32Lines & 0xF0000) >> 16);
//	    }

//	    if (WDR_MODE_2To1_LINE == pstSnsState->enWDRMode)
//	    {
//	        pstSnsState->u32FLStd = u32Lines * 2;
//	
//	        /*   RHS1 limitation:    4n + 1 and RHS1 <= 2 * BRL
//	             RHS1(N+1)>= RHS1(N)+ 2*BRL - 2*VMAX + 2 ===> RHS1(N)-RHS1(N+1)<= 2*VMAX - 2*BRL -2*/
//	
//	        g_astov426State[ViPipe].u32RHS1_MAX = g_astov426State[ViPipe].u32BRL * 2 ;
//	        g_astov426State[ViPipe].u32deltaRHS1 = u32Lines * 2 - g_astov426State[ViPipe].u32BRL * 2 - 2;
//	    }
//	    else
//	    {
        pstSnsState->u32FLStd = u32Lines;
//	    }

    pstAeSnsDft->f32Fps = f32Fps;
    pstAeSnsDft->u32LinesPer500ms = pstSnsState->u32FLStd * f32Fps / 2;
    pstAeSnsDft->u32FullLinesStd = pstSnsState->u32FLStd;
    pstAeSnsDft->u32MaxIntTime = pstSnsState->u32FLStd - 5;
    pstSnsState->au32FL[0] = pstSnsState->u32FLStd;
    pstAeSnsDft->u32FullLines = pstSnsState->au32FL[0];

    return;
}

static HI_VOID cmos_slow_framerate_set(VI_PIPE ViPipe, HI_U32 u32FullLines, AE_SENSOR_DEFAULT_S *pstAeSnsDft)
{
    ISP_SNS_STATE_S *pstSnsState = HI_NULL;

    CMOS_CHECK_POINTER_VOID(pstAeSnsDft);
    OV426_SENSOR_GET_CTX(ViPipe, pstSnsState);
    CMOS_CHECK_POINTER_VOID(pstSnsState);

    pstSnsState->au32FL[0] = (u32FullLines > OV426_FULL_LINES_MAX) ? OV426_FULL_LINES_MAX : u32FullLines;
    pstAeSnsDft->u32FullLines = pstSnsState->au32FL[0];
    pstAeSnsDft->u32MaxIntTime = pstSnsState->au32FL[0] - 8;

    if (WDR_MODE_NONE == pstSnsState->enWDRMode)
    {
        pstSnsState->astRegsInfo[0].astI2cData[0].u32Data = LOW_8BITS(pstSnsState->au32FL[0]);
        pstSnsState->astRegsInfo[0].astI2cData[1].u32Data = HIGH_8BITS(pstSnsState->au32FL[0]);
    }
    else
    {
        pstSnsState->astRegsInfo[0].astI2cData[0].u32Data = LOW_8BITS(pstSnsState->au32FL[0]);
        pstSnsState->astRegsInfo[0].astI2cData[1].u32Data = HIGH_8BITS(pstSnsState->au32FL[0]);
    }

    return;
}

/* while isp notify ae to update sensor regs, ae call these funcs. */
static HI_VOID cmos_inttime_update(VI_PIPE ViPipe, HI_U32 u32IntTime)
{
    ISP_SNS_STATE_S *pstSnsState = HI_NULL;
    static HI_BOOL bFirst[ISP_MAX_PIPE_NUM] = {[0 ... (ISP_MAX_PIPE_NUM - 1)] = 1};
    HI_U32 u32Value = 0;
    //HI_S32 deltaST = 0;
    //HI_S32 deltaLT = 0;

    static HI_U32 u32ShortIntTime[ISP_MAX_PIPE_NUM] = {0};
    static HI_U32 u32LongIntTime[ISP_MAX_PIPE_NUM] = {0};

    static HI_U32 u32RHS1[ISP_MAX_PIPE_NUM]  = {0};
    static HI_U32 u32LastRHS1[ISP_MAX_PIPE_NUM]  = {0};
    static HI_U32 u32deltaRHS1[ISP_MAX_PIPE_NUM]  = {0};
    static HI_U32 u32SHR0[ISP_MAX_PIPE_NUM]  = {0};
    static HI_U32 u32SHR1[ISP_MAX_PIPE_NUM]  = {0};
    static HI_U32 u32YOUTSIZE[ISP_MAX_PIPE_NUM] = {0};

    OV426_SENSOR_GET_CTX(ViPipe, pstSnsState);
    CMOS_CHECK_POINTER_VOID(pstSnsState);

//	    if (WDR_MODE_2To1_LINE == pstSnsState->enWDRMode)
//	    {
//	        /* short exposure */
//	        if (bFirst[ViPipe])
//	        {
//	            pstSnsState->au32WDRIntTime[0] = u32IntTime;
//	            u32ShortIntTime[ViPipe] = u32IntTime;
//	            bFirst[ViPipe] = HI_FALSE;
//	        }
//	
//	        /* long exposure */
//	        else
//	        {
//	            pstSnsState->au32WDRIntTime[1] = u32IntTime;
//	            u32LongIntTime[ViPipe] = u32IntTime;
//	
//	            //printf("L = %d; S = %d\n",u32LongIntTime[ViPipe],u32ShortIntTime[ViPipe]);
//	
//	            u32SHR0[ViPipe] = pstSnsState->au32FL[1] - u32LongIntTime[ViPipe];
//	            u32SHR1[ViPipe] = 9 ;
//	
//	            if (u32ShortIntTime[ViPipe] % 4 == 0)
//	            {
//	                u32SHR1[ViPipe] = 9 ;
//	                u32RHS1[ViPipe] = u32ShortIntTime[ViPipe] + u32SHR1[ViPipe];
//	            }
//	            else if (u32ShortIntTime[ViPipe] % 4 == 2)
//	            {
//	                u32SHR1[ViPipe] = 11 ;
//	                u32RHS1[ViPipe] = u32ShortIntTime[ViPipe] + u32SHR1[ViPipe];
//	            }
//	
//	            if (u32RHS1[ViPipe] >= u32LastRHS1[ViPipe])
//	            {
//	            }
//	            else if (u32RHS1[ViPipe] < u32LastRHS1[ViPipe])
//	            {
//	                u32deltaRHS1[ViPipe] = u32LastRHS1[ViPipe] - u32RHS1[ViPipe];
//	                if (u32deltaRHS1[ViPipe] <= g_astov426State[ViPipe].u32deltaRHS1)
//	                {
//	                }
//	                else
//	                {
//	                    if (u32ShortIntTime[ViPipe] % 4 == 0)
//	                    {
//	                        u32SHR1[ViPipe] = 9 + (u32deltaRHS1[ViPipe] - g_astov426State[ViPipe].u32deltaRHS1 + 2);
//	                        u32RHS1[ViPipe] = u32LastRHS1[ViPipe] - (g_astov426State[ViPipe].u32deltaRHS1 - 2);
//	                    }
//	                    else if (u32ShortIntTime[ViPipe] % 4 == 2)
//	                    {
//	                        u32SHR1[ViPipe] = 11 + (u32deltaRHS1[ViPipe] - g_astov426State[ViPipe].u32deltaRHS1 + 2);
//	                        u32RHS1[ViPipe] = u32LastRHS1[ViPipe] - (g_astov426State[ViPipe].u32deltaRHS1 - 2);
//	                    }
//	                }
//	
//	                if (u32SHR0[ViPipe] < u32LastRHS1[ViPipe] + 9)
//	                {
//	                    u32SHR0[ViPipe] = u32LastRHS1[ViPipe] + 9;
//	                }
//	                else
//	                {
//	                }
//	            }
//	            else
//	            {
//	            }
//	
//	            //deltaST = (HI_S32)(u32RHS1[ViPipe] - u32SHR1[ViPipe] - u32ShortIntTime[ViPipe]);
//	            //deltaLT = (HI_S32)(pstSnsState->au32FL[1] - u32SHR0[ViPipe] - u32LongIntTime[ViPipe]);
//	
//	            //printf("SHR1= %d,RHS1=%d,deltaST=%d,SHR0=%d,deltaLT=%d\n",u32SHR1[ViPipe],u32RHS1[ViPipe],
//	            //  deltaST,u32SHR0[ViPipe],deltaLT);
//	
//	            //Virtural Channel Mode
//	            u32YOUTSIZE[ViPipe] = 2180 + (u32RHS1[ViPipe] - 3) / 2;
//	            u32YOUTSIZE[ViPipe] = (u32YOUTSIZE[ViPipe] >= 0x1FFF) ? 0x1FFF : u32YOUTSIZE[ViPipe];
//	
//	            pstSnsState->astRegsInfo[0].astI2cData[0].u32Data = LOW_8BITS(u32SHR0[ViPipe]);
//	            pstSnsState->astRegsInfo[0].astI2cData[1].u32Data = HIGH_8BITS(u32SHR0[ViPipe]);
//	            pstSnsState->astRegsInfo[0].astI2cData[2].u32Data = HIGHER_4BITS(u32SHR0[ViPipe]);
//	
//	            pstSnsState->astRegsInfo[0].astI2cData[5].u32Data = LOW_8BITS(u32SHR1[ViPipe]);
//	            pstSnsState->astRegsInfo[0].astI2cData[6].u32Data = HIGH_8BITS(u32SHR1[ViPipe]);
//	            pstSnsState->astRegsInfo[0].astI2cData[7].u32Data = HIGHER_4BITS(u32SHR1[ViPipe]);
//	
//	            pstSnsState->astRegsInfo[0].astI2cData[11].u32Data = LOW_8BITS(u32RHS1[ViPipe]);
//	            pstSnsState->astRegsInfo[0].astI2cData[12].u32Data = HIGH_8BITS(u32RHS1[ViPipe]);
//	            pstSnsState->astRegsInfo[0].astI2cData[13].u32Data = HIGHER_4BITS(u32RHS1[ViPipe]);
//	
//	            pstSnsState->astRegsInfo[0].astI2cData[14].u32Data = (u32YOUTSIZE[ViPipe] & 0xFF);
//	            pstSnsState->astRegsInfo[0].astI2cData[15].u32Data = ((u32YOUTSIZE[ViPipe] & 0x1F00) >> 8);
//	
//	            bFirst[ViPipe] = HI_TRUE;
//	            u32LastRHS1[ViPipe] = u32RHS1[ViPipe];
//	        }
//	
//	    }
//	
//	    else
//	    {
        u32Value = pstSnsState->au32FL[0] - u32IntTime;
        u32Value = MIN(u32Value, 0xfffff);
        u32Value = MIN(MAX(u32Value, 5), pstSnsState->au32FL[0] - 2);

        pstSnsState->astRegsInfo[0].astI2cData[2].u32Data = LOW_8BITS(u32Value); // by andreas
        pstSnsState->astRegsInfo[0].astI2cData[3].u32Data = HIGH_8BITS(u32Value); // by andreas
        pstSnsState->astRegsInfo[0].astI2cData[4].u32Data = HIGHER_4BITS(u32Value); // by andreas

        bFirst[ViPipe] = HI_TRUE;
//	    }

    return;

}

static HI_U32 gain_table[262] =
{
    1024,  1059,  1097,  1135,  1175,  1217,  1259,  1304,  1349,  1397,  1446,  1497,  1549,  1604,  1660,  1719,  1779,  1842,  1906,
    1973,  2043,  2048,  2119,  2194,  2271,  2351,  2434,  2519,  2608,  2699,  2794,  2892,  2994,  3099,  3208,  3321,  3438,  3559,
    3684,  3813,  3947,  4086,  4229,  4378,  4532,  4691,  4856,  5027,  5203,  5386,  5576,  5772,  5974,  6184,  6402,  6627,  6860,
    7101,  7350,  7609,  7876,  8153,  8439,  8736,  9043,  9361,  9690, 10030, 10383, 10748, 11125, 11516, 11921, 12340, 12774, 13222,
    13687, 14168, 14666, 15182, 15715, 16267, 16839, 17431, 18043, 18677, 19334, 20013, 20717, 21445, 22198, 22978, 23786, 24622, 25487,
    26383, 27310, 28270, 29263, 30292, 31356, 32458, 33599, 34780, 36002, 37267, 38577, 39932, 41336, 42788, 44292, 45849, 47460, 49128,
    50854, 52641, 54491, 56406, 58388, 60440, 62564, 64763, 67039, 69395, 71833, 74358, 76971, 79676, 82476, 85374, 88375, 91480, 94695,
    98023, 101468, 105034, 108725, 112545, 116501, 120595, 124833, 129220, 133761, 138461, 143327, 148364, 153578, 158975, 164562, 170345, 176331, 182528,
    188942, 195582, 202455, 209570, 216935, 224558, 232450, 240619, 249074, 257827, 266888, 276267, 285976, 296026, 306429, 317197, 328344, 339883, 351827,
    364191, 376990, 390238, 403952, 418147, 432842, 448053, 463799, 480098, 496969, 514434, 532512, 551226, 570597, 590649, 611406, 632892, 655133, 678156,
    701988, 726657, 752194, 778627, 805990, 834314, 863634, 893984, 925400, 957921, 991585, 1026431, 1062502, 1099841, 1138491, 1178500, 1219916, 1262786,
    1307163, 1353100, 1400651, 1449872, 1500824, 1553566, 1608162, 1664676, 1723177, 1783733, 1846417, 1911304, 1978472, 2048000, 2119971, 2194471, 2271590,
    2351418, 2434052, 2519590, 2608134, 2699789, 2794666, 2892876, 2994538, 3099773, 3208706, 3321467, 3438190, 3559016, 3684087, 3813554, 3947571, 4086297,
    4229898, 4378546, 4532417, 4691696, 4856573, 5027243, 5203912, 5386788, 5576092, 5772048, 5974890, 6184861, 6402210, 6627198, 6860092, 7101170, 7350721,
    7609041, 7876439, 8153234
};

static HI_VOID cmos_again_calc_table(VI_PIPE ViPipe, HI_U32 *pu32AgainLin, HI_U32 *pu32AgainDb)
{
    int i;

    CMOS_CHECK_POINTER_VOID(pu32AgainLin);
    CMOS_CHECK_POINTER_VOID(pu32AgainDb);

    if (*pu32AgainLin >= gain_table[100])
    {
        *pu32AgainLin = gain_table[100];
        *pu32AgainDb = 100;
        return ;
    }

    for (i = 1; i < 101; i++)
    {
        if (*pu32AgainLin < gain_table[i])
        {
            *pu32AgainLin = gain_table[i - 1];
            *pu32AgainDb = i - 1;
            break;
        }
    }
    return;
}

static HI_VOID cmos_dgain_calc_table(VI_PIPE ViPipe, HI_U32 *pu32DgainLin, HI_U32 *pu32DgainDb)
{
    int i;

    CMOS_CHECK_POINTER_VOID(pu32DgainLin);
    CMOS_CHECK_POINTER_VOID(pu32DgainDb);

    if (*pu32DgainLin >= gain_table[140])
    {
        *pu32DgainLin = gain_table[140];
        *pu32DgainDb = 140;
        return ;
    }

    for (i = 1; i < 141; i++)
    {
        if (*pu32DgainLin < gain_table[i])
        {
            *pu32DgainLin = gain_table[i - 1];
            *pu32DgainDb = i - 1;
            break;
        }
    }

    return;
}

static HI_VOID cmos_gains_update(VI_PIPE ViPipe, HI_U32 u32Again, HI_U32 u32Dgain)
{
    ISP_SNS_STATE_S *pstSnsState = HI_NULL;
    HI_U32 u32Tmp;

    OV426_SENSOR_GET_CTX(ViPipe, pstSnsState);
    CMOS_CHECK_POINTER_VOID(pstSnsState);

    u32Tmp = u32Again + u32Dgain;

    pstSnsState->astRegsInfo[0].astI2cData[3].u32Data = LOW_8BITS(u32Tmp);
    pstSnsState->astRegsInfo[0].astI2cData[4].u32Data = HIGH_8BITS(u32Tmp);
    return;
}

/*
           Linear:     SHR0 [5, VMAX-1] ;    Initime = VMAX - SHR0  Initime [1,VMAX -5]
           WDR 2t1 :   SHS1 2n+1 (n=0,1,2....)  and 9 <= SHS1 <= RHS1-2
                       RHS1 4n+1 (n=0,1,2....)  and (SHR1+2)<= RHS1<=(SHR0-9) and RHS1<(BRL*2)
                       SHR0 2n(n=0,1,2.....) and (RHS1+9) <= SHR0<=(2*VMAX-2)  VMAX :2n (n=0,1,2.....)
*/

static HI_VOID cmos_get_inttime_max(VI_PIPE ViPipe, HI_U16 u16ManRatioEnable, HI_U32 *au32Ratio, HI_U32 *au32IntTimeMax, HI_U32 *au32IntTimeMin, HI_U32 *pu32LFMaxIntTime)
{
    HI_U32 u32IntTimeMaxTmp0 = 0;
    HI_U32 u32IntTimeMaxTmp  = 0;
    HI_U32 u32RatioTmp = 0x40;
    HI_U32 u32ShortTimeMinLimit = 0;

    ISP_SNS_STATE_S *pstSnsState = HI_NULL;

    CMOS_CHECK_POINTER_VOID(au32Ratio);
    CMOS_CHECK_POINTER_VOID(au32IntTimeMax);
    CMOS_CHECK_POINTER_VOID(au32IntTimeMin);
    CMOS_CHECK_POINTER_VOID(pu32LFMaxIntTime);
    OV426_SENSOR_GET_CTX(ViPipe, pstSnsState);
    CMOS_CHECK_POINTER_VOID(pstSnsState);

    u32ShortTimeMinLimit = 2;

    if (WDR_MODE_2To1_LINE == pstSnsState->enWDRMode)
    {
        /*  limitation for line base WDR

             SHR1 limitation:
             9 or more
             RHS1 - 2 or less  and 2n+1(n=0,1,2....)

             SHR0 limitation:
             2n(n=0,1,2....);
             RHS1 + 9 or more
             FSC - 2 or less

             RHS1 Limitation
             4n+1 (n=0,1,2....)
             (SHR1+2)<=RHS1 <= (SHR0-9)

             RHS1 = st + 9 - (st % 4) +4;
             SHR0 = FSC - st * ratio + (lt %2) - 2;

             RHS1<= SHR0 -9=FSC -st*ratio +(lt%2) - 11
             st *(1+ratio) <= FSC -24 + (lt %2) +(st %4)
             st *(1+ratio) < = FSC -24

             short exposure time = RHS1 - SHR1 <= RHS1 - 9
             long exposure time = FSC -  SHR0  <= FSC - (RHS1 + 9)
             ExposureShort + ExposureLong <= FSC - 18
             short exposure time <= (FSC - 18) / (ratio + 1)
           */
        if (ISP_FSWDR_LONG_FRAME_MODE == genFSWDRMode[ViPipe])
        {
            u32IntTimeMaxTmp0 = pstSnsState->au32FL[1] - 280 - pstSnsState->au32WDRIntTime[0];
            u32IntTimeMaxTmp = pstSnsState->au32FL[0] - 280;
            u32IntTimeMaxTmp = (u32IntTimeMaxTmp0 < u32IntTimeMaxTmp) ? u32IntTimeMaxTmp0 : u32IntTimeMaxTmp;
            au32IntTimeMax[0] = u32IntTimeMaxTmp;
            au32IntTimeMin[0] = u32ShortTimeMinLimit;
            return;
        }
        else
        {
            u32IntTimeMaxTmp0 = ((pstSnsState->au32FL[1] - 280 - pstSnsState->au32WDRIntTime[0]) * 0x40)  / DIV_0_TO_1(au32Ratio[0]);
            u32IntTimeMaxTmp = ((pstSnsState->au32FL[0] - 280) * 0x40)  / DIV_0_TO_1(au32Ratio[0] + 0x40);
            u32IntTimeMaxTmp = (u32IntTimeMaxTmp0 < u32IntTimeMaxTmp) ? u32IntTimeMaxTmp0 : u32IntTimeMaxTmp;
            u32IntTimeMaxTmp = (u32IntTimeMaxTmp > (g_astov426State[ViPipe].u32RHS1_MAX - 2)) ? (g_astov426State[ViPipe].u32RHS1_MAX - 2) : u32IntTimeMaxTmp;
            u32IntTimeMaxTmp = (0 == u32IntTimeMaxTmp) ? 1 : u32IntTimeMaxTmp;
        }

    }

    else
    {
    }

    if (u32IntTimeMaxTmp >= u32ShortTimeMinLimit)
    {
        if (IS_LINE_WDR_MODE(pstSnsState->enWDRMode))
        {
            au32IntTimeMax[0] = u32IntTimeMaxTmp;
            au32IntTimeMax[1] = au32IntTimeMax[0] * au32Ratio[0] >> 6;
            au32IntTimeMax[2] = au32IntTimeMax[1] * au32Ratio[1] >> 6;
            au32IntTimeMax[3] = au32IntTimeMax[2] * au32Ratio[2] >> 6;
            au32IntTimeMin[0] = u32ShortTimeMinLimit;
            au32IntTimeMin[1] = au32IntTimeMin[0] * au32Ratio[0] >> 6;
            au32IntTimeMin[2] = au32IntTimeMin[1] * au32Ratio[1] >> 6;
            au32IntTimeMin[3] = au32IntTimeMin[2] * au32Ratio[2] >> 6;
        }
        else
        {
        }
    }
    else
    {
        if (1 == u16ManRatioEnable)
        {
            ISP_TRACE(HI_DBG_ERR, "Manaul ExpRatio is too large!\n");
            return;
        }
        else
        {
            u32IntTimeMaxTmp = u32ShortTimeMinLimit;

            if (WDR_MODE_2To1_LINE == pstSnsState->enWDRMode)
            {
                u32RatioTmp = 0xFFF;
                au32IntTimeMax[0] = u32IntTimeMaxTmp;
                au32IntTimeMax[1] = au32IntTimeMax[0] * u32RatioTmp >> 6;
            }
            else
            {
            }
            au32IntTimeMin[0] = au32IntTimeMax[0];
            au32IntTimeMin[1] = au32IntTimeMax[1];
            au32IntTimeMin[2] = au32IntTimeMax[2];
            au32IntTimeMin[3] = au32IntTimeMax[3];
        }
    }

    return;

}

/* Only used in LINE_WDR mode */
static HI_VOID cmos_ae_fswdr_attr_set(VI_PIPE ViPipe, AE_FSWDR_ATTR_S *pstAeFSWDRAttr)
{
    CMOS_CHECK_POINTER_VOID(pstAeFSWDRAttr);

    genFSWDRMode[ViPipe] = pstAeFSWDRAttr->enFSWDRMode;
    gu32MaxTimeGetCnt[ViPipe] = 0;
}


static HI_S32 cmos_init_ae_exp_function(AE_SENSOR_EXP_FUNC_S *pstExpFuncs)
{
    CMOS_CHECK_POINTER(pstExpFuncs);

    memset(pstExpFuncs, 0, sizeof(AE_SENSOR_EXP_FUNC_S));

    pstExpFuncs->pfn_cmos_get_ae_default    = cmos_get_ae_default;
    pstExpFuncs->pfn_cmos_fps_set           = cmos_fps_set;
    pstExpFuncs->pfn_cmos_slow_framerate_set = cmos_slow_framerate_set;
    pstExpFuncs->pfn_cmos_inttime_update    = cmos_inttime_update;
    pstExpFuncs->pfn_cmos_gains_update      = cmos_gains_update;
    pstExpFuncs->pfn_cmos_again_calc_table  = cmos_again_calc_table;
    pstExpFuncs->pfn_cmos_dgain_calc_table  = cmos_dgain_calc_table;
    pstExpFuncs->pfn_cmos_get_inttime_max   = cmos_get_inttime_max;
    pstExpFuncs->pfn_cmos_ae_fswdr_attr_set = cmos_ae_fswdr_attr_set;

    return HI_SUCCESS;
}

//awb static param for Fuji Lens New IR_Cut
#define CALIBRATE_STATIC_WB_R_GAIN  494
#define CALIBRATE_STATIC_WB_GR_GAIN 256
#define CALIBRATE_STATIC_WB_GB_GAIN 256
#define CALIBRATE_STATIC_WB_B_GAIN  464

/* Calibration results for Auto WB Planck */
#define CALIBRATE_AWB_P1 -41
#define CALIBRATE_AWB_P2 297
#define CALIBRATE_AWB_Q1 0
#define CALIBRATE_AWB_A1 155760
#define CALIBRATE_AWB_B1 128
#define CALIBRATE_AWB_C1 -105396

/* Rgain and Bgain of the golden sample */
#define GOLDEN_RGAIN 0
#define GOLDEN_BGAIN 0
static HI_S32 cmos_get_awb_default(VI_PIPE ViPipe, AWB_SENSOR_DEFAULT_S *pstAwbSnsDft)
{
    ISP_SNS_STATE_S *pstSnsState = HI_NULL;

    CMOS_CHECK_POINTER(pstAwbSnsDft);
    OV426_SENSOR_GET_CTX(ViPipe, pstSnsState);
    CMOS_CHECK_POINTER(pstSnsState);

    memset(pstAwbSnsDft, 0, sizeof(AWB_SENSOR_DEFAULT_S));

    pstAwbSnsDft->u16WbRefTemp = 4900;

    pstAwbSnsDft->au16GainOffset[0] = CALIBRATE_STATIC_WB_R_GAIN;
    pstAwbSnsDft->au16GainOffset[1] = CALIBRATE_STATIC_WB_GR_GAIN;
    pstAwbSnsDft->au16GainOffset[2] = CALIBRATE_STATIC_WB_GB_GAIN;
    pstAwbSnsDft->au16GainOffset[3] = CALIBRATE_STATIC_WB_B_GAIN;

    pstAwbSnsDft->as32WbPara[0] = CALIBRATE_AWB_P1;
    pstAwbSnsDft->as32WbPara[1] = CALIBRATE_AWB_P2;
    pstAwbSnsDft->as32WbPara[2] = CALIBRATE_AWB_Q1;
    pstAwbSnsDft->as32WbPara[3] = CALIBRATE_AWB_A1;
    pstAwbSnsDft->as32WbPara[4] = CALIBRATE_AWB_B1;
    pstAwbSnsDft->as32WbPara[5] = CALIBRATE_AWB_C1;

    pstAwbSnsDft->u16GoldenRgain = GOLDEN_RGAIN;
    pstAwbSnsDft->u16GoldenBgain = GOLDEN_BGAIN;
    switch (pstSnsState->enWDRMode)
    {
        default:
        case WDR_MODE_NONE:
            memcpy(&pstAwbSnsDft->stCcm, &g_stAwbCcm, sizeof(AWB_CCM_S));
            memcpy(&pstAwbSnsDft->stAgcTbl, &g_stAwbAgcTable, sizeof(AWB_AGC_TABLE_S));
            break;
//	        case WDR_MODE_2To1_LINE:
//	            memcpy(&pstAwbSnsDft->stCcm,    &g_stAwbCcmFsWdr, sizeof(AWB_CCM_S));
//	            memcpy(&pstAwbSnsDft->stAgcTbl, &g_stAwbAgcTableFSWDR, sizeof(AWB_AGC_TABLE_S));
//	
//	            break;
    }
    pstAwbSnsDft->u16InitRgain = g_au16InitWBGain[ViPipe][0];
    pstAwbSnsDft->u16InitGgain = g_au16InitWBGain[ViPipe][1];
    pstAwbSnsDft->u16InitBgain = g_au16InitWBGain[ViPipe][2];
    pstAwbSnsDft->u16SampleRgain = g_au16SampleRgain[ViPipe];
    pstAwbSnsDft->u16SampleBgain = g_au16SampleBgain[ViPipe];

    return HI_SUCCESS;
}

static HI_S32 cmos_get_awb_spec_default(VI_PIPE ViPipe, AWB_SPEC_SENSOR_DEFAULT_S *pstAwbSpecSnsDft)
{
    CMOS_CHECK_POINTER(pstAwbSpecSnsDft);

    memset(pstAwbSpecSnsDft, 0, sizeof(AWB_SPEC_SENSOR_DEFAULT_S));

    memcpy(&pstAwbSpecSnsDft->stSpecAwbAttrs, &g_SpecAWBFactTbl, sizeof(ISP_SPECAWB_ATTR_S));
    memcpy(&pstAwbSpecSnsDft->stCaaControl, &g_SpecKCAWBCaaTblControl, sizeof(ISP_SPECAWB_CAA_CONTROl_S));

    return HI_SUCCESS;
}

static HI_S32 cmos_init_awb_exp_function(AWB_SENSOR_EXP_FUNC_S *pstExpFuncs)
{
    CMOS_CHECK_POINTER(pstExpFuncs);

    memset(pstExpFuncs, 0, sizeof(AWB_SENSOR_EXP_FUNC_S));

    pstExpFuncs->pfn_cmos_get_awb_default = cmos_get_awb_default;
    pstExpFuncs->pfn_cmos_get_awb_spec_default = cmos_get_awb_spec_default;

    return HI_SUCCESS;
}

static ISP_CMOS_DNG_COLORPARAM_S g_stDngColorParam =
{
    {378, 256, 430},
    {439, 256, 439}
};
static HI_S32 cmos_get_isp_default(VI_PIPE ViPipe, ISP_CMOS_DEFAULT_S *pstDef)
{
    ISP_SNS_STATE_S *pstSnsState = HI_NULL;

    CMOS_CHECK_POINTER(pstDef);
    OV426_SENSOR_GET_CTX(ViPipe, pstSnsState);
    CMOS_CHECK_POINTER(pstSnsState);

    memset(pstDef, 0, sizeof(ISP_CMOS_DEFAULT_S));
    pstDef->unKey.bit1RLsc     = 0;
    pstDef->pstRLsc            = &g_stCmosRLsc;
    pstDef->unKey.bit1Clut     = 1;
    pstDef->pstClut            = &g_stIspCLUT;
    pstDef->unKey.bit1EdgeMark = 1;
    pstDef->pstEdgeMark        = &g_stIspEdgeMark;
    pstDef->unKey.bit1Wdr      = 1;
    pstDef->pstWdr             = &g_stIspWDR;
    pstDef->unKey.bit1Lsc      = 0;
    pstDef->pstLsc             = &g_stCmosLsc;
    pstDef->unKey.bit1Dpc      = 1;
    pstDef->pstDpc             = &g_stCmosDpc;

    switch (pstSnsState->enWDRMode)
    {
        default:
        case WDR_MODE_NONE:
            pstDef->unKey.bit1Demosaic       = 1;
            pstDef->pstDemosaic              = &g_stIspDemosaic;
            pstDef->unKey.bit1Sharpen        = 1;
            pstDef->pstSharpen               = &g_stIspYuvSharpen;
            pstDef->unKey.bit1Drc            = 1;
            pstDef->pstDrc                   = &g_stIspDRC;
            pstDef->unKey.bit1Ca             = 1;
            pstDef->pstCa                    = &g_stIspCA;
            pstDef->unKey.bit1Gamma          = 1;
            pstDef->pstGamma                 = &g_stIspGamma;
            pstDef->unKey.bit1BayerNr        = 1;
            pstDef->pstBayerNr               = &g_stIspBayerNr;
            pstDef->unKey.bit1Ge             = 1;
            pstDef->pstGe                    = &g_stIspGe;
            pstDef->unKey.bit1AntiFalseColor = 1;
            pstDef->pstAntiFalseColor        = &g_stIspAntiFalseColor;
            pstDef->unKey.bit1Ldci           = 1;
            pstDef->pstLdci                  = &g_stIspLdci;
            memcpy(&pstDef->stNoiseCalibration, &g_stIspNoiseCalibratio, sizeof(ISP_CMOS_NOISE_CALIBRATION_S));
            break;
//	        case WDR_MODE_2To1_LINE:
//	            pstDef->unKey.bit1Demosaic       = 1;
//	            pstDef->pstDemosaic              = &g_stIspDemosaicWdr;
//	            pstDef->unKey.bit1Sharpen        = 1;
//	            pstDef->pstSharpen               = &g_stIspYuvSharpenWdr;
//	            pstDef->unKey.bit1Drc            = 1;
//	            pstDef->pstDrc                   = &g_stIspDRCWDR;
//	            pstDef->unKey.bit1Ca       = 1;
//	            pstDef->pstCa              = &g_stIspCAWDR;
//	            pstDef->unKey.bit1Gamma          = 1;
//	            pstDef->pstGamma                 = &g_stIspGammaFSWDR;
//	            pstDef->unKey.bit1PreGamma       = 0;
//	            pstDef->pstPreGamma              = &g_stPreGamma;
//	            pstDef->unKey.bit1BayerNr        = 1;
//	            pstDef->pstBayerNr               = &g_stIspBayerNrWdr2To1;
//	            pstDef->unKey.bit1Ge             = 1;
//	            pstDef->pstGe                    = &g_stIspWdrGe;
//	            pstDef->unKey.bit1AntiFalseColor = 1;
//	            pstDef->pstAntiFalseColor        = &g_stIspWdrAntiFalseColor;
//	            pstDef->unKey.bit1Ldci           = 1;
//	            pstDef->pstLdci                  = &g_stIspWdrLdci;
//	            pstDef->stWdrSwitchAttr.au32ExpRatio[0] = 0x40;
//	            memcpy(&pstDef->stNoiseCalibration, &g_stIspNoiseCalibratio, sizeof(ISP_CMOS_NOISE_CALIBRATION_S));
//	            break;
    }

    pstDef->stSensorMaxResolution.u32MaxWidth  = 400; // add by andreas
    pstDef->stSensorMaxResolution.u32MaxHeight = 400; // add by andreas
    
    pstDef->stSensorMode.u32SensorID = OV426_ID;
    pstDef->stSensorMode.u8SensorMode = pstSnsState->u8ImgMode;

    memcpy(&pstDef->stDngColorParam, &g_stDngColorParam, sizeof(ISP_CMOS_DNG_COLORPARAM_S));

    pstDef->stSensorMode.stDngRawFormat.stDefaultScale.stDefaultScaleH.u32Denominator = 1;
    pstDef->stSensorMode.stDngRawFormat.stDefaultScale.stDefaultScaleH.u32Numerator = 1;
    pstDef->stSensorMode.stDngRawFormat.stDefaultScale.stDefaultScaleV.u32Denominator = 1;
    pstDef->stSensorMode.stDngRawFormat.stDefaultScale.stDefaultScaleV.u32Numerator = 1;
    pstDef->stSensorMode.stDngRawFormat.stCfaRepeatPatternDim.u16RepeatPatternDimRows = 2;
    pstDef->stSensorMode.stDngRawFormat.stCfaRepeatPatternDim.u16RepeatPatternDimCols = 2;
    pstDef->stSensorMode.stDngRawFormat.stBlcRepeatDim.u16BlcRepeatRows = 2;
    pstDef->stSensorMode.stDngRawFormat.stBlcRepeatDim.u16BlcRepeatCols = 2;
    pstDef->stSensorMode.stDngRawFormat.enCfaLayout = CFALAYOUT_TYPE_RECTANGULAR;
    pstDef->stSensorMode.stDngRawFormat.au8CfaPlaneColor[0] = 0;
    pstDef->stSensorMode.stDngRawFormat.au8CfaPlaneColor[1] = 1;
    pstDef->stSensorMode.stDngRawFormat.au8CfaPlaneColor[2] = 2;
    pstDef->stSensorMode.stDngRawFormat.au8CfaPattern[0] = 0;
    pstDef->stSensorMode.stDngRawFormat.au8CfaPattern[1] = 1;
    pstDef->stSensorMode.stDngRawFormat.au8CfaPattern[2] = 1;
    pstDef->stSensorMode.stDngRawFormat.au8CfaPattern[3] = 2;
    pstDef->stSensorMode.bValidDngRawFormat = HI_TRUE;
    return HI_SUCCESS;
}

static HI_S32 cmos_get_isp_black_level(VI_PIPE ViPipe, ISP_CMOS_BLACK_LEVEL_S *pstBlackLevel)
{
    HI_S32  i;
    ISP_SNS_STATE_S *pstSnsState = HI_NULL;

    CMOS_CHECK_POINTER(pstBlackLevel);
    OV426_SENSOR_GET_CTX(ViPipe, pstSnsState);
    CMOS_CHECK_POINTER(pstSnsState);

    /* Don't need to update black level when iso change */
    pstBlackLevel->bUpdate = HI_FALSE;
//	    if (WDR_MODE_NONE == pstSnsState->enWDRMode)
//	    {
//	        for (i = 0; i < 4; i++)
//	        {
//	            pstBlackLevel->au16BlackLevel[i] = 64; /*12bit,0x40*/
//	        }
//	    }
//	    else if (WDR_MODE_2To1_LINE == pstSnsState->enWDRMode)
//	    {
//	        for (i = 0; i < 4; i++)
//	        {
//	            pstBlackLevel->au16BlackLevel[i] = 256; /*10bit,0x100*/
//	        }
//	    }
    pstBlackLevel->au16BlackLevel[0] = 0x40;
    pstBlackLevel->au16BlackLevel[1] = 0x40;
    pstBlackLevel->au16BlackLevel[2] = 0x40;
    pstBlackLevel->au16BlackLevel[3] = 0x40;

    return HI_SUCCESS;
}

static HI_VOID cmos_set_pixel_detect(VI_PIPE ViPipe, HI_BOOL bEnable)
{
    HI_U32 u32FullLines_5Fps, u32MaxIntTime_5Fps;
    ISP_SNS_STATE_S *pstSnsState = HI_NULL;

    OV426_SENSOR_GET_CTX(ViPipe, pstSnsState);
    CMOS_CHECK_POINTER_VOID(pstSnsState);

//	    if (WDR_MODE_2To1_LINE == pstSnsState->enWDRMode)
//	    {
//	        return;
//	    }
//	    else
//	    {
//	        if (Ov426_400x400_12bit_30fps == pstSnsState->u8ImgMode)
//	        {
//	            u32FullLines_5Fps = (OV426_VMAX_12bit_LINEAR * 30) / 5;
//	        }
//	        else
//	        {
//	            return;
//	        }
//	    }

    if (WDR_MODE_NONE == pstSnsState->enWDRMode)
    {
        if (Ov426_400x400_12bit_30fps == pstSnsState->u8ImgMode)
        {
            u32FullLines_5Fps = (OV426_VMAX_12bit_LINEAR * 30) / 5;
        }
        else
        {
            return;
        }
    }
    else
    {
        return;
    }

    u32MaxIntTime_5Fps = u32FullLines_5Fps - 4; // why?

    if (bEnable) /* setup for ISP pixel calibration mode */
    {
        ov426_write_register(ViPipe, VMAX_ADDR_L, LOW_8BITS(u32FullLines_5Fps));
        ov426_write_register(ViPipe, VMAX_ADDR_H, HIGH_8BITS(u32FullLines_5Fps));

        ov426_write_register(ViPipe, AEC_PK_EXPO_L, ((u32MaxIntTime_5Fps & 0xF)<<4));    /* shutter */
        ov426_write_register(ViPipe, AEC_PK_EXPO_M, ((u32MaxIntTime_5Fps & 0xFFF)>>4));
        ov426_write_register(ViPipe, AEC_PK_EXPO_H, (u32MaxIntTime_5Fps >> 12));

        ov426_write_register(ViPipe, GAIN_ADDR_H, 0x00); //gain
        ov426_write_register(ViPipe, GAIN_ADDR_L, 0x90);
    }
    else /* setup for ISP 'normal mode' */
    {
        ov426_write_register (ViPipe, VMAX_ADDR_L, LOW_8BITS(pstSnsState->u32FLStd));
        ov426_write_register (ViPipe, VMAX_ADDR_H, HIGH_8BITS(pstSnsState->u32FLStd));
        pstSnsState->bSyncInit = HI_FALSE ;
    }
    return;
}

static HI_S32 cmos_set_wdr_mode(VI_PIPE ViPipe, HI_U8 u8Mode)
{
    ISP_SNS_STATE_S *pstSnsState = HI_NULL;

    OV426_SENSOR_GET_CTX(ViPipe, pstSnsState);
    CMOS_CHECK_POINTER(pstSnsState);

    pstSnsState->bSyncInit = HI_FALSE;

    switch (u8Mode)
    {
        case WDR_MODE_NONE:
            pstSnsState->enWDRMode = WDR_MODE_NONE;
            pstSnsState->u8ImgMode = Ov426_400x400_12bit_30fps;
            pstSnsState->u32FLStd = OV426_VMAX_12bit_LINEAR ;
            ISP_TRACE(HI_DBG_ERR, "linear mode\n");
            break;

        default:
            ISP_TRACE(HI_DBG_ERR, "NOT support this mode!\n");
            return HI_FAILURE;
            break;
    }

    pstSnsState->au32FL[0] = pstSnsState->u32FLStd;
    pstSnsState->au32FL[1] = pstSnsState->au32FL[0];
    memset(pstSnsState->au32WDRIntTime, 0, sizeof(pstSnsState->au32WDRIntTime));

    return HI_SUCCESS;
}

static HI_S32 cmos_get_sns_regs_info(VI_PIPE ViPipe, ISP_SNS_REGS_INFO_S *pstSnsRegsInfo)
{
    HI_S32 i;
    ISP_SNS_STATE_S *pstSnsState = HI_NULL;

    CMOS_CHECK_POINTER(pstSnsRegsInfo);
    OV426_SENSOR_GET_CTX(ViPipe, pstSnsState);
    CMOS_CHECK_POINTER(pstSnsState);

    if ((HI_FALSE == pstSnsState->bSyncInit) || (HI_FALSE == pstSnsRegsInfo->bConfig))
    {
        pstSnsState->astRegsInfo[0].enSnsType = ISP_SNS_I2C_TYPE;
        pstSnsState->astRegsInfo[0].unComBus.s8I2cDev = g_aunOv426BusInfo[ViPipe].s8I2cDev;
        pstSnsState->astRegsInfo[0].u8Cfg2ValidDelayMax = 2;
        pstSnsState->astRegsInfo[0].u32RegNum = 7;

//	        if ((WDR_MODE_2To1_LINE == pstSnsState->enWDRMode) || (WDR_MODE_2To1_FRAME == pstSnsState->enWDRMode) || (WDR_MODE_2To1_FRAME_FULL_RATE == pstSnsState->enWDRMode))
//	        {
//	            pstSnsState->astRegsInfo[0].u32RegNum += 6;
//	        }

        for (i = 0; i < pstSnsState->astRegsInfo[0].u32RegNum; i++)
        {
            pstSnsState->astRegsInfo[0].astI2cData[i].bUpdate = HI_TRUE;
            pstSnsState->astRegsInfo[0].astI2cData[i].u8DevAddr = ov426_i2c_addr;
            pstSnsState->astRegsInfo[0].astI2cData[i].u32AddrByteNum = ov426_addr_byte;
            pstSnsState->astRegsInfo[0].astI2cData[i].u32DataByteNum = ov426_data_byte;
        }
        pstSnsState->astRegsInfo[0].astI2cData[0].u8DelayFrmNum = 0;
        pstSnsState->astRegsInfo[0].astI2cData[0].u32RegAddr = VMAX_ADDR_L;        // Vmax
        pstSnsState->astRegsInfo[0].astI2cData[1].u8DelayFrmNum = 0;
        pstSnsState->astRegsInfo[0].astI2cData[1].u32RegAddr = VMAX_ADDR_H;        // Vmax
        pstSnsState->astRegsInfo[0].astI2cData[2].u8DelayFrmNum = 0;
        pstSnsState->astRegsInfo[0].astI2cData[2].u32RegAddr = AEC_PK_EXPO_L;        // shutter
        pstSnsState->astRegsInfo[0].astI2cData[3].u8DelayFrmNum = 0;
        pstSnsState->astRegsInfo[0].astI2cData[3].u32RegAddr = AEC_PK_EXPO_M;        // shutter
        pstSnsState->astRegsInfo[0].astI2cData[4].u8DelayFrmNum = 0;
        pstSnsState->astRegsInfo[0].astI2cData[4].u32RegAddr = AEC_PK_EXPO_H;        // shutter
        pstSnsState->astRegsInfo[0].astI2cData[5].u8DelayFrmNum = 0;
        pstSnsState->astRegsInfo[0].astI2cData[5].u32RegAddr = GAIN_ADDR_L;        // gain
        pstSnsState->astRegsInfo[0].astI2cData[6].u8DelayFrmNum = 0;
        pstSnsState->astRegsInfo[0].astI2cData[6].u32RegAddr = GAIN_ADDR_H;     // 

//	        if (WDR_MODE_2To1_LINE == pstSnsState->enWDRMode)
//	        {
//	            pstSnsState->astRegsInfo[0].astI2cData[8].u8DelayFrmNum = 0;
//	            pstSnsState->astRegsInfo[0].astI2cData[8].u32RegAddr = EXPOSURE_TIME_SHORT_L;     // Short shutter
//	            pstSnsState->astRegsInfo[0].astI2cData[9].u8DelayFrmNum = 0;
//	            pstSnsState->astRegsInfo[0].astI2cData[9].u32RegAddr = EXPOSURE_TIME_SHORT_H;     // Short shutter
//	            pstSnsState->astRegsInfo[0].astI2cData[10].u8DelayFrmNum = 0;
//	            pstSnsState->astRegsInfo[0].astI2cData[10].u32RegAddr = SHORT_AGAIN_L;     // Short Again
//	            pstSnsState->astRegsInfo[0].astI2cData[11].u8DelayFrmNum = 0;
//	            pstSnsState->astRegsInfo[0].astI2cData[11].u32RegAddr = SHORT_AGAIN_H;     // Short Again
//	            pstSnsState->astRegsInfo[0].astI2cData[12].u8DelayFrmNum = 0;
//	            pstSnsState->astRegsInfo[0].astI2cData[12].u32RegAddr = SHORT_DGAIN_L;     // Short Dgain
//	            pstSnsState->astRegsInfo[0].astI2cData[13].u8DelayFrmNum = 0;
//	            pstSnsState->astRegsInfo[0].astI2cData[13].u32RegAddr = SHORT_DGAIN_H;     // Short Dgain
//	        }
        pstSnsState->bSyncInit = HI_TRUE;
    }
    else
    {
        for (i = 0; i < pstSnsState->astRegsInfo[0].u32RegNum; i++)
        {
            if (pstSnsState->astRegsInfo[0].astI2cData[i].u32Data == pstSnsState->astRegsInfo[1].astI2cData[i].u32Data)
            {
                pstSnsState->astRegsInfo[0].astI2cData[i].bUpdate = HI_FALSE;
            }

            else
            {
                pstSnsState->astRegsInfo[0].astI2cData[i].bUpdate = HI_TRUE;
            }
        }
    }

//	    if (WDR_MODE_2To1_LINE == pstSnsState->enWDRMode )
//	    {
//	        if (pstSnsState->au32FL[1] > pstSnsState->au32FL[0])
//	        {
//	            pstSnsState->astRegsInfo[0].astI2cData[0].u8DelayFrmNum = 2;
//	            pstSnsState->astRegsInfo[0].astI2cData[1].u8DelayFrmNum = 2;
//	        }
//	        else if (pstSnsState->au32FL[1] < pstSnsState->au32FL[0])
//	        {
//	            pstSnsState->astRegsInfo[0].astI2cData[0].u8DelayFrmNum = 0;
//	            pstSnsState->astRegsInfo[0].astI2cData[1].u8DelayFrmNum = 0;
//	        }
//	        else
//	        {
//	            //do nothing
//	        }
//	    }

    pstSnsRegsInfo->bConfig = HI_FALSE;
    memcpy(pstSnsRegsInfo, &pstSnsState->astRegsInfo[0], sizeof(ISP_SNS_REGS_INFO_S));
    memcpy(&pstSnsState->astRegsInfo[1], &pstSnsState->astRegsInfo[0], sizeof(ISP_SNS_REGS_INFO_S));
    pstSnsState->au32FL[1] = pstSnsState->au32FL[0];

    return HI_SUCCESS;
}

static HI_S32 cmos_set_image_mode(VI_PIPE ViPipe, ISP_CMOS_SENSOR_IMAGE_MODE_S *pstSensorImageMode)
{
    HI_U8 u8SensorImageMode;
    ISP_SNS_STATE_S *pstSnsState = HI_NULL;

    CMOS_CHECK_POINTER(pstSensorImageMode);
    OV426_SENSOR_GET_CTX(ViPipe, pstSnsState);
    CMOS_CHECK_POINTER(pstSnsState);

    u8SensorImageMode = pstSnsState->u8ImgMode;
    pstSnsState->bSyncInit = HI_FALSE;

    if (HI_NULL == pstSensorImageMode )
    {
        ISP_TRACE(HI_DBG_ERR, "null pointer when set image mode\n");
        return HI_FAILURE;
    }

    if ((pstSensorImageMode->u16Width <= 2688) && (pstSensorImageMode->u16Height <= 1944))
    {
        if (WDR_MODE_NONE == pstSnsState->enWDRMode)
        {
            if (pstSensorImageMode->f32Fps <= 30)
            {
                u8SensorImageMode = Ov426_400x400_12bit_30fps;
            }
            else
            {
                ISP_TRACE(HI_DBG_ERR, "Not support! Width:%d, Height:%d, Fps:%f, WDRMode:%d\n",
                          pstSensorImageMode->u16Width,
                          pstSensorImageMode->u16Height,
                          pstSensorImageMode->f32Fps,
                          pstSnsState->enWDRMode);
                return HI_FAILURE;
            }
        }
        else if (WDR_MODE_NONE == pstSnsState->enWDRMode)
        {
            ISP_TRACE(HI_DBG_ERR, "Not support! Width:%d, Height:%d, Fps:%f, WDRMode:%d\n",
                      pstSensorImageMode->u16Width,
                      pstSensorImageMode->u16Height,
                      pstSensorImageMode->f32Fps,
                      pstSnsState->enWDRMode);
            return HI_FAILURE;
        }
    }
    else
    {
        ISP_TRACE(HI_DBG_ERR, "Not support! Width:%d, Height:%d, Fps:%f, WDRMode:%d\n",
                  pstSensorImageMode->u16Width,
                  pstSensorImageMode->u16Height,
                  pstSensorImageMode->f32Fps,
                  pstSnsState->enWDRMode);

        return HI_FAILURE;
    }

    /* Sensor first init */
    if (HI_FALSE == pstSnsState->bInit)
    {
        pstSnsState->u8ImgMode = u8SensorImageMode;
        return HI_SUCCESS;
    }

    /* Switch SensorImageMode */
    if (u8SensorImageMode == pstSnsState->u8ImgMode)
    {
        /* Don't need to switch SensorImageMode */
        return HI_FAILURE;
    }
    pstSnsState->u8ImgMode = u8SensorImageMode;
    memset(pstSnsState->au32WDRIntTime, 0, sizeof(pstSnsState->au32WDRIntTime));

    return HI_SUCCESS;
}

static HI_VOID sensor_global_init(VI_PIPE ViPipe)
{
    ISP_SNS_STATE_S *pstSnsState = HI_NULL;

    OV426_SENSOR_GET_CTX(ViPipe, pstSnsState);
    CMOS_CHECK_POINTER_VOID(pstSnsState);

    pstSnsState->bInit = HI_FALSE;
    pstSnsState->bSyncInit = HI_FALSE;
    pstSnsState->u8ImgMode = Ov426_400x400_12bit_30fps;
    pstSnsState->enWDRMode = WDR_MODE_NONE;
    pstSnsState->u32FLStd = OV426_VMAX_12bit_LINEAR;
    pstSnsState->au32FL[0] = OV426_VMAX_12bit_LINEAR;
    pstSnsState->au32FL[1] = OV426_VMAX_12bit_LINEAR;

    memset(&pstSnsState->astRegsInfo[0], 0, sizeof(ISP_SNS_REGS_INFO_S));
    memset(&pstSnsState->astRegsInfo[1], 0, sizeof(ISP_SNS_REGS_INFO_S));
}

static HI_S32 cmos_init_sensor_exp_function(ISP_SENSOR_EXP_FUNC_S *pstSensorExpFunc)
{
    CMOS_CHECK_POINTER(pstSensorExpFunc);

    memset(pstSensorExpFunc, 0, sizeof(ISP_SENSOR_EXP_FUNC_S));

    pstSensorExpFunc->pfn_cmos_sensor_init = ov426_init;
    pstSensorExpFunc->pfn_cmos_sensor_exit = ov426_exit;
    pstSensorExpFunc->pfn_cmos_sensor_global_init = sensor_global_init;
    pstSensorExpFunc->pfn_cmos_set_image_mode = cmos_set_image_mode;
    pstSensorExpFunc->pfn_cmos_set_wdr_mode = cmos_set_wdr_mode;
    pstSensorExpFunc->pfn_cmos_get_isp_default = cmos_get_isp_default;
    pstSensorExpFunc->pfn_cmos_get_isp_black_level = cmos_get_isp_black_level;
    pstSensorExpFunc->pfn_cmos_set_pixel_detect = cmos_set_pixel_detect;
    pstSensorExpFunc->pfn_cmos_get_sns_reg_info = cmos_get_sns_regs_info;

    return HI_SUCCESS;
}

/****************************************************************************
 * callback structure                                                       *
 ****************************************************************************/

static HI_S32 ov426_set_bus_info(VI_PIPE ViPipe, ISP_SNS_COMMBUS_U unSNSBusInfo)
{
    g_aunOv426BusInfo[ViPipe].s8I2cDev = unSNSBusInfo.s8I2cDev;

    return HI_SUCCESS;
}

static HI_S32 sensor_ctx_init(VI_PIPE ViPipe)
{
    ISP_SNS_STATE_S *pastSnsStateCtx = HI_NULL;

    OV426_SENSOR_GET_CTX(ViPipe, pastSnsStateCtx);

    if (HI_NULL == pastSnsStateCtx)
    {
        pastSnsStateCtx = (ISP_SNS_STATE_S *)malloc(sizeof(ISP_SNS_STATE_S));
        if (HI_NULL == pastSnsStateCtx)
        {
            ISP_TRACE(HI_DBG_ERR, "Isp[%d] SnsCtx malloc memory failed!\n", ViPipe);
            return HI_ERR_ISP_NOMEM;
        }
    }

    memset(pastSnsStateCtx, 0, sizeof(ISP_SNS_STATE_S));

    OV426_SENSOR_SET_CTX(ViPipe, pastSnsStateCtx);

    return HI_SUCCESS;
}

static HI_VOID sensor_ctx_exit(VI_PIPE ViPipe)
{
    ISP_SNS_STATE_S *pastSnsStateCtx = HI_NULL;

    OV426_SENSOR_GET_CTX(ViPipe, pastSnsStateCtx);
    SENSOR_FREE(pastSnsStateCtx);
    OV426_SENSOR_RESET_CTX(ViPipe);
}

static HI_S32 sensor_register_callback(VI_PIPE ViPipe, ALG_LIB_S *pstAeLib, ALG_LIB_S *pstAwbLib)
{
    HI_S32 s32Ret;
    ISP_SENSOR_REGISTER_S stIspRegister;
    AE_SENSOR_REGISTER_S  stAeRegister;
    AWB_SENSOR_REGISTER_S stAwbRegister;
    ISP_SNS_ATTR_INFO_S   stSnsAttrInfo;

    CMOS_CHECK_POINTER(pstAeLib);
    CMOS_CHECK_POINTER(pstAwbLib);

    s32Ret = sensor_ctx_init(ViPipe);

    if (HI_SUCCESS != s32Ret)
    {
        return HI_FAILURE;
    }

    stSnsAttrInfo.eSensorId = OV426_ID;

    s32Ret  = cmos_init_sensor_exp_function(&stIspRegister.stSnsExp);
    s32Ret |= HI_MPI_ISP_SensorRegCallBack(ViPipe, &stSnsAttrInfo, &stIspRegister);

    if (HI_SUCCESS != s32Ret)
    {
        ISP_TRACE(HI_DBG_ERR, "sensor register callback function failed!\n");
        return s32Ret;
    }

    s32Ret  = cmos_init_ae_exp_function(&stAeRegister.stSnsExp);
    s32Ret |= HI_MPI_AE_SensorRegCallBack(ViPipe, pstAeLib, &stSnsAttrInfo, &stAeRegister);

    if (HI_SUCCESS != s32Ret)
    {
        ISP_TRACE(HI_DBG_ERR, "sensor register callback function to ae lib failed!\n");
        return s32Ret;
    }

    s32Ret  = cmos_init_awb_exp_function(&stAwbRegister.stSnsExp);
    s32Ret |= HI_MPI_AWB_SensorRegCallBack(ViPipe, pstAwbLib, &stSnsAttrInfo, &stAwbRegister);

    if (HI_SUCCESS != s32Ret)
    {
        ISP_TRACE(HI_DBG_ERR, "sensor register callback function to awb lib failed!\n");
        return s32Ret;
    }

    return HI_SUCCESS;
}

static HI_S32 sensor_unregister_callback(VI_PIPE ViPipe, ALG_LIB_S *pstAeLib, ALG_LIB_S *pstAwbLib)
{
    HI_S32 s32Ret;

    CMOS_CHECK_POINTER(pstAeLib);
    CMOS_CHECK_POINTER(pstAwbLib);

    s32Ret = HI_MPI_ISP_SensorUnRegCallBack(ViPipe, OV426_ID);

    if (HI_SUCCESS != s32Ret)
    {
        ISP_TRACE(HI_DBG_ERR, "sensor unregister callback function failed!\n");
        return s32Ret;
    }

    s32Ret = HI_MPI_AE_SensorUnRegCallBack(ViPipe, pstAeLib, OV426_ID);

    if (HI_SUCCESS != s32Ret)
    {
        ISP_TRACE(HI_DBG_ERR, "sensor unregister callback function to ae lib failed!\n");
        return s32Ret;
    }

    s32Ret = HI_MPI_AWB_SensorUnRegCallBack(ViPipe, pstAwbLib, OV426_ID);

    if (HI_SUCCESS != s32Ret)
    {
        ISP_TRACE(HI_DBG_ERR, "sensor unregister callback function to awb lib failed!\n");
        return s32Ret;
    }

    sensor_ctx_exit(ViPipe);

    return HI_SUCCESS;
}

static HI_S32 sensor_set_init(VI_PIPE ViPipe, ISP_INIT_ATTR_S *pstInitAttr)
{
    CMOS_CHECK_POINTER(pstInitAttr);

    g_au32InitExposure[ViPipe] = pstInitAttr->u32Exposure;
    g_au32LinesPer500ms[ViPipe] = pstInitAttr->u32LinesPer500ms;
    g_au16InitWBGain[ViPipe][0] = pstInitAttr->u16WBRgain;
    g_au16InitWBGain[ViPipe][1] = pstInitAttr->u16WBGgain;
    g_au16InitWBGain[ViPipe][2] = pstInitAttr->u16WBBgain;
    g_au16SampleRgain[ViPipe] = pstInitAttr->u16SampleRgain;
    g_au16SampleBgain[ViPipe] = pstInitAttr->u16SampleBgain;

    return HI_SUCCESS;
}

ISP_SNS_OBJ_S stSnsOv426Obj =
{
    .pfnRegisterCallback    = sensor_register_callback,
    .pfnUnRegisterCallback  = sensor_unregister_callback,
    .pfnStandby             = ov426_standby,
    .pfnRestart             = ov426_restart,
    .pfnMirrorFlip          = HI_NULL,
    .pfnWriteReg            = ov426_write_register,
    .pfnReadReg             = ov426_read_register,
    .pfnSetBusInfo          = ov426_set_bus_info,
    .pfnSetInit             = sensor_set_init
};


#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */

#endif /* __OV426_CMOS_H_ */
