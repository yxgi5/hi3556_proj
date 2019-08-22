#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/version.h>
#include <linux/of_platform.h>
#include <asm/io.h>

#define SENSOR_LIST_CMDLINE_LEN     256
#define SENSOR_NAME_LEN             64

#define SYS_WRITEL(Addr, Value) ((*(volatile unsigned int *)(Addr)) = (Value))
#define SYS_READ(Addr)          (*((volatile int *)(Addr)))

static void  *reg_crg_base=0;
static void  *reg_sys_base=0;
static void  *reg_ddr_base=0;
static void  *reg_misc_base=0;
static void  *reg_iocfg_base=0;
static void  *reg_gpio_base=0;

static int g_online_flag = 0;
static int g_bt1120_flag = 0;
static char sensor_list[SENSOR_LIST_CMDLINE_LEN]= "sns0=ov9712";

module_param(g_online_flag, int, S_IRUGO);
module_param(g_bt1120_flag, int, S_IRUGO);

module_param_string(sensors, sensor_list, SENSOR_LIST_CMDLINE_LEN, 0600);

MODULE_PARM_DESC(sensors,"sns0=ov9712");

typedef enum {
    BUS_TYPE_I2C = 0,
    BUS_TYPE_SPI = 1,
} BUS_TYPE;

static int parse_sensor_index(char *s)
{
    char tmp[8];
    int i;
    char* line = NULL;
    int index = -1;

    line = strsep(&s, "=");

    if (NULL == line)
    {
        printk("FUNC:%s line:%d  err sensor index: [%s] \n", __FUNCTION__, __LINE__, s);
        return index;
    }

    for (i = 0; i < 8; i++)
    {
        snprintf(tmp, sizeof(tmp), "sns%d", i);

        if (0 == strncmp(tmp, line, 8))
        {
            index = i;
            return index;
        }
    }

    printk("FUNC:%s line:%d  SNS prefix:[%s] is not supported !\n", __FUNCTION__,__LINE__, line);

    return index;
}

static int parse_sensor_name(char *s, char *name)
{
    unsigned int len;
    char* line = NULL;

    line = strsep(&s, "=");

    line=strsep(&s, "=");
    if(NULL != line)
    {
        len = strlen(line);

        if(len >= SENSOR_NAME_LEN)
        {
            printk("FUNC:%s line:%d  name:[%s] is too long, can not longer than %d\n", __FUNCTION__,__LINE__,  line, SENSOR_NAME_LEN);

            return -1;
        }

        strncpy(name, line, SENSOR_NAME_LEN - 1);

        return 0;
    }

    return -1;
}

static inline void reg_write32(unsigned long value, unsigned long mask, unsigned long addr)
{
    unsigned long t;

    t = SYS_READ((const volatile void *)addr);
    t &= ~mask;
    t |= value & mask;
    SYS_WRITEL((volatile void *)addr, t);
}

static void sensor_clock_config(int index, int clock)
{
    int clock_index = index / 2;

    reg_write32(clock << (8 * clock_index), 0xF << (8 * clock_index), (unsigned long)reg_crg_base+0x0114);

    /* slave mode unreset*/
    reg_write32(0x0 << (6 + 8 * clock_index), 0x1 << (6 + 8 * clock_index), (unsigned long)reg_crg_base+0x0114);
}

static BUS_TYPE parse_sensor_bus_type(char *name)
{
    unsigned int len;
    BUS_TYPE bus_type = BUS_TYPE_I2C;

    len = SENSOR_NAME_LEN;

    if (   (0 == strncmp("imx377", name, len))
        || (0 == strncmp("imx334", name, len))
        || (0 == strncmp("imx477", name, len))
        || (0 == strncmp("imx290", name, len))
        || (0 == strncmp("imx290_slave", name, len))
        || (0 == strncmp("imx299", name, len))
        || (0 == strncmp("imx117", name, len))
        || (0 == strncmp("ov9712", name, len)))
    {
        bus_type = BUS_TYPE_I2C;
    }
    else if ((0 == strncmp("imx299_slvs", name, len))
          || (0 == strncmp("cmv50000", name, len))
          || (0 == strncmp("imx277_slvs", name, len))
          || (0 == strncmp("imx117", name, len)))
    {
        bus_type = BUS_TYPE_SPI;
    }
    else
    {
        printk("FUNC:%s line:%d  SNS:[%s] is not supported !\n", __FUNCTION__,__LINE__, name);
        bus_type = BUS_TYPE_I2C;
    }

    return bus_type;
}

/*
0x0: 74.25MHz;  0x1: 72MHz; 0x2: 54MHz; 0x3: 50MHz  0x4: 24MHz 0x6: 32.4MHz;
0x8: 37.125MHz; 0x9: 36MHz; 0xA: 27MHz; 0xB: 25MHz; 0xC: 12MHz;
*/
static int parse_sensor_clock(char *name)
{
    int clock = 0x0;
    unsigned int len;

    len = SENSOR_NAME_LEN;

    if ( (0 == strncmp("imx377", name, len))
         || (0 == strncmp("imx334", name, len))
         || (0 == strncmp("imx477", name, len))
         || (0 == strncmp("imx299", name, len))
         || (0 == strncmp("cmv50000", name, len))
         || (0 == strncmp("ov9712", name, len)))
    {
        clock = 0x4;
    }
    else if (0 == strncmp("imx290", name, len))
    {
        clock = 0x8;
    }
    else if (0 == strncmp("imx290_slave", name, len))
    {
        clock = 0x0;
    }
    else if ( (0 == strncmp("imx299_slvs", name, len))
              || (0 == strncmp("imx277_slvs", name, len))
              || (0 == strncmp("imx117", name, len)))
    {
        clock = 0x1;
    }
    else
    {
        printk("FUNC:%s line:%d  SNS:[%s] is not supported !\n", __FUNCTION__, __LINE__, name);
        return clock;
    }

    return clock;
}

