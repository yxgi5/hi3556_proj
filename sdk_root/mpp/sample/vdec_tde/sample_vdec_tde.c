
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <math.h>
#include <unistd.h>
#include <signal.h>

#include <fcntl.h>
#include <signal.h>


#include "hi_tde_api.h"
#include "hi_tde_type.h"
#include "hi_tde_errcode.h"
#include "hifb.h"
#include "hi_type.h"
#include "hi_comm_vo.h"
#include "mpi_sys.h"
#include "mpi_vo.h"
#include "sample_comm.h"

#define SAMPLE_STREAM_PATH "./source_file"

#define TDE_PRINT printf

#define MIN(x,y) ((x) > (y) ? (y) : (x))
#define MAX(x,y) ((x) > (y) ? (x) : (y))

#ifdef __HuaweiLite__
static const HI_CHAR *pszImageNames[] =
{
    "/nfs/res/apple.bits",
    "/nfs/res/applets.bits",
    "/nfs/res/calendar.bits",
    "/nfs/res/foot.bits",
    "/nfs/res/gmush.bits",
    "/nfs/res/gimp.bits",
    "/nfs/res/gsame.bits",
    "/nfs/res/keys.bits"
};
#define BACKGROUND_NAME  "/nfs/res/background.bits"
#else
static const HI_CHAR *pszImageNames[] =
{
    "res/apple.bits",
    "res/applets.bits",
    "res/calendar.bits",
    "res/foot.bits",
    "res/gmush.bits",
    "res/gimp.bits",
    "res/gsame.bits",
    "res/keys.bits"
};
#define BACKGROUND_NAME  "res/background.bits"
#endif

#define N_IMAGES (HI_S32)((sizeof (pszImageNames) / sizeof (pszImageNames[0])))

#define GRAPHICS_LAYER_G0      0
#define GRAPHICS_LAYER_G1      1
#define GRAPHICS_LAYER_G3      2

#define PIXFMT  TDE2_COLOR_FMT_ARGB1555
#define BPP     2
#define SCREEN_WIDTH    720
#define SCREEN_HEIGHT   576
#define CYCLE_LEN       60

//static HI_S32   g_s32FrameNum;
//static TDE2_SURFACE_S g_stScreen[2];
//static TDE2_SURFACE_S g_stBackGround;
//static TDE2_SURFACE_S g_stImgSur[N_IMAGES];
//HI_S32 g_s32Fd = -1;
//HI_U32 g_u32Size;
//HI_U8* g_pu8Screen = NULL;
//HI_U8* g_pu8BackGroundVir = NULL;


int IntType = 0;
pthread_t        g_stHifbThread = 0;
pthread_t        g_stHifbThread1= 0;

typedef struct hiPTHREAD_HIFB_SAMPLE
{
    HI_S32           fd;          /* fb's file describ */
    HI_S32           layer;       /* which graphic layer */
    HI_S32           ctrlkey;     /* {0,1,2,3}={1buffer, 2buffer, 0buffer pan display, 0buffer refresh} */
    HI_BOOL          bCompress;   /* image compressed or not */
    HIFB_COLOR_FMT_E enColorFmt;  /* color format. */
} PTHREAD_HIFB_SAMPLE_INFO;


#ifndef __HuaweiLite__
HI_VOID SAMPLE_VDEC_HandleSig(HI_S32 signo)
{
    if (SIGINT == signo || SIGTSTP == signo || SIGTERM == signo)
    {
        SAMPLE_COMM_SYS_Exit();
        printf("\033[0;31mprogram exit abnormally!\033[0;39m\n");
    }

    exit(0);
}
#endif

static HI_S32 TDE_CreateSurfaceByFile(const HI_CHAR *pszFileName, TDE2_SURFACE_S *pstSurface, HI_U8 *pu8Virt)
{
    FILE *fp;
    HI_U32 colorfmt, w, h, stride;
    HI_U64 packagelen;

    if((NULL == pszFileName) || (NULL == pstSurface))
    {
        TDE_PRINT("%s, LINE %d, NULL ptr!\n", __FUNCTION__, __LINE__);
        return -1;
    }

    fp = fopen(pszFileName, "rb");
    if(NULL == fp)
    {
        TDE_PRINT("error when open pszFileName %s, line:%d\n", pszFileName, __LINE__);
        return -1;
    }

    if (4 != fread(&colorfmt, 1, 4, fp))
    {
        TDE_PRINT("error when read pszFileName %s, line:%d\n", pszFileName, __LINE__);
        fclose(fp);
        return -1;
    }
    if (4 != fread(&w, 1, 4, fp))
    {
        TDE_PRINT("error when read pszFileName %s, line:%d\n", pszFileName, __LINE__);
        fclose(fp);
        return -1;
    }
    if (4 != fread(&h, 1, 4, fp))
    {
        TDE_PRINT("error when read pszFileName %s, line:%d\n", pszFileName, __LINE__);
        fclose(fp);
        return -1;
    }
    if (4 != fread(&stride, 1, 4, fp))
    {
        TDE_PRINT("error when read pszFileName %s, line:%d\n", pszFileName, __LINE__);
        fclose(fp);
        return -1;
    }

    pstSurface->enColorFmt = colorfmt;
    pstSurface->u32Width = w;
    pstSurface->u32Height = h;
    pstSurface->u32Stride = stride;
    pstSurface->u8Alpha0 = 0xff;
    pstSurface->u8Alpha1 = 0xff;
    pstSurface->bAlphaMax255 = HI_TRUE;
    pstSurface->bAlphaExt1555 = HI_TRUE;

    packagelen = (HI_U64)stride * (HI_U64)h;
    if ( packagelen > 0x7FFFFFFF)
    {
        TDE_PRINT("stride * h not valid: %d %d, line:%d\n", stride, h, __LINE__);
        fclose(fp);
        return -1;
    }

    fread(pu8Virt, 1, stride*h, fp);

    fclose(fp);

    return 0;
}

static HI_VOID circumrotate (HI_U32 u32CurOnShow, TDE2_SURFACE_S * p_stBackGround, \
                                TDE2_SURFACE_S * p_stScreen, TDE2_SURFACE_S *p_stImgSur)
{
    TDE_HANDLE s32Handle;
    TDE2_OPT_S stOpt = {0};
    HI_FLOAT eXMid, eYMid;
    HI_FLOAT eRadius;
    HI_U32 i;
    HI_FLOAT f;
    HI_U32 u32NextOnShow;
    TDE2_RECT_S stSrcRect;
    TDE2_RECT_S stDstRect;
    HI_S32 s32Ret = HI_SUCCESS;
    TDE2_SURFACE_S g_stBackGround = * p_stBackGround;
    HI_S32   g_s32FrameNum;
    
    u32NextOnShow = !u32CurOnShow;

    stOpt.enOutAlphaFrom = TDE2_COLORKEY_MODE_FOREGROUND;
    stOpt.unColorKeyValue.struCkARGB.stRed.u8CompMask = 0xff;
    stOpt.unColorKeyValue.struCkARGB.stGreen.u8CompMask = 0xff;
    stOpt.unColorKeyValue.struCkARGB.stBlue.u8CompMask = 0xff;
    stOpt.enColorKeyMode = TDE2_COLORKEY_MODE_FOREGROUND;
    stOpt.unColorKeyValue.struCkARGB.stAlpha.bCompIgnore = HI_TRUE;

    f = (float) (g_s32FrameNum % CYCLE_LEN) / CYCLE_LEN;

    stSrcRect.s32Xpos = 0;
    stSrcRect.s32Ypos = 0;
    stSrcRect.u32Width = g_stBackGround.u32Width;
    stSrcRect.u32Height = g_stBackGround.u32Height;

    eXMid = g_stBackGround.u32Width/2.16f;
    eYMid = g_stBackGround.u32Height/2.304f;

    eRadius = MIN (eXMid, eYMid) / 2.0f;

    /* 1. start job */
    s32Handle = HI_TDE2_BeginJob();
    if(HI_ERR_TDE_INVALID_HANDLE == s32Handle)
    {
        TDE_PRINT("start job failed!\n");
        return ;
    }

    /* 2. bitblt background to screen */
    s32Ret = HI_TDE2_QuickCopy(s32Handle, &g_stBackGround, &stSrcRect,
        p_stScreen+u32NextOnShow, &stSrcRect);
    if(s32Ret < 0)
    {
        TDE_PRINT("Line:%d failed,ret=0x%x!\n", __LINE__, s32Ret);
        HI_TDE2_CancelJob(s32Handle);
        return ;
    }

    for(i = 0; i < N_IMAGES; i++)
    {
        HI_FLOAT ang;
        HI_FLOAT r;

        stSrcRect.s32Xpos = 0;
        stSrcRect.s32Ypos = 0;
        stSrcRect.u32Width = (*(p_stImgSur+i)).u32Width;
        stSrcRect.u32Height = (*(p_stImgSur+i)).u32Height;

        /* 3. calculate new pisition */
        ang = 2.0f * (HI_FLOAT) M_PI * (HI_FLOAT) i / N_IMAGES - f * 2.0f * (HI_FLOAT) M_PI;
        r = eRadius + (eRadius / 3.0f) * sinf (f * 2.0 * M_PI);

        stDstRect.s32Xpos = eXMid + r * cosf (ang) - (*(p_stImgSur+i)).u32Width / 2.0f;;
        stDstRect.s32Ypos = eYMid + r * sinf (ang) - (*(p_stImgSur+i)).u32Height / 2.0f;
        stDstRect.u32Width = (*(p_stImgSur+i)).u32Width;
        stDstRect.u32Height = (*(p_stImgSur+i)).u32Height;

        /* 4. bitblt image to screen */
        s32Ret = HI_TDE2_Bitblit(s32Handle, p_stScreen+u32NextOnShow, &stDstRect,
            p_stImgSur+i, &stSrcRect, p_stScreen+u32NextOnShow, &stDstRect, &stOpt);
        if(s32Ret < 0)
        {
        	TDE_PRINT("Line:%d,HI_TDE2_Bitblit failed,ret=0x%x!\n", __LINE__, s32Ret);
        	HI_TDE2_CancelJob(s32Handle);
        	return ;
        }
    }

    /* 5. submit job */
    s32Ret = HI_TDE2_EndJob(s32Handle, HI_FALSE, HI_TRUE, 1000);
    if(s32Ret < 0)
    {
        TDE_PRINT("Line:%d,HI_TDE2_EndJob failed,ret=0x%x!\n", __LINE__, s32Ret);
        HI_TDE2_CancelJob(s32Handle);
        return ;
    }

    g_s32FrameNum++;
    return;
}


HI_S32 TDE_DrawGraphicSample(void* pData)
{
    HI_U32 u32Times;
    HI_U32 u32PhyAddr;
    HI_S32 s32Ret = -1;
    HI_U32 i = 0;
    HI_BOOL bShow,bCompress;
    HIFB_ALPHA_S stAlpha = {0};
    HI_CHAR                  file[12]           = {0};
    PTHREAD_HIFB_SAMPLE_INFO* pstInfo;
    HI_U32 u32Size;
    HI_U8* pu8Screen = NULL;
    TDE2_SURFACE_S g_stBackGround;
    TDE2_SURFACE_S g_stScreen[2];
    TDE2_SURFACE_S g_stImgSur[N_IMAGES];
    HI_U8* g_pu8BackGroundVir = NULL;

    if (HI_NULL == pData)
    {
        return HI_NULL;
    }
    pstInfo = (PTHREAD_HIFB_SAMPLE_INFO*)pData;

    struct fb_fix_screeninfo stFixInfo;
    struct fb_var_screeninfo stVarInfo;
    struct fb_bitfield stR32 = {10, 5, 0};
    struct fb_bitfield stG32 = {5, 5, 0};
    struct fb_bitfield stB32 = {0, 5, 0};
    struct fb_bitfield stA32 = {15, 1, 0};

    /* 1. open tde device */
    s32Ret = HI_TDE2_Open();
    if (HI_SUCCESS != s32Ret)
    {
        TDE_PRINT("HI_TDE2_Open failed:0x%x\n", s32Ret);
        return s32Ret;
    }

    SAMPLE_PRT("pstInfo->layer = %d\n", pstInfo->layer);

    switch (pstInfo->layer)
    {
        case GRAPHICS_LAYER_G0 :
            strncpy(file, "/dev/fb0", 12);
            break;
        case GRAPHICS_LAYER_G1 :
            strncpy(file, "/dev/fb1", 12);
            break;
        case GRAPHICS_LAYER_G3:
            strncpy(file, "/dev/fb2", 12);
            break;
        default:
            strncpy(file, "/dev/fb0", 12);
            break;
    }

    
    pstInfo->fd = open(file, O_RDWR);
    if (pstInfo->fd < 0)
    {
        TDE_PRINT("open frame buffer device error\n");
        goto FB_OPEN_ERROR;
    }

    bCompress = HI_FALSE ;
    if (ioctl(pstInfo->fd, FBIOPUT_COMPRESSION_HIFB, &bCompress) < 0)
    {
        TDE_PRINT(" FBIOPUT_COMPRESSION_HIFB failed!\n");
        close(pstInfo->fd);
        goto FB_PROCESS_ERROR2;
    }
    stAlpha.bAlphaChannel = HI_FALSE;
    stAlpha.bAlphaEnable = HI_FALSE;
    if (ioctl(pstInfo->fd, FBIOPUT_ALPHA_HIFB, &stAlpha) < 0)
    {
        TDE_PRINT("Put alpha info failed!\n");
        goto FB_PROCESS_ERROR0;
    }

    /* get the variable screen info */
    if (ioctl(pstInfo->fd, FBIOGET_VSCREENINFO, &stVarInfo) < 0)
    {
        TDE_PRINT("Get variable screen info failed!\n");
        goto FB_PROCESS_ERROR0;
    }

    stVarInfo.xres_virtual	 	= SCREEN_WIDTH;
    stVarInfo.yres_virtual		= SCREEN_HEIGHT*2;
    stVarInfo.xres      		= SCREEN_WIDTH;
    stVarInfo.yres      		= SCREEN_HEIGHT;
    stVarInfo.activate  		= FB_ACTIVATE_NOW;
    stVarInfo.bits_per_pixel	= 16;
    stVarInfo.xoffset = 0;
    stVarInfo.yoffset = 0;
    stVarInfo.red   = stR32;
    stVarInfo.green = stG32;
    stVarInfo.blue  = stB32;
    stVarInfo.transp = stA32;

    if (ioctl(pstInfo->fd, FBIOPUT_VSCREENINFO, &stVarInfo) < 0)
    {
        TDE_PRINT("process frame buffer device error\n");
        goto FB_PROCESS_ERROR0;
    }

    if (ioctl(pstInfo->fd, FBIOGET_FSCREENINFO, &stFixInfo) < 0)
    {
        TDE_PRINT("process frame buffer device error\n");
        goto FB_PROCESS_ERROR0;
    }

    u32Size   = stFixInfo.smem_len;
    u32PhyAddr  = stFixInfo.smem_start;
#ifdef __HuaweiLite__
    pu8Screen = stFixInfo.smem_start;
#else
    pu8Screen   = mmap(NULL, u32Size, PROT_READ|PROT_WRITE, MAP_SHARED, pstInfo->fd, 0);
    if (NULL == pu8Screen)
    {
        TDE_PRINT("mmap fb0 failed!\n");
        goto FB_PROCESS_ERROR0;
    }
#endif

#if 0
            GRAPHIC_LAYER            GraphicLayer     = {0};
            VO_CSC_S                 stGraphicCSC     = {0};
            s32Ret = HI_MPI_VO_GetGraphicLayerCSC(GraphicLayer, &stGraphicCSC);
            if (HI_SUCCESS != s32Ret)
            {
                SAMPLE_PRT("HI_MPI_VO_GetGraphicLayerCSC failed!\n");
                SAMPLE_COMM_VO_StopDev(VoDev);
                return s32Ret;
            }
            SAMPLE_PRT("stGraphicCSC.enCscMatrix = %d\n", stGraphicCSC.enCscMatrix);
#else       
        if ((IntType == VO_INTF_MIPI) && (pstInfo->layer != GRAPHICS_LAYER_G0))
        {
            GRAPHIC_LAYER            GraphicLayer     = {0};
            VO_CSC_S                 stGraphicCSC     = {0};
            GraphicLayer = pstInfo->layer; // if vodev = 1, then use GRAPHICS_LAYER_G1
            s32Ret = HI_MPI_VO_GetGraphicLayerCSC(GraphicLayer, &stGraphicCSC);
            if (HI_SUCCESS != s32Ret)
            {
                SAMPLE_PRT("HI_MPI_VO_GetGraphicLayerCSC failed!\n");
                return s32Ret;
            }
            SAMPLE_PRT("stGraphicCSC.enCscMatrix = %d\n", stGraphicCSC.enCscMatrix);

            #if 1
            stGraphicCSC.enCscMatrix =VO_CSC_MATRIX_IDENTITY;
            //stGraphicCSC.enCscMatrix = VO_CSC_MATRIX_RGB_TO_BT709_PC;
            //stGraphicCSC.enCscMatrix = VO_CSC_MATRIX_BT709_TO_RGB_PC;
            //stGraphicCSC.enCscMatrix = VO_CSC_MATRIX_BT601_TO_RGB_PC;
            s32Ret = HI_MPI_VO_SetGraphicLayerCSC(GraphicLayer, &stGraphicCSC);
            if (HI_SUCCESS != s32Ret)
            {
                SAMPLE_PRT("HI_MPI_VO_SetGraphicLayerCSC failed!\n");
                return s32Ret;
            }
            SAMPLE_PRT("stGraphicCSC.enCscMatrix = %d\n", stGraphicCSC.enCscMatrix);
            #endif
        }
#endif


    memset_s(pu8Screen, stFixInfo.smem_len, 0x00, stFixInfo.smem_len);
    /* 3. create surface */
    g_stScreen[0].enColorFmt = PIXFMT;
    g_stScreen[0].PhyAddr = u32PhyAddr;
    g_stScreen[0].u32Width = SCREEN_WIDTH;
    g_stScreen[0].u32Height = SCREEN_HEIGHT;
    g_stScreen[0].u32Stride = stFixInfo.line_length;
    g_stScreen[0].bAlphaMax255 = HI_TRUE;

    g_stScreen[1] = g_stScreen[0];
    g_stScreen[1].PhyAddr = g_stScreen[0].PhyAddr + (HI_U64)g_stScreen[0].u32Stride * (HI_U64)g_stScreen[0].u32Height;

    /* allocate memory (720*576*2*N_IMAGES bytes) to save Images' infornation */
    if (HI_FAILURE == HI_MPI_SYS_MmzAlloc(&(g_stBackGround.PhyAddr), ((void**)&g_pu8BackGroundVir),
            NULL, NULL, 720*576*2*N_IMAGES))
    {
        TDE_PRINT("allocate memory (720*576*2*N_IMAGES bytes) failed\n");
        goto FB_PROCESS_ERROR1;
    }
    TDE_CreateSurfaceByFile(BACKGROUND_NAME, &g_stBackGround, g_pu8BackGroundVir);

    g_stImgSur[0].PhyAddr = g_stBackGround.PhyAddr + (HI_U64)g_stBackGround.u32Stride * (HI_U64)g_stBackGround.u32Height;
    for(i = 0; i < N_IMAGES - 1; i++)
    {
        TDE_CreateSurfaceByFile(pszImageNames[i], &g_stImgSur[i],
            g_pu8BackGroundVir + ((HI_U32)g_stImgSur[i].PhyAddr - g_stBackGround.PhyAddr));
        g_stImgSur[i+1].PhyAddr = g_stImgSur[i].PhyAddr + (HI_U64)g_stImgSur[i].u32Stride * (HI_U64)g_stImgSur[i].u32Height;
    }
    TDE_CreateSurfaceByFile(pszImageNames[i], &g_stImgSur[i],
            g_pu8BackGroundVir + ((HI_U32)g_stImgSur[i].PhyAddr - g_stBackGround.PhyAddr));

    bShow = HI_TRUE;
    if (ioctl(pstInfo->fd, FBIOPUT_SHOW_HIFB, &bShow) < 0)
    {
        fprintf (stderr, "Couldn't show fb\n");
        goto FB_PROCESS_ERROR2;
    }

    //g_s32FrameNum = 0;

    /* 3. use tde and framebuffer to realize rotational effect */
    //for (u32Times = 0; u32Times < 20; u32Times++)
	    while(1)
	    {
    	    if ((IntType == VO_INTF_MIPI) && (pstInfo->layer != GRAPHICS_LAYER_G0))
            {
                GRAPHIC_LAYER            GraphicLayer     = {0};
                VO_CSC_S                 stGraphicCSC     = {0};
                GraphicLayer = pstInfo->layer; // if vodev = 1, then use GRAPHICS_LAYER_G1
                s32Ret = HI_MPI_VO_GetGraphicLayerCSC(GraphicLayer, &stGraphicCSC);
                if (HI_SUCCESS != s32Ret)
                {
                    SAMPLE_PRT("HI_MPI_VO_GetGraphicLayerCSC failed!\n");
                    return s32Ret;
                }
                SAMPLE_PRT("stGraphicCSC.enCscMatrix = %d\n", stGraphicCSC.enCscMatrix);
                
                stGraphicCSC.enCscMatrix =VO_CSC_MATRIX_IDENTITY;
                //stGraphicCSC.enCscMatrix = VO_CSC_MATRIX_RGB_TO_BT709_PC;
                s32Ret = HI_MPI_VO_SetGraphicLayerCSC(GraphicLayer, &stGraphicCSC);
                if (HI_SUCCESS != s32Ret)
                {
                    SAMPLE_PRT("HI_MPI_VO_SetGraphicLayerCSC failed!\n");
                    return s32Ret;
                }
                SAMPLE_PRT("stGraphicCSC.enCscMatrix = %d\n", stGraphicCSC.enCscMatrix);
            }
	        circumrotate(u32Times%2, &g_stBackGround, &g_stScreen, &g_stImgSur);
	        stVarInfo.yoffset = (u32Times%2)?0:576;
	
	        /*set frame buffer start position*/
	        if (ioctl(pstInfo->fd, FBIOPAN_DISPLAY, &stVarInfo) < 0)
	        {
	            TDE_PRINT("process frame buffer device error\n");
	            goto FB_PROCESS_ERROR2;
	        }
	        sleep(1);
	    }
    TDE_PRINT("exit\n");
    s32Ret = 0;

FB_PROCESS_ERROR2:
    HI_MPI_SYS_MmzFree(g_stBackGround.PhyAddr, g_pu8BackGroundVir);
    g_pu8BackGroundVir = NULL;
FB_PROCESS_ERROR1:
#ifndef __HuaweiLite__
    (HI_VOID)munmap(pu8Screen, u32Size);
#endif

    pu8Screen = NULL;
FB_PROCESS_ERROR0:
    close(pstInfo->fd);
    pstInfo->fd = -1;
FB_OPEN_ERROR:
    HI_TDE2_Close();

    return s32Ret;
}



