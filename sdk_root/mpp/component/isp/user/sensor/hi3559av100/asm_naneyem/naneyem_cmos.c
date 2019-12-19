#if !defined(__NANEYEM_CMOS_H_)
#define __NANEYEM_CMOS_H_

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "hi_comm_sns.h"
#include "hi_comm_video.h"
#include "hi_sns_ctrl.h"
#include "mpi_isp.h"
#include "mpi_ae.h"
#include "mpi_awb.h"

#include "naneyem_cmos_ex.h"
#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* End of #ifdef __cplusplus */


#define NANEYEM_ID 109 // asscii code of 'm'
//#define __DEBUG__

#define HIGHER_4BITS(x) (((x) & 0xf0000) >> 16)
#define HIGH_8BITS(x) (((x) & 0xff00) >> 8)
#define LOW_8BITS(x)  ((x) & 0x00ff)

#ifndef MAX
#define MAX(a, b) (((a) < (b)) ?  (b) : (a))
#endif

#ifndef MIN
#define MIN(a, b) (((a) > (b)) ?  (b) : (a))
#endif

ISP_SNS_STATE_S *g_pastNaneyem[ISP_MAX_PIPE_NUM] = {HI_NULL};

#define NANEYEM_SENSOR_GET_CTX(dev, pstCtx)   (pstCtx = g_pastNaneyem[dev])
#define NANEYEM_SENSOR_SET_CTX(dev, pstCtx)   (g_pastNaneyem[dev] = pstCtx)
#define NANEYEM_SENSOR_RESET_CTX(dev)         (g_pastNaneyem[dev] = HI_NULL)

static HI_U32 g_au32InitExposure[ISP_MAX_PIPE_NUM]  = {0};
static HI_U32 g_au32LinesPer500ms[ISP_MAX_PIPE_NUM]   = {0};
static HI_U16 g_au16InitWBGain[ISP_MAX_PIPE_NUM][3] = {{0}};
static HI_U16 g_au16SampleRgain[ISP_MAX_PIPE_NUM] = {0};
static HI_U16 g_au16SampleBgain[ISP_MAX_PIPE_NUM] = {0};

ISP_SNS_COMMBUS_U g_aunNaneyemBusInfo[ISP_MAX_PIPE_NUM] =
{
    [0] = { .s8I2cDev = 0},
    [1 ... ISP_MAX_PIPE_NUM - 1] = { .s8I2cDev = -1}
};

static ISP_FSWDR_MODE_E genFSWDRMode[ISP_MAX_PIPE_NUM] =
{
    [0 ... ISP_MAX_PIPE_NUM - 1] = ISP_FSWDR_NORMAL_MODE
};

static HI_U32 gu32MaxTimeGetCnt[ISP_MAX_PIPE_NUM] = {0};

typedef struct hiNANEYEM_STATE_S
{
    HI_U32      u32BRL;
    HI_U32      u32RHS1_MAX;
    HI_U32      u32deltaRHS1;
} NANEYEM_STATE_S;

NANEYEM_STATE_S g_astnaneyemState[ISP_MAX_PIPE_NUM] = {{0}};


/****************************************************************************
 * extern                                                                   *
 ****************************************************************************/
extern unsigned char naneyem_i2c_addr;
extern unsigned int  naneyem_addr_byte;
extern unsigned int  naneyem_data_byte;

extern void naneyem_init(VI_PIPE ViPipe);
extern void naneyem_exit(VI_PIPE ViPipe);
extern void naneyem_standby(VI_PIPE ViPipe);
extern void naneyem_restart(VI_PIPE ViPipe);

#ifdef __DEBUG__
static int  naneyem_write_register(VI_PIPE ViPipe, int addr, int data)
{
    printf("fake naneyem_write_register, addr = #%x data = #%x\n", addr, data);
    return 0;
}
#else
extern int  naneyem_write_register(VI_PIPE ViPipe, int addr, int data);
#endif
extern int  naneyem_read_register(VI_PIPE ViPipe, int addr);

/****************************************************************************
 * local variables                                                            *
 ****************************************************************************/
#define NANEYEM_FULL_LINES_MAX  (0xFFFF)