static void bt1120_pin_mux(void)
{
//    himm(0x1F001084 ,0x000014f3); // iocfg_reg109 VOU1120_DATA8 cfg as VI0_CLK
//    himm(0x1F0010A4 ,0x000014f3); // iocfg_reg117 VOU1120_DATA7 cfg as VI0_DATA0
//    himm(0x1F0010A0 ,0x000014f3); // iocfg_reg116 VOU1120_DATA6 cfg as VI0_DATA1
//    himm(0x1F00109c ,0x000014f3); // iocfg_reg115 VOU1120_DATA5 cfg as VI0_DATA2
//    himm(0x1F001098 ,0x000014f3); // iocfg_reg114 VOU1120_DATA4 cfg as VI0_DATA3
//    himm(0x1F001094 ,0x000014f3); // iocfg_reg113 VOU1120_DATA3 cfg as VI0_DATA4
//    himm(0x1F001090 ,0x000014f3); // iocfg_reg112 VOU1120_DATA2 cfg as VI0_DATA5
//    himm(0x1F00108c ,0x000014f3); // iocfg_reg111 VOU1120_DATA1 cfg as VI0_DATA6
//    himm(0x1F001088 ,0x000014f3); // iocfg_reg110 VOU1120_DATA0 cfg as VI0_DATA7
//    himm(0x1F0010c4 ,0x000014f3); // iocfg_reg125 VOU1120_DATA15 cfg as VI0_DATA8
//    himm(0x1F0010c0 ,0x000014f3); // iocfg_reg124 VOU1120_DATA14 cfg as VI0_DATA9
//    himm(0x1F0010bc ,0x000014f3); // iocfg_reg123 VOU1120_DATA13 cfg as VI0_DATA10
//    himm(0x1F0010b8 ,0x000014f3); // iocfg_reg122 VOU1120_DATA12 cfg as VI0_DATA11
//    himm(0x1F0010b4 ,0x000014f3); // iocfg_reg121 VOU1120_DATA11 cfg as VI0_DATA12
//    himm(0x1F0010b0 ,0x000014f3); // iocfg_reg120 VOU1120_DATA10 cfg as VI0_DATA13
//    himm(0x1F0010ac ,0x000014f3); // iocfg_reg119 VOU1120_DATA9 cfg as VI0_DATA14
//    himm(0x1F0010a8 ,0x000014f3); // iocfg_reg118 VOU1120_DATA8 cfg as VI0_DATA15
    
    SYS_WRITEL(reg_iocfg_base+0x1088 ,0x000014f3);
    SYS_WRITEL(reg_iocfg_base+0x108C ,0x000014f3);
    SYS_WRITEL(reg_iocfg_base+0x1090 ,0x000014f3);
    SYS_WRITEL(reg_iocfg_base+0x1094 ,0x000014f3);
    SYS_WRITEL(reg_iocfg_base+0x1098 ,0x000014f3);
    SYS_WRITEL(reg_iocfg_base+0x109C ,0x000014f3);
    SYS_WRITEL(reg_iocfg_base+0x10A0 ,0x000014f3);
    SYS_WRITEL(reg_iocfg_base+0x10A4 ,0x000014f3);
    SYS_WRITEL(reg_iocfg_base+0x10A8 ,0x000014f3);
    SYS_WRITEL(reg_iocfg_base+0x10AC ,0x000014f3);
    SYS_WRITEL(reg_iocfg_base+0x10B0 ,0x000014f3);
    SYS_WRITEL(reg_iocfg_base+0x10B4 ,0x000014f3);
    SYS_WRITEL(reg_iocfg_base+0x10B8 ,0x000014f3);
    SYS_WRITEL(reg_iocfg_base+0x10BC ,0x000014f3);
    SYS_WRITEL(reg_iocfg_base+0x10C0 ,0x000014f3);
    SYS_WRITEL(reg_iocfg_base+0x10C4 ,0x000014f3);
    printk("============input bt1120_pin_mux done=============\n");
}

static void dc12_pin_mux(void)
{
//    himm(0x1F001084 ,0x000014f3); // iocfg_reg109 VOU1120_DATA8 cfg as VI0_CLK
//    himm(0x1F0010A4 ,0x000014f3); // iocfg_reg117 VOU1120_DATA7 cfg as VI0_DATA0
//    himm(0x1F0010A0 ,0x000014f3); // iocfg_reg116 VOU1120_DATA6 cfg as VI0_DATA1
//    himm(0x1F00109c ,0x000014f3); // iocfg_reg115 VOU1120_DATA5 cfg as VI0_DATA2
//    himm(0x1F001098 ,0x000014f3); // iocfg_reg114 VOU1120_DATA4 cfg as VI0_DATA3
//    himm(0x1F001094 ,0x000014f3); // iocfg_reg113 VOU1120_DATA3 cfg as VI0_DATA4
//    himm(0x1F001090 ,0x000014f3); // iocfg_reg112 VOU1120_DATA2 cfg as VI0_DATA5
//    himm(0x1F00108c ,0x000014f3); // iocfg_reg111 VOU1120_DATA1 cfg as VI0_DATA6
//    himm(0x1F001088 ,0x000014f3); // iocfg_reg110 VOU1120_DATA0 cfg as VI0_DATA7
//    himm(0x1F0010c4 ,0x000014f3); // iocfg_reg125 VOU1120_DATA15 cfg as VI0_DATA8
//    himm(0x1F0010c0 ,0x000014f3); // iocfg_reg124 VOU1120_DATA14 cfg as VI0_DATA9
//    himm(0x1F0010bc ,0x000014f3); // iocfg_reg123 VOU1120_DATA13 cfg as VI0_DATA10
//    himm(0x1F0010b8 ,0x000014f3); // iocfg_reg122 VOU1120_DATA12 cfg as VI0_DATA11
    
    SYS_WRITEL(reg_iocfg_base+0x1084 ,0x000014f3); // VI0_CLK
    SYS_WRITEL(reg_iocfg_base+0x1088 ,0x000014f3);
    SYS_WRITEL(reg_iocfg_base+0x108C ,0x000014f3);
    SYS_WRITEL(reg_iocfg_base+0x1090 ,0x000014f3);
    SYS_WRITEL(reg_iocfg_base+0x1094 ,0x000014f3);
    SYS_WRITEL(reg_iocfg_base+0x1098 ,0x000014f3);
    SYS_WRITEL(reg_iocfg_base+0x109C ,0x000014f3);
    SYS_WRITEL(reg_iocfg_base+0x10A0 ,0x000014f3);
    SYS_WRITEL(reg_iocfg_base+0x10A4 ,0x000014f3);
    
    SYS_WRITEL(reg_iocfg_base+0x10A8 ,0x000014f6); // VI0_VS
    SYS_WRITEL(reg_iocfg_base+0x10AC ,0x000014f6); // VI0_HS
//    SYS_WRITEL(reg_iocfg_base+0x10B0 ,0x000014f3);
//    SYS_WRITEL(reg_iocfg_base+0x10B4 ,0x000014f3);
    SYS_WRITEL(reg_iocfg_base+0x10B8 ,0x000014f3);
    SYS_WRITEL(reg_iocfg_base+0x10BC ,0x000014f3);
    SYS_WRITEL(reg_iocfg_base+0x10C0 ,0x000014f3);
    SYS_WRITEL(reg_iocfg_base+0x10C4 ,0x000014f3);
    printk("============digital_camera_pin_mux done=============\n");
}