HI_VOID SAMPLE_VDEC_Usage(char *sPrgNm)
{
    printf("\n/************************************/\n");
    printf("Usage : %s <index> <IntfSync >\n", sPrgNm);
    printf("index:\n");
    printf("\t0:  VDEC(H265)-VPSS-VO\n");
    printf("\t1:  VDEC(H264)-VPSS-VO\n");
    printf("\t2:  VDEC(JPEG->YUV)-VPSS-VO\n");
    printf("\t3:  VDEC(JPEG->RGB)\n");
    printf("\t4:  VDEC(H264)->VPSS-VO(DHD0-hdmi,DHD1-mipi_tx)\n");
    printf("\t5:  VDEC(H264)->VPSS-VO(DHD0-hdmi),VDEC(H265)->VPSS-VO(DHD1-mipi_tx)\n");
    printf("\t6:  VDEC(H265)->VPSS-VO\n");
    printf("\t7:  VDEC(H265)->VPSS-VO(DHD0-mipi_tx)\n");
    printf("\t8:  VDEC(H264)->VPSS-VO(DHD0-hdmi720p,DHD1-mipi_tx1280x800)\n");

    printf("\nIntfSync :\n");
    printf("\t0: VO HDMI 4K@30fps.\n");
    printf("\t1: VO HDMI 1080P@30fps.\n");
    printf("\t2: VO HDMI 1080P@60fps.\n");
    printf("/************************************/\n\n");
}

VO_INTF_SYNC_E g_enIntfSync = VO_OUTPUT_3840x2160_30;


HI_S32 SAMPLE_H265_VDEC_VPSS_VO(HI_VOID)
{
    VB_CONFIG_S stVbConfig;
    HI_S32 i, s32Ret = HI_SUCCESS;
    VDEC_THREAD_PARAM_S stVdecSend[VDEC_MAX_CHN_NUM];
    SIZE_S stDispSize;
    VO_LAYER VoLayer;
    HI_U32 u32VdecChnNum, VpssGrpNum;
    VPSS_GRP VpssGrp;
    pthread_t   VdecThread[2*VDEC_MAX_CHN_NUM];
    PIC_SIZE_E enDispPicSize;
    SAMPLE_VDEC_ATTR astSampleVdec[VDEC_MAX_CHN_NUM];
    VPSS_CHN_ATTR_S astVpssChnAttr[VPSS_MAX_CHN_NUM];
    SAMPLE_VO_CONFIG_S stVoConfig;
    VPSS_GRP_ATTR_S stVpssGrpAttr;
    HI_BOOL abChnEnable[VPSS_MAX_CHN_NUM];
    VO_INTF_SYNC_E enIntfSync;
    VPSS_MOD_PARAM_S stModParam;

    u32VdecChnNum = 1;
    VpssGrpNum    = u32VdecChnNum;
    /************************************************
    step1:  init SYS, init common VB(for VPSS and VO)
    *************************************************/

    if(VO_OUTPUT_3840x2160_30 == g_enIntfSync)
    {
        enDispPicSize = PIC_3840x2160;
        enIntfSync    = VO_OUTPUT_3840x2160_30;
    }
    else if(VO_OUTPUT_1080P30 == g_enIntfSync)
    {
        enDispPicSize = PIC_1080P;
        enIntfSync    = VO_OUTPUT_1080P30;
    }
    else if(VO_OUTPUT_1080P60 == g_enIntfSync)
    {
        enDispPicSize = PIC_1080P;
        enIntfSync    = VO_OUTPUT_1080P60;
    }

    s32Ret =  SAMPLE_COMM_SYS_GetPicSize(enDispPicSize, &stDispSize);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("sys get pic size fail for %#x!\n", s32Ret);
        goto END1;
    }

    memset(&stVbConfig, 0, sizeof(VB_CONFIG_S));
    stVbConfig.u32MaxPoolCnt             = 1;
    stVbConfig.astCommPool[0].u32BlkCnt  = 10*u32VdecChnNum;
    stVbConfig.astCommPool[0].u64BlkSize = COMMON_GetPicBufferSize(stDispSize.u32Width, stDispSize.u32Height,
                                                PIXEL_FORMAT_YVU_SEMIPLANAR_420, DATA_BITWIDTH_10, COMPRESS_MODE_SEG, 0);
    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConfig);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("init sys fail for %#x!\n", s32Ret);
        goto END1;
    }

    /************************************************
    step2:  init module VB or user VB(for VDEC)
    *************************************************/
    for(i=0; i<u32VdecChnNum; i++)
    {
        astSampleVdec[i].enType                           = PT_H265;
        astSampleVdec[i].u32Width                         = 3840;
        astSampleVdec[i].u32Height                        = 2160;
        astSampleVdec[i].enMode                           = VIDEO_MODE_FRAME;
        astSampleVdec[i].stSapmleVdecVideo.enDecMode      = VIDEO_DEC_MODE_IP;
        astSampleVdec[i].stSapmleVdecVideo.enBitWidth     = DATA_BITWIDTH_10;
        astSampleVdec[i].stSapmleVdecVideo.u32RefFrameNum = 3;
        astSampleVdec[i].u32DisplayFrameNum               = 2;
        astSampleVdec[i].u32FrameBufCnt = astSampleVdec[i].stSapmleVdecVideo.u32RefFrameNum + astSampleVdec[i].u32DisplayFrameNum + 1;
    }
    s32Ret = SAMPLE_COMM_VDEC_InitVBPool(u32VdecChnNum, &astSampleVdec[0]);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("init mod common vb fail for %#x!\n", s32Ret);
        goto END2;
    }

    /************************************************
    step3:  start VDEC
    *************************************************/
    s32Ret = SAMPLE_COMM_VDEC_Start(u32VdecChnNum, &astSampleVdec[0]);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("start VDEC fail for %#x!\n", s32Ret);
        goto END3;
    }

    /************************************************
    step4:  start VPSS
    *************************************************/
    s32Ret = HI_MPI_VPSS_GetModParam(&stModParam);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("Get VPSS module param fali %#x!\n", s32Ret);
        goto END4;
    }

    stModParam.bHdrSupport = HI_TRUE;

    s32Ret = HI_MPI_VPSS_SetModParam(&stModParam);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("Set VPSS module param fali %#x!\n", s32Ret);
        goto END4;
    }

    stVpssGrpAttr.u32MaxW = 3840;
    stVpssGrpAttr.u32MaxH = 2160;
    stVpssGrpAttr.stFrameRate.s32SrcFrameRate = -1;
    stVpssGrpAttr.stFrameRate.s32DstFrameRate = -1;
    stVpssGrpAttr.enDynamicRange = DYNAMIC_RANGE_HDR10;
    stVpssGrpAttr.enPixelFormat  = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    stVpssGrpAttr.bNrEn   = HI_FALSE;
    memset(abChnEnable, 0, sizeof(abChnEnable));
    abChnEnable[0] = HI_TRUE;
    astVpssChnAttr[0].u32Width                    = stDispSize.u32Width;
    astVpssChnAttr[0].u32Height                   = stDispSize.u32Height;
    astVpssChnAttr[0].enChnMode                   = VPSS_CHN_MODE_AUTO;
    astVpssChnAttr[0].enCompressMode              = COMPRESS_MODE_SEG;
    astVpssChnAttr[0].enDynamicRange              = DYNAMIC_RANGE_HDR10;
    astVpssChnAttr[0].enPixelFormat               = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    astVpssChnAttr[0].stFrameRate.s32SrcFrameRate = -1;
    astVpssChnAttr[0].stFrameRate.s32DstFrameRate = -1;
    astVpssChnAttr[0].u32Depth                    = 0;
    astVpssChnAttr[0].bMirror                     = HI_FALSE;
    astVpssChnAttr[0].bFlip                       = HI_FALSE;
    astVpssChnAttr[0].stAspectRatio.enMode        = ASPECT_RATIO_NONE;
    astVpssChnAttr[0].enVideoFormat               = VIDEO_FORMAT_LINEAR;
    for(i=0; i<u32VdecChnNum; i++)
    {
        VpssGrp = i;
        s32Ret = SAMPLE_COMM_VPSS_Start(VpssGrp, &abChnEnable[0], &stVpssGrpAttr, &astVpssChnAttr[0]);

        if (s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("start VPSS fail for %#x!\n", s32Ret);
            goto END4;
        }
    }


    /************************************************
    step5:  start VO
    *************************************************/
    stVoConfig.VoDev                 = SAMPLE_VO_DEV_UHD;
    stVoConfig.enVoIntfType          = VO_INTF_HDMI;
    stVoConfig.enIntfSync            = enIntfSync;
    stVoConfig.enPicSize             = enDispPicSize;
    stVoConfig.u32BgColor            = COLOR_RGB_BLUE;
    stVoConfig.u32DisBufLen          = 3;
    stVoConfig.enDstDynamicRange     = DYNAMIC_RANGE_HDR10;
    stVoConfig.enVoMode              = VO_MODE_1MUX;
    stVoConfig.enPixFormat           = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    stVoConfig.stDispRect.s32X       = 0;
    stVoConfig.stDispRect.s32Y       = 0;
    stVoConfig.stDispRect.u32Width   = stDispSize.u32Width;
    stVoConfig.stDispRect.u32Height  = stDispSize.u32Height;
    stVoConfig.stImageSize.u32Width  = stDispSize.u32Width;
    stVoConfig.stImageSize.u32Height = stDispSize.u32Height;
    stVoConfig.enVoPartMode          = VO_PART_MODE_SINGLE;

    s32Ret = SAMPLE_COMM_VO_StartVO(&stVoConfig);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("start VO fail for %#x!\n", s32Ret);
        goto END5;
    }

    /************************************************
    step6:  VDEC bind VPSS
    *************************************************/
    for(i=0; i<u32VdecChnNum; i++)
    {
        s32Ret = SAMPLE_COMM_VDEC_Bind_VPSS(i, i);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("vdec bind vpss fail for %#x!\n", s32Ret);
            goto END6;
        }
    }

    /************************************************
    step7:  VPSS bind VO
    *************************************************/
    VoLayer = stVoConfig.VoDev;
    for(i=0; i<VpssGrpNum; i++)
    {
        s32Ret = SAMPLE_COMM_VPSS_Bind_VO(i, 0, VoLayer, i);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("vpss bind vo fail for %#x!\n", s32Ret);
            goto END7;
        }
    }

    /************************************************
    step8:  send stream to VDEC
    *************************************************/
    for(i=0; i<u32VdecChnNum; i++)
    {
        snprintf(stVdecSend[i].cFileName, sizeof(stVdecSend[i].cFileName), "3840x2160_10bit.h265");
        snprintf(stVdecSend[i].cFilePath, sizeof(stVdecSend[i].cFilePath), "%s", SAMPLE_STREAM_PATH);
        stVdecSend[i].enType          = astSampleVdec[i].enType;
        stVdecSend[i].s32StreamMode   = astSampleVdec[i].enMode;
        stVdecSend[i].s32ChnId        = i;
        stVdecSend[i].s32IntervalTime = 1000;
        stVdecSend[i].u64PtsInit      = 0;
        stVdecSend[i].u64PtsIncrease  = 0;
        stVdecSend[i].eThreadCtrl     = THREAD_CTRL_START;
        stVdecSend[i].bCircleSend     = HI_TRUE;
        stVdecSend[i].s32MilliSec     = 0;
        stVdecSend[i].s32MinBufSize   = (astSampleVdec[i].u32Width * astSampleVdec[i].u32Height * 3)>>1;
    }
    SAMPLE_COMM_VDEC_StartSendStream(u32VdecChnNum, &stVdecSend[0], &VdecThread[0]);

    SAMPLE_COMM_VDEC_CmdCtrl(u32VdecChnNum, &stVdecSend[0], &VdecThread[0]);

    SAMPLE_COMM_VDEC_StopSendStream(u32VdecChnNum, &stVdecSend[0], &VdecThread[0]);

END7:
    for(i=0; i<VpssGrpNum; i++)
    {
        s32Ret = SAMPLE_COMM_VPSS_UnBind_VO(i, 0, VoLayer, i);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("vpss unbind vo fail for %#x!\n", s32Ret);
        }
    }

END6:
    for(i=0; i<u32VdecChnNum; i++)
    {
        s32Ret = SAMPLE_COMM_VDEC_UnBind_VPSS(i, i);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("vdec unbind vpss fail for %#x!\n", s32Ret);
        }
    }

END5:
    SAMPLE_COMM_VO_StopVO(&stVoConfig);

END4:
    for(i = VpssGrp; i >= 0; i--)
    {
        VpssGrp = i;
        SAMPLE_COMM_VPSS_Stop(VpssGrp, &abChnEnable[0]);
    }
END3:
    SAMPLE_COMM_VDEC_Stop(u32VdecChnNum);

END2:
    SAMPLE_COMM_VDEC_ExitVBPool();

END1:
    SAMPLE_COMM_SYS_Exit();

    return s32Ret;
}