#define VMAX_ADDR_H             (0x2e)
#define VMAX_ADDR_L             (0x2d)
#define AE_CTRL_H               (0x10)
#define AE_CTRL_L               (0x16)
#define GAIN_ADDR               (0x00)
#define HTP_ADDR_H              (0x2b)
#define HTP_ADDR_L              (0x2a)

#define NANEYEM_INCREASE_LINES (0) /* make real fps less than stand fps because NVR require*/

#define NANEYEM_VMAX_320P24_LINEAR  (320+NANEYEM_INCREASE_LINES)

//sensor fps mode
#define NANEYEM_320P_24FPS_MODE      (1)

#define CMOS_NANEYEM_SLOW_FRAMERATE_MODE (0)

static HI_S32 cmos_get_ae_default(VI_PIPE ViPipe, AE_SENSOR_DEFAULT_S *pstAeSnsDft)
{
    ISP_SNS_STATE_S *pstSnsState = HI_NULL;

    CMOS_CHECK_POINTER(pstAeSnsDft);
    NANEYEM_SENSOR_GET_CTX(ViPipe, pstSnsState);
    CMOS_CHECK_POINTER(pstSnsState);

    memset(&pstAeSnsDft->stAERouteAttr, 0, sizeof(ISP_AE_ROUTE_S));

	pstAeSnsDft->u32LinesPer500ms = pstSnsState->u32FLStd*25/2;
    pstAeSnsDft->u32FullLinesStd = pstSnsState->u32FLStd;
    pstAeSnsDft->u32FlickerFreq = 0;
    pstAeSnsDft->u32FullLinesMax = NANEYEM_FULL_LINES_MAX;

    pstAeSnsDft->au8HistThresh[0] = 0xd;
    pstAeSnsDft->au8HistThresh[1] = 0x28;
    pstAeSnsDft->au8HistThresh[2] = 0x60;
    pstAeSnsDft->au8HistThresh[3] = 0x80;

    pstAeSnsDft->u8AeCompensation = 0x38;
    
    pstAeSnsDft->stIntTimeAccu.enAccuType = AE_ACCURACY_LINEAR;
    pstAeSnsDft->stIntTimeAccu.f32Accuracy = 1;
    pstAeSnsDft->stIntTimeAccu.f32Offset = 0;
    pstAeSnsDft->u32MaxIntTime = pstSnsState->u32FLStd - 2;
    pstAeSnsDft->u32MinIntTime = 1;
    pstAeSnsDft->u32MaxIntTimeTarget = 65535;
    pstAeSnsDft->u32MinIntTimeTarget = 1;

    pstAeSnsDft->stAgainAccu.enAccuType = AE_ACCURACY_DB;
    pstAeSnsDft->stAgainAccu.f32Accuracy = 6;
    pstAeSnsDft->u32MaxAgain = 4;/* 1, 2, 4, ... 16 (0~24db, unit is 6db) */
    pstAeSnsDft->u32MinAgain = 0;
    pstAeSnsDft->u32MaxAgainTarget = pstAeSnsDft->u32MaxAgain;
    pstAeSnsDft->u32MinAgainTarget = pstAeSnsDft->u32MinAgain;

    pstAeSnsDft->stDgainAccu.enAccuType = AE_ACCURACY_LINEAR;
    pstAeSnsDft->stDgainAccu.f32Accuracy = 0.0625;
	/* 1 ~ 31/16, unit is 1/16 */
    pstAeSnsDft->u32MaxDgain = 31;  /* if Dgain enable,please set ispdgain bigger than 1*/
    pstAeSnsDft->u32MinDgain = 1;
    pstAeSnsDft->u32MaxDgainTarget = pstAeSnsDft->u32MaxDgain;
    pstAeSnsDft->u32MinDgainTarget = pstAeSnsDft->u32MinDgainTarget;

    pstAeSnsDft->u32ISPDgainShift = 8;
    pstAeSnsDft->u32MinISPDgainTarget = 1 << pstAeSnsDft->u32ISPDgainShift;
    pstAeSnsDft->u32MaxISPDgainTarget = 4 << pstAeSnsDft->u32ISPDgainShift;

    pstAeSnsDft->u32LinesPer500ms = pstSnsState->u32FLStd * 30 / 2;

    //memcpy(&pstAeSnsDft->stPirisAttr, &gstPirisAttr, sizeof(ISP_PIRIS_ATTR_S));
    pstAeSnsDft->enMaxIrisFNO = ISP_IRIS_F_NO_1_4;
    pstAeSnsDft->enMinIrisFNO = ISP_IRIS_F_NO_5_6;

    pstAeSnsDft->bAERouteExValid = HI_FALSE;
    pstAeSnsDft->stAERouteAttr.u32TotalNum = 0;
    pstAeSnsDft->stAERouteAttrEx.u32TotalNum = 0;

    /*For some OV sensors, AERunInterval needs to be set more than 1*/    
    pstAeSnsDft->u8AERunInterval = 2;

    return HI_SUCCESS;
}