static int is_coms(char *name)
{
    unsigned int len;

    len = SENSOR_NAME_LEN;

    if (   (0 == strncmp("bt1120", name, len))
        || (0 == strncmp("bt656", name, len))
        || (0 == strncmp("bt601", name, len)))
    {
		bt1120_pin_mux();
        return 1;
    }
	if ( (0 == strncmp("ov426", name, len))
	  || (0 == strncmp("ov9712", name, len)))
	{
		dc12_pin_mux();
		return 1;
	}
    else
    {
        return 0;
    }
}

// 3.2.6 CRG 寄存器概览 reg_crg_base = 0x12010000
static void coms_clock_config(int index)
{
    if(0 == index)
    {
        reg_write32(0x5 << 15, 0x7 << 15, (unsigned long)reg_crg_base+0x0104); // value 0x28000‬, mask 0x38000, vi_p5_cksel 0x101
    }
    else if(1 == index)
    {
        reg_write32(0x5 << 21, 0x6 << 21, (unsigned long)reg_crg_base+0x0104);
    }
    else if(2 == index)
    {
        reg_write32(0x5 << 9, 0x7 << 9, (unsigned long)reg_crg_base+0x0104);
    }
}

static int clkcfg(void)
{
    unsigned long lpvdp = 0x0;
    /* clk_cfg */
    SYS_WRITEL(reg_crg_base+0x0104, 0x006db6db);   //#MIPI_RX 600M
    SYS_WRITEL(reg_crg_base+0x0108, 0x006db6db);   //#ISP 600M
    SYS_WRITEL(reg_crg_base+0x010c, 0x000db030);   //#(VI_PPC VIPROC0/offline) 600M
    lpvdp = SYS_READ(reg_crg_base+0x0100);
    SYS_WRITEL(reg_crg_base+0x0100, 0x00002000 & lpvdp);   //#VICAP VIPROC ISP LOW-POWER clock
    SYS_WRITEL(reg_crg_base+0x011c, 0x00002011);
    SYS_WRITEL(reg_crg_base+0x011c, 0x00000011);   //# AIAO(50M 1188M)and MIPITX clk and reset(mipitx_ref_cksel)
    SYS_WRITEL(reg_crg_base+0x0120, 0x2815e4c3);   //# AIAO&LCD clk and reset(lcd_mclk_div)
    SYS_WRITEL(reg_crg_base+0x0160, 0x00000015);   //#(JPEGD 500M;  PNG 396M)
    SYS_WRITEL(reg_crg_base+0x0164, 0x00fdbfff);   //#(VEDU 710M; JPGE 750M;  VPSS GDC AVS 600M;  VGS 631M)
    SYS_WRITEL(reg_crg_base+0x0168, 0x00000005);   //#VPSS offline(IVE 750M;VDH 568M  GME 600M)

    /* power on */
    SYS_WRITEL(reg_misc_base+0x00ac, 0x33333332);  //# set MISC_CTRL43, NNIE 0/1, GDC1/VGS1, DSP 0/1/2/3, GPU power up
    SYS_WRITEL(reg_misc_base+0x00ac, 0x22222222);  //# set MISC_CTRL43, NNIE 0/1, GDC1/VGS1, DSP 0/1/2/3, GPU power up
    SYS_WRITEL(reg_misc_base+0x00b0, 0x00033333);  //# set MISC_CTRL44, AVSP VDH VEDU2 VPSS1 VIPROC1 power up
    SYS_WRITEL(reg_misc_base+0x00b0, 0x00022222);  //# set MISC_CTRL44, AVSP VDH VEDU2 VPSS1 VIPROC1 power up

    return 0;
}