HI_S32 SAMPLE_H265_VDEC_VPSS_VO2(HI_VOID)
{
    VB_CONFIG_S stVbConfig;
    HI_S32 i, s32Ret = HI_SUCCESS;
    VDEC_THREAD_PARAM_S stVdecSend[VDEC_MAX_CHN_NUM];
    SIZE_S stDispSize;
    VO_LAYER VoLayer;
    HI_U32 u32VdecChnNum, VpssGrpNum;
    VPSS_GRP VpssGrp;
    pthread_t   VdecThread[2*VDEC_MAX_CHN_NUM];
    PIC_SIZE_E enDispPicSize;
    SAMPLE_VDEC_ATTR astSampleVdec[VDEC_MAX_CHN_NUM];
    VPSS_CHN_ATTR_S astVpssChnAttr[VPSS_MAX_CHN_NUM];
    SAMPLE_VO_CONFIG_S stVoConfig;
    VPSS_GRP_ATTR_S stVpssGrpAttr;
    HI_BOOL abChnEnable[VPSS_MAX_CHN_NUM];
    VO_INTF_SYNC_E enIntfSync;
    VPSS_MOD_PARAM_S stModParam;

    u32VdecChnNum = 1;
    VpssGrpNum    = u32VdecChnNum;
    /************************************************
    step1:  init SYS, init common VB(for VPSS and VO)
    *************************************************/

    if(VO_OUTPUT_3840x2160_30 == g_enIntfSync)
    {
        enDispPicSize = PIC_3840x2160;
        enIntfSync    = VO_OUTPUT_3840x2160_30;
    }
    else if(VO_OUTPUT_1080P30 == g_enIntfSync)
    {
        enDispPicSize = PIC_1080P;
        enIntfSync    = VO_OUTPUT_1080P30;
    }
    else if(VO_OUTPUT_1080P60 == g_enIntfSync)
    {
        enDispPicSize = PIC_1080P;
        enIntfSync    = VO_OUTPUT_1080P60;
    }

    s32Ret =  SAMPLE_COMM_SYS_GetPicSize(enDispPicSize, &stDispSize);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("sys get pic size fail for %#x!\n", s32Ret);
        goto END1;
    }

    memset(&stVbConfig, 0, sizeof(VB_CONFIG_S));
    stVbConfig.u32MaxPoolCnt             = 1;
    stVbConfig.astCommPool[0].u32BlkCnt  = 10*u32VdecChnNum;
    stVbConfig.astCommPool[0].u64BlkSize = COMMON_GetPicBufferSize(stDispSize.u32Width, stDispSize.u32Height,
                                                PIXEL_FORMAT_YVU_SEMIPLANAR_420, DATA_BITWIDTH_8, COMPRESS_MODE_SEG, 0);
    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConfig);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("init sys fail for %#x!\n", s32Ret);
        goto END1;
    }

    /************************************************
    step2:  init module VB or user VB(for VDEC)
    *************************************************/
    for(i=0; i<u32VdecChnNum; i++)
    {
        astSampleVdec[i].enType                           = PT_H265;
        astSampleVdec[i].u32Width                         = 3840;
        astSampleVdec[i].u32Height                        = 2160;
        astSampleVdec[i].enMode                           = VIDEO_MODE_FRAME;
        astSampleVdec[i].stSapmleVdecVideo.enDecMode      = VIDEO_DEC_MODE_IPB;
        astSampleVdec[i].stSapmleVdecVideo.enBitWidth     = DATA_BITWIDTH_8;
        astSampleVdec[i].stSapmleVdecVideo.u32RefFrameNum = 3;
        astSampleVdec[i].u32DisplayFrameNum               = 2;
        astSampleVdec[i].u32FrameBufCnt = astSampleVdec[i].stSapmleVdecVideo.u32RefFrameNum + astSampleVdec[i].u32DisplayFrameNum + 1;
    }
    s32Ret = SAMPLE_COMM_VDEC_InitVBPool(u32VdecChnNum, &astSampleVdec[0]);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("init mod common vb fail for %#x!\n", s32Ret);
        goto END2;
    }

    /************************************************
    step3:  start VDEC
    *************************************************/
    s32Ret = SAMPLE_COMM_VDEC_Start(u32VdecChnNum, &astSampleVdec[0]);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("start VDEC fail for %#x!\n", s32Ret);
        goto END3;
    }

    /************************************************
    step4:  start VPSS
    *************************************************/
    s32Ret = HI_MPI_VPSS_GetModParam(&stModParam);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("Get VPSS module param fali %#x!\n", s32Ret);
        goto END4;
    }

    stModParam.bHdrSupport = HI_TRUE;

    s32Ret = HI_MPI_VPSS_SetModParam(&stModParam);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("Set VPSS module param fali %#x!\n", s32Ret);
        goto END4;
    }

    stVpssGrpAttr.u32MaxW = 3840;
    stVpssGrpAttr.u32MaxH = 2160;
    stVpssGrpAttr.stFrameRate.s32SrcFrameRate = -1;
    stVpssGrpAttr.stFrameRate.s32DstFrameRate = -1;
    stVpssGrpAttr.enDynamicRange = DYNAMIC_RANGE_SDR8;
    stVpssGrpAttr.enPixelFormat  = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    stVpssGrpAttr.bNrEn   = HI_FALSE;
    memset(abChnEnable, 0, sizeof(abChnEnable));
    abChnEnable[0] = HI_TRUE;
    astVpssChnAttr[0].u32Width                    = stDispSize.u32Width;
    astVpssChnAttr[0].u32Height                   = stDispSize.u32Height;
    astVpssChnAttr[0].enChnMode                   = VPSS_CHN_MODE_AUTO;
    astVpssChnAttr[0].enCompressMode              = COMPRESS_MODE_SEG;
    astVpssChnAttr[0].enDynamicRange              = DYNAMIC_RANGE_SDR8;
    astVpssChnAttr[0].enPixelFormat               = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    astVpssChnAttr[0].stFrameRate.s32SrcFrameRate = -1;
    astVpssChnAttr[0].stFrameRate.s32DstFrameRate = -1;
    astVpssChnAttr[0].u32Depth                    = 0;
    astVpssChnAttr[0].bMirror                     = HI_FALSE;
    astVpssChnAttr[0].bFlip                       = HI_FALSE;
    astVpssChnAttr[0].stAspectRatio.enMode        = ASPECT_RATIO_NONE;
    astVpssChnAttr[0].enVideoFormat               = VIDEO_FORMAT_LINEAR;
    for(i=0; i<u32VdecChnNum; i++)
    {
        VpssGrp = i;
        s32Ret = SAMPLE_COMM_VPSS_Start(VpssGrp, &abChnEnable[0], &stVpssGrpAttr, &astVpssChnAttr[0]);

        if (s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("start VPSS fail for %#x!\n", s32Ret);
            goto END4;
        }
    }


    /************************************************
    step5:  start VO
    *************************************************/
    stVoConfig.VoDev                 = SAMPLE_VO_DEV_UHD;
    stVoConfig.enVoIntfType          = VO_INTF_HDMI;
    stVoConfig.enIntfSync            = enIntfSync;
    stVoConfig.enPicSize             = enDispPicSize;
    stVoConfig.u32BgColor            = COLOR_RGB_BLUE;
    stVoConfig.u32DisBufLen          = 3;
    stVoConfig.enDstDynamicRange     = DYNAMIC_RANGE_SDR8;
    stVoConfig.enVoMode              = VO_MODE_1MUX;
    stVoConfig.enPixFormat           = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    stVoConfig.stDispRect.s32X       = 0;
    stVoConfig.stDispRect.s32Y       = 0;
    stVoConfig.stDispRect.u32Width   = stDispSize.u32Width;
    stVoConfig.stDispRect.u32Height  = stDispSize.u32Height;
    stVoConfig.stImageSize.u32Width  = stDispSize.u32Width;
    stVoConfig.stImageSize.u32Height = stDispSize.u32Height;
    stVoConfig.enVoPartMode          = VO_PART_MODE_SINGLE;

    s32Ret = SAMPLE_COMM_VO_StartVO(&stVoConfig);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("start VO fail for %#x!\n", s32Ret);
        goto END5;
    }

    /************************************************
    step6:  VDEC bind VPSS
    *************************************************/
    for(i=0; i<u32VdecChnNum; i++)
    {
        s32Ret = SAMPLE_COMM_VDEC_Bind_VPSS(i, i);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("vdec bind vpss fail for %#x!\n", s32Ret);
            goto END6;
        }
    }

    /************************************************
    step7:  VPSS bind VO
    *************************************************/
    VoLayer = stVoConfig.VoDev;
    for(i=0; i<VpssGrpNum; i++)
    {
        s32Ret = SAMPLE_COMM_VPSS_Bind_VO(i, 0, VoLayer, i);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("vpss bind vo fail for %#x!\n", s32Ret);
            goto END7;
        }
    }

    /************************************************
    step8:  send stream to VDEC
    *************************************************/
    for(i=0; i<u32VdecChnNum; i++)
    {
        snprintf(stVdecSend[i].cFileName, sizeof(stVdecSend[i].cFileName), "3840x2160_8bit.h265");
        snprintf(stVdecSend[i].cFilePath, sizeof(stVdecSend[i].cFilePath), "%s", SAMPLE_STREAM_PATH);
        stVdecSend[i].enType          = astSampleVdec[i].enType;
        stVdecSend[i].s32StreamMode   = astSampleVdec[i].enMode;
        stVdecSend[i].s32ChnId        = i;
        stVdecSend[i].s32IntervalTime = 1000;
        stVdecSend[i].u64PtsInit      = 0;
        stVdecSend[i].u64PtsIncrease  = 0;
        stVdecSend[i].eThreadCtrl     = THREAD_CTRL_START;
        stVdecSend[i].bCircleSend     = HI_TRUE;
        stVdecSend[i].s32MilliSec     = 0;
        stVdecSend[i].s32MinBufSize   = (astSampleVdec[i].u32Width * astSampleVdec[i].u32Height * 3)>>1;
    }
    SAMPLE_COMM_VDEC_StartSendStream(u32VdecChnNum, &stVdecSend[0], &VdecThread[0]);

    SAMPLE_COMM_VDEC_CmdCtrl(u32VdecChnNum, &stVdecSend[0], &VdecThread[0]);

    SAMPLE_COMM_VDEC_StopSendStream(u32VdecChnNum, &stVdecSend[0], &VdecThread[0]);

END7:
    for(i=0; i<VpssGrpNum; i++)
    {
        s32Ret = SAMPLE_COMM_VPSS_UnBind_VO(i, 0, VoLayer, i);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("vpss unbind vo fail for %#x!\n", s32Ret);
        }
    }

END6:
    for(i=0; i<u32VdecChnNum; i++)
    {
        s32Ret = SAMPLE_COMM_VDEC_UnBind_VPSS(i, i);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("vdec unbind vpss fail for %#x!\n", s32Ret);
        }
    }

END5:
    SAMPLE_COMM_VO_StopVO(&stVoConfig);

END4:
    for(i = VpssGrp; i >= 0; i--)
    {
        VpssGrp = i;
        SAMPLE_COMM_VPSS_Stop(VpssGrp, &abChnEnable[0]);
    }
END3:
    SAMPLE_COMM_VDEC_Stop(u32VdecChnNum);

END2:
    SAMPLE_COMM_VDEC_ExitVBPool();

END1:
    SAMPLE_COMM_SYS_Exit();

    return s32Ret;
}


HI_S32 SAMPLE_H264_VDEC_VPSS_VO(HI_VOID)
{
    VB_CONFIG_S stVbConfig;
    HI_S32 i, s32Ret = HI_SUCCESS;
    VDEC_THREAD_PARAM_S stVdecSend[VDEC_MAX_CHN_NUM];
    SIZE_S stDispSize;
    VO_LAYER VoLayer;
    HI_U32 u32VdecChnNum, VpssGrpNum;
    VPSS_GRP VpssGrp;
    pthread_t   VdecThread[2*VDEC_MAX_CHN_NUM];
    PIC_SIZE_E enDispPicSize;
    SAMPLE_VDEC_ATTR astSampleVdec[VDEC_MAX_CHN_NUM];
    VPSS_CHN_ATTR_S astVpssChnAttr[VPSS_MAX_CHN_NUM];
    SAMPLE_VO_CONFIG_S stVoConfig;
    VPSS_GRP_ATTR_S stVpssGrpAttr;
    HI_BOOL abChnEnable[VPSS_MAX_CHN_NUM];
    VO_INTF_SYNC_E enIntfSync;

    u32VdecChnNum = 1;
    VpssGrpNum    = u32VdecChnNum;

    /************************************************
    step1:  init SYS, init common VB(for VPSS and VO)
    *************************************************/
    if(VO_OUTPUT_3840x2160_30 == g_enIntfSync)
    {
        enDispPicSize = PIC_3840x2160;
        enIntfSync    = VO_OUTPUT_3840x2160_30;
    }
    else if(VO_OUTPUT_1080P30 == g_enIntfSync)
    {
        enDispPicSize = PIC_1080P;
        enIntfSync    = VO_OUTPUT_1080P30;
    }
    else if(VO_OUTPUT_1080P60 == g_enIntfSync)
    {
        enDispPicSize = PIC_1080P;
        enIntfSync    = VO_OUTPUT_1080P60;
    }

    s32Ret =  SAMPLE_COMM_SYS_GetPicSize(enDispPicSize, &stDispSize);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("sys get pic size fail for %#x!\n", s32Ret);
        goto END1;
    }

    memset(&stVbConfig, 0, sizeof(VB_CONFIG_S));
    stVbConfig.u32MaxPoolCnt             = 1;
    stVbConfig.astCommPool[0].u32BlkCnt  = 10*u32VdecChnNum;
    stVbConfig.astCommPool[0].u64BlkSize = COMMON_GetPicBufferSize(stDispSize.u32Width, stDispSize.u32Height,
                                                PIXEL_FORMAT_YVU_SEMIPLANAR_420, DATA_BITWIDTH_8, COMPRESS_MODE_SEG, 0);
    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConfig);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("init sys fail for %#x!\n", s32Ret);
        goto END1;
    }

    /************************************************
    step2:  init module VB or user VB(for VDEC)
    *************************************************/
    for(i=0; i<u32VdecChnNum; i++)
    {
        astSampleVdec[i].enType                           = PT_H264;
        astSampleVdec[i].u32Width                         = 3840;
        astSampleVdec[i].u32Height                        = 2160;
        astSampleVdec[i].enMode                           = VIDEO_MODE_FRAME;
        astSampleVdec[i].stSapmleVdecVideo.enDecMode      = VIDEO_DEC_MODE_IPB;
        astSampleVdec[i].stSapmleVdecVideo.enBitWidth     = DATA_BITWIDTH_8;
        astSampleVdec[i].stSapmleVdecVideo.u32RefFrameNum = 3;
        astSampleVdec[i].u32DisplayFrameNum               = 2;
        astSampleVdec[i].u32FrameBufCnt = astSampleVdec[i].stSapmleVdecVideo.u32RefFrameNum + astSampleVdec[i].u32DisplayFrameNum + 1;
    }
    s32Ret = SAMPLE_COMM_VDEC_InitVBPool(u32VdecChnNum, &astSampleVdec[0]);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("init mod common vb fail for %#x!\n", s32Ret);
        goto END2;
    }

    /************************************************
    step3:  start VDEC
    *************************************************/
    s32Ret = SAMPLE_COMM_VDEC_Start(u32VdecChnNum, &astSampleVdec[0]);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("start VDEC fail for %#x!\n", s32Ret);
        goto END3;
    }

    /************************************************
    step4:  start VPSS
    *************************************************/
    stVpssGrpAttr.u32MaxW = 3840;
    stVpssGrpAttr.u32MaxH = 2160;
    stVpssGrpAttr.stFrameRate.s32SrcFrameRate = -1;
    stVpssGrpAttr.stFrameRate.s32DstFrameRate = -1;
    stVpssGrpAttr.enDynamicRange = DYNAMIC_RANGE_SDR8;
    stVpssGrpAttr.enPixelFormat  = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    stVpssGrpAttr.bNrEn   = HI_FALSE;

    memset(abChnEnable, 0, sizeof(abChnEnable));
    abChnEnable[0] = HI_TRUE;
    astVpssChnAttr[0].u32Width                    = stDispSize.u32Width;
    astVpssChnAttr[0].u32Height                   = stDispSize.u32Height;
    astVpssChnAttr[0].enChnMode                   = VPSS_CHN_MODE_AUTO;
    astVpssChnAttr[0].enCompressMode              = COMPRESS_MODE_SEG;
    astVpssChnAttr[0].enDynamicRange              = DYNAMIC_RANGE_SDR8;
    astVpssChnAttr[0].enPixelFormat               = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    astVpssChnAttr[0].stFrameRate.s32SrcFrameRate = -1;
    astVpssChnAttr[0].stFrameRate.s32DstFrameRate = -1;
    astVpssChnAttr[0].u32Depth                    = 0;
    astVpssChnAttr[0].bMirror                     = HI_FALSE;
    astVpssChnAttr[0].bFlip                       = HI_FALSE;
    astVpssChnAttr[0].stAspectRatio.enMode        = ASPECT_RATIO_NONE;
    astVpssChnAttr[0].enVideoFormat               = VIDEO_FORMAT_LINEAR;
    for(i=0; i<u32VdecChnNum; i++)
    {
        VpssGrp = i;
        s32Ret = SAMPLE_COMM_VPSS_Start(VpssGrp, &abChnEnable[0], &stVpssGrpAttr, &astVpssChnAttr[0]);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("start VPSS fail for %#x!\n", s32Ret);
            goto END4;
        }
    }

    /************************************************
    step5:  start VO
    *************************************************/
    stVoConfig.VoDev                 = SAMPLE_VO_DEV_UHD;
    stVoConfig.enVoIntfType          = VO_INTF_HDMI;
    stVoConfig.enIntfSync            = enIntfSync;
    stVoConfig.enPicSize             = enDispPicSize;
    stVoConfig.u32BgColor            = COLOR_RGB_BLUE;
    stVoConfig.u32DisBufLen          = 3;
    stVoConfig.enDstDynamicRange     = DYNAMIC_RANGE_SDR8;
    stVoConfig.enVoMode              = VO_MODE_1MUX;
    stVoConfig.enPixFormat           = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    stVoConfig.stDispRect.s32X       = 0;
    stVoConfig.stDispRect.s32Y       = 0;
    stVoConfig.stDispRect.u32Width   = stDispSize.u32Width;
    stVoConfig.stDispRect.u32Height  = stDispSize.u32Height;
    stVoConfig.stImageSize.u32Width  = stDispSize.u32Width;
    stVoConfig.stImageSize.u32Height = stDispSize.u32Height;
    stVoConfig.enVoPartMode          = VO_PART_MODE_SINGLE;
    s32Ret = SAMPLE_COMM_VO_StartVO(&stVoConfig);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("start VO fail for %#x!\n", s32Ret);
        goto END5;
    }

    /************************************************
    step6:  VDEC bind VPSS
    *************************************************/
    for(i=0; i<u32VdecChnNum; i++)
    {
        s32Ret = SAMPLE_COMM_VDEC_Bind_VPSS(i, i);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("vdec bind vpss fail for %#x!\n", s32Ret);
            goto END6;
        }
    }

    /************************************************
    step7:  VPSS bind VO
    *************************************************/
    VoLayer = stVoConfig.VoDev;
    for(i=0; i<VpssGrpNum; i++)
    {
        s32Ret = SAMPLE_COMM_VPSS_Bind_VO(i, 0, VoLayer, i);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("vpss bind vo fail for %#x!\n", s32Ret);
            goto END7;
        }
    }

    /************************************************
    step8:  send stream to VDEC
    *************************************************/
    for(i=0; i<u32VdecChnNum; i++)
    {
        snprintf(stVdecSend[i].cFileName, sizeof(stVdecSend[i].cFileName), "3840x2160_8bit.h264");
        snprintf(stVdecSend[i].cFilePath, sizeof(stVdecSend[i].cFilePath), "%s", SAMPLE_STREAM_PATH);
        stVdecSend[i].enType          = astSampleVdec[i].enType;
        stVdecSend[i].s32StreamMode   = astSampleVdec[i].enMode;
        stVdecSend[i].s32ChnId        = i;
        stVdecSend[i].s32IntervalTime = 1000;
        stVdecSend[i].u64PtsInit      = 0;
        stVdecSend[i].u64PtsIncrease  = 0;
        stVdecSend[i].eThreadCtrl     = THREAD_CTRL_START;
        stVdecSend[i].bCircleSend     = HI_TRUE;
        stVdecSend[i].s32MilliSec     = 0;
        stVdecSend[i].s32MinBufSize   = (astSampleVdec[i].u32Width * astSampleVdec[i].u32Height * 3)>>1;
    }
    SAMPLE_COMM_VDEC_StartSendStream(u32VdecChnNum, &stVdecSend[0], &VdecThread[0]);

    SAMPLE_COMM_VDEC_CmdCtrl(u32VdecChnNum, &stVdecSend[0], &VdecThread[0]);

    SAMPLE_COMM_VDEC_StopSendStream(u32VdecChnNum, &stVdecSend[0], &VdecThread[0]);

