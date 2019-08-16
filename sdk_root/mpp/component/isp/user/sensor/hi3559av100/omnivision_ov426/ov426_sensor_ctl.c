/******************************************************************************

  Copyright (C), 2019, Xinhai Medic. Co., Ltd.

 ******************************************************************************
  File Name     : ov426_sensor_ctl.c

  Version       : Initial Draft
  Author        : Andreas Zhang
  Created       : 2019/07/03
  Description   :
  History       :
  1.Date        : 2019/07/03
    Author      : Andreas Zhang
    Modification: Created file

******************************************************************************/
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#include "hi_comm_video.h"
#include "hi_sns_ctrl.h"

#ifdef HI_GPIO_I2C
#include "gpioi2c_ex.h"
#else
#include "hi_i2c.h"
#endif

const unsigned char ov426_i2c_addr     =    0x6c; /* I2C Address of ov426 */
const unsigned int  ov426_addr_byte    =    2;
const unsigned int  ov426_data_byte    =    1;
static int g_fd[ISP_MAX_PIPE_NUM] = {[0 ... (ISP_MAX_PIPE_NUM - 1)] = -1};

extern ISP_SNS_STATE_S       *g_pastOv426[ISP_MAX_PIPE_NUM];
extern ISP_SNS_COMMBUS_U      g_aunOv426BusInfo[];

#define Ov426_400x400_12bit_30fps      (1)

void ov426_init_400x400_12bit_30fps(VI_PIPE ViPipe);

int ov426_i2c_init(VI_PIPE ViPipe)
{
    char acDevFile[16] = {0};
    HI_U8 u8DevNum;

    if (g_fd[ViPipe] >= 0)
    {
        return HI_SUCCESS;
    }
#ifdef HI_GPIO_I2C
    int ret;

    g_fd[ViPipe] = open("/dev/gpioi2c_ex", O_RDONLY, S_IRUSR);
    if (g_fd[ViPipe] < 0)
    {
        ISP_TRACE(HI_DBG_ERR, "Open gpioi2c_ex error!\n");
        return HI_FAILURE;
    }
#else
    int ret;

    u8DevNum = g_aunOv426BusInfo[ViPipe].s8I2cDev;
    snprintf(acDevFile, sizeof(acDevFile),  "/dev/i2c-%u", u8DevNum);

    g_fd[ViPipe] = open(acDevFile, O_RDWR, S_IRUSR | S_IWUSR);

    if (g_fd[ViPipe] < 0)
    {
        ISP_TRACE(HI_DBG_ERR, "Open /dev/hi_i2c_drv-%u error!\n", u8DevNum);
        return HI_FAILURE;
    }

    ret = ioctl(g_fd[ViPipe], I2C_SLAVE_FORCE, (ov426_i2c_addr >> 1));
    if (ret < 0)
    {
        ISP_TRACE(HI_DBG_ERR, "I2C_SLAVE_FORCE error!\n");
        close(g_fd[ViPipe]);
        g_fd[ViPipe] = -1;
        return ret;
    }
#endif

    return HI_SUCCESS;
}

int ov426_i2c_exit(VI_PIPE ViPipe)
{
    if (g_fd[ViPipe] >= 0)
    {
        close(g_fd[ViPipe]);
        g_fd[ViPipe] = -1;
        return HI_SUCCESS;
    }
    return HI_FAILURE;
}

int ov426_read_register(VI_PIPE ViPipe, int addr)
{
    // TODO:

    return HI_SUCCESS;
}