/* the function of sensor set fps */
static HI_VOID cmos_fps_set(VI_PIPE ViPipe, HI_FLOAT f32Fps, AE_SENSOR_DEFAULT_S *pstAeSnsDft)
{
    HI_U32 u32MaxFps;
    HI_U32 u32Lines;
    ISP_SNS_STATE_S *pstSnsState = HI_NULL;
    HI_U32 tp = 320;    
    HI_U32 u32VblankingLines = NANEYEM_FULL_LINES_MAX;

    CMOS_CHECK_POINTER_VOID(pstAeSnsDft);
    NANEYEM_SENSOR_GET_CTX(ViPipe, pstSnsState);
    CMOS_CHECK_POINTER_VOID(pstSnsState);

    switch (pstSnsState->u8ImgMode)
    {
        case NANEYEM_320P_24FPS_MODE:
            u32MaxFps = 30;
            if ((f32Fps <= 30) && (f32Fps >= 6.5))
            {
                //u32Lines = NANEYEM_VMAX_320P24_LINEAR * u32MaxFps / DIV_0_TO_1_FLOAT(f32Fps);
                u32VblankingLines = tp * 24 / f32Fps;
            }
            else
            {
                ISP_TRACE(HI_DBG_ERR, "Not support Fps: %f\n", f32Fps);
                return;
            }
            //u32Lines = (u32Lines > NANEYEM_FULL_LINES_MAX) ? NANEYEM_FULL_LINES_MAX : u32Lines;
            break;

        default:
            ISP_TRACE(HI_DBG_ERR, "Not support ImgMode: %d\n", pstSnsState->u8ImgMode);
            return;
    }

//	    if (WDR_MODE_NONE == pstSnsState->enWDRMode)
//	    {
//	        pstSnsState->astRegsInfo[0].astI2cData[3].u32Data = LOW_8BITS(u32Lines);
//	        pstSnsState->astRegsInfo[0].astI2cData[4].u32Data = HIGH_8BITS(u32Lines);
//	    }

    pstSnsState->astRegsInfo[0].astI2cData[3].u32Data = LOW_8BITS(u32VblankingLines);
    pstSnsState->astRegsInfo[0].astI2cData[4].u32Data = HIGH_8BITS(u32VblankingLines);


    //pstSnsState->u32FLStd = u32Lines;
    pstSnsState->u32FLStd = NANEYEM_VMAX_320P24_LINEAR;
    pstAeSnsDft->f32Fps = f32Fps;
    pstAeSnsDft->u32LinesPer500ms = pstSnsState->u32FLStd * f32Fps / 2;
    pstAeSnsDft->u32FullLinesStd = pstSnsState->u32FLStd;
    pstAeSnsDft->u32MaxIntTime = 320;
    pstSnsState->au32FL[0] = pstSnsState->u32FLStd;
    pstAeSnsDft->u32FullLines = pstSnsState->au32FL[0];

    return;
}

