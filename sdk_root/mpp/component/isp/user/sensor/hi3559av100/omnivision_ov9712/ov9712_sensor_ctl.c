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

const unsigned char ov9712_i2c_addr     =    0x60; /* I2C Address of ov9712 */
const unsigned int  ov9712_addr_byte    =    1;
const unsigned int  ov9712_data_byte    =    1;
static int g_fd[ISP_MAX_PIPE_NUM] = {[0 ... (ISP_MAX_PIPE_NUM - 1)] = -1};

extern ISP_SNS_STATE_S       *g_pastOv9712[ISP_MAX_PIPE_NUM];
extern ISP_SNS_COMMBUS_U      g_aunOv9712BusInfo[];

#define OV9712_720P_25FPS_MODE  (1)

void ov9712_linear_720p25_init(VI_PIPE ViPipe);

int ov9712_i2c_init(VI_PIPE ViPipe)
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

    u8DevNum = g_aunOv9712BusInfo[ViPipe].s8I2cDev;
    snprintf(acDevFile, sizeof(acDevFile),  "/dev/i2c-%u", u8DevNum);

    g_fd[ViPipe] = open(acDevFile, O_RDWR, S_IRUSR | S_IWUSR);

    if (g_fd[ViPipe] < 0)
    {
        ISP_TRACE(HI_DBG_ERR, "Open /dev/hi_i2c_drv-%u error!\n", u8DevNum);
        return HI_FAILURE;
    }

    ret = ioctl(g_fd[ViPipe], I2C_SLAVE_FORCE, (ov9712_i2c_addr >> 1));
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

int ov9712_i2c_exit(VI_PIPE ViPipe)
{
    if (g_fd[ViPipe] >= 0)
    {
        close(g_fd[ViPipe]);
        g_fd[ViPipe] = -1;
        return HI_SUCCESS;
    }
    return HI_FAILURE;
}

int ov9712_read_register(VI_PIPE ViPipe, int addr)
{
    // TODO:

    return HI_SUCCESS;
}

int ov9712_write_register(VI_PIPE ViPipe, int addr, int data)
{
    if (0 > g_fd[ViPipe])
    {
        return HI_SUCCESS;
    }

#ifdef HI_GPIO_I2C
    i2c_data.dev_addr = ov9712_i2c_addr;
    i2c_data.reg_addr = addr;
    i2c_data.addr_byte_num = ov9712_addr_byte;
    i2c_data.data = data;
    i2c_data.data_byte_num = ov9712_data_byte;

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

    if (ov9712_addr_byte == 2)
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

    if (ov9712_data_byte == 2)
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

	//printf("[file]:%s [func]:%s [line]:%d:I2C_WRITING! #%x #%x\n", __FILE__, __FUNCTION__, __LINE__, addr, data);
    ret = write(g_fd[ViPipe], buf, (ov9712_addr_byte + ov9712_data_byte));
    if (ret < 0)
    {
        ISP_TRACE(HI_DBG_ERR, "I2C_WRITE error! #%x #%x\n", addr, data);
        return HI_FAILURE;
    }

#endif
    return HI_SUCCESS;
}

static void delay_ms(int ms)
{
    usleep(ms * 1000);
}

void ov9712_standby(VI_PIPE ViPipe)
{
    // TODO:
    return;
}

void ov9712_restart(VI_PIPE ViPipe)
{
    // TODO:
    return;
}

void ov9712_prog(VI_PIPE ViPipe, int *rom)
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
            ov9712_write_register(ViPipe, addr, data);
        }
    }
}

void ov9712_init(VI_PIPE ViPipe)
{
	HI_U8           u8ImgMode;
    HI_BOOL         bInit;
    
    bInit       = g_pastOv9712[ViPipe]->bInit;
    u8ImgMode   = g_pastOv9712[ViPipe]->u8ImgMode;
    
    /* 1. sensor i2c init */
    ov9712_i2c_init(ViPipe);
    
    /* When sensor first init, config all registers */
    if (HI_FALSE == bInit)
    {
        switch (u8ImgMode)
        {
            case OV9712_720P_25FPS_MODE:
                ov9712_linear_720p25_init(ViPipe);
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
            case OV9712_720P_25FPS_MODE:
                ov9712_linear_720p25_init(ViPipe);
                break;
            default:
                ISP_TRACE(HI_DBG_ERR, "Not Support Image Mode %d\n", u8ImgMode);
                break;
        }
    }
    
    g_pastOv9712[ViPipe]->bInit = HI_TRUE;
}

void ov9712_exit(VI_PIPE ViPipe)
{
    ov9712_i2c_exit(ViPipe);

    return;
}

