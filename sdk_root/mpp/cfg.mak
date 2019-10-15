#
# Automatically generated file; DO NOT EDIT.
# BVT SDK Platform Configuration
#

#
# General Setup
#
# CONFIG_HI3559AV100ES is not set
export CONFIG_HI3559AV100=y
# CONFIG_HI3519AV100 is not set
# CONFIG_HI3516CV500 is not set
# CONFIG_HI3516EV200 is not set
export CONFIG_HI_CHIP_TYPE=0x3559A100
export CONFIG_HI_ARCH=hi3559av100
export CONFIG_SMP=y
# CONFIG_AMP is not set
export CONFIG_ARM_ARCH_TYPE=smp
# CONFIG_CPU_TYPE_SINGLE is not set
# CONFIG_CPU_TYPE_BIG_LITTLE is not set
export CONFIG_CPU_TYPE_MULTI_CORE=y
export CONFIG_CPU_TYPE=multi-core
export CONFIG_VERSION_ASIC=y
export CONFIG_HI_FPGA=n
export CONFIG_LINUX_OS=y
# CONFIG_HUAWEI_LITEOS_OS is not set
export CONFIG_OS_TYPE=linux
export CONFIG_LINUX_4_9_y=y
export CONFIG_KERNEL_VERSION=linux-4.9.y
# CONFIG_KERNEL_AARCH64_HISIV610 is not set
export CONFIG_KERNEL_AARCH64_HIMIX100=y
export CONFIG_HI_CROSS=aarch64-himix100-linux-
export CONFIG_LIBC_TYPE=glibc
export CONFIG_KERNEL_BIT=KERNEL_BIT_64
# CONFIG_USER_AARCH64_HISIV610 is not set
export CONFIG_USER_AARCH64_HIMIX100=y
export CONFIG_HI_CROSS_LIB=aarch64-himix100-linux-
export CONFIG_USER_BIT=USER_BIT_64
export CONFIG_RELEASE_TYPE_RELEASE=y
# CONFIG_RELEASE_TYPE_DEBUG is not set
export CONFIG_HI_RLS_MODE=HI_RELEASE

#
# Media Modules Setup
#

#
# media base config
#
export CONFIG_HI_VB_EXTPOOL_SUPPORT=y

#
# media sys config
#
export CONFIG_HI_SYS_SUPPORT=y
export CONFIG_HI_SYS_SCALE_COEF_SUPPORT=y
export CONFIG_HI_SYS_SCALE_COEF_ONLINE_SUPPORT=y

#
# media vi config
#
export CONFIG_HI_VI_SUPPORT=y
export CONFIG_HI_VI_BT656=y
export CONFIG_HI_VI_MIPI=y
export CONFIG_HI_ISP_2DOF_DIS_SUPPORT=y
# CONFIG_HI_VI_MONO_COLOR_FUSION is not set
export CONFIG_HI_VI_LDCV1_SUPPORT=y
# CONFIG_HI_VI_LDCV2_SUPPORT is not set
# CONFIG_HI_VI_LDCV3_SUPPORT is not set
export CONFIG_HI_VI_SENDYUV_SUPPORT=y
export CONFIG_HI_VI_SRC_SUPPORT=y
export CONFIG_HI_VI_STITCH_SUPPORT=y
# CONFIG_HI_VI_CHN_EARLY_INT_SUPPORT is not set
export CONFIG_HI_VI_PARALLEL_SUPPORT=y
export CONFIG_HI_VI_3DNRX_SUPPORT=y
export CONFIG_HI_VI_USERPIC_SUPPORT=y
export CONFIG_HI_VI_COVEREX_OVERLAYEX_SUPPORT=y
export CONFIG_HI_VI_LUMA_SUPPORT=y
export CONFIG_HI_VI_FPN_SUPPORT=y
export CONFIG_HI_VI_EXTCHN_SUPPORT=y
# CONFIG_HI_VI_CASCADE_SUPPORT is not set
export CONFIG_HI_SNAP_SUPPORT=y
export CONFIG_HI_PIPE_RECEIVER=y

#
# media isp config
#
export CONFIG_HI_ISP_SUPPORT=y

#
# media dis config
#
export CONFIG_HI_DIS_SUPPORT=y

#
# media vpss config
#
export CONFIG_HI_VPSS_SUPPORT=y
export CONFIG_HI_VPSS_SBS=y
export CONFIG_HI_VPSS_SHARPEN=y
export CONFIG_HI_VPSS_2SCALE_SUPPORT=y

#
# media avs config
#
export CONFIG_HI_AVS_SUPPORT=y

#
# media gdc config
#
export CONFIG_HI_GDC_SUPPORT=y
export CONFIG_HI_GDC_FISHEYE_SUPPORT=y
# CONFIG_HI_GDC_NNIE_SHARE_RAM is not set

#
# media vgs config
#
export CONFIG_HI_VGS_SUPPORT=y
export CONFIG_HI_VGS_STITCH_SUPPORT=y
# CONFIG_HI_VGS_IVE_PRE_SUPPORT is not set

#
# media chnl config
#
export CONFIG_HI_CHNL_SUPPORT=y