static int sysctl(int online_flag)
{
    if (1 == online_flag) /*online*/
    {
                                                         //#each module 3bit:          30:28           26:24           22:20          18:16        14:12         10:8         6:4         2:0
        SYS_WRITEL( reg_misc_base+0x2000, 0x00000011);    //#each module 3bit:          SPACC             FMC           EDMA0          EDMA1          UFS         EMMC         GSF1         GSF0
        SYS_WRITEL( reg_misc_base+0x2004, 0x00454545);    //#each module 3bit:                                         vedu2(W)        vedu2(R)     vedu1(W)     vedu1(R)     vedu0(W)     vedu0(R)
        SYS_WRITEL( reg_misc_base+0x2008, 0x44444545);    //#each module 3bit:         vgs1(W)         vgs1(R)          vgs0(W)         vgs0(R)     vpss1(W)     vpss1(R)    vpss0 (W)     vpss0(R)
        SYS_WRITEL( reg_misc_base+0x200c, 0x55554545);    //#each module 3bit:         gdc1(W)         gdc1(R)          gdc0(W)         gdc0(R)     jpge1(W)     jpge1(R)     jpge0(W)     jpge0(R)
        SYS_WRITEL( reg_misc_base+0x2010, 0x55003333);    //#each module 3bit:          gme(W)          gme(R)          gzip(W)         gzip(R)      jpgd(W)      jpgd(R)       pgd(W)       pgd(R)
        SYS_WRITEL( reg_misc_base+0x2014, 0x67676767);    //#each module 3bit:      viproc1(W)      viproc1(R)       viproc0(W)      viproc0(R)     vi_m1(W)     vi_m1(R)     vi_m0(W)     vi_m0(R)
        SYS_WRITEL( reg_misc_base+0x2018, 0x33666767);    //#each module 3bit:          gpu(W)          gpu(R)           aio(W)          aio(R)      vdp1(W)      vdp1(R)      vdp0(W)      vdp0(R)
        SYS_WRITEL( reg_misc_base+0x201c, 0x00000000);    //#each module 3bit:                                         sdio2(W)        sdio2(R)     sdio1(W)     sdio1(R)     sdio0(W)     sdio0(R)
        SYS_WRITEL( reg_misc_base+0x2020, 0x11333333);    //#each module 3bit:           fd(W)           fd(R)           dpu(W)          dpu(R)      cnn1(W)      cnn1(R)      cnn0(W)      cnn0(R)
        SYS_WRITEL( reg_misc_base+0x2024, 0x66336633);    //#each module 3bit:    dsp1_idma(W)    dsp1_idma(R)        dsp1_m(W)       dsp1_m(R) dsp0_idma(W) dsp0_idma(R)    dsp0_m(W)    dsp0_m(R)
        SYS_WRITEL( reg_misc_base+0x2028, 0x66336633);    //#each module 3bit:    dsp3_idma(W)    dsp3_idma(R)        dsp3_m(W)       dsp3_m(R) dsp2_idma(W) dsp2_idma(R)    dsp2_m(W)    dsp2_m(R)
        SYS_WRITEL( reg_misc_base+0x202c, 0x00444444);    //#each module 3bit:         pcie(W)         pcie(R)        avs_m2(W)       avs_m2(R)    avs_m1(W)    avs_m1(R)    avs_m0(W)    avs_m0(R)
        SYS_WRITEL( reg_misc_base+0x2030, 0x55552211);    //#each module 3bit:         peri(W)         peri(R)           a53(W)          a53(R)       ive(W)       ive(R)       TDE(W)      TDE(R)
        SYS_WRITEL( reg_misc_base+0x2034, 0x55550000);    //#each module 3bit:         cpu1(W)         cpu1(R)          cpu0(W)         cpu0(R)     ddrt1(W)     ddrt1(R)     ddrt0(W)     ddrt0(R)
        SYS_WRITEL( reg_misc_base+0x2038, 0x77776666);    //#each module 3bit:       usb3p1(W)       usb3p1(R)        usb3p0(W)       usb3p0(R)      vdh1(W)      vdh1(R)      vdh0(W)      vdh0(R)
    }
    else /* offline */
    {
                                                         //#each module 3bit:          30:28           26:24           22:20          18:16        14:12         10:8         6:4         2:0
        SYS_WRITEL( reg_misc_base+0x2000, 0x00000011);    //#each module 3bit:          SPACC             FMC           EDMA0          EDMA1          UFS         EMMC         GSF1         GSF0
        SYS_WRITEL( reg_misc_base+0x2004, 0x00454545);    //#each module 3bit:                                         vedu2(W)        vedu2(R)     vedu1(W)     vedu1(R)     vedu0(W)     vedu0(R)
        SYS_WRITEL( reg_misc_base+0x2008, 0x44444545);    //#each module 3bit:         vgs1(W)         vgs1(R)          vgs0(W)         vgs0(R)     vpss1(W)     vpss1(R)    vpss0 (W)     vpss0(R)
        SYS_WRITEL( reg_misc_base+0x200c, 0x55554545);    //#each module 3bit:         gdc1(W)         gdc1(R)          gdc0(W)         gdc0(R)     jpge1(W)     jpge1(R)     jpge0(W)     jpge0(R)
        SYS_WRITEL( reg_misc_base+0x2010, 0x55003333);    //#each module 3bit:          gme(W)          gme(R)          gzip(W)         gzip(R)      jpgd(W)      jpgd(R)       pgd(W)       pgd(R)
        SYS_WRITEL( reg_misc_base+0x2014, 0x67677777);    //#each module 3bit:      viproc1(W)      viproc1(R)       viproc0(W)      viproc0(R)     vi_m1(W)     vi_m1(R)     vi_m0(W)     vi_m0(R)
        SYS_WRITEL( reg_misc_base+0x2018, 0x33666767);    //#each module 3bit:          gpu(W)          gpu(R)           aio(W)          aio(R)      vdp1(W)      vdp1(R)      vdp0(W)      vdp0(R)
        SYS_WRITEL( reg_misc_base+0x201c, 0x00000000);    //#each module 3bit:                                         sdio2(W)        sdio2(R)     sdio1(W)     sdio1(R)     sdio0(W)     sdio0(R)
        SYS_WRITEL( reg_misc_base+0x2020, 0x11333333);    //#each module 3bit:           fd(W)           fd(R)           dpu(W)          dpu(R)      cnn1(W)      cnn1(R)      cnn0(W)      cnn0(R)
        SYS_WRITEL( reg_misc_base+0x2024, 0x66336633);    //#each module 3bit:    dsp1_idma(W)    dsp1_idma(R)        dsp1_m(W)       dsp1_m(R) dsp0_idma(W) dsp0_idma(R)    dsp0_m(W)    dsp0_m(R)
        SYS_WRITEL( reg_misc_base+0x2028, 0x66336633);    //#each module 3bit:    dsp3_idma(W)    dsp3_idma(R)        dsp3_m(W)       dsp3_m(R) dsp2_idma(W) dsp2_idma(R)    dsp2_m(W)    dsp2_m(R)
        SYS_WRITEL( reg_misc_base+0x202c, 0x00444444);    //#each module 3bit:         pcie(W)         pcie(R)        avs_m2(W)       avs_m2(R)    avs_m1(W)    avs_m1(R)    avs_m0(W)    avs_m0(R)
        SYS_WRITEL( reg_misc_base+0x2030, 0x55552211);    //#each module 3bit:         peri(W)         peri(R)           a53(W)          a53(R)       ive(W)       ive(R)       TDE(W)      TDE(R)
        SYS_WRITEL( reg_misc_base+0x2034, 0x55550000);    //#each module 3bit:         cpu1(W)         cpu1(R)          cpu0(W)         cpu0(R)     ddrt1(W)     ddrt1(R)     ddrt0(W)     ddrt0(R)
        SYS_WRITEL( reg_misc_base+0x2038, 0x77776666);    //#each module 3bit:       usb3p1(W)       usb3p1(R)        usb3p0(W)       usb3p0(R)      vdh1(W)      vdh1(R)      vdh0(W)      vdh0(R)
    }

    //##############  qosbuf #############
    SYS_WRITEL( reg_ddr_base+0x4000, 0x2         );
    SYS_WRITEL( reg_ddr_base+0x410c, 0x17        );
    SYS_WRITEL( reg_ddr_base+0x4110, 0x17        );
    SYS_WRITEL( reg_ddr_base+0x4088, 0x5         );
    SYS_WRITEL( reg_ddr_base+0x408c, 0x92821e14  );
    SYS_WRITEL( reg_ddr_base+0x4090, 0x92821e14  );
    SYS_WRITEL( reg_ddr_base+0x4068, 0x51        );
    SYS_WRITEL( reg_ddr_base+0x406c, 0x51        );
    SYS_WRITEL( reg_ddr_base+0x40ac, 0x80        );
    SYS_WRITEL( reg_ddr_base+0x40ec, 0x11        );
    SYS_WRITEL( reg_ddr_base+0x40f0, 0x1111      );
    SYS_WRITEL( reg_ddr_base+0x40f4, 0x33        );
    SYS_WRITEL( reg_ddr_base+0x41f0, 0x1         );
    SYS_WRITEL( reg_ddr_base+0x41f4, 0x0         );
    SYS_WRITEL( reg_ddr_base+0x41f8, 0x00800002  );

    return 0;
}