static HI_VOID cmos_slow_framerate_set(VI_PIPE ViPipe, HI_U32 u32FullLines, AE_SENSOR_DEFAULT_S *pstAeSnsDft)
{
    ISP_SNS_STATE_S *pstSnsState = HI_NULL;
    
    CMOS_CHECK_POINTER_VOID(pstAeSnsDft);
    NANEYEM_SENSOR_GET_CTX(ViPipe, pstSnsState);
    CMOS_CHECK_POINTER_VOID(pstSnsState);

#if CMOS_NANEYEM_SLOW_FRAMERATE_MODE
/*Mode 1: slow framerate by add the time of each line*/
	HI_U32 u32Tp = 50760;   /* (0x69c * 30) = 50760*/
	HI_U16 u16SlowFrameRate = (u32FullLines << 8) / pstSnsState->u32FLStd;
	u32Tp = (((u32Tp * u16SlowFrameRate) / 30) >> 8) + 3;
	if (u32Tp > 0x2000)     /* the register 0x2a adn 0x2b's max value is 0x2000 */
    {
        u32Tp = 0x2000;
        u16SlowFrameRate = ((gu8Fps * u32Tp) << 8) / 50760;
        printf("Warning! The slow_framerate is out of naneyem's range!\n");
    }
    
    pstAeSnsDft->u32LinesPer500ms = pstSnsState->u32FLStd * (30 << 8) / (2*u16SlowFrameRate);
    pstAeSnsDft->u32MaxIntTime = ((pstSnsState->u32FLStd * u16SlowFrameRate) >> 8) - 4;
    pstAeSnsDft->u32FullLines = pstSnsState->u32FLStd;
    
    pstSnsState->astRegsInfo[0].astI2cData[3].u32Data = u32Tp & 0xfc;
    pstSnsState->astRegsInfo[0].astI2cData[4].u32Data = (u32Tp & 0xff00) >> 8;
    
#else
/*Mode 2:slow framerate by add the lines of each time*/
	HI_U32 u32VblankingLines;
    static HI_U32 u32LastVblankingLines = 0;



    pstSnsState->au32FL[0] = (u32FullLines > NANEYEM_FULL_LINES_MAX) ? NANEYEM_FULL_LINES_MAX : u32FullLines;
    pstAeSnsDft->u32FullLines = pstSnsState->au32FL[0];
        
    u32VblankingLines=pstSnsState->au32FL[0] - pstSnsState->u32FLStd;
    if ( u32LastVblankingLines < u32VblankingLines)
    {
        pstSnsState->astRegsInfo[0].astI2cData[5].u8DelayFrmNum = 0;
        pstSnsState->astRegsInfo[0].astI2cData[6].u8DelayFrmNum = 0;
    }
    else
    {
        pstSnsState->astRegsInfo[0].astI2cData[5].u8DelayFrmNum = 1;
        pstSnsState->astRegsInfo[0].astI2cData[6].u8DelayFrmNum = 1;
    }

    pstSnsState->astRegsInfo[0].astI2cData[5].u32Data = LOW_8BITS(pstSnsState->au32FL[0]);
    pstSnsState->astRegsInfo[0].astI2cData[6].u32Data = HIGH_8BITS(pstSnsState->au32FL[0]);

	u32LastVblankingLines = u32VblankingLines;
    
    pstAeSnsDft->u32MaxIntTime = pstSnsState->au32FL[0] - 4;
#endif

    return;
}

/* while isp notify ae to update sensor regs, ae call these funcs. */
static HI_VOID cmos_inttime_update(VI_PIPE ViPipe, HI_U32 u32IntTime)
{
    ISP_SNS_STATE_S *pstSnsState = HI_NULL;
    static HI_BOOL bFirst[ISP_MAX_PIPE_NUM] = {[0 ... (ISP_MAX_PIPE_NUM - 1)] = 1};
    HI_U32 u32Value = 0;

    static HI_U32 u32ShortIntTime[ISP_MAX_PIPE_NUM] = {0};
    static HI_U32 u32LongIntTime[ISP_MAX_PIPE_NUM] = {0};

    static HI_U32 u32RHS1[ISP_MAX_PIPE_NUM]  = {0};
    static HI_U32 u32LastRHS1[ISP_MAX_PIPE_NUM]  = {0};
    static HI_U32 u32deltaRHS1[ISP_MAX_PIPE_NUM]  = {0};
    static HI_U32 u32SHR0[ISP_MAX_PIPE_NUM]  = {0};
    static HI_U32 u32SHR1[ISP_MAX_PIPE_NUM]  = {0};
    static HI_U32 u32YOUTSIZE[ISP_MAX_PIPE_NUM] = {0};

    NANEYEM_SENSOR_GET_CTX(ViPipe, pstSnsState);
    CMOS_CHECK_POINTER_VOID(pstSnsState);

    //u32Value = pstSnsState->au32FL[0] - u32IntTime;
    //u32Value = MIN(u32Value, 0xfffff);
    //u32Value = MIN(MAX(u32Value, 5), pstSnsState->au32FL[0] - 2);
    //pstSnsState->astRegsInfo[0].astI2cData[0].u32Data = LOW_8BITS(u32Value);
    //pstSnsState->astRegsInfo[0].astI2cData[1].u32Data = HIGH_8BITS(u32Value);

    pstSnsState->astRegsInfo[0].astI2cData[0].u32Data = LOW_8BITS(u32IntTime);
    pstSnsState->astRegsInfo[0].astI2cData[1].u32Data = HIGH_8BITS(u32IntTime);
    bFirst[ViPipe] = HI_TRUE;

    return;
}