END7:
    for(i=0; i<VpssGrpNum; i++)
    {
        s32Ret = SAMPLE_COMM_VPSS_UnBind_VO(i, 0, VoLayer, i);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("vpss unbind vo fail for %#x!\n", s32Ret);
        }
    }

END6:
    for(i=0; i<u32VdecChnNum; i++)
    {
        s32Ret = SAMPLE_COMM_VDEC_UnBind_VPSS(i, i);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("vdec unbind vpss fail for %#x!\n", s32Ret);
        }
    }

END5:
    SAMPLE_COMM_VO_StopVO(&stVoConfig);

END4:
    for(i = VpssGrp; i >= 0; i--)
    {
        VpssGrp = i;
        SAMPLE_COMM_VPSS_Stop(VpssGrp, &abChnEnable[0]);
    }
END3:
    SAMPLE_COMM_VDEC_Stop(u32VdecChnNum);

END2:
    SAMPLE_COMM_VDEC_ExitVBPool();

END1:
    SAMPLE_COMM_SYS_Exit();

    return s32Ret;
}




HI_S32 SAMPLE_JPEG_VDEC_VPSS_VO(HI_VOID)
{
    VB_CONFIG_S stVbConfig;
    HI_S32 i, s32Ret = HI_SUCCESS;
    VDEC_THREAD_PARAM_S stVdecSend[VDEC_MAX_CHN_NUM];
    SIZE_S stDispSize;
    VO_LAYER VoLayer;
    HI_U32 u32VdecChnNum, VpssGrpNum;
    VPSS_GRP VpssGrp;
    pthread_t   VdecThread[2*VDEC_MAX_CHN_NUM];
    PIC_SIZE_E enDispPicSize;
    SAMPLE_VDEC_ATTR astSampleVdec[VDEC_MAX_CHN_NUM];
    VPSS_CHN_ATTR_S astVpssChnAttr[VPSS_MAX_CHN_NUM];
    SAMPLE_VO_CONFIG_S stVoConfig;
    VPSS_GRP_ATTR_S stVpssGrpAttr;
    HI_BOOL abChnEnable[VPSS_MAX_CHN_NUM];
    VO_INTF_SYNC_E enIntfSync;

    u32VdecChnNum = 1;
    VpssGrpNum    = u32VdecChnNum;

    /************************************************
    step1:  init SYS, init common VB(for VPSS and VO)
    *************************************************/
    if(VO_OUTPUT_3840x2160_30 == g_enIntfSync)
    {
        enDispPicSize = PIC_3840x2160;
        enIntfSync    = VO_OUTPUT_3840x2160_30;
    }
    else if(VO_OUTPUT_1080P30 == g_enIntfSync)
    {
        enDispPicSize = PIC_1080P;
        enIntfSync    = VO_OUTPUT_1080P30;
    }
    else if(VO_OUTPUT_1080P60 == g_enIntfSync)
    {
        enDispPicSize = PIC_1080P;
        enIntfSync    = VO_OUTPUT_1080P60;
    }

    s32Ret =  SAMPLE_COMM_SYS_GetPicSize(enDispPicSize, &stDispSize);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("sys get pic size fail for %#x!\n", s32Ret);
        goto END1;
    }

    memset(&stVbConfig, 0, sizeof(VB_CONFIG_S));
    stVbConfig.u32MaxPoolCnt             = 1;
    stVbConfig.astCommPool[0].u32BlkCnt  = 10*u32VdecChnNum;
    stVbConfig.astCommPool[0].u64BlkSize = COMMON_GetPicBufferSize(stDispSize.u32Width, stDispSize.u32Height,
                                                PIXEL_FORMAT_YVU_SEMIPLANAR_420, DATA_BITWIDTH_8, COMPRESS_MODE_SEG, 0);
    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConfig);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("init sys fail for %#x!\n", s32Ret);
        goto END1;
    }

    /************************************************
    step2:  init module VB or user VB(for VDEC)
    *************************************************/
    for(i=0; i<u32VdecChnNum; i++)
    {
        astSampleVdec[i].enType                            = PT_JPEG;
        astSampleVdec[i].u32Width                          = 3840;
        astSampleVdec[i].u32Height                         = 2160;
        astSampleVdec[i].enMode                            = VIDEO_MODE_FRAME;
        astSampleVdec[i].stSapmleVdecPicture.enPixelFormat = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        astSampleVdec[i].stSapmleVdecPicture.u32Alpha      = 255;
        astSampleVdec[i].u32DisplayFrameNum                = 2;
        astSampleVdec[i].u32FrameBufCnt                    = astSampleVdec[i].u32DisplayFrameNum + 1;
    }
    s32Ret = SAMPLE_COMM_VDEC_InitVBPool(u32VdecChnNum, &astSampleVdec[0]);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("init mod common vb fail for %#x!\n", s32Ret);
        goto END2;
    }

    /************************************************
    step3:  start VDEC
    *************************************************/
    s32Ret = SAMPLE_COMM_VDEC_Start(u32VdecChnNum, &astSampleVdec[0]);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("start VDEC fail for %#x!\n", s32Ret);
        goto END3;
    }

    /************************************************
    step4:  start VPSS
    *************************************************/
    stVpssGrpAttr.u32MaxW = 3840;
    stVpssGrpAttr.u32MaxH = 2160;
    stVpssGrpAttr.stFrameRate.s32SrcFrameRate = -1;
    stVpssGrpAttr.stFrameRate.s32DstFrameRate = -1;
    stVpssGrpAttr.enDynamicRange = DYNAMIC_RANGE_SDR8;
    stVpssGrpAttr.enPixelFormat  = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    stVpssGrpAttr.bNrEn   = HI_FALSE;

    memset(abChnEnable, 0, sizeof(abChnEnable));
    abChnEnable[0] = HI_TRUE;
    astVpssChnAttr[0].u32Width                    = stDispSize.u32Width;
    astVpssChnAttr[0].u32Height                   = stDispSize.u32Height;
    astVpssChnAttr[0].enChnMode                   = VPSS_CHN_MODE_AUTO;
    astVpssChnAttr[0].enCompressMode              = COMPRESS_MODE_SEG;
    astVpssChnAttr[0].enDynamicRange              = DYNAMIC_RANGE_SDR8;
    astVpssChnAttr[0].enPixelFormat               = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    astVpssChnAttr[0].stFrameRate.s32SrcFrameRate = -1;
    astVpssChnAttr[0].stFrameRate.s32DstFrameRate = -1;
    astVpssChnAttr[0].u32Depth                    = 0;
    astVpssChnAttr[0].bMirror                     = HI_FALSE;
    astVpssChnAttr[0].bFlip                       = HI_FALSE;
    astVpssChnAttr[0].stAspectRatio.enMode        = ASPECT_RATIO_NONE;
    astVpssChnAttr[0].enVideoFormat               = VIDEO_FORMAT_LINEAR;

    for(i=0; i<u32VdecChnNum; i++)
    {
        VpssGrp = i;
        s32Ret = SAMPLE_COMM_VPSS_Start(VpssGrp, &abChnEnable[0], &stVpssGrpAttr, &astVpssChnAttr[0]);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("start VPSS fail for %#x!\n", s32Ret);
            goto END4;
        }
    }



    /************************************************
    step5:  start VO
    *************************************************/
    stVoConfig.VoDev                 = SAMPLE_VO_DEV_UHD;
    stVoConfig.enVoIntfType          = VO_INTF_HDMI;
    stVoConfig.enIntfSync            = enIntfSync;
    stVoConfig.enPicSize             = enDispPicSize;
    stVoConfig.u32BgColor            = COLOR_RGB_BLUE;
    stVoConfig.u32DisBufLen          = 3;
    stVoConfig.enDstDynamicRange     = DYNAMIC_RANGE_SDR8;
    stVoConfig.enVoMode              = VO_MODE_1MUX;
    stVoConfig.enPixFormat           = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    stVoConfig.stDispRect.s32X       = 0;
    stVoConfig.stDispRect.s32Y       = 0;
    stVoConfig.stDispRect.u32Width   = stDispSize.u32Width;
    stVoConfig.stDispRect.u32Height  = stDispSize.u32Height;
    stVoConfig.stImageSize.u32Width  = stDispSize.u32Width;
    stVoConfig.stImageSize.u32Height = stDispSize.u32Height;
    stVoConfig.enVoPartMode          = VO_PART_MODE_SINGLE;
    s32Ret = SAMPLE_COMM_VO_StartVO(&stVoConfig);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("start VO fail for %#x!\n", s32Ret);
        goto END5;
    }

    /************************************************
    step6:  VDEC bind VPSS
    *************************************************/
    for(i=0; i<u32VdecChnNum; i++)
    {
        s32Ret = SAMPLE_COMM_VDEC_Bind_VPSS(i, i);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("vdec bind vpss fail for %#x!\n", s32Ret);
            goto END6;
        }
    }

    /************************************************
    step7:  VPSS bind VO
    *************************************************/
    VoLayer = stVoConfig.VoDev;
    for(i=0; i<VpssGrpNum; i++)
    {
        s32Ret = SAMPLE_COMM_VPSS_Bind_VO(i, 0, VoLayer, i);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("vpss bind vo fail for %#x!\n", s32Ret);
            goto END7;
        }
    }

    /************************************************
    step8:  send stream to VDEC
    *************************************************/
    for(i=0; i<u32VdecChnNum; i++)
    {
        snprintf(stVdecSend[i].cFileName, sizeof(stVdecSend[i].cFileName), "3840x2160.jpg");
        snprintf(stVdecSend[i].cFilePath, sizeof(stVdecSend[i].cFilePath), "%s", SAMPLE_STREAM_PATH);
        stVdecSend[i].enType          = astSampleVdec[i].enType;
        stVdecSend[i].s32StreamMode   = astSampleVdec[i].enMode;
        stVdecSend[i].s32ChnId        = i;
        stVdecSend[i].s32IntervalTime = 1000;
        stVdecSend[i].u64PtsInit      = 0;
        stVdecSend[i].u64PtsIncrease  = 0;
        stVdecSend[i].eThreadCtrl     = THREAD_CTRL_START;
        stVdecSend[i].bCircleSend     = HI_TRUE;
        stVdecSend[i].s32MilliSec     = 0;
        stVdecSend[i].s32MinBufSize   = (astSampleVdec[i].u32Width * astSampleVdec[i].u32Height * 3)>>1;
    }
    SAMPLE_COMM_VDEC_StartSendStream(u32VdecChnNum, &stVdecSend[0], &VdecThread[0]);

    SAMPLE_COMM_VDEC_CmdCtrl(u32VdecChnNum, &stVdecSend[0], &VdecThread[0]);

    SAMPLE_COMM_VDEC_StopSendStream(u32VdecChnNum, &stVdecSend[0], &VdecThread[0]);

END7:
    for(i=0; i<VpssGrpNum; i++)
    {
        s32Ret = SAMPLE_COMM_VPSS_UnBind_VO(i, 0, VoLayer, i);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("vpss unbind vo fail for %#x!\n", s32Ret);
        }
    }

END6:
    for(i=0; i<u32VdecChnNum; i++)
    {
        s32Ret = SAMPLE_COMM_VDEC_UnBind_VPSS(i, i);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("vdec unbind vpss fail for %#x!\n", s32Ret);
        }
    }

END5:
    SAMPLE_COMM_VO_StopVO(&stVoConfig);

END4:
    for(i = VpssGrp; i >= 0; i--)
    {
        VpssGrp = i;
        SAMPLE_COMM_VPSS_Stop(VpssGrp, &abChnEnable[0]);
    }

END3:
    SAMPLE_COMM_VDEC_Stop(u32VdecChnNum);

END2:
    SAMPLE_COMM_VDEC_ExitVBPool();

END1:
    SAMPLE_COMM_SYS_Exit();

    return s32Ret;
}




HI_S32 SAMPLE_JPEG_VDEC_To_RGB(HI_VOID)
{
    VB_CONFIG_S stVbConfig;
    HI_S32 i, s32Ret = HI_SUCCESS;
    VDEC_THREAD_PARAM_S stVdecSend[VDEC_MAX_CHN_NUM];
    HI_U32 u32VdecChnNum;
    pthread_t   VdecThread[2*VDEC_MAX_CHN_NUM];
    SAMPLE_VDEC_ATTR astSampleVdec[VDEC_MAX_CHN_NUM];

    u32VdecChnNum = 1;

    /************************************************
    step1:  init SYS
    *************************************************/
    memset(&stVbConfig, 0, sizeof(VB_CONFIG_S));
    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConfig);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("init sys fail for %#x!\n", s32Ret);
        goto END1;
    }

    /************************************************
    step2:  init module VB or user VB(for VDEC)
    *************************************************/
    for(i=0; i<u32VdecChnNum; i++)
    {
        astSampleVdec[i].enType                            = PT_JPEG;
        astSampleVdec[i].u32Width                          = 1920;
        astSampleVdec[i].u32Height                         = 1080;
        astSampleVdec[i].enMode                            = VIDEO_MODE_FRAME;
        astSampleVdec[i].stSapmleVdecPicture.enPixelFormat = PIXEL_FORMAT_ARGB_8888;
        astSampleVdec[i].stSapmleVdecPicture.u32Alpha      = 255;
        astSampleVdec[i].u32DisplayFrameNum                = 0;
        astSampleVdec[i].u32FrameBufCnt                    = astSampleVdec[i].u32DisplayFrameNum + 1;
    }
    s32Ret = SAMPLE_COMM_VDEC_InitVBPool(u32VdecChnNum, &astSampleVdec[0]);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("init mod common vb fail for %#x!\n", s32Ret);
        goto END2;
    }

    /************************************************
    step3:  start VDEC
    *************************************************/
    s32Ret = SAMPLE_COMM_VDEC_Start(u32VdecChnNum, &astSampleVdec[0]);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("start VDEC fail for %#x!\n", s32Ret);
        goto END3;
    }

    /************************************************
    step4:  send stream to VDEC
    *************************************************/
    for(i=0; i<u32VdecChnNum; i++)
    {
        snprintf(stVdecSend[i].cFileName, sizeof(stVdecSend[i].cFileName), "1080P.jpg");
        snprintf(stVdecSend[i].cFilePath, sizeof(stVdecSend[i].cFilePath), "%s", SAMPLE_STREAM_PATH);
        stVdecSend[i].enType          = astSampleVdec[i].enType;
        stVdecSend[i].s32StreamMode   = astSampleVdec[i].enMode;
        stVdecSend[i].s32ChnId        = i;
        stVdecSend[i].s32IntervalTime = 1000;
        stVdecSend[i].u64PtsInit      = 0;
        stVdecSend[i].u64PtsIncrease  = 0;
        stVdecSend[i].eThreadCtrl     = THREAD_CTRL_START;
        stVdecSend[i].bCircleSend     = HI_FALSE;
        stVdecSend[i].s32MilliSec     = 0;
        stVdecSend[i].s32MinBufSize   = astSampleVdec[i].u32Width * astSampleVdec[i].u32Height;
    }
    SAMPLE_COMM_VDEC_StartSendStream(u32VdecChnNum, &stVdecSend[0], &VdecThread[0]);
    SAMPLE_COMM_VDEC_StartGetPic(u32VdecChnNum, &stVdecSend[0], &VdecThread[0]);

    SAMPLE_COMM_VDEC_CmdCtrl(u32VdecChnNum, &stVdecSend[0], &VdecThread[0]);

    SAMPLE_COMM_VDEC_StopSendStream(u32VdecChnNum, &stVdecSend[0], &VdecThread[0]);
    SAMPLE_COMM_VDEC_StopGetPic(u32VdecChnNum, &stVdecSend[0], &VdecThread[0]);

END3:
    SAMPLE_COMM_VDEC_Stop(u32VdecChnNum);

END2:
    SAMPLE_COMM_VDEC_ExitVBPool();

END1:
    SAMPLE_COMM_SYS_Exit();

    return s32Ret;
}