static void sensor_pin_mux(void)
{
    //#CLK_A CLK_B
    SYS_WRITEL(reg_iocfg_base+0x0000 ,0x000004f1);
    SYS_WRITEL(reg_iocfg_base+0x0004 ,0x000004f1);

    SYS_WRITEL(reg_iocfg_base+0x0008 ,0x000004f1);
    SYS_WRITEL(reg_iocfg_base+0x000C ,0x000004f1);

    SYS_WRITEL(reg_iocfg_base+0x0010 ,0x000004f1);
    SYS_WRITEL(reg_iocfg_base+0x0014 ,0x000004f1);

    SYS_WRITEL(reg_iocfg_base+0x0018 ,0x000004f1);
    SYS_WRITEL(reg_iocfg_base+0x001C ,0x000004f1);


    //#RST
    SYS_WRITEL(reg_iocfg_base+0x0020 ,0x000004f1);
    SYS_WRITEL(reg_iocfg_base+0x0024 ,0x000004f1);
    SYS_WRITEL(reg_iocfg_base+0x0028 ,0x000004f1);
    SYS_WRITEL(reg_iocfg_base+0x002C ,0x000004f1);
    //printk( "============vicap_pin_mux done=============\n");
}

static void slave_pin_mux(void)
{
    //#VS HS
    SYS_WRITEL(reg_iocfg_base+0x0030 ,0x000004f1);
    SYS_WRITEL(reg_iocfg_base+0x0034 ,0x000004f1);

    SYS_WRITEL(reg_iocfg_base+0x0038 ,0x000004f1);
    SYS_WRITEL(reg_iocfg_base+0x003C ,0x000004f1);

    SYS_WRITEL(reg_iocfg_base+0x0040 ,0x000004f1);
    SYS_WRITEL(reg_iocfg_base+0x0044 ,0x000004f1);

    SYS_WRITEL(reg_iocfg_base+0x0048 ,0x000004f1);
    SYS_WRITEL(reg_iocfg_base+0x004C ,0x000004f1);

    //#VSOUT
    SYS_WRITEL(reg_iocfg_base+0x0050 ,0x000004f1);
    SYS_WRITEL(reg_iocfg_base+0x0054 ,0x000004f1);
    SYS_WRITEL(reg_iocfg_base+0x0058 ,0x000004f1);
    SYS_WRITEL(reg_iocfg_base+0x005C ,0x000004f1);
    //printk( "============slave_pin_mux done=============\n");
}



//#i2c0 -> sensor
static void i2c0_pin_mux(void)
{
    //#SCL SDA
    SYS_WRITEL(reg_iocfg_base+0x0068 ,0x000014f3);
    SYS_WRITEL(reg_iocfg_base+0x006C ,0x000014f3);

    printk( "============i2c0_pin_mux done=============\n");
}

//#i2c1 -> sensor
static void i2c1_pin_mux(void)
{
    //#SCL SDA
    SYS_WRITEL(reg_iocfg_base+0x007C ,0x000014f3);
    SYS_WRITEL(reg_iocfg_base+0x0080 ,0x000014f3);

    //printk( "============i2c1_pin_mux done=============\n");
}


//#i2c2 -> sensor
static void i2c2_pin_mux(void)
{
    //#SCL SDA
    SYS_WRITEL(reg_iocfg_base+0x0090 ,0x000014f3);
    SYS_WRITEL(reg_iocfg_base+0x0094 ,0x000014f3);

    //printk( "============i2c2_pin_mux done=============\n");
}

//#i2c3 -> sensor
static void i2c3_pin_mux(void)
{
    //#SCL SDA
    SYS_WRITEL(reg_iocfg_base+0x00A4 ,0x000014f3);
    SYS_WRITEL(reg_iocfg_base+0x00A8 ,0x000014f3);

    //printk( "============i2c3_pin_mux done=============\n");
}


//#i2c4 -> sensor
static void i2c4_pin_mux(void)
{
    //#SCL SDA
    SYS_WRITEL(reg_iocfg_base+0x0074 ,0x000014f3);
    SYS_WRITEL(reg_iocfg_base+0x0078 ,0x000014f3);

    //printk( "============i2c4_pin_mux done=============\n");
}


//#i2c5 -> sensor
static void i2c5_pin_mux(void)
{
    //#SCL SDA
    SYS_WRITEL(reg_iocfg_base+0x0088 ,0x000014f3);
    SYS_WRITEL(reg_iocfg_base+0x008C ,0x000014f3);

    //printk( "============i2c5_pin_mux done=============\n");
}

//#i2c6 -> sensor
static void i2c6_pin_mux(void)
{
    //#SCL SDA
    SYS_WRITEL(reg_iocfg_base+0x009C ,0x000014f3);
    SYS_WRITEL(reg_iocfg_base+0x00A0 ,0x000014f3);


    //printk( "============i2c6_pin_mux done=============\n");
}

//#i2c7 -> sensor
static void i2c7_pin_mux(void)
{
    //#SCL SDA
    SYS_WRITEL(reg_iocfg_base+0x00B0 ,0x000014f3);
    SYS_WRITEL(reg_iocfg_base+0x00B4 ,0x000014f3);

    //printk( "============i2c7_pin_mux done=============\n");
}

//#i2c8 -> sensor
static void i2c8_pin_mux(void)
{
    //#SCL SDA
    SYS_WRITEL(reg_iocfg_base+0x10C8 ,0x000014f2);
    SYS_WRITEL(reg_iocfg_base+0x10CC ,0x000014f2);

    //printk( "============i2c8_pin_mux done=============\n");
}

//#i2c9 -> sensor
static void i2c9_pin_mux(void)
{
    //#SCL SDA
    SYS_WRITEL(reg_iocfg_base+0x10D0 ,0x000014f2);
    SYS_WRITEL(reg_iocfg_base+0x10D4 ,0x000014f2);

    //printk( "============i2c9_pin_mux done=============\n");
}