int ov426_write_register(VI_PIPE ViPipe, int addr, int data)
{
    if (0 > g_fd[ViPipe])
    {
        return HI_SUCCESS;
    }

#ifdef HI_GPIO_I2C
    i2c_data.dev_addr = ov426_i2c_addr;
    i2c_data.reg_addr = addr;
    i2c_data.addr_byte_num = ov426_addr_byte;
    i2c_data.data = data;
    i2c_data.data_byte_num = ov426_data_byte;

    ret = ioctl(g_fd[ViPipe], GPIO_I2C_WRITE, &i2c_data);

    if (ret)
    {
        ISP_TRACE(HI_DBG_ERR, "GPIO-I2C write faild!\n");
        return ret;
    }
#else
    int idx = 0;
    int ret;
    char buf[8];

    if (ov426_addr_byte == 2)
    {
        buf[idx] = (addr >> 8) & 0xff;
        idx++;
        buf[idx] = addr & 0xff;
        idx++;
    }
    else
    {
        buf[idx] = addr & 0xff;
        idx++;
    }

    if (ov426_data_byte == 2)
    {
        buf[idx] = (data >> 8) & 0xff;
        idx++;
        buf[idx] = data & 0xff;
        idx++;
    }
    else
    {
        buf[idx] = data & 0xff;
        idx++;
    }

    ret = write(g_fd[ViPipe], buf, (ov426_addr_byte + ov426_data_byte));
    if (ret < 0)
    {
        ISP_TRACE(HI_DBG_ERR, "I2C_WRITE error!\n");
        return HI_FAILURE;
    }

#endif
    return HI_SUCCESS;
}


static void delay_ms(int ms)
{
    usleep(ms * 1000);
}

void ov426_standby(VI_PIPE ViPipe)
{
    // TODO:
    return;
}

void ov426_restart(VI_PIPE ViPipe)
{
    // TODO:
    return;
}


void ov426_prog(VI_PIPE ViPipe, int *rom)
{
    int i = 0;
    while (1)
    {
        int lookup = rom[i++];
        int addr = (lookup >> 16) & 0xFFFF;
        int data = lookup & 0xFFFF;
        if (addr == 0xFFFE)
        {
            delay_ms(data);
        }
        else if (addr == 0xFFFF)
        {
            return;
        }
        else
        {
            ov426_write_register(ViPipe, addr, data);
        }
    }
}


void ov426_init(VI_PIPE ViPipe)
{
    HI_U8           u8ImgMode;
    HI_BOOL         bInit;
//    HI_U32          i;

    bInit       = g_pastOv426[ViPipe]->bInit;
    u8ImgMode   = g_pastOv426[ViPipe]->u8ImgMode;

    /* 1. sensor i2c init */
    ov426_i2c_init(ViPipe);

    /* When sensor first init, config all registers */
    if (HI_FALSE == bInit)
    {
        switch (u8ImgMode)
        {
            case Ov426_400x400_12bit_30fps:
                ov426_init_400x400_12bit_30fps(ViPipe);
                break;
            default:
                ISP_TRACE(HI_DBG_ERR, "Not Support Image Mode %d\n", u8ImgMode);
                break;
        }

    }
    /* When sensor switch mode(linear<->WDR or resolution), config different registers(if possible) */
    else
    {
        switch (u8ImgMode)
        {
            case Ov426_400x400_12bit_30fps:
                ov426_init_400x400_12bit_30fps(ViPipe);
                break;
            default:
                ISP_TRACE(HI_DBG_ERR, "Not Support Image Mode %d\n", u8ImgMode);
                break;
        }

    }

    /*for (i = 0; i < g_pastOv426[ViPipe]->astRegsInfo[0].u32RegNum; i++)
    {
        ov426_write_register(ViPipe, g_pastOv426[ViPipe]->astRegsInfo[0].astI2cData[i].u32RegAddr, g_pastOv426[ViPipe]->astRegsInfo[0].astI2cData[i].u32Data);
    }*/
    g_pastOv426[ViPipe]->bInit = HI_TRUE;

    return ;
}

void ov426_exit(VI_PIPE ViPipe)
{
    ov426_i2c_exit(ViPipe);

    return;
}