static HI_VOID cmos_again_calc_table(VI_PIPE ViPipe, HI_U32 *pu32AgainLin, HI_U32 *pu32AgainDb)
{
    *pu32AgainDb = (*pu32AgainLin >> 3);
    
    return;
}

static HI_VOID cmos_dgain_calc_table(VI_PIPE ViPipe, HI_U32 *pu32DgainLin, HI_U32 *pu32DgainDb)
{
    //*pu32AgainLin = (*pu32AgainLin >> 10) << 10;
    return;
}

static HI_VOID cmos_gains_update(VI_PIPE ViPipe, HI_U32 u32Again, HI_U32 u32Dgain)
{
    ISP_SNS_STATE_S *pstSnsState = HI_NULL;
    HI_U8 u8High, u8Low;

    NANEYEM_SENSOR_GET_CTX(ViPipe, pstSnsState);
    CMOS_CHECK_POINTER_VOID(pstSnsState);
    
    switch (u32Again)
    {
        case 0 :    /* 0db, 1 multiplies */
            u8High = 0x00;
            break;
        case 1 :    /* 6db, 2 multiplies */
            u8High = 0x10;
            break;
        case 2 :    /* 12db, 4 multiplies */
            u8High = 0x30;
            break;
        case 3 :    /* 18db, 8 multiplies */
            u8High = 0x70;
            break;
        case 4 :    /* 24db, 16 multiplies */
            u8High = 0xf0;
            break;
        default:
            u8High = 0x00;
            break;
    }
    
    u8Low = (u32Dgain - 16) & 0xf;

    pstSnsState->astRegsInfo[0].astI2cData[2].u32Data = (u8High | u8Low);
    return;
}

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
    NANEYEM_SENSOR_GET_CTX(ViPipe, pstSnsState);
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
            u32IntTimeMaxTmp = (u32IntTimeMaxTmp > (g_astnaneyemState[ViPipe].u32RHS1_MAX - 2)) ? (g_astnaneyemState[ViPipe].u32RHS1_MAX - 2) : u32IntTimeMaxTmp;
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

//awb static param for Lens New IR_Cut
#define CALIBRATE_STATIC_WB_R_GAIN  0x17e
#define CALIBRATE_STATIC_WB_GR_GAIN 0x100
#define CALIBRATE_STATIC_WB_GB_GAIN 0x100
#define CALIBRATE_STATIC_WB_B_GAIN  0x179

/* Calibration results for Auto WB Planck */
#define CALIBRATE_AWB_P1 95
#define CALIBRATE_AWB_P2 45
#define CALIBRATE_AWB_Q1 -116
#define CALIBRATE_AWB_A1 182769
#define CALIBRATE_AWB_B1 128
#define CALIBRATE_AWB_C1 -132156