HI_S32 SAMPLE_COMM_VO_StartVO2(SAMPLE_VO_CONFIG_S *pstVoConfig)
{
    RECT_S                 stDefDispRect  = {0, 0, 1280, 720};
    SIZE_S                 stDefImageSize = {1920, 1080};

    /*******************************************
    * VO device VoDev# information declaration.
    ********************************************/
    VO_DEV                 VoDev          = 0;
    VO_LAYER               VoLayer        = 0;
    SAMPLE_VO_MODE_E       enVoMode       = 0;
    VO_INTF_TYPE_E         enVoIntfType   = VO_INTF_HDMI;
    VO_INTF_SYNC_E         enIntfSync     = VO_OUTPUT_1080P30;
    DYNAMIC_RANGE_E        enDstDyRg      = DYNAMIC_RANGE_SDR8;
    VO_PART_MODE_E         enVoPartMode   = VO_PART_MODE_SINGLE;
    VO_PUB_ATTR_S          stVoPubAttr    = {0};
    VO_VIDEO_LAYER_ATTR_S  stLayerAttr    = {0};
    VO_CSC_S               stVideoCSC     = {0};
    HI_S32                 s32Ret         = HI_SUCCESS;
    GRAPHIC_LAYER          GraphicLayer   = {0};
    VO_CSC_S               stGraphicCSC   = {0};

    if (NULL == pstVoConfig)
    {
        SAMPLE_PRT("Error:argument can not be NULL\n");
        return HI_FAILURE;
    }
    VoDev          = pstVoConfig->VoDev;
    VoLayer        = pstVoConfig->VoDev;
    enVoMode       = pstVoConfig->enVoMode;
    enVoIntfType   = pstVoConfig->enVoIntfType;
    enIntfSync     = pstVoConfig->enIntfSync;
    enDstDyRg      = pstVoConfig->enDstDynamicRange;
    enVoPartMode   = pstVoConfig->enVoPartMode;

    /********************************
    * Set and start VO device VoDev#.
    *********************************/
    stVoPubAttr.enIntfType  = enVoIntfType;
    stVoPubAttr.enIntfSync  = enIntfSync;
    stVoPubAttr.stSyncInfo.bSynm = 0;     /* RW; sync mode(0:timing,as BT.656; 1:signal,as LCD) */
    stVoPubAttr.stSyncInfo.bIop = 1;      /* RW; interlaced or progressive display(0:i; 1:p) */
    stVoPubAttr.stSyncInfo.u8Intfb = 0;   /* RW; interlace bit width while output */
        
    stVoPubAttr.stSyncInfo.u16Vact = 800;  /* RW; vertical active area */
    stVoPubAttr.stSyncInfo.u16Vbb = 15;    /* RW; vertical back blank porch */
    stVoPubAttr.stSyncInfo.u16Vfb = 5;    /* RW; vertical front blank porch */
        
    stVoPubAttr.stSyncInfo.u16Hact = 1280;   /* RW; horizontal active area */
    stVoPubAttr.stSyncInfo.u16Hbb = 110;    /* RW; horizontal back blank porch */
    stVoPubAttr.stSyncInfo.u16Hfb = 110;    /* RW; horizontal front blank porch */
    stVoPubAttr.stSyncInfo.u16Hmid = 0;   /* RW; bottom horizontal active area */
        
    stVoPubAttr.stSyncInfo.u16Bvact = 0;  /* RW; bottom vertical active area */
    stVoPubAttr.stSyncInfo.u16Bvbb = 0;   /* RW; bottom vertical back blank porch */
    stVoPubAttr.stSyncInfo.u16Bvfb = 0;   /* RW; bottom vertical front blank porch */
        
    stVoPubAttr.stSyncInfo.u16Hpw = 40;    /* RW; horizontal pulse width */
    stVoPubAttr.stSyncInfo.u16Vpw = 5;    /* RW; vertical pulse width */
        
    stVoPubAttr.stSyncInfo.bIdv = 0;      /* RW; inverse data valid of output */
    stVoPubAttr.stSyncInfo.bIhs = 0;      /* RW; inverse horizontal synch signal */
    stVoPubAttr.stSyncInfo.bIvs = 0;      /* RW; inverse vertical synch signal */

    stVoPubAttr.u32BgColor  = pstVoConfig->u32BgColor;

    s32Ret = SAMPLE_COMM_VO_StartDev(VoDev, &stVoPubAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VO_StartDev failed!\n");
        return s32Ret;
    }

    /******************************
    * Set and start layer VoDev#.
    ********************************/

    s32Ret = SAMPLE_COMM_VO_GetWH(stVoPubAttr.enIntfSync,
                                  &stLayerAttr.stDispRect.u32Width, &stLayerAttr.stDispRect.u32Height,
                                  &stLayerAttr.u32DispFrmRt);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VO_GetWH failed!\n");
        SAMPLE_COMM_VO_StopDev(VoDev);
        return s32Ret;
    }
    else
    {
		SAMPLE_PRT("stLayerAttr.stDispRect.u32Width = %d\n\
		stLayerAttr.stDispRect.u32Height= %d\n\
		stLayerAttr.u32DispFrmRt = %d\n", \
		stLayerAttr.stDispRect.u32Width, stLayerAttr.stDispRect.u32Height, stLayerAttr.u32DispFrmRt);
	}
	
    stLayerAttr.bClusterMode     = HI_FALSE;
    stLayerAttr.bDoubleFrame    = HI_FALSE;
    stLayerAttr.enPixFormat       = pstVoConfig->enPixFormat;

    stLayerAttr.stDispRect.s32X = 0;
    stLayerAttr.stDispRect.s32Y = 0;

    /******************************
    // Set display rectangle if changed.
    ********************************/
    if (0 != memcmp(&pstVoConfig->stDispRect, &stDefDispRect, sizeof(RECT_S)))
    {
        stLayerAttr.stDispRect = pstVoConfig->stDispRect;
    }
    //stLayerAttr.stImageSize.u32Width  = stLayerAttr.stDispRect.u32Width;
    //stLayerAttr.stImageSize.u32Height = stLayerAttr.stDispRect.u32Height;

    stLayerAttr.stImageSize.u32Width  = 1280;
    stLayerAttr.stImageSize.u32Height = 720;

    /******************************
    //Set image size if changed.
    ********************************/
    if (0 != memcmp(&pstVoConfig->stImageSize, &stDefImageSize, sizeof(SIZE_S)))
    {
        stLayerAttr.stImageSize = pstVoConfig->stImageSize;
    }
    stLayerAttr.enDstDynamicRange     = pstVoConfig->enDstDynamicRange;


    if (VO_PART_MODE_MULTI == enVoPartMode)
    {
        s32Ret = HI_MPI_VO_SetVideoLayerPartitionMode(VoLayer, enVoPartMode);
        if (HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("HI_MPI_VO_SetVideoLayerPartitionMode failed!\n");
            SAMPLE_COMM_VO_StopDev(VoDev);
            return s32Ret;
        }
    }

    if (pstVoConfig->u32DisBufLen)
    {
        s32Ret = HI_MPI_VO_SetDisplayBufLen(VoLayer, pstVoConfig->u32DisBufLen);
        if (HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("HI_MPI_VO_SetDisplayBufLen failed with %#x!\n",s32Ret);
            SAMPLE_COMM_VO_StopDev(VoDev);
            return s32Ret;
        }
    }

    s32Ret = SAMPLE_COMM_VO_StartLayer(VoLayer, &stLayerAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VO_Start video layer failed!\n");
        SAMPLE_COMM_VO_StopDev(VoDev);
        return s32Ret;
    }

    if(VO_INTF_MIPI == enVoIntfType)
    {
        s32Ret = HI_MPI_VO_GetVideoLayerCSC(VoLayer, &stVideoCSC);
        if (HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("HI_MPI_VO_GetVideoLayerCSC failed!\n");
            SAMPLE_COMM_VO_StopDev(VoDev);
            return s32Ret;
        }
        SAMPLE_PRT("stVideoCSC.enCscMatrix = %d\n", stVideoCSC.enCscMatrix);
        stVideoCSC.enCscMatrix = VO_CSC_MATRIX_BT709_TO_RGB_PC;
        //stVideoCSC.enCscMatrix = VO_CSC_MATRIX_IDENTITY;
        //stVideoCSC.enCscMatrix = VO_CSC_MATRIX_BT709_TO_BT601;
        s32Ret = HI_MPI_VO_SetVideoLayerCSC(VoLayer, &stVideoCSC);
        if (HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("HI_MPI_VO_SetVideoLayerCSC failed!\n");
            SAMPLE_COMM_VO_StopDev(VoDev);
            return s32Ret;
        }
        SAMPLE_PRT("stVideoCSC.enCscMatrix = %d\n", stVideoCSC.enCscMatrix);

        s32Ret = HI_MPI_VO_GetGraphicLayerCSC(GraphicLayer, &stGraphicCSC);
        if (HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("HI_MPI_VO_GetGraphicLayerCSC failed!\n");
            SAMPLE_COMM_VO_StopDev(VoDev);
            return s32Ret;
        }
        SAMPLE_PRT("stGraphicCSC.enCscMatrix = %d\n", stGraphicCSC.enCscMatrix);
        
        stGraphicCSC.enCscMatrix =VO_CSC_MATRIX_IDENTITY;
        //SAMPLE_PRT("stGraphicCSC.enCscMatrix = %d\n", stGraphicCSC.enCscMatrix);
        s32Ret = HI_MPI_VO_SetGraphicLayerCSC(GraphicLayer, &stGraphicCSC);
        if (HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("HI_MPI_VO_SetGraphicLayerCSC failed!\n");
            SAMPLE_COMM_VO_StopDev(VoDev);
            return s32Ret;
        }
        SAMPLE_PRT("stGraphicCSC.enCscMatrix = %d\n", stGraphicCSC.enCscMatrix);
    }

    /******************************
    * start vo channels.
    ********************************/
    s32Ret = SAMPLE_COMM_VO_StartChn(VoLayer, enVoMode);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VO_StartChn failed!\n");
        SAMPLE_COMM_VO_StopLayer(VoLayer);
        SAMPLE_COMM_VO_StopDev(VoDev);
        return s32Ret;
    }

    /******************************
    * Start hdmi device.
    * Note : do this after vo device started.
    ********************************/
    if(VO_INTF_HDMI == enVoIntfType)
    {
        SAMPLE_COMM_VO_HdmiStartByDyRg(enIntfSync, enDstDyRg);
    }

    /******************************
    * Start mipi_tx device.
    * Note : do this after vo device started.
    ********************************/
    if(VO_INTF_MIPI == enVoIntfType)
    {
        SAMPLE_COMM_VO_StartMipiTx(enIntfSync);
    }

    return HI_SUCCESS;
}

HI_S32 SAMPLE_H264_VDEC_VPSS_VO_MIPI_Tx(HI_VOID)
{
    VB_CONFIG_S         stVbConfig;
    HI_S32              i, s32Ret     = HI_SUCCESS;
    VDEC_THREAD_PARAM_S stVdecSend[VDEC_MAX_CHN_NUM];
    SIZE_S              stDispSizeUHD = {0};
    SIZE_S              stDispSizeHD  = {0};
    VO_LAYER            VoLayer;
    HI_U32              u32VdecChnNum, VpssGrpNum;
    VPSS_GRP            VpssGrp;
    pthread_t           VdecThread[2*VDEC_MAX_CHN_NUM];
    PIC_SIZE_E          enDispPicSize;
    SAMPLE_VDEC_ATTR    astSampleVdec[VDEC_MAX_CHN_NUM];
    VPSS_CHN_ATTR_S     astVpssChnAttr[VPSS_MAX_CHN_NUM];
    SAMPLE_VO_CONFIG_S  stVoConfigUHD;
    SAMPLE_VO_CONFIG_S  stVoConfigHD;
    VPSS_GRP_ATTR_S     stVpssGrpAttr;
    HI_BOOL             abChnEnable[VPSS_MAX_CHN_NUM];
    VO_INTF_SYNC_E      enIntfSync;

    u32VdecChnNum = 1;
    VpssGrpNum    = u32VdecChnNum;

    /************************************************
    step1:  init SYS, init common VB(for VPSS and VO)
    *************************************************/
    if(VO_OUTPUT_3840x2160_30 == g_enIntfSync)
    {
        enDispPicSize = PIC_3840x2160;
        enIntfSync    = VO_OUTPUT_3840x2160_30;
    }
    else if(VO_OUTPUT_1080P30 == g_enIntfSync)
    {
        enDispPicSize = PIC_1080P;
        enIntfSync    = VO_OUTPUT_1080P30;
    }
    else if(VO_OUTPUT_1080P60 == g_enIntfSync)
    {
        enDispPicSize = PIC_1080P;
        enIntfSync    = VO_OUTPUT_1080P60;
    }

    s32Ret =  SAMPLE_COMM_SYS_GetPicSize(enDispPicSize, &stDispSizeUHD);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("sys get pic size fail for %#x!\n", s32Ret);
        goto END1;
    }

    memset(&stVbConfig, 0, sizeof(VB_CONFIG_S));
    stVbConfig.u32MaxPoolCnt             = 1;
    stVbConfig.astCommPool[0].u32BlkCnt  = 10*u32VdecChnNum;
    stVbConfig.astCommPool[0].u64BlkSize = COMMON_GetPicBufferSize(stDispSizeUHD.u32Width, stDispSizeUHD.u32Height,
                                                PIXEL_FORMAT_YVU_SEMIPLANAR_420, DATA_BITWIDTH_8, COMPRESS_MODE_SEG, 0);
    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConfig);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("init sys fail for %#x!\n", s32Ret);
        goto END1;
    }

    /************************************************
    step2:  init module VB or user VB(for VDEC)
    *************************************************/
    for(i=0; i<u32VdecChnNum; i++)
    {
        astSampleVdec[i].enType                           = PT_H264;
        astSampleVdec[i].u32Width                         = 3840;
        astSampleVdec[i].u32Height                        = 2160;
        astSampleVdec[i].enMode                           = VIDEO_MODE_FRAME;
        astSampleVdec[i].stSapmleVdecVideo.enDecMode      = VIDEO_DEC_MODE_IPB;
        astSampleVdec[i].stSapmleVdecVideo.enBitWidth     = DATA_BITWIDTH_8;
        astSampleVdec[i].stSapmleVdecVideo.u32RefFrameNum = 3;
        astSampleVdec[i].u32DisplayFrameNum               = 2;
        astSampleVdec[i].u32FrameBufCnt = astSampleVdec[i].stSapmleVdecVideo.u32RefFrameNum + astSampleVdec[i].u32DisplayFrameNum + 1;
    }
    s32Ret = SAMPLE_COMM_VDEC_InitVBPool(u32VdecChnNum, &astSampleVdec[0]);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("init mod common vb fail for %#x!\n", s32Ret);
        goto END2;
    }

    /************************************************
    step3:  start VDEC
    *************************************************/
    s32Ret = SAMPLE_COMM_VDEC_Start(u32VdecChnNum, &astSampleVdec[0]);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("start VDEC fail for %#x!\n", s32Ret);
        goto END3;
    }

    /************************************************
    step4:  start VPSS
    *************************************************/
    stVpssGrpAttr.u32MaxW = 3840;
    stVpssGrpAttr.u32MaxH = 2160;
    stVpssGrpAttr.stFrameRate.s32SrcFrameRate = -1;
    stVpssGrpAttr.stFrameRate.s32DstFrameRate = -1;
    stVpssGrpAttr.enDynamicRange = DYNAMIC_RANGE_SDR8;
    stVpssGrpAttr.enPixelFormat  = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    stVpssGrpAttr.bNrEn   = HI_FALSE;

    memset(abChnEnable, 0, sizeof(abChnEnable));
    abChnEnable[0] = HI_TRUE;
    astVpssChnAttr[0].u32Width                    = stDispSizeUHD.u32Width;
    astVpssChnAttr[0].u32Height                   = stDispSizeUHD.u32Height;
    astVpssChnAttr[0].enChnMode                   = VPSS_CHN_MODE_USER;
    astVpssChnAttr[0].enCompressMode              = COMPRESS_MODE_SEG;
    astVpssChnAttr[0].enDynamicRange              = DYNAMIC_RANGE_SDR8;
    astVpssChnAttr[0].enPixelFormat               = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    astVpssChnAttr[0].stFrameRate.s32SrcFrameRate = -1;
    astVpssChnAttr[0].stFrameRate.s32DstFrameRate = -1;
    astVpssChnAttr[0].u32Depth                    = 0;
    astVpssChnAttr[0].bMirror                     = HI_FALSE;
    astVpssChnAttr[0].bFlip                       = HI_FALSE;
    astVpssChnAttr[0].stAspectRatio.enMode        = ASPECT_RATIO_NONE;
    astVpssChnAttr[0].enVideoFormat               = VIDEO_FORMAT_LINEAR;
    for(i=0; i<u32VdecChnNum; i++)
    {
        VpssGrp = i;
        s32Ret = SAMPLE_COMM_VPSS_Start(VpssGrp, &abChnEnable[0], &stVpssGrpAttr, &astVpssChnAttr[0]);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("start VPSS fail for %#x!\n", s32Ret);
            goto END4;
        }
    }

    /************************************************
    step5:  start VO Dev UHD(DHD0-hdmi)
    *************************************************/
    stVoConfigUHD.VoDev                 = SAMPLE_VO_DEV_UHD;
    stVoConfigUHD.enVoIntfType          = VO_INTF_HDMI;
    stVoConfigUHD.enIntfSync            = enIntfSync;
    stVoConfigUHD.enPicSize             = enDispPicSize;
    stVoConfigUHD.u32BgColor            = COLOR_RGB_BLUE;
    stVoConfigUHD.u32DisBufLen          = 3;
    stVoConfigUHD.enDstDynamicRange     = DYNAMIC_RANGE_SDR8;
    stVoConfigUHD.enVoMode              = VO_MODE_1MUX;
    stVoConfigUHD.enPixFormat           = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    stVoConfigUHD.stDispRect.s32X       = 0;
    stVoConfigUHD.stDispRect.s32Y       = 0;
    stVoConfigUHD.stDispRect.u32Width   = stDispSizeUHD.u32Width;
    stVoConfigUHD.stDispRect.u32Height  = stDispSizeUHD.u32Height;
    stVoConfigUHD.stImageSize.u32Width  = stDispSizeUHD.u32Width;
    stVoConfigUHD.stImageSize.u32Height = stDispSizeUHD.u32Height;
    stVoConfigUHD.enVoPartMode          = VO_PART_MODE_SINGLE;
    s32Ret = SAMPLE_COMM_VO_StartVO(&stVoConfigUHD);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("start VO %d fail for %#x!\n", stVoConfigUHD.VoDev, s32Ret);
        goto END5;
    }

    /************************************************
    step6:  start VO Dev HD(DHD1-mipi_tx)
    *************************************************/
    //stDispSizeHD.u32Width  = 1080;
    //stDispSizeHD.u32Height = 1920;
    stDispSizeHD.u32Width  = 1280;
    //stDispSizeHD.u32Height = 800;
    stDispSizeHD.u32Height = 720;
    stVoConfigHD.VoDev                 = SAMPLE_VO_DEV_HD;
    stVoConfigHD.enVoIntfType          = VO_INTF_MIPI;
    //stVoConfigHD.enIntfSync            = VO_OUTPUT_1080x1920_60;
    //stVoConfigHD.enIntfSync            = VO_OUTPUT_1280x800_60;
    stVoConfigHD.enIntfSync            = VO_OUTPUT_720P60;
    stVoConfigHD.enPicSize             = PIC_720P;
    stVoConfigHD.u32BgColor            = COLOR_RGB_CYN;
    stVoConfigHD.u32DisBufLen          = 3;
    stVoConfigHD.enDstDynamicRange     = DYNAMIC_RANGE_SDR8;
    stVoConfigHD.enVoMode              = VO_MODE_1MUX;
    stVoConfigHD.enPixFormat           = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    stVoConfigHD.stDispRect.s32X       = 0;
    stVoConfigHD.stDispRect.s32Y       = 0;
    stVoConfigHD.stDispRect.u32Width   = stDispSizeHD.u32Width;
    stVoConfigHD.stDispRect.u32Height  = stDispSizeHD.u32Height;
    stVoConfigHD.stImageSize.u32Width  = stDispSizeHD.u32Width;
    stVoConfigHD.stImageSize.u32Height = stDispSizeHD.u32Height;
    stVoConfigHD.enVoPartMode          = VO_PART_MODE_SINGLE;
    s32Ret = SAMPLE_COMM_VO_StartVO2(&stVoConfigHD);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("start VO %d fail for %#x!\n", stVoConfigHD.VoDev, s32Ret);
        goto END6;
    }


    /************************************************
    step7:  VDEC bind VPSS
    *************************************************/
    for(i=0; i<u32VdecChnNum; i++)
    {
        s32Ret = SAMPLE_COMM_VDEC_Bind_VPSS(i, i);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("vdec bind vpss fail for %#x!\n", s32Ret);
            goto END7;
        }
    }

    /************************************************
    step8:  VPSS bind VO Layer 0
    *************************************************/
    VoLayer = stVoConfigUHD.VoDev;
    for(i=0; i<VpssGrpNum; i++)
    {
        s32Ret = SAMPLE_COMM_VPSS_Bind_VO(i, 0, VoLayer, i);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("vpss bind vo fail for %#x!\n", s32Ret);
            goto END8;
        }
    }

    /************************************************
    step9:  VPSS bind VO Layer 1
    *************************************************/
    VoLayer = stVoConfigHD.VoDev;
    for(i=0; i<VpssGrpNum; i++)
    {
        s32Ret = SAMPLE_COMM_VPSS_Bind_VO(i, 0, VoLayer, i);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("vpss bind vo fail for %#x!\n", s32Ret);
            goto END9;
        }
    }

    /************************************************
    step10:  send stream to VDEC
    *************************************************/
    for(i=0; i<u32VdecChnNum; i++)
    {
        snprintf(stVdecSend[i].cFileName, sizeof(stVdecSend[i].cFileName), "3840x2160_8bit.h264");
        snprintf(stVdecSend[i].cFilePath, sizeof(stVdecSend[i].cFilePath), "%s", SAMPLE_STREAM_PATH);
        stVdecSend[i].enType          = astSampleVdec[i].enType;
        stVdecSend[i].s32StreamMode   = astSampleVdec[i].enMode;
        stVdecSend[i].s32ChnId        = i;
        stVdecSend[i].s32IntervalTime = 1000;
        stVdecSend[i].u64PtsInit      = 0;
        stVdecSend[i].u64PtsIncrease  = 0;
        stVdecSend[i].eThreadCtrl     = THREAD_CTRL_START;
        stVdecSend[i].bCircleSend     = HI_TRUE;
        stVdecSend[i].s32MilliSec     = 0;
        stVdecSend[i].s32MinBufSize   = (astSampleVdec[i].u32Width * astSampleVdec[i].u32Height * 3)>>1;
    }
    SAMPLE_COMM_VDEC_StartSendStream(u32VdecChnNum, &stVdecSend[0], &VdecThread[0]);

    SAMPLE_COMM_VDEC_CmdCtrl(u32VdecChnNum, &stVdecSend[0], &VdecThread[0]);

    SAMPLE_COMM_VDEC_StopSendStream(u32VdecChnNum, &stVdecSend[0], &VdecThread[0]);