//#i2c10 -> sensor
static void i2c10_pin_mux(void)
{
    //#SCL SDA
    SYS_WRITEL(reg_iocfg_base+0x10DC ,0x000014f2);
    SYS_WRITEL(reg_iocfg_base+0x10E0 ,0x000014f2);

    //printk( "============i2c10_pin_mux done=============\n");
}

//#i2c11 -> 9022
static void i2c11_pin_mux(void)
{
    SYS_WRITEL(reg_iocfg_base+0x10F4 ,0x00001C00);
    SYS_WRITEL(reg_iocfg_base+0x10F0 ,0x00001C00);
    //printk( "============i2c11_pin_mux done=============\n");
}

static void spi0_pin_mux(void)
{
    SYS_WRITEL(reg_iocfg_base+0x0068, 0x00000461);
    SYS_WRITEL(reg_iocfg_base+0x006c, 0x00000461);
    SYS_WRITEL(reg_iocfg_base+0x0070, 0x000014f1);
    SYS_WRITEL(reg_iocfg_base+0x0074, 0x00000461);
    SYS_WRITEL(reg_iocfg_base+0x0078, 0x000004f1);
    //printk( "============spi0_pin_mux done=============\n");
}

static void spi1_pin_mux(void)
{
    SYS_WRITEL(reg_iocfg_base+0x007c, 0x00000461);
    SYS_WRITEL(reg_iocfg_base+0x0080, 0x00000461);
    SYS_WRITEL(reg_iocfg_base+0x0084, 0x000014f1);
    SYS_WRITEL(reg_iocfg_base+0x0088, 0x00000461);
    SYS_WRITEL(reg_iocfg_base+0x008c, 0x000004f1);
    //printk( "============spi1_pin_mux done=============\n");
}

static void spi2_pin_mux(void)
{
    SYS_WRITEL(reg_iocfg_base+0x0090, 0x00000461);
    SYS_WRITEL(reg_iocfg_base+0x0094, 0x00000461);
    SYS_WRITEL(reg_iocfg_base+0x0098, 0x000014f1);
    SYS_WRITEL(reg_iocfg_base+0x009c, 0x00000461);
    SYS_WRITEL(reg_iocfg_base+0x00a0, 0x000004f1);
    //printk( "============spi2_pin_mux done=============\n");
}

static void spi3_pin_mux(void)
{
    SYS_WRITEL(reg_iocfg_base+0x00a4, 0x00000461);
    SYS_WRITEL(reg_iocfg_base+0x00a8, 0x00000461);
    SYS_WRITEL(reg_iocfg_base+0x00ac, 0x000014f1);
    SYS_WRITEL(reg_iocfg_base+0x00b0, 0x00000461);
    SYS_WRITEL(reg_iocfg_base+0x00b4, 0x000004f1);
    //printk( "============spi3_pin_mux done=============\n");
}

static void spi4_pin_mux(void)
{
    SYS_WRITEL(reg_iocfg_base+0x10c8,0x00000421);       //#(GPIO16_0,SPI4_SCLK,     I2C8_SCL)  = (0x0,0x1,0x2)
    SYS_WRITEL(reg_iocfg_base+0x10cc,0x00000421);       //#(GPIO16_1,SPI4_SDO ,     I2C8_SCA)  = (0x0,0x1,0x2)
    SYS_WRITEL(reg_iocfg_base+0x10d0,0x00000001);       //#(GPIO16_2,SPI4_SDI ,     I2C9_SCL)  = (0x0,0x1,0x2)
    SYS_WRITEL(reg_iocfg_base+0x10d4,0x00000421);       //#(GPIO16_3,SPI4_CSN0,     I2C9_SDA)  = (0x0,0x1,0x2)
    SYS_WRITEL(reg_iocfg_base+0x10d8,0x00000001);       //#(GPIO16_4,SPI4_CSN1)                = (0x0,0x1)
    SYS_WRITEL(reg_iocfg_base+0x10dc,0x00000001);       //#(GPIO16_5,SPI4_CSN2,     I2C10_SCL) = (0x0,0x1,0x2)
    SYS_WRITEL(reg_iocfg_base+0x10e0,0x00000001);       //#(GPIO16_6,SPI4_CSN3,     I2C10_SDA) = (0x0,0x1,0x2)
    //printk( "============spi4_pin_mux done=============\n");
}

//* sync<=1080P@30:1, 1080P@30<sync<=1080P@60:0
static void vo_bt1120_mode(int bt1120_mode)
{
    if(1 == bt1120_mode)
    {
        SYS_WRITEL(reg_iocfg_base+0x1084 ,0x00000061);
    }
    else
    {
        SYS_WRITEL(reg_iocfg_base+0x1084 ,0x00000091);
    }

    SYS_WRITEL(reg_iocfg_base+0x1088 ,0x00000451);
    SYS_WRITEL(reg_iocfg_base+0x108C ,0x00000451);
    SYS_WRITEL(reg_iocfg_base+0x1090 ,0x00000451);
    SYS_WRITEL(reg_iocfg_base+0x1094 ,0x00000451);
    SYS_WRITEL(reg_iocfg_base+0x1098 ,0x00000451);
    SYS_WRITEL(reg_iocfg_base+0x109C ,0x00000451);
    SYS_WRITEL(reg_iocfg_base+0x10A0 ,0x00000451);
    SYS_WRITEL(reg_iocfg_base+0x10A4 ,0x00000451);
    SYS_WRITEL(reg_iocfg_base+0x10A8 ,0x00000451);
    SYS_WRITEL(reg_iocfg_base+0x10AC ,0x00000451);
    SYS_WRITEL(reg_iocfg_base+0x10B0 ,0x00000451);
    SYS_WRITEL(reg_iocfg_base+0x10B4 ,0x00000451);
    SYS_WRITEL(reg_iocfg_base+0x10B8 ,0x00000451);
    SYS_WRITEL(reg_iocfg_base+0x10BC ,0x00000451);
    SYS_WRITEL(reg_iocfg_base+0x10C0 ,0x00000451);
    SYS_WRITEL(reg_iocfg_base+0x10C4 ,0x00000451);
    //printk( "============vo_output_mode done=============\n");
}