#
# media venc config
#
export CONFIG_HI_VENC_SUPPORT=y
export CONFIG_HI_H265E_SUPPORT=y
export CONFIG_HI_H264E_SUPPORT=y
export CONFIG_HI_JPEGE_SUPPORT=y
export CONFIG_HI_PRORES_SUPPORT=y
export CONFIG_HI_LOWDELAY_SUPPORT=y
export CONFIG_HI_JPEGE_DCF=y
# CONFIG_HI_BUFFSHARE_SUPPORT is not set
export CONFIG_HI_ADVSMARTP_SUPPORT=y
export CONFIG_HI_BIPREDB_SUPPORT=y

#
# media vdec config
#
export CONFIG_HI_VDEC_SUPPORT=y
export CONFIG_HI_H265D_SUPPORT=y
export CONFIG_HI_H264D_SUPPORT=y
export CONFIG_VDEC_IP=VDEC_IP_VDH
export CONFIG_HI_JPEGD_SUPPORT=y
export CONFIG_HI_JPEGD_RGB_OUTPUT_SUPPORT=y
export CONFIG_VDEC_ROTATION_SUPPORT=y
export CONFIG_VDEC_USERPIC_SUPPORT=y
export CONFIG_VDEC_USERDATA_SUPPORT=y

#
# media vo config
#
export CONFIG_HI_VO_SUPPORT=y
export CONFIG_HI_VO_HD=y
export CONFIG_HI_VO_PLAY_CTL=y
export CONFIG_HI_VO_LUMA=y
export CONFIG_HI_VO_COVER_OSD_SUPPORT=y
export CONFIG_HI_VO_VGS=y
export CONFIG_HI_VO_GRAPH=y
export CONFIG_HI_VO_BATCH=y
export CONFIG_HI_VO_WBC=y

#
# media region config
#
export CONFIG_HI_REGION_SUPPORT=y

#
# media audio config
#
export CONFIG_HI_AUDIO_SUPPORT=y
export CONFIG_HI_ACODEC_SUPPORT=y
export CONFIG_HI_ACODEC_VERSION=V900
export CONFIG_HI_ACODEC_MAX_GAIN=56
export CONFIG_HI_ACODEC_MIN_GAIN=0
export CONFIG_HI_ACODEC_GAIN_STEP=3
export CONFIG_HI_ACODEC_FAST_POWER_SUPPORT=y
export CONFIG_HI_DOWNVQE_SUPPORT=y
export CONFIG_HI_TALKVQE_SUPPORT=y
export CONFIG_HI_RECORDVQE_SUPPORT=y

#
# media hdr config
#
export CONFIG_HI_HDR_SUPPORT=y

#
# media mcf config
#
# CONFIG_HI_MONO_COLOR_FUSION_SUPPORT is not set

#
# Device Driver Setup
#

#
# drv config
#
export CONFIG_DRV=y
export CONFIG_EXTDRV=y
export CONFIG_INTERDRV=y
export CONFIG_CIPHER=y
export CONFIG_HIUSER=y
export CONFIG_MIPI_TX=y
export CONFIG_MIPI_RX=y
export CONFIG_HI_IR=y
export CONFIG_HI_WDG=y
export CONFIG_HI_SYSCFG=y

#
# Component Setup
#

#
# Component hdmi Config
#
export CONFIG_HI_HDMI_SUPPORT=y

#
# Component hifb Config
#
export CONFIG_HI_HIFB_SUPPORT=y

#
# Component svp Config
#
export CONFIG_HI_SVP_SUPPORT=y
export CONFIG_HI_SVP_DSP=y
export CONFIG_HI_SVP_LITEOS=y
export CONFIG_HI_SVP_CNN=y
export CONFIG_HI_SVP_RUNTIME=y
export CONFIG_HI_SVP_IVE=y
export CONFIG_HI_SVP_MD=y
export CONFIG_HI_SVP_DPU_RECT=y
export CONFIG_HI_SVP_DPU_MATCH=y
# CONFIG_HI_SVP_OTHERS is not set

#
# Component photo Config
#
export CONFIG_HI_PHOTO_SUPPORT=y

#
# Component tde Config
#
export CONFIG_HI_TDE_SUPPORT=y

#
# Component pciv Config
#
export CONFIG_HI_PCIV_SUPPORT=y

#
# Component avs lut Config
#
export CONFIG_HI_AVS_LUT_SUPPORT=y

#
# Component pos_query Config
#
export CONFIG_HI_POS_QUERY_SUPPORT=y

#
# Component tzasc Config
#
# CONFIG_HI_TZASC_SUPPORT is not set

#
# Component motionfusion config
#
# CONFIG_HI_MOTIONFUSION_SUPPORT is not set

#
# Component pm Config
#
# CONFIG_HI_PM_SUPPORT is not set

#
# HISYSLINK Setup
#

#
# hisyslink config
#
export CONFIG_HISYSLINK=y

#
# Debug Config
#
export CONFIG_HI_GDB_NO=y
# CONFIG_HI_GDB_YES is not set
export CONFIG_HI_GDB=n
export CONFIG_HI_LOG_TRACE_SUPPORT=y
export CONFIG_HI_LOG_TRACE_ALL=y
# CONFIG_HI_LOG_TRACE_ERR is not set
# CONFIG_HI_LOG_TRACE_WARN is not set
# CONFIG_HI_LOG_TRACE_INFO is not set
export CONFIG_HI_LOG_TRACE_LEVEL=7

#
# Test Config
#