END9:
    VoLayer = stVoConfigHD.VoDev;
    for(i=0; i<VpssGrpNum; i++)
    {
        s32Ret = SAMPLE_COMM_VPSS_UnBind_VO(i, 0, VoLayer, i);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("vpss unbind vo (%d,%d) fail for %#x!\n",VoLayer, i, s32Ret);
        }
    }
END8:
    VoLayer = stVoConfigUHD.VoDev;
    for(i=0; i<VpssGrpNum; i++)
    {
        s32Ret = SAMPLE_COMM_VPSS_UnBind_VO(i, 0, VoLayer, i);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("vpss unbind vo (%d,%d) fail for %#x!\n",VoLayer, i, s32Ret);
        }
    }
END7:
    for(i=0; i<u32VdecChnNum; i++)
    {
        s32Ret = SAMPLE_COMM_VDEC_UnBind_VPSS(i, i);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("vdec unbind vpss fail for %#x!\n", s32Ret);
        }
    }
END6:
    SAMPLE_COMM_VO_StopVO(&stVoConfigHD);
END5:
    SAMPLE_COMM_VO_StopVO(&stVoConfigUHD);

END4:
    for(i = VpssGrp; i >= 0; i--)
    {
        VpssGrp = i;
        SAMPLE_COMM_VPSS_Stop(VpssGrp, &abChnEnable[0]);
    }
END3:
    SAMPLE_COMM_VDEC_Stop(u32VdecChnNum);

END2:
    SAMPLE_COMM_VDEC_ExitVBPool();

END1:
    SAMPLE_COMM_SYS_Exit();

    return s32Ret;
}

HI_S32 SAMPLE_H264_VDEC_VPSS_VO_MIPI_Tx2(HI_VOID)
{
    VB_CONFIG_S         stVbConfig;
    HI_S32              i, s32Ret     = HI_SUCCESS;
    VDEC_THREAD_PARAM_S stVdecSend[VDEC_MAX_CHN_NUM];
    SIZE_S              stDispSizeUHD = {0};
    SIZE_S              stDispSizeHD  = {0};
    VO_LAYER            VoLayer;
    HI_U32              u32VdecChnNum, VpssGrpNum;
    VPSS_GRP            VpssGrp;
    pthread_t           VdecThread[2*VDEC_MAX_CHN_NUM];
    PIC_SIZE_E          enDispPicSize;
    SAMPLE_VDEC_ATTR    astSampleVdec[VDEC_MAX_CHN_NUM];
    VPSS_CHN_ATTR_S     astVpssChnAttr[VPSS_MAX_CHN_NUM];
    SAMPLE_VO_CONFIG_S  stVoConfigUHD;
    SAMPLE_VO_CONFIG_S  stVoConfigHD;
    VPSS_GRP_ATTR_S     stVpssGrpAttr;
    HI_BOOL             abChnEnable[VPSS_MAX_CHN_NUM];
    VO_INTF_SYNC_E      enIntfSync;

    PTHREAD_HIFB_SAMPLE_INFO stInfo0;
    PTHREAD_HIFB_SAMPLE_INFO stInfo1;

    u32VdecChnNum = 1;
    VpssGrpNum    = u32VdecChnNum;

    /************************************************
    step1:  init SYS, init common VB(for VPSS and VO)
    *************************************************/
    if(VO_OUTPUT_3840x2160_30 == g_enIntfSync)
    {
        enDispPicSize = PIC_3840x2160;
        enIntfSync    = VO_OUTPUT_3840x2160_30;
    }
    else if(VO_OUTPUT_1080P30 == g_enIntfSync)
    {
        enDispPicSize = PIC_1080P;
        enIntfSync    = VO_OUTPUT_1080P30;
    }
    else if(VO_OUTPUT_1080P60 == g_enIntfSync)
    {
        enDispPicSize = PIC_1080P;
        enIntfSync    = VO_OUTPUT_1080P60;
    }

    s32Ret =  SAMPLE_COMM_SYS_GetPicSize(enDispPicSize, &stDispSizeUHD);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("sys get pic size fail for %#x!\n", s32Ret);
        goto END1;
    }

    memset(&stVbConfig, 0, sizeof(VB_CONFIG_S));
    stVbConfig.u32MaxPoolCnt             = 1;
    stVbConfig.astCommPool[0].u32BlkCnt  = 10*u32VdecChnNum;
    stVbConfig.astCommPool[0].u64BlkSize = COMMON_GetPicBufferSize(stDispSizeUHD.u32Width, stDispSizeUHD.u32Height,
                                                PIXEL_FORMAT_YVU_SEMIPLANAR_420, DATA_BITWIDTH_8, COMPRESS_MODE_SEG, 0);
    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConfig);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("init sys fail for %#x!\n", s32Ret);
        goto END1;
    }

    /************************************************
    step2:  init module VB or user VB(for VDEC)
    *************************************************/
    for(i=0; i<u32VdecChnNum; i++)
    {
        astSampleVdec[i].enType                           = PT_H264;
        astSampleVdec[i].u32Width                         = 3840;
        astSampleVdec[i].u32Height                        = 2160;
        astSampleVdec[i].enMode                           = VIDEO_MODE_FRAME;
        astSampleVdec[i].stSapmleVdecVideo.enDecMode      = VIDEO_DEC_MODE_IPB;
        astSampleVdec[i].stSapmleVdecVideo.enBitWidth     = DATA_BITWIDTH_8;
        astSampleVdec[i].stSapmleVdecVideo.u32RefFrameNum = 3;
        astSampleVdec[i].u32DisplayFrameNum               = 2;
        astSampleVdec[i].u32FrameBufCnt = astSampleVdec[i].stSapmleVdecVideo.u32RefFrameNum + astSampleVdec[i].u32DisplayFrameNum + 1;
    }
    s32Ret = SAMPLE_COMM_VDEC_InitVBPool(u32VdecChnNum, &astSampleVdec[0]);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("init mod common vb fail for %#x!\n", s32Ret);
        goto END2;
    }

    /************************************************
    step3:  start VDEC
    *************************************************/
    s32Ret = SAMPLE_COMM_VDEC_Start(u32VdecChnNum, &astSampleVdec[0]);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("start VDEC fail for %#x!\n", s32Ret);
        goto END3;
    }

    /************************************************
    step4:  start VPSS
    *************************************************/
    stVpssGrpAttr.u32MaxW = 3840;
    stVpssGrpAttr.u32MaxH = 2160;
    stVpssGrpAttr.stFrameRate.s32SrcFrameRate = -1;
    stVpssGrpAttr.stFrameRate.s32DstFrameRate = -1;
    stVpssGrpAttr.enDynamicRange = DYNAMIC_RANGE_SDR8;
    stVpssGrpAttr.enPixelFormat  = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    stVpssGrpAttr.bNrEn   = HI_FALSE;

    memset(abChnEnable, 0, sizeof(abChnEnable));
    abChnEnable[0] = HI_TRUE;
    astVpssChnAttr[0].u32Width                    = stDispSizeUHD.u32Width;
    astVpssChnAttr[0].u32Height                   = stDispSizeUHD.u32Height;
    astVpssChnAttr[0].enChnMode                   = VPSS_CHN_MODE_USER;
    astVpssChnAttr[0].enCompressMode              = COMPRESS_MODE_SEG;
    astVpssChnAttr[0].enDynamicRange              = DYNAMIC_RANGE_SDR8;
    astVpssChnAttr[0].enPixelFormat               = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    astVpssChnAttr[0].stFrameRate.s32SrcFrameRate = -1;
    astVpssChnAttr[0].stFrameRate.s32DstFrameRate = -1;
    astVpssChnAttr[0].u32Depth                    = 0;
    astVpssChnAttr[0].bMirror                     = HI_FALSE;
    astVpssChnAttr[0].bFlip                       = HI_FALSE;
    astVpssChnAttr[0].stAspectRatio.enMode        = ASPECT_RATIO_NONE;
    astVpssChnAttr[0].enVideoFormat               = VIDEO_FORMAT_LINEAR;
    for(i=0; i<u32VdecChnNum; i++)
    {
        VpssGrp = i;
        s32Ret = SAMPLE_COMM_VPSS_Start(VpssGrp, &abChnEnable[0], &stVpssGrpAttr, &astVpssChnAttr[0]);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("start VPSS fail for %#x!\n", s32Ret);
            goto END4;
        }
    }

    /************************************************
    step5:  start VO Dev UHD(DHD0-hdmi)
    *************************************************/
    stVoConfigUHD.VoDev                 = SAMPLE_VO_DEV_UHD;
    stVoConfigUHD.enVoIntfType          = VO_INTF_HDMI;
    stVoConfigUHD.enIntfSync            = enIntfSync;
    stVoConfigUHD.enPicSize             = enDispPicSize;
    stVoConfigUHD.u32BgColor            = COLOR_RGB_BLUE;
    stVoConfigUHD.u32DisBufLen          = 3;
    stVoConfigUHD.enDstDynamicRange     = DYNAMIC_RANGE_SDR8;
    stVoConfigUHD.enVoMode              = VO_MODE_1MUX;
    stVoConfigUHD.enPixFormat           = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    stVoConfigUHD.stDispRect.s32X       = 0;
    stVoConfigUHD.stDispRect.s32Y       = 0;
    stVoConfigUHD.stDispRect.u32Width   = stDispSizeUHD.u32Width;
    stVoConfigUHD.stDispRect.u32Height  = stDispSizeUHD.u32Height;
    stVoConfigUHD.stImageSize.u32Width  = stDispSizeUHD.u32Width;
    stVoConfigUHD.stImageSize.u32Height = stDispSizeUHD.u32Height;
    stVoConfigUHD.enVoPartMode          = VO_PART_MODE_SINGLE;
    s32Ret = SAMPLE_COMM_VO_StartVO(&stVoConfigUHD);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("start VO %d fail for %#x!\n", stVoConfigUHD.VoDev, s32Ret);
        goto END5;
    }

    /************************************************
    step6:  start VO Dev HD(DHD1-mipi_tx)
    *************************************************/
    //stDispSizeHD.u32Width  = 1080;
    //stDispSizeHD.u32Height = 1920;
    stDispSizeHD.u32Width  = 1280;
    stDispSizeHD.u32Height = 800;
    //stDispSizeHD.u32Height = 720;
    stVoConfigHD.VoDev                 = SAMPLE_VO_DEV_HD;
    stVoConfigHD.enVoIntfType          = VO_INTF_MIPI;
    //stVoConfigHD.enIntfSync            = VO_OUTPUT_1080x1920_60;
    //stVoConfigHD.enIntfSync            = VO_OUTPUT_1280x800_60;
    //stVoConfigHD.enIntfSync            = VO_OUTPUT_720P60;
    stVoConfigHD.enIntfSync            = VO_OUTPUT_USER;
    stVoConfigHD.enPicSize             = PIC_1280x800;
    //stVoConfigHD.enPicSize             = PIC_720P;
    stVoConfigHD.u32BgColor            = COLOR_RGB_BLACK;
    stVoConfigHD.u32DisBufLen          = 3;
    stVoConfigHD.enDstDynamicRange     = DYNAMIC_RANGE_SDR8;
    stVoConfigHD.enVoMode              = VO_MODE_1MUX;
    stVoConfigHD.enPixFormat           = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    stVoConfigHD.stDispRect.s32X       = 400;
    stVoConfigHD.stDispRect.s32Y       = 40;
    stVoConfigHD.stDispRect.u32Width   = 880;
    stVoConfigHD.stDispRect.u32Height  = 720;
    stVoConfigHD.stImageSize.u32Width  = 400;
    //stVoConfigHD.stImageSize.u32Height = stDispSizeHD.u32Height;
    stVoConfigHD.stImageSize.u32Height = 1000;
    stVoConfigHD.enVoPartMode          = VO_PART_MODE_SINGLE;
    s32Ret = SAMPLE_COMM_VO_StartVO2(&stVoConfigHD);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("start VO %d fail for %#x!\n", stVoConfigHD.VoDev, s32Ret);
        goto END9;
    }
    IntType = VO_INTF_MIPI;

    /************************************************
    step7:  VDEC bind VPSS
    *************************************************/
    for(i=0; i<u32VdecChnNum; i++)
    {
        s32Ret = SAMPLE_COMM_VDEC_Bind_VPSS(i, i);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("vdec bind vpss fail for %#x!\n", s32Ret);
            goto END7;
        }
    }

    /************************************************
    step8:  VPSS bind VO Layer 0
    *************************************************/
    VoLayer = stVoConfigUHD.VoDev;
    for(i=0; i<VpssGrpNum; i++)
    {
        s32Ret = SAMPLE_COMM_VPSS_Bind_VO(i, 0, VoLayer, i);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("vpss bind vo fail for %#x!\n", s32Ret);
            goto END8;
        }
    }

    /************************************************
    step9:  VPSS bind VO Layer 1
    *************************************************/
    VoLayer = stVoConfigHD.VoDev;
    for(i=0; i<VpssGrpNum; i++)
    {
        s32Ret = SAMPLE_COMM_VPSS_Bind_VO(i, 0, VoLayer, i);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("vpss bind vo fail for %#x!\n", s32Ret);
            goto END9;
        }
    }

#if 1
    SAMPLE_PRT("graph appears?\n");
    stInfo0.layer     =  0;    /* VO device number */
    stInfo0.fd        = -1;
    stInfo0.ctrlkey   =  2;        /* None buffer */
    stInfo0.bCompress =  HI_FALSE; /* Compress opened or not */
    stInfo0.enColorFmt = HIFB_FMT_ARGB8888;
    if (0 != pthread_create(&g_stHifbThread, 0, TDE_DrawGraphicSample, (void*)(&stInfo0)))
    {
        SAMPLE_PRT("start hifb thread0 failed!\n");
        //goto SAMPLE_HIFB_StandarMode_1;
    }