void ov426_init_400x400_12bit_30fps(VI_PIPE ViPipe)
{
    ov426_write_register(ViPipe, 0x0103, 0x01); // software_reset @ system control
	ov426_write_register(ViPipe, 0x3025, 0x16); // div_pre_s/div_post_s/div_sys_s @ system control
	ov426_write_register(ViPipe, 0x3026, 0x1c); // PLL optimized: cp_s/div_loop_s @ system control
	ov426_write_register(ViPipe, 0x3024, 0x04); // adc_reg[7:0] @ system control
	ov426_write_register(ViPipe, 0x0100, 0x01); // Streaming/Standby mode @ system control
	ov426_write_register(ViPipe, 0x3209, 0x03); // auto AEC enable @ SPI control
	ov426_write_register(ViPipe, 0x3701, 0x40); // manu_dat_dly_detect_en, ASYNC_1 @ ADC_sync
	ov426_write_register(ViPipe, 0x3702, 0x4a);	// rd_data_dly @ ADC_sync
	ov426_write_register(ViPipe, 0x3003, 0x35); // sclk2x_sel/adclk_inv/sclk_div /2, oversample_factor /2 @ system control, ov6946(400X400),0x31
	ov426_write_register(ViPipe, 0x3204, 0x87); // Ana[7:0] @ SPI control, 8b fine turn VFPN
	ov426_write_register(ViPipe, 0x3028, 0xb0); // Vcom/black_ref/Vrbg @ system control
	ov426_write_register(ViPipe, 0x3027, 0x19); // 5mA driver_ih / vr_sync @ system control, increase loading capacity,0x020,x022
	ov426_write_register(ViPipe, 0x5a40, 0x05); // gainf_ana_bit_num @ AEC/AGC registers, Max gain sent to OV6946
	ov426_write_register(ViPipe, 0x3a19, 0x15); // gain_ceiling[7:0] Upper bound of output sensor gain @ AEC/AGC registers, Gain ceiling for sensor,0x3e
	ov426_write_register(ViPipe, 0x5a00, 0x04); // dig_comp CTRL0 @ AEC/AGC registers, compensation block on wtih no base subtracted ,0x08
	ov426_write_register(ViPipe, 0x4009, 0x10); // blc_blackleveltarget0 @ BLC control, BLC target adjust along with 5a00,0x20
	ov426_write_register(ViPipe, 0x4005, 0x1a); // BLC alway on, BLC CTRL05 @ BLC control
	ov426_write_register(ViPipe, 0x3020, 0x09); // ADC_Range, 0x08~770mV, adc_reg[39:32] @ system control
	ov426_write_register(ViPipe, 0x3024, 0x00); // adc_reg[7:0] @ system control
	ov426_write_register(ViPipe, 0x3a0f, 0x26); // AEC/AGC stable range, WPT Stable range high limit @ AEC/AGC registers
	ov426_write_register(ViPipe, 0x3a10, 0x25); // BPT Stable range low limit @ AEC/AGC registers
	ov426_write_register(ViPipe, 0x3a1b, 0x27); // WPT2 Stable range high limit @ AEC/AGC registers
	ov426_write_register(ViPipe, 0x3a1e, 0x24); // BPT2 Stable range low limit @ AEC/AGC registers
	ov426_write_register(ViPipe, 0x3203, 0x27); // BB version? , Ana[15:8] @ SPI registers
	ov426_write_register(ViPipe, 0x4052, 0x01); // Get BLC all,one channel blc @ BLC control
	ov426_write_register(ViPipe, 0x302a, 0x01); // Pregain 1.25X @ system control
	ov426_write_register(ViPipe, 0x350b, 0x10); // AEC real gain[7:0] @ AEC/AGC registers
	ov426_write_register(ViPipe, 0x3503, 0x03); // AEC Auto / AGC Auto @ AEC/AGC registers
	ov426_write_register(ViPipe, 0x302a, 0x10); // Pregain 1.0X @ system control
    // ov426_default_reg_init(ViPipe); // hi3556 sdk pattern
	ov426_write_register(ViPipe, 0x0100, 0x00); // software_standby
	ov426_write_register(ViPipe, 0x0100, 0x01); // Streaming

    printf("-------OV ov426_init_400x400_12bit_30fps Initial OK!-------\n");
    return;

}