static void vo_lcd_mode(void)
{
    SYS_WRITEL(reg_iocfg_base+0x10EC ,0x000004f2);
    SYS_WRITEL(reg_iocfg_base+0x10A4 ,0x000004f2);
    SYS_WRITEL(reg_iocfg_base+0x109C ,0x000004f2);
    SYS_WRITEL(reg_iocfg_base+0x10E4 ,0x000004f2);
    SYS_WRITEL(reg_iocfg_base+0x1098 ,0x000004f2);
    SYS_WRITEL(reg_iocfg_base+0x1094 ,0x000004f2);
    SYS_WRITEL(reg_iocfg_base+0x1090 ,0x000004f2);
    SYS_WRITEL(reg_iocfg_base+0x108C ,0x000004f2);
    SYS_WRITEL(reg_iocfg_base+0x1088 ,0x000004f2);
    SYS_WRITEL(reg_iocfg_base+0x10C4 ,0x000004f2);
    SYS_WRITEL(reg_iocfg_base+0x10C0 ,0x000004f2);
    SYS_WRITEL(reg_iocfg_base+0x10A8 ,0x000004f2);
    SYS_WRITEL(reg_iocfg_base+0x10B8 ,0x000004f2);
    SYS_WRITEL(reg_iocfg_base+0x10B4 ,0x000004f2);
    SYS_WRITEL(reg_iocfg_base+0x10B0 ,0x000004f2);
    SYS_WRITEL(reg_iocfg_base+0x10AC ,0x000004f2);
    SYS_WRITEL(reg_iocfg_base+0x1008 ,0x000004f2);
    SYS_WRITEL(reg_iocfg_base+0x100C ,0x000004f2);
    SYS_WRITEL(reg_iocfg_base+0x1010 ,0x000004f2);
    SYS_WRITEL(reg_iocfg_base+0x1000 ,0x000004f2);
    SYS_WRITEL(reg_iocfg_base+0x1014 ,0x000004f2);
    SYS_WRITEL(reg_iocfg_base+0x1038 ,0x000004f2);
    SYS_WRITEL(reg_iocfg_base+0x101C ,0x000004f2);
    SYS_WRITEL(reg_iocfg_base+0x1030 ,0x000004f2);
    SYS_WRITEL(reg_iocfg_base+0x1020 ,0x000004f2);
    SYS_WRITEL(reg_iocfg_base+0x1024 ,0x000004f2);
    SYS_WRITEL(reg_iocfg_base+0x1028 ,0x000004f2);
    SYS_WRITEL(reg_iocfg_base+0x102C ,0x000004f2);

    //printk( "============vo_output_mode done=============\n");
}

static void vo_lcd8bit_mode(void)
{
    //#lcd8bit
    SYS_WRITEL(reg_iocfg_base+0x1088 ,0x4f2);
    SYS_WRITEL(reg_iocfg_base+0x108c ,0x4f2);
    SYS_WRITEL(reg_iocfg_base+0x1090 ,0x4f2);
    SYS_WRITEL(reg_iocfg_base+0x1094 ,0x4f2);
    SYS_WRITEL(reg_iocfg_base+0x1098 ,0x4f2);
    SYS_WRITEL(reg_iocfg_base+0x109c ,0x4f2);
    SYS_WRITEL(reg_iocfg_base+0x10a4 ,0x4f2);
    SYS_WRITEL(reg_iocfg_base+0x10a8 ,0x4f2);
    SYS_WRITEL(reg_iocfg_base+0x10c0 ,0x4f2);
    SYS_WRITEL(reg_iocfg_base+0x10c4 ,0x4f2);
    SYS_WRITEL(reg_iocfg_base+0x10e4 ,0x4f2);
    SYS_WRITEL(reg_iocfg_base+0x10ec ,0x4f2);
    //printk( "============vo_lcd8bit_mode done=============\n");
}

static void vo_mipi_tx_mode(void)
{
    //#mipi_tx
    SYS_WRITEL(reg_iocfg_base+0x1088 ,0x0);
    SYS_WRITEL(reg_gpio_base+0x6004 ,0x1);
    SYS_WRITEL(reg_gpio_base+0x6400 ,0x1);
    SYS_WRITEL(reg_gpio_base+0x0010 ,0x4);
    SYS_WRITEL(reg_gpio_base+0x0400 ,0x4);

    //printk( "============vo_mipi_tx_mode done=============\n");
}

static void hmdi_pin_mode(void)
{
    SYS_WRITEL(reg_iocfg_base+0x00C4 ,0x00001400);
    SYS_WRITEL(reg_iocfg_base+0x00C8 ,0x00001400);
    SYS_WRITEL(reg_iocfg_base+0x00CC ,0x00001400);
    SYS_WRITEL(reg_iocfg_base+0x00D0 ,0x00001400);
}

static void i2s_pin_mux(void)
{
    SYS_WRITEL(reg_iocfg_base+0x00FC ,0x00000ef2);
    SYS_WRITEL(reg_iocfg_base+0x0100 ,0x00001ef2);
    SYS_WRITEL(reg_iocfg_base+0x0104 ,0x00001ef2);
    SYS_WRITEL(reg_iocfg_base+0x0108 ,0x00000ef2);
    SYS_WRITEL(reg_iocfg_base+0x010C ,0x00001ef2);

    //printk( "============i2s_pin_mux done=============\n");
}

static void sensor_bus_pin_mux(int index, BUS_TYPE bus_type)
{
    if(BUS_TYPE_I2C == bus_type)
    {
        if(0 == index)
        {
            i2c0_pin_mux();
        }
        else if(1 == index)
        {
            i2c1_pin_mux();
        }
        else if(2 == index)
        {
            i2c2_pin_mux();
        }
        else if(3 == index)
        {
            i2c3_pin_mux();
        }
        else if(4 == index)
        {
            i2c4_pin_mux();
        }
        else if(5 == index)
        {
            i2c5_pin_mux();
        }
        else if(6 == index)
        {
            i2c6_pin_mux();
        }
        else if(7 == index)
        {
            i2c7_pin_mux();
        }


    }
    else if(BUS_TYPE_SPI == bus_type)
    {
        if(0 == index)
        {
            spi0_pin_mux();
        }
        else if(1 == index)
        {
            spi1_pin_mux();
        }
        else if(2 == index)
        {
            spi2_pin_mux();
        }
        else if(3 == index)
        {
            spi3_pin_mux();
        }
        else if(4 == index)
        {
            spi4_pin_mux();
        }
    }
}