/* 720P30 and 720P25 */
void ov9712_linear_720p25_init(VI_PIPE ViPipe)
{
    //Reset
    ov9712_write_register(ViPipe, 0x12, 0x80);
    delay_ms(10);
    ov9712_write_register(ViPipe, 0x09, 0x10);
    //delay_ms(10);
    //Core Settings
    ov9712_write_register(ViPipe, 0x1e, 0x07);
    ov9712_write_register(ViPipe, 0x5f, 0x18);
    ov9712_write_register(ViPipe, 0x69, 0x04);
    ov9712_write_register(ViPipe, 0x65, 0x2a);
    ov9712_write_register(ViPipe, 0x68, 0x0a);
    ov9712_write_register(ViPipe, 0x39, 0x28);
    ov9712_write_register(ViPipe, 0x4d, 0x90);
    ov9712_write_register(ViPipe, 0xc1, 0x80);
    ov9712_write_register(ViPipe, 0x0c, 0x30);
    ov9712_write_register(ViPipe, 0x6d, 0x02);
	//delay_ms(10);
    //DSP
    //ov9712_write_register(ViPipe, 0x96, 0xf1);
    ov9712_write_register(ViPipe, 0x96, 0x01);
    ov9712_write_register(ViPipe, 0xbc, 0x68);
	//delay_ms(10);
    //Resolution and Format
    ov9712_write_register(ViPipe, 0x12, 0x00);
    ov9712_write_register(ViPipe, 0x3b, 0x00);
    ov9712_write_register(ViPipe, 0x97, 0x80);
    ov9712_write_register(ViPipe, 0x17, 0x25);
    ov9712_write_register(ViPipe, 0x18, 0xA2);
    ov9712_write_register(ViPipe, 0x19, 0x01);
    ov9712_write_register(ViPipe, 0x1a, 0xCA);
    ov9712_write_register(ViPipe, 0x03, 0x0A);
    ov9712_write_register(ViPipe, 0x32, 0x07);
    ov9712_write_register(ViPipe, 0x98, 0x00);
    ov9712_write_register(ViPipe, 0x99, 0x28);
    ov9712_write_register(ViPipe, 0x9a, 0x00);
    ov9712_write_register(ViPipe, 0x57, 0x00);
    ov9712_write_register(ViPipe, 0x58, 0xB4);
    ov9712_write_register(ViPipe, 0x59, 0xA0);
    ov9712_write_register(ViPipe, 0x4c, 0x13);
    ov9712_write_register(ViPipe, 0x4b, 0x36);
    ov9712_write_register(ViPipe, 0x3d, 0x3c);
    ov9712_write_register(ViPipe, 0x3e, 0x03);
    ov9712_write_register(ViPipe, 0xbd, 0xA0);
    ov9712_write_register(ViPipe, 0xbe, 0xb4);
    ov9712_write_register(ViPipe, 0x37, 0x02);
    ov9712_write_register(ViPipe, 0x60, 0x9d);
	//delay_ms(10);
    //YAVG
    ov9712_write_register(ViPipe, 0x4e, 0x55);
    ov9712_write_register(ViPipe, 0x4f, 0x55);
    ov9712_write_register(ViPipe, 0x50, 0x55);
    ov9712_write_register(ViPipe, 0x51, 0x55);
    ov9712_write_register(ViPipe, 0x24, 0x55);
    ov9712_write_register(ViPipe, 0x25, 0x40);
    ov9712_write_register(ViPipe, 0x26, 0xa1);
	//delay_ms(10);
    //Clock
    ov9712_write_register(ViPipe, 0x5c, 0x52);
    ov9712_write_register(ViPipe, 0x5d, 0x00);
    ov9712_write_register(ViPipe, 0x11, 0x01);
    //delay_ms(100);
    ov9712_write_register(ViPipe, 0x2a, 0x9c);
    ov9712_write_register(ViPipe, 0x2b, 0x06);
    ov9712_write_register(ViPipe, 0x2d, 0x00);
    ov9712_write_register(ViPipe, 0x2e, 0x00);
	//delay_ms(10);
    //General
    ov9712_write_register(ViPipe, 0x13, 0xA5);
    ov9712_write_register(ViPipe, 0x14, 0x40);
	//delay_ms(10);
    //Banding
    ov9712_write_register(ViPipe, 0x4a, 0x00);
    ov9712_write_register(ViPipe, 0x49, 0xce);
    ov9712_write_register(ViPipe, 0x22, 0x03);
    ov9712_write_register(ViPipe, 0x09, 0x00);
	//delay_ms(10);
    //close AE_AWB
    ov9712_write_register(ViPipe, 0x13, 0x80);
    ov9712_write_register(ViPipe, 0x16, 0x00);
    //delay_ms(100);
    ov9712_write_register(ViPipe, 0x10, 0xf0);
    ov9712_write_register(ViPipe, 0x00, 0x3f);
    ov9712_write_register(ViPipe, 0x38, 0x00);
    ov9712_write_register(ViPipe, 0x01, 0x40);
    ov9712_write_register(ViPipe, 0x02, 0x40);
    ov9712_write_register(ViPipe, 0x05, 0x40);
    ov9712_write_register(ViPipe, 0x06, 0x00);
    ov9712_write_register(ViPipe, 0x07, 0x00);
	//delay_ms(10);
    //BLC
    ov9712_write_register(ViPipe, 0x41, 0x84);

//	delay_ms(100);
//    ov9712_write_register(ViPipe, 0x2a, 0x9c);
//    ov9712_write_register(ViPipe, 0x2b, 0x06);
//    ov9712_write_register(ViPipe, 0x10, 0xf0);
//    ov9712_write_register(ViPipe, 0x00, 0x3f);
    printf("=========================================================\n");
    printf("===ominivision ov9712 sensor 720P30fps(ViPipe, Parallel port) init success!=====\n");
    printf("=========================================================\n");

    return;   
}