/* Rgain and Bgain of the golden sample */
#define GOLDEN_RGAIN 0
#define GOLDEN_BGAIN 0
static HI_S32 cmos_get_awb_default(VI_PIPE ViPipe, AWB_SENSOR_DEFAULT_S *pstAwbSnsDft)
{
    ISP_SNS_STATE_S *pstSnsState = HI_NULL;

    CMOS_CHECK_POINTER(pstAwbSnsDft);
    NANEYEM_SENSOR_GET_CTX(ViPipe, pstSnsState);
    CMOS_CHECK_POINTER(pstSnsState);

    memset(pstAwbSnsDft, 0, sizeof(AWB_SENSOR_DEFAULT_S));

    pstAwbSnsDft->u16WbRefTemp = 5120;

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
    NANEYEM_SENSOR_GET_CTX(ViPipe, pstSnsState);
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
    }

    //pstDef->stSensorMaxResolution.u32MaxWidth  = 1280;
    //pstDef->stSensorMaxResolution.u32MaxHeight = 720;
    
    pstDef->stSensorMode.u32SensorID = NANEYEM_ID;
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
    NANEYEM_SENSOR_GET_CTX(ViPipe, pstSnsState);
    CMOS_CHECK_POINTER(pstSnsState);

    /* Don't need to update black level when iso change */
    pstBlackLevel->bUpdate = HI_FALSE;

    pstBlackLevel->au16BlackLevel[0] = 0x46;
    pstBlackLevel->au16BlackLevel[1] = 0x45;
    pstBlackLevel->au16BlackLevel[2] = 0x46;
    pstBlackLevel->au16BlackLevel[3] = 0x46;

    return HI_SUCCESS;
}