//#else

    stInfo1.layer     =  1;    /* VO device number */
    stInfo1.fd        = -1;
    stInfo1.ctrlkey   =  2;        /* None buffer */
    stInfo1.bCompress =  HI_FALSE; /* Compress opened or not */
    stInfo1.enColorFmt = HIFB_FMT_ARGB8888;
    if (0 != pthread_create(&g_stHifbThread1, 0, TDE_DrawGraphicSample, (void*)(&stInfo1)))
    {
        SAMPLE_PRT("start hifb thread0 failed!\n");
        //goto SAMPLE_HIFB_StandarMode_1;
    }
#endif

    /************************************************
    step10:  send stream to VDEC
    *************************************************/
    for(i=0; i<u32VdecChnNum; i++)
    {
        snprintf(stVdecSend[i].cFileName, sizeof(stVdecSend[i].cFileName), "3840x2160_8bit.h264");
        snprintf(stVdecSend[i].cFilePath, sizeof(stVdecSend[i].cFilePath), "%s", SAMPLE_STREAM_PATH);
        stVdecSend[i].enType          = astSampleVdec[i].enType;
        stVdecSend[i].s32StreamMode   = astSampleVdec[i].enMode;
        stVdecSend[i].s32ChnId        = i;
        stVdecSend[i].s32IntervalTime = 1000;
        stVdecSend[i].u64PtsInit      = 0;
        stVdecSend[i].u64PtsIncrease  = 0;
        stVdecSend[i].eThreadCtrl     = THREAD_CTRL_START;
        stVdecSend[i].bCircleSend     = HI_TRUE;
        stVdecSend[i].s32MilliSec     = 0;
        stVdecSend[i].s32MinBufSize   = (astSampleVdec[i].u32Width * astSampleVdec[i].u32Height * 3)>>1;
    }
    SAMPLE_COMM_VDEC_StartSendStream(u32VdecChnNum, &stVdecSend[0], &VdecThread[0]);

    SAMPLE_COMM_VDEC_CmdCtrl(u32VdecChnNum, &stVdecSend[0], &VdecThread[0]);

    SAMPLE_COMM_VDEC_StopSendStream(u32VdecChnNum, &stVdecSend[0], &VdecThread[0]);

//	    stInfo1.layer     =  1;    /* VO device number */
//	    stInfo1.fd        = -1;
//	    stInfo1.ctrlkey   =  2;        /* None buffer */
//	    stInfo1.bCompress =  HI_FALSE; /* Compress opened or not */
//	    stInfo1.enColorFmt = HIFB_FMT_ARGB8888;
//	    TDE_DrawGraphicSample((void*)(&stInfo1));
END9:
    VoLayer = stVoConfigHD.VoDev;
    for(i=0; i<VpssGrpNum; i++)
    {
        s32Ret = SAMPLE_COMM_VPSS_UnBind_VO(i, 0, VoLayer, i);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("vpss unbind vo (%d,%d) fail for %#x!\n",VoLayer, i, s32Ret);
        }
    }
END8:
    VoLayer = stVoConfigUHD.VoDev;
    for(i=0; i<VpssGrpNum; i++)
    {
        s32Ret = SAMPLE_COMM_VPSS_UnBind_VO(i, 0, VoLayer, i);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("vpss unbind vo (%d,%d) fail for %#x!\n",VoLayer, i, s32Ret);
        }
    }
END7:
    for(i=0; i<u32VdecChnNum; i++)
    {
        s32Ret = SAMPLE_COMM_VDEC_UnBind_VPSS(i, i);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("vdec unbind vpss fail for %#x!\n", s32Ret);
        }
    }
END6:
    SAMPLE_COMM_VO_StopVO(&stVoConfigHD);
END5:
    SAMPLE_COMM_VO_StopVO(&stVoConfigUHD);

END4:
    for(i = VpssGrp; i >= 0; i--)
    {
        VpssGrp = i;
        SAMPLE_COMM_VPSS_Stop(VpssGrp, &abChnEnable[0]);
    }
END3:
    SAMPLE_COMM_VDEC_Stop(u32VdecChnNum);

END2:
    SAMPLE_COMM_VDEC_ExitVBPool();

END1:
    SAMPLE_COMM_SYS_Exit();

    return s32Ret;
}

HI_S32 SAMPLE_H265_VDEC_VPSS_VO_MIPI_Tx(HI_VOID)
{
    VB_CONFIG_S         stVbConfig;
    HI_S32              i, s32Ret     = HI_SUCCESS;
    VDEC_THREAD_PARAM_S stVdecSend[VDEC_MAX_CHN_NUM];
    SIZE_S              stDispSizeUHD = {0};
    SIZE_S              stDispSizeHD  = {0};
    VO_LAYER            VoLayer;
    HI_U32              u32VdecChnNum, VpssGrpNum;
    VPSS_GRP            VpssGrp;
    pthread_t           VdecThread[2*VDEC_MAX_CHN_NUM];
    PIC_SIZE_E          enDispPicSize;
    SAMPLE_VDEC_ATTR    astSampleVdec[VDEC_MAX_CHN_NUM];
    VPSS_CHN_ATTR_S     astVpssChnAttr[VPSS_MAX_CHN_NUM];
    SAMPLE_VO_CONFIG_S  stVoConfigUHD;
    SAMPLE_VO_CONFIG_S  stVoConfigHD;
    VPSS_GRP_ATTR_S     stVpssGrpAttr;
    HI_BOOL             abChnEnable[VPSS_MAX_CHN_NUM];
    VO_INTF_SYNC_E      enIntfSync;

    u32VdecChnNum = 1;
    VpssGrpNum    = u32VdecChnNum;

    /************************************************
    step1:  init SYS, init common VB(for VPSS and VO)
    *************************************************/
    if(VO_OUTPUT_3840x2160_30 == g_enIntfSync)
    {
        enDispPicSize = PIC_3840x2160;
        enIntfSync    = VO_OUTPUT_3840x2160_30;
    }
    else if(VO_OUTPUT_1080P30 == g_enIntfSync)
    {
        enDispPicSize = PIC_1080P;
        enIntfSync    = VO_OUTPUT_1080P30;
    }
    else if(VO_OUTPUT_1080P60 == g_enIntfSync)
    {
        enDispPicSize = PIC_1080P;
        enIntfSync    = VO_OUTPUT_1080P60;
    }

    s32Ret =  SAMPLE_COMM_SYS_GetPicSize(enDispPicSize, &stDispSizeUHD);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("sys get pic size fail for %#x!\n", s32Ret);
        goto END1;
    }

    memset(&stVbConfig, 0, sizeof(VB_CONFIG_S));
    stVbConfig.u32MaxPoolCnt             = 1;
    stVbConfig.astCommPool[0].u32BlkCnt  = 10*u32VdecChnNum;
    stVbConfig.astCommPool[0].u64BlkSize = COMMON_GetPicBufferSize(stDispSizeUHD.u32Width, stDispSizeUHD.u32Height,
                                                PIXEL_FORMAT_YVU_SEMIPLANAR_420, DATA_BITWIDTH_8, COMPRESS_MODE_SEG, 0);
    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConfig);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("init sys fail for %#x!\n", s32Ret);
        goto END1;
    }

	stDispSizeUHD.u32Width = 1280;
	stDispSizeUHD.u32Height = 800;
	
    /************************************************
    step2:  init module VB or user VB(for VDEC)
    *************************************************/
    for(i=0; i<u32VdecChnNum; i++)
    {
        astSampleVdec[i].enType                           = PT_H264;
        astSampleVdec[i].u32Width                         = 3840;
        astSampleVdec[i].u32Height                        = 2160;
        //astSampleVdec[i].u32Width                         = 480;
        //astSampleVdec[i].u32Height                        = 272;
        astSampleVdec[i].enMode                           = VIDEO_MODE_FRAME;
        astSampleVdec[i].stSapmleVdecVideo.enDecMode      = VIDEO_DEC_MODE_IPB;
        astSampleVdec[i].stSapmleVdecVideo.enBitWidth     = DATA_BITWIDTH_8;
        astSampleVdec[i].stSapmleVdecVideo.u32RefFrameNum = 3;
        astSampleVdec[i].u32DisplayFrameNum               = 2;
        astSampleVdec[i].u32FrameBufCnt = astSampleVdec[i].stSapmleVdecVideo.u32RefFrameNum + astSampleVdec[i].u32DisplayFrameNum + 1;
    }
    s32Ret = SAMPLE_COMM_VDEC_InitVBPool(u32VdecChnNum, &astSampleVdec[0]);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("init mod common vb fail for %#x!\n", s32Ret);
        goto END2;
    }

    /************************************************
    step3:  start VDEC
    *************************************************/
    s32Ret = SAMPLE_COMM_VDEC_Start(u32VdecChnNum, &astSampleVdec[0]);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("start VDEC fail for %#x!\n", s32Ret);
        goto END3;
    }

    /************************************************
    step4:  start VPSS
    *************************************************/
    stVpssGrpAttr.u32MaxW = 3840;
    stVpssGrpAttr.u32MaxH = 2160;
    stVpssGrpAttr.stFrameRate.s32SrcFrameRate = -1;
    stVpssGrpAttr.stFrameRate.s32DstFrameRate = -1;
    stVpssGrpAttr.enDynamicRange = DYNAMIC_RANGE_SDR8;
    stVpssGrpAttr.enPixelFormat  = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    stVpssGrpAttr.bNrEn   = HI_FALSE;

    memset(abChnEnable, 0, sizeof(abChnEnable));
    abChnEnable[0] = HI_TRUE;
    astVpssChnAttr[0].u32Width                    = stDispSizeUHD.u32Width;
    astVpssChnAttr[0].u32Height                   = stDispSizeUHD.u32Height;
    astVpssChnAttr[0].enChnMode                   = VPSS_CHN_MODE_USER;
    astVpssChnAttr[0].enCompressMode              = COMPRESS_MODE_SEG;
    astVpssChnAttr[0].enDynamicRange              = DYNAMIC_RANGE_SDR8;
    astVpssChnAttr[0].enPixelFormat               = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    astVpssChnAttr[0].stFrameRate.s32SrcFrameRate = -1;
    astVpssChnAttr[0].stFrameRate.s32DstFrameRate = -1;
    astVpssChnAttr[0].u32Depth                    = 0;
    astVpssChnAttr[0].bMirror                     = HI_FALSE;
    astVpssChnAttr[0].bFlip                       = HI_FALSE;
    astVpssChnAttr[0].stAspectRatio.enMode        = ASPECT_RATIO_NONE;
    astVpssChnAttr[0].enVideoFormat               = VIDEO_FORMAT_LINEAR;
    for(i=0; i<u32VdecChnNum; i++)
    {
        VpssGrp = i;
        s32Ret = SAMPLE_COMM_VPSS_Start(VpssGrp, &abChnEnable[0], &stVpssGrpAttr, &astVpssChnAttr[0]);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("start VPSS fail for %#x!\n", s32Ret);
            goto END4;
        }
    }

    /************************************************
    step5:  start VO Dev UHD(DHD0-mipi_tx)
    *************************************************/
    //stDispSizeUHD.u32Width  = 1280;
    //stDispSizeUHD.u32Height = 720;
    stDispSizeUHD.u32Width  = 1280;
    stDispSizeUHD.u32Height = 800;
    stVoConfigUHD.VoDev                 = SAMPLE_VO_DEV_HD;
    stVoConfigUHD.enVoIntfType          = VO_INTF_MIPI;
    //stVoConfigUHD.enIntfSync            = VO_OUTPUT_720P60;
    //stVoConfigUHD.enIntfSync            = VO_OUTPUT_1280x800_60;
    stVoConfigUHD.enIntfSync            = VO_OUTPUT_USER;
	//stVoConfigUHD.enPicSize             = PIC_720P;
    stVoConfigUHD.enPicSize             = PIC_1280x800;
    stVoConfigUHD.u32BgColor            = COLOR_RGB_BLUE;
    stVoConfigUHD.u32DisBufLen          = 3;
    stVoConfigUHD.enDstDynamicRange     = DYNAMIC_RANGE_SDR8;
    stVoConfigUHD.enVoMode              = VO_MODE_1MUX;
    stVoConfigUHD.enPixFormat           = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    stVoConfigUHD.stDispRect.s32X       = 0;
    stVoConfigUHD.stDispRect.s32Y       = 0;
    stVoConfigUHD.stDispRect.u32Width   = stDispSizeUHD.u32Width;
    stVoConfigUHD.stDispRect.u32Height  = stDispSizeUHD.u32Height;
    stVoConfigUHD.stImageSize.u32Width  = stDispSizeUHD.u32Width;
    stVoConfigUHD.stImageSize.u32Height = stDispSizeUHD.u32Height;
    stVoConfigUHD.enVoPartMode          = VO_PART_MODE_SINGLE;
    s32Ret = SAMPLE_COMM_VO_StartVO2(&stVoConfigUHD);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("start VO %d fail for %#x!\n", stVoConfigUHD.VoDev, s32Ret);
        goto END5;
    }

    /************************************************
    step6:  VDEC bind VPSS
    *************************************************/
    for(i=0; i<u32VdecChnNum; i++)
    {
        s32Ret = SAMPLE_COMM_VDEC_Bind_VPSS(i, i);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("vdec bind vpss fail for %#x!\n", s32Ret);
            goto END7;
        }
    }

    /************************************************
    step7:  VPSS bind VO Layer 0
    *************************************************/
    VoLayer = stVoConfigUHD.VoDev;
    for(i=0; i<VpssGrpNum; i++)
    {
        s32Ret = SAMPLE_COMM_VPSS_Bind_VO(i, 0, VoLayer, i);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("vpss bind vo fail for %#x!\n", s32Ret);
            goto END8;
        }
    }

    /************************************************
    step8:  send stream to VDEC
    *************************************************/
    for(i=0; i<u32VdecChnNum; i++)
    {
        snprintf(stVdecSend[i].cFileName, sizeof(stVdecSend[i].cFileName), "3840x2160_8bit.h264");
        //snprintf(stVdecSend[i].cFileName, sizeof(stVdecSend[i].cFileName), "bigbuckbunny_480x272.h264");
        snprintf(stVdecSend[i].cFilePath, sizeof(stVdecSend[i].cFilePath), "%s", SAMPLE_STREAM_PATH);
        stVdecSend[i].enType          = astSampleVdec[i].enType;
        stVdecSend[i].s32StreamMode   = astSampleVdec[i].enMode;
        stVdecSend[i].s32ChnId        = i;
        stVdecSend[i].s32IntervalTime = 1000;
        stVdecSend[i].u64PtsInit      = 0;
        stVdecSend[i].u64PtsIncrease  = 0;
        stVdecSend[i].eThreadCtrl     = THREAD_CTRL_START;
        stVdecSend[i].bCircleSend     = HI_TRUE;
        stVdecSend[i].s32MilliSec     = 0;
        stVdecSend[i].s32MinBufSize   = (astSampleVdec[i].u32Width * astSampleVdec[i].u32Height * 3)>>1;
    }
    SAMPLE_COMM_VDEC_StartSendStream(u32VdecChnNum, &stVdecSend[0], &VdecThread[0]);

    SAMPLE_COMM_VDEC_CmdCtrl(u32VdecChnNum, &stVdecSend[0], &VdecThread[0]);

    SAMPLE_COMM_VDEC_StopSendStream(u32VdecChnNum, &stVdecSend[0], &VdecThread[0]);

END9:
    VoLayer = stVoConfigHD.VoDev;
    for(i=0; i<VpssGrpNum; i++)
    {
        s32Ret = SAMPLE_COMM_VPSS_UnBind_VO(i, 0, VoLayer, i);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("vpss unbind vo (%d,%d) fail for %#x!\n",VoLayer, i, s32Ret);
        }
    }
END8:
    VoLayer = stVoConfigUHD.VoDev;
    for(i=0; i<VpssGrpNum; i++)
    {
        s32Ret = SAMPLE_COMM_VPSS_UnBind_VO(i, 0, VoLayer, i);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("vpss unbind vo (%d,%d) fail for %#x!\n",VoLayer, i, s32Ret);
        }
    }
END7:
    for(i=0; i<u32VdecChnNum; i++)
    {
        s32Ret = SAMPLE_COMM_VDEC_UnBind_VPSS(i, i);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("vdec unbind vpss fail for %#x!\n", s32Ret);
        }
    }
END6:
    SAMPLE_COMM_VO_StopVO(&stVoConfigHD);
END5:
    SAMPLE_COMM_VO_StopVO(&stVoConfigUHD);

END4:
    for(i = VpssGrp; i >= 0; i--)
    {
        VpssGrp = i;
        SAMPLE_COMM_VPSS_Stop(VpssGrp, &abChnEnable[0]);
    }
END3:
    SAMPLE_COMM_VDEC_Stop(u32VdecChnNum);

END2:
    SAMPLE_COMM_VDEC_ExitVBPool();

END1:
    SAMPLE_COMM_SYS_Exit();

    return s32Ret;
}