static int sn65dsi83_pinmux(void)
{
	//PWM GPIO14_0
    SYS_WRITEL(reg_iocfg_base+0x0124 ,0x00001600);
    SYS_WRITEL(reg_gpio_base+0xe004 ,0x00000001);
    SYS_WRITEL(reg_gpio_base+0xe400 ,0x00000001); // dir
    SYS_WRITEL(reg_gpio_base+0xe004 ,0x00000001);
    
    //BLE GPIO16_3
    SYS_WRITEL(reg_iocfg_base+0x10d4 ,0x00001600);
    SYS_WRITEL(reg_gpio_base+0x10020 ,0x00000008);
    SYS_WRITEL(reg_gpio_base+0x10400 ,0x00000008); // dir
    SYS_WRITEL(reg_gpio_base+0x10020 ,0x00000008);
    
    //CSE GPIO0_6
    SYS_WRITEL(reg_iocfg_base+0x00b8 ,0x00001600);
    SYS_WRITEL(reg_gpio_base+0x0100 ,0x00000040);
    SYS_WRITEL(reg_gpio_base+0x0400 ,0x00000040); // dir
    SYS_WRITEL(reg_gpio_base+0x0100 ,0x00000040);
    //SYS_WRITEL(reg_gpio_base+0x0100 ,0x00000000);

    return 0;
}

static int pinmux(void)
{
    sensor_pin_mux();
    slave_pin_mux();

    //#sensor
    i2c0_pin_mux();
    i2c1_pin_mux();
    i2c2_pin_mux();
    i2c3_pin_mux();
    i2c4_pin_mux();
    i2c5_pin_mux();
    i2c6_pin_mux();
    i2c7_pin_mux();

    // bt656
    //i2c8_pin_mux();

    //#9022
    i2c11_pin_mux();

    //#bt1120
    //vo_bt1120_mode(g_bt1120_flag);

    //#lcd_24bit
    //vo_lcd_mode();

    //#lcd_8bit
    //spi4_pin_mux();
    //vo_lcd8bit_mode();

    hmdi_pin_mode();

    //#mipi_Tx
    //vo_mipi_tx_mode();

    //i2s_pin_mux();
    
    sn65dsi83_pinmux();

    return 0;
}

static int sensor_config(char *s)
{
    int ret;
    int index;
    int clock;
    char* line;
    BUS_TYPE bus_type;
    char sensor_name[SENSOR_NAME_LEN];

    while ((line = strsep(&s, ":")) != NULL)
    {
        int i;
        char* argv[8];

        for (i = 0; (argv[i] = strsep(&line, ",")) != NULL;)
        {
            ret = parse_sensor_name(argv[i], sensor_name);
            if(ret >= 0)
            {
                index = parse_sensor_index(argv[i]);
                printk( "index= %d\n", index);

                if (index >= 0)
                {
                    clock = parse_sensor_clock(sensor_name);
                    bus_type = parse_sensor_bus_type(sensor_name);

                    if (is_coms(sensor_name)) // cmos vi
                    {
                        coms_clock_config(index);
                        sensor_bus_pin_mux(index, bus_type);//by alex
                        sensor_clock_config(index, clock);//by alex
                    }
                    else
                    {
                        sensor_bus_pin_mux(index, bus_type);
                        sensor_clock_config(index, clock);
                    }
                }
            }

            if (++i == ARRAY_SIZE(argv))
            {
                break;
            }
        }
    }

    return 0;
}

static int ampunmute(void)
{
    SYS_WRITEL(reg_iocfg_base+0x2050 ,0x00000cf0);

    SYS_WRITEL(reg_gpio_base+0xe200 ,0x00000080);
    SYS_WRITEL(reg_gpio_base+0xe400 ,0x00000080);
    SYS_WRITEL(reg_gpio_base+0xe200 ,0x00000080);

    return 0;
}

static int __init hi_sysconfig_init(void)
{
    reg_crg_base = (void*)ioremap(0x12010000, 0x10000);
    if (NULL == reg_crg_base)
    {
        goto out;
    }
    reg_sys_base = (void*)ioremap(0x12020000, 0x10000);
    if (NULL == reg_sys_base)
    {
        goto out;
    }
    reg_ddr_base = (void*)ioremap(0x12060000, 0x10000);
    if (NULL == reg_ddr_base)
    {
        goto out;
    }
    reg_misc_base = (void*)ioremap(0x12030000, 0x10000);
    if (NULL == reg_misc_base)
    {
        goto out;
    }
    reg_iocfg_base = (void*)ioremap(0x1F000000, 0x10000);
    if (NULL == reg_iocfg_base)
    {
        goto out;
    }
    reg_gpio_base = (void*)ioremap(0x12140000, 0x20000);
    if (NULL == reg_gpio_base)
    {
        goto out;
    }

    sysctl(g_online_flag);

    clkcfg();

    pinmux();

    //ampunmute();

	sensor_config(sensor_list);

out:
    if (NULL != reg_crg_base)
    {
        iounmap(reg_crg_base);
        reg_crg_base = 0;
    }
    if (NULL != reg_sys_base)
    {
        iounmap(reg_sys_base);
        reg_sys_base = 0;
    }
    if (NULL != reg_ddr_base)
    {
        iounmap(reg_ddr_base);
        reg_ddr_base = 0;
    }
    if (NULL != reg_misc_base)
    {
        iounmap(reg_misc_base);
        reg_misc_base = 0;
    }
    if (NULL != reg_iocfg_base)
    {
        iounmap(reg_iocfg_base);
        reg_iocfg_base = 0;
    }
    if (NULL != reg_gpio_base)
    {
        iounmap(reg_gpio_base);
        reg_gpio_base = 0;
    }
    return 0;
}

static void __exit hi_sysconfig_exit(void)
{
    if (NULL != reg_crg_base)
    {
        iounmap(reg_crg_base);
        reg_crg_base = 0;
    }
    if (NULL != reg_sys_base)
    {
        iounmap(reg_sys_base);
        reg_sys_base = 0;
    }
    if (NULL != reg_ddr_base)
    {
        iounmap(reg_ddr_base);
        reg_ddr_base = 0;
    }
    if (NULL != reg_misc_base)
    {
        iounmap(reg_misc_base);
        reg_misc_base = 0;
    }
    if (NULL != reg_iocfg_base)
    {
        iounmap(reg_iocfg_base);
        reg_iocfg_base = 0;
    }

    return;
}

module_init(hi_sysconfig_init);
module_exit(hi_sysconfig_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Hisilicon");