static HI_VOID cmos_set_pixel_detect(VI_PIPE ViPipe, HI_BOOL bEnable)
{
    HI_U32 u32FullLines_5Fps, u32MaxIntTime_5Fps;
    ISP_SNS_STATE_S *pstSnsState = HI_NULL;

    NANEYEM_SENSOR_GET_CTX(ViPipe, pstSnsState);
    CMOS_CHECK_POINTER_VOID(pstSnsState);

    if (WDR_MODE_NONE == pstSnsState->enWDRMode)
    {
        if (NANEYEM_320P_24FPS_MODE == pstSnsState->u8ImgMode)
        {
            u32FullLines_5Fps = (NANEYEM_VMAX_320P24_LINEAR * 24) / 5;
            //u32FullLines_5Fps = (NANEYEM_VMAX_320P24_LINEAR * 25) / 5;
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

    u32MaxIntTime_5Fps = u32FullLines_5Fps - 4;

    if (bEnable) /* setup for ISP pixel calibration mode */
    {
		/* 5 fps */
        naneyem_write_register(ViPipe, VMAX_ADDR_L, LOW_8BITS(u32FullLines_5Fps));
        naneyem_write_register(ViPipe, VMAX_ADDR_H, HIGH_8BITS(u32FullLines_5Fps));

		/* min gain */
		naneyem_write_register(ViPipe, GAIN_ADDR, 0x00);
		
		/* max exposure time*/
        naneyem_write_register(ViPipe, AE_CTRL_L, LOW_8BITS(u32MaxIntTime_5Fps));
        naneyem_write_register(ViPipe, AE_CTRL_H, HIGH_8BITS(u32MaxIntTime_5Fps));
    }
    else /* setup for ISP 'normal mode' */
    {
        //naneyem_write_register (ViPipe, VMAX_ADDR_L, 0x0);
        //naneyem_write_register (ViPipe, VMAX_ADDR_H, 0x0);
        naneyem_write_register (ViPipe, VMAX_ADDR_L, LOW_8BITS(pstSnsState->u32FLStd));
        naneyem_write_register (ViPipe, VMAX_ADDR_H, HIGH_8BITS(pstSnsState->u32FLStd));
        pstSnsState->bSyncInit = HI_FALSE ;
    }
    return;
}

static HI_S32 cmos_set_wdr_mode(VI_PIPE ViPipe, HI_U8 u8Mode)
{
    ISP_SNS_STATE_S *pstSnsState = HI_NULL;

    NANEYEM_SENSOR_GET_CTX(ViPipe, pstSnsState);
    CMOS_CHECK_POINTER(pstSnsState);

    pstSnsState->bSyncInit = HI_FALSE;

    switch (u8Mode)
    {
        case WDR_MODE_NONE:
            pstSnsState->enWDRMode = WDR_MODE_NONE;
            pstSnsState->u8ImgMode = NANEYEM_320P_24FPS_MODE;
            pstSnsState->u32FLStd = NANEYEM_VMAX_320P24_LINEAR ;
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
    NANEYEM_SENSOR_GET_CTX(ViPipe, pstSnsState);
    CMOS_CHECK_POINTER(pstSnsState);

    if ((HI_FALSE == pstSnsState->bSyncInit) || (HI_FALSE == pstSnsRegsInfo->bConfig))
    {
        pstSnsState->astRegsInfo[0].enSnsType = ISP_SNS_I2C_TYPE;
        pstSnsState->astRegsInfo[0].unComBus.s8I2cDev = g_aunNaneyemBusInfo[ViPipe].s8I2cDev;
        pstSnsState->astRegsInfo[0].u8Cfg2ValidDelayMax = 2;
    #ifdef __DEBUG__
        pstSnsState->astRegsInfo[0].u32RegNum = 0;
    #else
        pstSnsState->astRegsInfo[0].u32RegNum = 5;
    #endif
        
#if(CMOS_NANEYEM_SLOW_FRAMERATE_MODE == 0)
    #ifdef __DEBUG__
        pstSnsState->astRegsInfo[0].u32RegNum = 0;
    #else
        pstSnsState->astRegsInfo[0].u32RegNum = 7;
    #endif
#endif		

        for (i = 0; i < pstSnsState->astRegsInfo[0].u32RegNum; i++)
        {
            pstSnsState->astRegsInfo[0].astI2cData[i].bUpdate = HI_TRUE;
            pstSnsState->astRegsInfo[0].astI2cData[i].u8DevAddr = naneyem_i2c_addr;
            pstSnsState->astRegsInfo[0].astI2cData[i].u32AddrByteNum = naneyem_addr_byte;
            pstSnsState->astRegsInfo[0].astI2cData[i].u32DataByteNum = naneyem_data_byte;
        }
        pstSnsState->astRegsInfo[0].astI2cData[0].u8DelayFrmNum = 0;
        pstSnsState->astRegsInfo[0].astI2cData[0].u32RegAddr = AE_CTRL_H;        //ae_ctrl_msb
        pstSnsState->astRegsInfo[0].astI2cData[1].u8DelayFrmNum = 0;
        pstSnsState->astRegsInfo[0].astI2cData[1].u32RegAddr = AE_CTRL_L;        //ae_ctrl_lsb
        pstSnsState->astRegsInfo[0].astI2cData[2].u8DelayFrmNum = 0;
        pstSnsState->astRegsInfo[0].astI2cData[2].u32RegAddr = GAIN_ADDR;        //dgain 
        
        pstSnsState->astRegsInfo[0].astI2cData[3].u8DelayFrmNum = 0;
        pstSnsState->astRegsInfo[0].astI2cData[3].u32RegAddr = HTP_ADDR_L;
        pstSnsState->astRegsInfo[0].astI2cData[4].u8DelayFrmNum = 0;
        pstSnsState->astRegsInfo[0].astI2cData[4].u32RegAddr = HTP_ADDR_H;

#if(CMOS_NANEYEM_SLOW_FRAMERATE_MODE == 0)
		pstSnsState->astRegsInfo[0].astI2cData[5].u8DelayFrmNum = 0;
        pstSnsState->astRegsInfo[0].astI2cData[5].u32RegAddr = VMAX_ADDR_L;
        pstSnsState->astRegsInfo[0].astI2cData[6].u8DelayFrmNum = 0;
        pstSnsState->astRegsInfo[0].astI2cData[6].u32RegAddr = VMAX_ADDR_H;
#endif  
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
    NANEYEM_SENSOR_GET_CTX(ViPipe, pstSnsState);
    CMOS_CHECK_POINTER(pstSnsState);

    u8SensorImageMode = pstSnsState->u8ImgMode;
    pstSnsState->bSyncInit = HI_FALSE;

    if (HI_NULL == pstSensorImageMode )
    {
        ISP_TRACE(HI_DBG_ERR, "null pointer when set image mode\n");
        return HI_FAILURE;
    }

    if ((pstSensorImageMode->u16Width <= 1296) && (pstSensorImageMode->u16Height <= 818))
    {
        if (WDR_MODE_NONE == pstSnsState->enWDRMode)
        {
            if (pstSensorImageMode->f32Fps <= 30)
            {
                u8SensorImageMode = NANEYEM_320P_24FPS_MODE;
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

    NANEYEM_SENSOR_GET_CTX(ViPipe, pstSnsState);
    CMOS_CHECK_POINTER_VOID(pstSnsState);

    pstSnsState->bInit = HI_FALSE;
    pstSnsState->bSyncInit = HI_FALSE;
    pstSnsState->u8ImgMode = NANEYEM_320P_24FPS_MODE;
    pstSnsState->enWDRMode = WDR_MODE_NONE;
    pstSnsState->u32FLStd = NANEYEM_VMAX_320P24_LINEAR;
    pstSnsState->au32FL[0] = NANEYEM_VMAX_320P24_LINEAR;
    pstSnsState->au32FL[1] = NANEYEM_VMAX_320P24_LINEAR;

    memset(&pstSnsState->astRegsInfo[0], 0, sizeof(ISP_SNS_REGS_INFO_S));
    memset(&pstSnsState->astRegsInfo[1], 0, sizeof(ISP_SNS_REGS_INFO_S));
}

static HI_S32 cmos_init_sensor_exp_function(ISP_SENSOR_EXP_FUNC_S *pstSensorExpFunc)
{
    CMOS_CHECK_POINTER(pstSensorExpFunc);

    memset(pstSensorExpFunc, 0, sizeof(ISP_SENSOR_EXP_FUNC_S));

    pstSensorExpFunc->pfn_cmos_sensor_init = naneyem_init;
    pstSensorExpFunc->pfn_cmos_sensor_exit = naneyem_exit;
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

static HI_S32 naneyem_set_bus_info(VI_PIPE ViPipe, ISP_SNS_COMMBUS_U unSNSBusInfo)
{
    g_aunNaneyemBusInfo[ViPipe].s8I2cDev = unSNSBusInfo.s8I2cDev;

    return HI_SUCCESS;
}

static HI_S32 sensor_ctx_init(VI_PIPE ViPipe)
{
    ISP_SNS_STATE_S *pastSnsStateCtx = HI_NULL;

    NANEYEM_SENSOR_GET_CTX(ViPipe, pastSnsStateCtx);

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

    NANEYEM_SENSOR_SET_CTX(ViPipe, pastSnsStateCtx);

    return HI_SUCCESS;
}

static HI_VOID sensor_ctx_exit(VI_PIPE ViPipe)
{
    ISP_SNS_STATE_S *pastSnsStateCtx = HI_NULL;

    NANEYEM_SENSOR_GET_CTX(ViPipe, pastSnsStateCtx);
    SENSOR_FREE(pastSnsStateCtx);
    NANEYEM_SENSOR_RESET_CTX(ViPipe);
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

    stSnsAttrInfo.eSensorId = NANEYEM_ID;

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

    s32Ret = HI_MPI_ISP_SensorUnRegCallBack(ViPipe, NANEYEM_ID);

    if (HI_SUCCESS != s32Ret)
    {
        ISP_TRACE(HI_DBG_ERR, "sensor unregister callback function failed!\n");
        return s32Ret;
    }

    s32Ret = HI_MPI_AE_SensorUnRegCallBack(ViPipe, pstAeLib, NANEYEM_ID);

    if (HI_SUCCESS != s32Ret)
    {
        ISP_TRACE(HI_DBG_ERR, "sensor unregister callback function to ae lib failed!\n");
        return s32Ret;
    }

    s32Ret = HI_MPI_AWB_SensorUnRegCallBack(ViPipe, pstAwbLib, NANEYEM_ID);

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


ISP_SNS_OBJ_S stSnsNaneyemObj =
{
    .pfnRegisterCallback    = sensor_register_callback,
    .pfnUnRegisterCallback  = sensor_unregister_callback,
    .pfnStandby             = naneyem_standby,
    .pfnRestart             = naneyem_restart,
    .pfnMirrorFlip          = HI_NULL,
    .pfnWriteReg            = naneyem_write_register,
    .pfnReadReg             = naneyem_read_register,
    .pfnSetBusInfo          = naneyem_set_bus_info,
    .pfnSetInit             = sensor_set_init
};

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */

#endif /* __NANEYEM_CMOS_H_ */