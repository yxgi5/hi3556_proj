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

const unsigned char naneyem_i2c_addr     =    0x50; /* I2C Address of naneyem decoder */
const unsigned int  naneyem_addr_byte    =    1;
const unsigned int  naneyem_data_byte    =    1;
static int g_fd[ISP_MAX_PIPE_NUM] = {[0 ... (ISP_MAX_PIPE_NUM - 1)] = -1};

extern ISP_SNS_STATE_S       *g_pastNaneyem[ISP_MAX_PIPE_NUM];
extern ISP_SNS_COMMBUS_U      g_aunNaneyemBusInfo[];

#define NANEYEM_320P_24FPS_MODE  (1)

void naneyem_linear_320p24_init(VI_PIPE ViPipe);

int naneyem_i2c_init(VI_PIPE ViPipe)
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

    u8DevNum = g_aunNaneyemBusInfo[ViPipe].s8I2cDev;
    snprintf(acDevFile, sizeof(acDevFile),  "/dev/i2c-%u", u8DevNum);

    g_fd[ViPipe] = open(acDevFile, O_RDWR, S_IRUSR | S_IWUSR);

    if (g_fd[ViPipe] < 0)
    {
        ISP_TRACE(HI_DBG_ERR, "Open /dev/hi_i2c_drv-%u error!\n", u8DevNum);
        return HI_FAILURE;
    }

    ret = ioctl(g_fd[ViPipe], I2C_SLAVE_FORCE, (naneyem_i2c_addr >> 1));
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

int naneyem_i2c_exit(VI_PIPE ViPipe)
{
    if (g_fd[ViPipe] >= 0)
    {
        close(g_fd[ViPipe]);
        g_fd[ViPipe] = -1;
        return HI_SUCCESS;
    }
    return HI_FAILURE;
}

int naneyem_read_register(VI_PIPE ViPipe, int addr)
{
    // TODO:

    return HI_SUCCESS;
}

int naneyem_write_register(VI_PIPE ViPipe, int addr, int data)
{
    if (0 > g_fd[ViPipe])
    {
        return HI_SUCCESS;
    }

#ifdef HI_GPIO_I2C
    i2c_data.dev_addr = naneyem_i2c_addr;
    i2c_data.reg_addr = addr;
    i2c_data.addr_byte_num = naneyem_addr_byte;
    i2c_data.data = data;
    i2c_data.data_byte_num = naneyem_data_byte;

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

    if (naneyem_addr_byte == 2)
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

    if (naneyem_data_byte == 2)
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

	printf("[file]:%s [func]:%s [line]:%d:I2C_WRITING! #%x #%x\n", __FILE__, __FUNCTION__, __LINE__, addr, data);
    ret = write(g_fd[ViPipe], buf, (naneyem_addr_byte + naneyem_data_byte));
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

void naneyem_standby(VI_PIPE ViPipe)
{
    // TODO:
    return;
}

void naneyem_restart(VI_PIPE ViPipe)
{
    // TODO:
    return;
}

void naneyem_prog(VI_PIPE ViPipe, int *rom)
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
            naneyem_write_register(ViPipe, addr, data);
        }
    }
}

void naneyem_init(VI_PIPE ViPipe)
{
	HI_U8           u8ImgMode;
    HI_BOOL         bInit;
    
    bInit       = g_pastNaneyem[ViPipe]->bInit;
    u8ImgMode   = g_pastNaneyem[ViPipe]->u8ImgMode;
    
    /* 1. sensor i2c init */
    naneyem_i2c_init(ViPipe);
    
    /* When sensor first init, config all registers */
    if (HI_FALSE == bInit)
    {
        switch (u8ImgMode)
        {
            case NANEYEM_320P_24FPS_MODE:
                naneyem_linear_320p24_init(ViPipe);
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
            case NANEYEM_320P_24FPS_MODE:
                naneyem_linear_320p24_init(ViPipe);
                break;
            default:
                ISP_TRACE(HI_DBG_ERR, "Not Support Image Mode %d\n", u8ImgMode);
                break;
        }
    }
    
    g_pastNaneyem[ViPipe]->bInit = HI_TRUE;
}

void naneyem_exit(VI_PIPE ViPipe)
{
    naneyem_i2c_exit(ViPipe);

    return;
}

/* 320P24 */
void naneyem_linear_320p24_init(VI_PIPE ViPipe)
{
    //Reset
    naneyem_write_register(ViPipe, 0x00, 0x80);
    naneyem_write_register(ViPipe, 0x01, 0x9f);
    naneyem_write_register(ViPipe, 0x02, 0x03);
    naneyem_write_register(ViPipe, 0x03, 0x6d);
    //delay_ms(10);
    printf("=========================================================\n");
    printf("===asm naneyem sensor 320P24fps(ViPipe, Parallel port) init success!=====\n");
    printf("=========================================================\n");

    return;   
}