HI_S32 SAMPLE_H264_VDEC_VPSS_VO_H265_VDEC_VPSS_VO_MIPI_Tx2(HI_VOID)
{
    VB_CONFIG_S         stVbConfig;
    HI_S32              i, s32Ret     = HI_SUCCESS;
    VDEC_THREAD_PARAM_S stVdecSend[VDEC_MAX_CHN_NUM];
    SIZE_S              stDispSizeUHD = {0};
    SIZE_S              stDispSizeHD  = {0};
    VO_LAYER            VoLayer;
    HI_U32              u32VdecChnNum, VpssGrpNum;
    VPSS_GRP            VpssGrp;
    pthread_t           VdecThread[2*VDEC_MAX_CHN_NUM];
    PIC_SIZE_E          enDispPicSize;
    SAMPLE_VDEC_ATTR    astSampleVdec[VDEC_MAX_CHN_NUM];
    VPSS_CHN_ATTR_S     astVpssChnAttr[VPSS_MAX_CHN_NUM];
    SAMPLE_VO_CONFIG_S  stVoConfigUHD;
    SAMPLE_VO_CONFIG_S  stVoConfigHD;
    VPSS_GRP_ATTR_S     stVpssGrpAttr;
    HI_BOOL             abChnEnable[VPSS_MAX_CHN_NUM];
    VO_INTF_SYNC_E      enIntfSync;
    VPSS_MOD_PARAM_S    stModParam;

    u32VdecChnNum = 2;
    VpssGrpNum    = u32VdecChnNum;

    /************************************************
    step1:  init SYS, init common VB(for VPSS and VO)
    *************************************************/
    if(VO_OUTPUT_3840x2160_30 == g_enIntfSync)
    {
        enDispPicSize = PIC_3840x2160;
        enIntfSync    = VO_OUTPUT_3840x2160_30;
    }
    else if(VO_OUTPUT_1080P30 == g_enIntfSync)
    {
        enDispPicSize = PIC_1080P;
        enIntfSync    = VO_OUTPUT_1080P30;
    }
    else if(VO_OUTPUT_1080P60 == g_enIntfSync)
    {
        enDispPicSize = PIC_1080P;
        enIntfSync    = VO_OUTPUT_1080P60;
    }

    s32Ret =  SAMPLE_COMM_SYS_GetPicSize(enDispPicSize, &stDispSizeUHD);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("sys get pic size fail for %#x!\n", s32Ret);
        goto END1;
    }

    memset(&stVbConfig, 0, sizeof(VB_CONFIG_S));
    stVbConfig.u32MaxPoolCnt             = 1;
    stVbConfig.astCommPool[0].u32BlkCnt  = 10*u32VdecChnNum;
    stVbConfig.astCommPool[0].u64BlkSize = COMMON_GetPicBufferSize(stDispSizeUHD.u32Width, stDispSizeUHD.u32Height,
                                                PIXEL_FORMAT_YVU_SEMIPLANAR_420, DATA_BITWIDTH_8, COMPRESS_MODE_SEG, 0);
    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConfig);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("init sys fail for %#x!\n", s32Ret);
        goto END1;
    }

    /************************************************
    step2:  init module VB or user VB(for VDEC)
    *************************************************/
    for(i=0; i<u32VdecChnNum; i++)
    {
        astSampleVdec[i].enType                           = PT_H264;
        astSampleVdec[i].u32Width                         = 3840;
        astSampleVdec[i].u32Height                        = 2160;
        astSampleVdec[i].enMode                           = VIDEO_MODE_FRAME;
        astSampleVdec[i].stSapmleVdecVideo.enDecMode      = VIDEO_DEC_MODE_IPB;
        astSampleVdec[i].stSapmleVdecVideo.enBitWidth     = DATA_BITWIDTH_8;
        astSampleVdec[i].stSapmleVdecVideo.u32RefFrameNum = 3;
        astSampleVdec[i].u32DisplayFrameNum               = 2;
        astSampleVdec[i].u32FrameBufCnt = astSampleVdec[i].stSapmleVdecVideo.u32RefFrameNum + astSampleVdec[i].u32DisplayFrameNum + 1;
    }
    astSampleVdec[1].enType = PT_H265;
    
    s32Ret = SAMPLE_COMM_VDEC_InitVBPool(u32VdecChnNum, &astSampleVdec[0]);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("init mod common vb fail for %#x!\n", s32Ret);
        goto END2;
    }

    /************************************************
    step3:  start VDEC
    *************************************************/
    s32Ret = SAMPLE_COMM_VDEC_Start(u32VdecChnNum, &astSampleVdec[0]);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("start VDEC fail for %#x!\n", s32Ret);
        goto END3;
    }

    /************************************************
    step4:  start VPSS
    *************************************************/
    s32Ret = HI_MPI_VPSS_GetModParam(&stModParam);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("Get VPSS module param fali %#x!\n", s32Ret);
        goto END4;
    }
    
    stModParam.bHdrSupport = HI_TRUE;
    
    s32Ret = HI_MPI_VPSS_SetModParam(&stModParam);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("Set VPSS module param fali %#x!\n", s32Ret);
        goto END4;
    }

    
    stVpssGrpAttr.u32MaxW = 3840;
    stVpssGrpAttr.u32MaxH = 2160;
    stVpssGrpAttr.stFrameRate.s32SrcFrameRate = -1;
    stVpssGrpAttr.stFrameRate.s32DstFrameRate = -1;
    stVpssGrpAttr.enDynamicRange = DYNAMIC_RANGE_SDR8;
    stVpssGrpAttr.enPixelFormat  = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    stVpssGrpAttr.bNrEn   = HI_FALSE;

    memset(abChnEnable, 0, sizeof(abChnEnable));
    abChnEnable[0] = HI_TRUE;
    astVpssChnAttr[0].u32Width                    = stDispSizeUHD.u32Width;
    astVpssChnAttr[0].u32Height                   = stDispSizeUHD.u32Height;
    astVpssChnAttr[0].enChnMode                   = VPSS_CHN_MODE_USER;
    astVpssChnAttr[0].enCompressMode              = COMPRESS_MODE_SEG;
    astVpssChnAttr[0].enDynamicRange              = DYNAMIC_RANGE_SDR8;
    astVpssChnAttr[0].enPixelFormat               = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    astVpssChnAttr[0].stFrameRate.s32SrcFrameRate = -1;
    astVpssChnAttr[0].stFrameRate.s32DstFrameRate = -1;
    astVpssChnAttr[0].u32Depth                    = 0;
    astVpssChnAttr[0].bMirror                     = HI_FALSE;
    astVpssChnAttr[0].bFlip                       = HI_FALSE;
    astVpssChnAttr[0].stAspectRatio.enMode        = ASPECT_RATIO_NONE;
    astVpssChnAttr[0].enVideoFormat               = VIDEO_FORMAT_LINEAR;
//	    abChnEnable[1] = HI_TRUE;
//	    astVpssChnAttr[1].u32Width                    = stDispSizeUHD.u32Width;
//	    astVpssChnAttr[1].u32Height                   = stDispSizeUHD.u32Height;
//	    astVpssChnAttr[1].enChnMode                   = VPSS_CHN_MODE_USER;
//	    astVpssChnAttr[1].enCompressMode              = COMPRESS_MODE_SEG;
//	    astVpssChnAttr[1].enDynamicRange              = DYNAMIC_RANGE_SDR8;
//	    astVpssChnAttr[1].enPixelFormat               = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
//	    astVpssChnAttr[1].stFrameRate.s32SrcFrameRate = -1;
//	    astVpssChnAttr[1].stFrameRate.s32DstFrameRate = -1;
//	    astVpssChnAttr[1].u32Depth                    = 0;
//	    astVpssChnAttr[1].bMirror                     = HI_FALSE;
//	    astVpssChnAttr[1].bFlip                       = HI_FALSE;
//	    astVpssChnAttr[1].stAspectRatio.enMode        = ASPECT_RATIO_NONE;
//	    astVpssChnAttr[1].enVideoFormat               = VIDEO_FORMAT_LINEAR;
    for(i=0; i<u32VdecChnNum; i++)
    {
        VpssGrp = i;
        s32Ret = SAMPLE_COMM_VPSS_Start(VpssGrp, &abChnEnable[0], &stVpssGrpAttr, &astVpssChnAttr[0]);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("start VPSS fail for %#x!\n", s32Ret);
            goto END4;
        }
    }

    /************************************************
    step5:  start VO Dev UHD(DHD0-hdmi)
    *************************************************/
    stVoConfigUHD.VoDev                 = SAMPLE_VO_DEV_UHD;
    stVoConfigUHD.enVoIntfType          = VO_INTF_HDMI;
    stVoConfigUHD.enIntfSync            = enIntfSync;
    stVoConfigUHD.enPicSize             = enDispPicSize;
    stVoConfigUHD.u32BgColor            = COLOR_RGB_BLUE;
    stVoConfigUHD.u32DisBufLen          = 3;
    stVoConfigUHD.enDstDynamicRange     = DYNAMIC_RANGE_SDR8;
    stVoConfigUHD.enVoMode              = VO_MODE_1MUX;
    stVoConfigUHD.enPixFormat           = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    stVoConfigUHD.stDispRect.s32X       = 0;
    stVoConfigUHD.stDispRect.s32Y       = 0;
    stVoConfigUHD.stDispRect.u32Width   = stDispSizeUHD.u32Width;
    stVoConfigUHD.stDispRect.u32Height  = stDispSizeUHD.u32Height;
    stVoConfigUHD.stImageSize.u32Width  = stDispSizeUHD.u32Width;
    stVoConfigUHD.stImageSize.u32Height = stDispSizeUHD.u32Height;
    stVoConfigUHD.enVoPartMode          = VO_PART_MODE_SINGLE;
    s32Ret = SAMPLE_COMM_VO_StartVO(&stVoConfigUHD);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("start VO %d fail for %#x!\n", stVoConfigUHD.VoDev, s32Ret);
        goto END5;
    }

    /************************************************
    step6:  start VO Dev HD(DHD1-mipi_tx)
    *************************************************/
    //stDispSizeHD.u32Width  = 1080;
    //stDispSizeHD.u32Height = 1920;
    stDispSizeHD.u32Width  = 1280;
    stDispSizeHD.u32Height = 800;
    //stDispSizeHD.u32Height = 720;
    stVoConfigHD.VoDev                 = SAMPLE_VO_DEV_HD;
    stVoConfigHD.enVoIntfType          = VO_INTF_MIPI;
    //stVoConfigHD.enIntfSync            = VO_OUTPUT_1080x1920_60;
    //stVoConfigHD.enIntfSync            = VO_OUTPUT_1280x800_60;
    //stVoConfigHD.enIntfSync            = VO_OUTPUT_720P60;
    stVoConfigHD.enIntfSync            = VO_OUTPUT_USER;
    //stVoConfigHD.enPicSize             = PIC_720P;
    stVoConfigHD.enPicSize             = PIC_1280x800;
    stVoConfigHD.u32BgColor            = COLOR_RGB_CYN;
    stVoConfigHD.u32DisBufLen          = 3;
    stVoConfigHD.enDstDynamicRange     = DYNAMIC_RANGE_SDR8;
    stVoConfigHD.enVoMode              = VO_MODE_1MUX;
    stVoConfigHD.enPixFormat           = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    stVoConfigHD.stDispRect.s32X       = 0;
    stVoConfigHD.stDispRect.s32Y       = 0;
    stVoConfigHD.stDispRect.u32Width   = stDispSizeHD.u32Width;
    stVoConfigHD.stDispRect.u32Height  = stDispSizeHD.u32Height;
    stVoConfigHD.stImageSize.u32Width  = stDispSizeHD.u32Width;
    stVoConfigHD.stImageSize.u32Height = stDispSizeHD.u32Height;
    stVoConfigHD.enVoPartMode          = VO_PART_MODE_SINGLE;
    s32Ret = SAMPLE_COMM_VO_StartVO2(&stVoConfigHD);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("start VO %d fail for %#x!\n", stVoConfigHD.VoDev, s32Ret);
        goto END6;
    }


    /************************************************
    step7:  VDEC bind VPSS
    *************************************************/
    for(i=0; i<u32VdecChnNum; i++)
    {
        s32Ret = SAMPLE_COMM_VDEC_Bind_VPSS(i, i);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("vdec bind vpss fail for %#x!\n", s32Ret);
            goto END7;
        }
    }

    /************************************************
    step8:  VPSS bind VO Layer 0
    *************************************************/
    VoLayer = stVoConfigUHD.VoDev;
    //for(i=0; i<VpssGrpNum; i++)
    {
        s32Ret = SAMPLE_COMM_VPSS_Bind_VO(0, 0, VoLayer, 0);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("vpss bind vo fail for %#x!\n", s32Ret);
            goto END8;
        }
    }

    /************************************************
    step9:  VPSS bind VO Layer 1
    *************************************************/
    VoLayer = stVoConfigHD.VoDev;
    //for(i=0; i<VpssGrpNum; i++)
    {
        s32Ret = SAMPLE_COMM_VPSS_Bind_VO(1, 0, VoLayer, 0);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("vpss bind vo fail for %#x!\n", s32Ret);
            goto END9;
        }
    }

    /************************************************
    step10:  send stream to VDEC
    *************************************************/
    for(i=0; i<u32VdecChnNum; i++)
    {
        snprintf(stVdecSend[i].cFileName, sizeof(stVdecSend[i].cFileName), "3840x2160_8bit.h264");
        snprintf(stVdecSend[i].cFilePath, sizeof(stVdecSend[i].cFilePath), "%s", SAMPLE_STREAM_PATH);
        stVdecSend[i].enType          = astSampleVdec[i].enType;
        stVdecSend[i].s32StreamMode   = astSampleVdec[i].enMode;
        stVdecSend[i].s32ChnId        = i;
        stVdecSend[i].s32IntervalTime = 1000;
        stVdecSend[i].u64PtsInit      = 0;
        stVdecSend[i].u64PtsIncrease  = 0;
        stVdecSend[i].eThreadCtrl     = THREAD_CTRL_START;
        stVdecSend[i].bCircleSend     = HI_TRUE;
        stVdecSend[i].s32MilliSec     = 0;
        stVdecSend[i].s32MinBufSize   = (astSampleVdec[i].u32Width * astSampleVdec[i].u32Height * 3)>>1;
    }
    snprintf(stVdecSend[1].cFileName, sizeof(stVdecSend[1].cFileName), "3840x2160_8bit.h265");
    
    SAMPLE_COMM_VDEC_StartSendStream(u32VdecChnNum, &stVdecSend[0], &VdecThread[0]);

    SAMPLE_COMM_VDEC_CmdCtrl(u32VdecChnNum, &stVdecSend[0], &VdecThread[0]);

    SAMPLE_COMM_VDEC_StopSendStream(u32VdecChnNum, &stVdecSend[0], &VdecThread[0]);

END9:
    VoLayer = stVoConfigHD.VoDev;
    //for(i=0; i<VpssGrpNum; i++)
    {
        //s32Ret = SAMPLE_COMM_VPSS_UnBind_VO(i, 0, VoLayer, i);
        s32Ret = SAMPLE_COMM_VPSS_UnBind_VO(1, 0, VoLayer, 0);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("vpss unbind vo (%d,%d) fail for %#x!\n",VoLayer, i, s32Ret);
        }
    }
END8:
    VoLayer = stVoConfigUHD.VoDev;
    //for(i=0; i<VpssGrpNum; i++)
    {
        //s32Ret = SAMPLE_COMM_VPSS_UnBind_VO(i, 0, VoLayer, i);
        s32Ret = SAMPLE_COMM_VPSS_UnBind_VO(0, 0, VoLayer, 0);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("vpss unbind vo (%d,%d) fail for %#x!\n",VoLayer, i, s32Ret);
        }
    }
END7:
    for(i=0; i<u32VdecChnNum; i++)
    {
        s32Ret = SAMPLE_COMM_VDEC_UnBind_VPSS(i, i);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("vdec unbind vpss fail for %#x!\n", s32Ret);
        }
    }
END6:
    SAMPLE_COMM_VO_StopVO(&stVoConfigHD);
END5:
    SAMPLE_COMM_VO_StopVO(&stVoConfigUHD);

END4:
    for(i = VpssGrp; i >= 0; i--)
    {
        VpssGrp = i;
        SAMPLE_COMM_VPSS_Stop(VpssGrp, &abChnEnable[0]);
    }
END3:
    SAMPLE_COMM_VDEC_Stop(u32VdecChnNum);

END2:
    SAMPLE_COMM_VDEC_ExitVBPool();

END1:
    SAMPLE_COMM_SYS_Exit();

    return s32Ret;
}



/******************************************************************************
* function    : main()
* Description : video vdec sample
******************************************************************************/
#ifdef __HuaweiLite__
    int app_main(int argc, char *argv[])
#else
    int main(int argc, char *argv[])
#endif
{
    HI_S32 s32Ret = HI_SUCCESS;

    if ((argc < 2) || (1 != strlen(argv[1])))
    {
        printf("\nInvaild input!  For examples:\n");
        SAMPLE_VDEC_Usage(argv[0]);
        return HI_FAILURE;
    }

    if ((argc > 2) && ('1' == *argv[2]))
    {
        g_enIntfSync = VO_OUTPUT_1080P30;
    }
    else if ((argc > 2) && ('2' == *argv[2]))
    {
		g_enIntfSync = VO_OUTPUT_1080P60;
	}
    else
    {
        g_enIntfSync = VO_OUTPUT_3840x2160_30;
    }

#ifndef __HuaweiLite__
    signal(SIGINT, SAMPLE_VDEC_HandleSig);
    signal(SIGTERM, SAMPLE_VDEC_HandleSig);
#endif

    /******************************************
     choose the case
    ******************************************/
    switch (*argv[1])
    {
        case '0':
        {
            s32Ret = SAMPLE_H265_VDEC_VPSS_VO();
            break;
        }
        case '1':
        {
            s32Ret = SAMPLE_H264_VDEC_VPSS_VO();
            break;
        }
        case '2':
        {
            s32Ret = SAMPLE_JPEG_VDEC_VPSS_VO();
            break;
        }
        case '3':
        {
            s32Ret = SAMPLE_JPEG_VDEC_To_RGB();
            break;
        }
        case '4':
        {
            s32Ret = SAMPLE_H264_VDEC_VPSS_VO_MIPI_Tx();
            break;
        }
        case '5':
        {
            s32Ret = SAMPLE_H264_VDEC_VPSS_VO_H265_VDEC_VPSS_VO_MIPI_Tx2();
            break;
        }
        case '6':
        {
            s32Ret = SAMPLE_H265_VDEC_VPSS_VO2();
            break;
        }
        case '7':
        {
            s32Ret = SAMPLE_H265_VDEC_VPSS_VO_MIPI_Tx();
            break;
        }
        case '8':
        {
            s32Ret = SAMPLE_H264_VDEC_VPSS_VO_MIPI_Tx2();
            break;
        }
        default :
        {
            SAMPLE_PRT("the index is invaild!\n");
            SAMPLE_VDEC_Usage(argv[0]);
            s32Ret = HI_FAILURE;
            break;
        }
    }

#if 0
    if (0 != g_stHifbThread)
    {
        pthread_join(g_stHifbThread, 0);
        g_stHifbThread = 0;
    }

    if (0 != g_stHifbThread1)
    {
        pthread_join(g_stHifbThread1, 0);
        g_stHifbThread1 = 0;
    }
#endif

    if (HI_SUCCESS == s32Ret)
    {
        SAMPLE_PRT("program exit normally!\n");
    }
    else
    {
        SAMPLE_PRT("program exit abnormally!\n");
    }

    return s32Ret;
}


