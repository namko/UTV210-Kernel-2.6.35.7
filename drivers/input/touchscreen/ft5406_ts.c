/*********************************************************************************
 * linux/drivers/input/touchscreen/ft5406_ts.c
 *
 * Copyright (c)    violet313.
 *                  violet313@gmail.com		
 *
 * Defines required for the focaltech ft5406 driver implementation
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *********************************************************************************/

#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/i2c/ft5406_ts.h>
#include <linux/earlysuspend.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/irq.h>

#include <mach/gpio.h>
#include <plat/gpio-cfg.h>


#ifdef CONFIG_TOUCHSCREEN_FT5406_ICS

#define TS_POSITION_X           ABS_MT_POSITION_X
#define TS_POSITION_Y           ABS_MT_POSITION_Y
#define TS_PRESSURE             ABS_MT_PRESSURE
#define TS_TOUCH_SIZE           ABS_MT_TOUCH_MAJOR
#define TS_TOUCH_BTN            BTN_TOUCH

#else

#define TS_POSITION_X           ABS_MT_POSITION_X
#define TS_POSITION_Y           ABS_MT_POSITION_Y
#define TS_PRESSURE             ABS_MT_TOUCH_MAJOR
#define TS_TOUCH_SIZE           ABS_MT_WIDTH_MAJOR

#endif

/***********************************************************************************************
some forward declarations: driver callbacks
***********************************************************************************************/
static int ft5406_ts_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int ft5406_ts_remove(struct i2c_client *client);



/***********************************************************************************************
struct definitions
***********************************************************************************************/
struct ts_event 
{
    u16    x1;
    u16    y1;
    u16    x2;
    u16    y2;
    u16    x3;
    u16    y3;
    u16    x4;
    u16    y4;
    u16    x5;
    u16    y5;
    u16    pressure;
    u8  touch_point;
};

/******************************************************
******************************************************/
struct ft5406_ts_data 
{
    struct input_dev        *input_dev;
    struct ts_event         event;
    struct work_struct      pen_event_work;
    struct workqueue_struct *ts_workqueue;
    struct early_suspend    early_suspend;
};

/******************************************************
******************************************************/
static const struct i2c_device_id ft5406_ts_id[] = 
{
    { FT5406_NAME, 0 },
    { }
};

/******************************************************
******************************************************/
static struct i2c_driver ft5406_ts_driver = 
{
    .probe=     ft5406_ts_probe,
    .remove=    __devexit_p(ft5406_ts_remove),
    .id_table=  ft5406_ts_id,

    .driver= 
    {
        .name=  FT5406_NAME,
        .owner= THIS_MODULE,
    },
};


/***********************************************************************************************
global variables
***********************************************************************************************/
static struct i2c_client   *this_client;




/***********************************************************************************************
Name:       u8tobinstr 
Input:      uiIn
Output:    
function:   lil debug print helper
***********************************************************************************************/
char* u8tobinstr(u8 uiIn)
{
    const u8 uiBits=8*sizeof(u8);
    static char szRet[uiBits +1]={0};
    u8 uiNextBit=uiBits;
    while (uiNextBit--)
        szRet[(uiBits-1)-uiNextBit]=(uiIn & (1 << uiNextBit) ? '1' : '0');
    
    return szRet;
}

/***********************************************************************************************
Name:       u8tobinstr 
Input:      uiIn
Output:    
function:   lil debug print helper
***********************************************************************************************/
char* u8arraytohexstr(u8* puiIn, u8 cbCount)
{
    const char szFormat[]="0x%02x,";
    const size_t cbFormatSize=5*sizeof(char);
    static char szRet[1280]={0};
    char* pOff=szRet;
 
    *(pOff++)='{';
    do
    {
        sprintf(pOff,szFormat,*(puiIn++));
        pOff+=cbFormatSize;
    }
    while (--cbCount);
    *(pOff-1)='}';
    *pOff=0;
    
    return szRet;
}

/***********************************************************************************************
Name:   ft5406_i2c_rxdata 
Input:  *rxdata
        *length
Output:    ret
function:    
***********************************************************************************************/
static int ft5406_i2c_rxdata(char *rxdata, int length)
{
    int ret;

    struct i2c_msg msgs[] = 
    {        
        {
            .addr   = this_client->addr,
            .flags  = 0 ,
            .len    = 1,
            .buf    = rxdata,
        },

        {
            .addr   = this_client->addr,
            .flags  = I2C_M_RD,
            .len    = length,
            .buf    = rxdata,
        },
    };

    ret = i2c_transfer(this_client->adapter, msgs, 2);
    if (ret < 0)
        pr_err("msg %s i2c write internal address error: %d\n", __func__, ret);

    return ret;
}

/***********************************************************************************************
Name:       
Input:      
Output:    
function:   
***********************************************************************************************/
static int ft5406_i2c_txdata(char *txdata, int length)
{
    int ret;

    struct i2c_msg msg[] = 
    {
        {
            .addr   = this_client->addr,
            .flags  = 0,
            .len    = length,
            .buf    = txdata,
        },
    };

    ret = i2c_transfer(this_client->adapter, msg, 1);
    if (ret < 0)
        pr_err("%s i2c write error: %d\n", __func__, ret);

    return ret;
}


/***********************************************************************************************
Name:
Input:
Output:
function:
***********************************************************************************************/
static void ft5406_ts_release(void)
{
    struct ft5406_ts_data *data = i2c_get_clientdata(this_client);

#ifdef CONFIG_TOUCHSCREEN_FT5406_ICS
    input_mt_sync(data->input_dev);
    input_report_key(data->input_dev, TS_TOUCH_BTN, 0);
#else
    input_report_abs(data->input_dev, TS_PRESSURE, 0);
#endif

    input_sync(data->input_dev);
}

/***********************************************************************************************
Name:
Input:
Output:
function:
***********************************************************************************************/
static int ft5406_read_data(void)
{
    struct ft5406_ts_data *data = i2c_get_clientdata(this_client);
    struct ts_event *event = &data->event;
    u8 buf[32] = {0};
    int ret = -1;
    bool bSane=0;

    /*voodoos say we read 26 bytes of five-point touch data from register 0xF9*/
    buf[0]=0xF9;
    ret = ft5406_i2c_rxdata(buf, 26);
    if (ret < 0) 
    {
        dev_err(&this_client->dev, "%s read_data i2c_rxdata failed: %d\n", __func__, ret);
        ft5406_ts_release();
        return ret;
    }


    dev_dbg(&this_client->dev, "ft5406_read_data: buf=%s\n", u8arraytohexstr(buf,sizeof(buf)));


    memset(event, 0, sizeof(struct ts_event));

    /*/////////////////////////////////////////////////////////////////////////////////////
    //current ft5406 touch data format as follows:
    ///////////////////////////////////////////////////////////////////////////////////////

    ---------------------------------------------------------------------------------------
    byte        description             value       comments
    ---------------------------------------------------------------------------------------
    00:         header/ID 1/2               0xAA        part 1 of 2 bytes
    01:         header/ID 2/2               0xAA        part 2 of 2 bytes
    02:         packet length               0x1A        should be 26 bytes
    03:         touch point data count      0x0X        this device supports up to 5 point simultaneous touch data. bits [3:0] ctn a value from 0-5d  
    04:         strength/gestures           0xXX        4bit strength [7:4]  2bit gesture code [3:2] 2bit geture value [1:0]

    05:         xtp(1) 12bit abscissa 1/2   0x0X        bits [3:0]
    06:         xtp(1) 12bit abscissa 2/2   0xXX        bits [7:0]
    07:         ytp(1) 12bit abscissa 1/2   0x0X        bits [3:0]
    08:         ytp(1) 12bit abscissa 2/2   0xXX        bits [7:0]

    09:         xtp(2) 12bit abscissa 1/2   0x0X        bits [3:0]
    10:         xtp(2) 12bit abscissa 2/2   0xXX        bits [7:0]
    11:         ytp(2) 12bit abscissa 1/2   0x0X        bits [3:0]
    12:         ytp(2) 12bit abscissa 2/2   0xXX        bits [7:0]

    13:         xtp(3) 12bit abscissa 1/2   0x0X        bits [3:0]
    14:         xtp(3) 12bit abscissa 2/2   0xXX        bits [7:0]
    15:         ytp(3) 12bit abscissa 1/2   0x0X        bits [3:0]
    16:         ytp(3) 12bit abscissa 2/2   0xXX        bits [7:0]

    17:         xtp(4) 12bit abscissa 1/2   0x0X        bits [3:0]
    18:         xtp(4) 12bit abscissa 2/2   0xXX        bits [7:0]
    19:         ytp(4) 12bit abscissa 1/2   0x0X        bits [3:0]
    20:         ytp(4) 12bit abscissa 2/2   0xXX        bits [7:0]
    
    21:         xtp(5) 12bit abscissa 1/2   0x0X        bits [3:0]
    22:         xtp(5) 12bit abscissa 2/2   0xXX        bits [7:0]
    23:         ytp(5) 12bit abscissa 1/2   0x0X        bits [3:0]
    24:         ytp(5) 12bit abscissa 2/2   0xXX        bits [7:0]

    25:         CRC                         0xXX        an XOR checksum of first 25 msg bytes to validate sanity    
    /////////////////////////////////////////////////////////////////////////////////////*/
    
    /*check msg header && msg length are as expected*/
    bSane=(buf[0] & 0xAA) && (buf[1] & 0xAA) && (buf[2] & 0x1A);
    if (!bSane)
    {
        dev_err(&this_client->dev, "ft5406_read_data: insane touch data ~not *dog\n");
        ft5406_ts_release();
        return 1;
    }

    /*read byte03 & count the number of touch points ctd in this msg*/
    event->touch_point = buf[3] & 0xF;

    if (event->touch_point == 0) 
    {
        ft5406_ts_release();
        return 2;
    }

    /*read byte04: strength/gestures: 4bit strength[7:4]  2bit gesture code[3:2] 2bit gesture value[1:0]*/



    /*read upto five points of touch data*/
    switch (event->touch_point) 
    {
        case 5:
            event->x5 = (s16)(buf[21] & 0x0F)<<8 | (s16)buf[22];
            event->y5 = (s16)(buf[23] & 0x0F)<<8 | (s16)buf[24];
        case 4:
            event->x4 = (s16)(buf[17] & 0x0F)<<8 | (s16)buf[18];
            event->y4 = (s16)(buf[19] & 0x0F)<<8 | (s16)buf[20];
        case 3:
            event->x3 = (s16)(buf[13] & 0x0F)<<8 | (s16)buf[14];
            event->y3 = (s16)(buf[15] & 0x0F)<<8 | (s16)buf[16];
        case 2:
            event->x2 = (s16)(buf[9] & 0x0F)<<8 | (s16)buf[10];
            event->y2 = (s16)(buf[11] & 0x0F)<<8 | (s16)buf[12];
        case 1:
            event->x1 = (s16)(buf[5] & 0x0F)<<8 | (s16)buf[6];
            event->y1 = (s16)(buf[7] & 0x0F)<<8 | (s16)buf[8];
            break;
        default:
            return -1;
    }

    event->pressure = 200;


    dev_dbg(&this_client->dev, "%s: 1:%d %d 2:%d %d \n", __func__, event->x1, event->y1, event->x2, event->y2);

    return 0;
}

/***********************************************************************************************
Name:       
Input:      
Output:    
function:   
***********************************************************************************************/
static void ft5406_report_value(void)
{
    struct ft5406_ts_data *data = i2c_get_clientdata(this_client);
    struct ts_event *event = &data->event;


    dev_dbg(&this_client->dev, "==ft5406_report_value =\n");

    switch(event->touch_point) 
    {
        case 5:
            input_report_abs(data->input_dev, TS_PRESSURE, event->pressure);
            input_report_abs(data->input_dev, TS_POSITION_X, event->x5);
            input_report_abs(data->input_dev, TS_POSITION_Y, event->y5);
            input_report_abs(data->input_dev, TS_TOUCH_SIZE, 1);
            input_mt_sync(data->input_dev);
            dev_dbg(&this_client->dev, "===x2 = %d,y2 = %d ====\n",event->x2,event->y2);
        case 4:
            input_report_abs(data->input_dev, TS_PRESSURE, event->pressure);
            input_report_abs(data->input_dev, TS_POSITION_X, event->x4);
            input_report_abs(data->input_dev, TS_POSITION_Y, event->y4);
            input_report_abs(data->input_dev, TS_TOUCH_SIZE, 1);
            input_mt_sync(data->input_dev);
            dev_dbg(&this_client->dev, "===x2 = %d,y2 = %d ====\n",event->x2,event->y2);
        case 3:
            input_report_abs(data->input_dev, TS_PRESSURE, event->pressure);
            input_report_abs(data->input_dev, TS_POSITION_X, event->x3);
            input_report_abs(data->input_dev, TS_POSITION_Y, event->y3);
            input_report_abs(data->input_dev, TS_TOUCH_SIZE, 1);
            input_mt_sync(data->input_dev);
            dev_dbg(&this_client->dev, "===x2 = %d,y2 = %d ====\n",event->x2,event->y2);
        case 2:
            input_report_abs(data->input_dev, TS_PRESSURE, event->pressure);
            input_report_abs(data->input_dev, TS_POSITION_X, event->x2);
            input_report_abs(data->input_dev, TS_POSITION_Y, event->y2);
            input_report_abs(data->input_dev, TS_TOUCH_SIZE, 1);
            input_mt_sync(data->input_dev);
            dev_dbg(&this_client->dev, "===x2 = %d,y2 = %d ====\n",event->x2,event->y2);
        case 1:
            input_report_abs(data->input_dev, TS_PRESSURE, event->pressure);
            input_report_abs(data->input_dev, TS_POSITION_X, event->x1);
            input_report_abs(data->input_dev, TS_POSITION_Y, event->y1);
            input_report_abs(data->input_dev, TS_TOUCH_SIZE, 1);
            input_mt_sync(data->input_dev);
            dev_dbg(&this_client->dev, "===x1 = %d,y1 = %d ====\n",event->x1,event->y1);

#ifdef CONFIG_TOUCHSCREEN_FT5406_ICS
            input_report_key(data->input_dev, TS_TOUCH_BTN, 1);
#endif

        default:
            break;
    }

    input_sync(data->input_dev);

    dev_dbg(&this_client->dev, "%s: 1:%d %d 2:%d %d \n", __func__, event->x1, event->y1, event->x2, event->y2);
    
}    /*end ft5406_report_value*/

/***********************************************************************************************
Name:       
Input:      
Output:    
function:   
***********************************************************************************************/
static void ft5406_ts_pen_irq_work(struct work_struct *work)
{
    int ret = -1;

    dev_dbg(&this_client->dev, "==ft5406_ts_pen_irq_work==\n");

    ret = ft5406_read_data();
    if (ret == 0) 
    {    
        ft5406_report_value();
    }
    else 
        dev_dbg(&this_client->dev, "data package read error\n");
}

/***********************************************************************************************
Name:       
Input:      
Output:    
function:   
***********************************************************************************************/
static irqreturn_t ft5406_ts_interrupt(int irq, void *dev_id)
{
    struct ft5406_ts_data *ft5406_ts = dev_id;
        
    if (!work_pending(&ft5406_ts->pen_event_work)) 
    {
        queue_work(ft5406_ts->ts_workqueue, &ft5406_ts->pen_event_work);
    }

    return IRQ_HANDLED;
}


#ifdef CONFIG_HAS_EARLYSUSPEND
/***********************************************************************************************
Name:       
Input:      
Output:    
function:   
***********************************************************************************************/
static void ft5406_ts_suspend(struct early_suspend *handler)
{
    struct ft5406_ts_data *ts;
    dev_dbg(&this_client->dev, "==ft5406_ts_suspend=\n");
    ts = container_of(handler, struct ft5406_ts_data, early_suspend);
    gpio_request(0x95, "ft5x0x-ts-power-en");
    gpio_direction_output(0x95, 0);
    gpio_free(0x95);
    disable_irq(this_client->irq);
    cancel_work_sync(&ts->pen_event_work);
    flush_workqueue(ts->ts_workqueue);
}

/***********************************************************************************************
Name:       
Input:      
Output:    
function:   
***********************************************************************************************/
static void ft5406_ts_resume(struct early_suspend *handler)
{
    struct ft5406_ts_data *ts;
    dev_dbg(&this_client->dev, "==ft5406_ts_resume=\n");
    ts = container_of(handler, struct ft5406_ts_data, early_suspend);
    gpio_request(0x95, "ft5x0x-ts-power-en");
    gpio_request(0x93, "ft5x0x-ts-wake-en");
    gpio_request(0x89, "???");
    s3c_gpio_cfgpin(0x89, 0);
    gpio_direction_output(0x95, 0);
    gpio_direction_output(0x89, 0);
    gpio_direction_output(0x93, 0);
    udelay(500);
    gpio_direction_output(0x95, 1);
    gpio_direction_output(0x93, 1);
    udelay(100);
    s3c_gpio_cfgpin(0x89, 0xF);
    gpio_set_value(0x89, 1);
    s3c_gpio_setpull(0x89, 0x2);
    set_irq_type(this_client->irq, IRQF_TRIGGER_FALLING);
    gpio_free(0x89);
    gpio_free(0x93);
    gpio_free(0x95);
    disable_irq(this_client->irq);
    free_irq(this_client->irq, ts);
    request_irq(this_client->irq, ft5406_ts_interrupt, IRQF_TRIGGER_FALLING, FT5406_NAME, ts);
}
#endif  /*CONFIG_HAS_EARLYSUSPEND*/

/***********************************************************************************************
Name:       
Input:      
Output:    
function:   
***********************************************************************************************/
static int ft5406_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    struct ft5406_ts_data *ft5406_ts;
    struct input_dev *input_dev;
    u8 auOEMVoodoos1[]={0x88, 0x05};
    u8 auOEMVoodoos2[]={0x80, 0x1E};
    int nGPIOWake, nGPIOPowerUp;
    int err = 0;

    
    printk("==ft5406_ts_probe=\n");
    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) 
    {
        err = -ENODEV;
        goto exit_check_functionality_failed;
    }

    ft5406_ts = kzalloc(sizeof(*ft5406_ts), GFP_KERNEL);
    if (!ft5406_ts)    
    {
        err = -ENOMEM;
        goto exit_alloc_data_failed;
    }
    
    this_client = client;
    i2c_set_clientdata(client, ft5406_ts);

    dev_dbg(&this_client->dev, "ft5406_ts_probe: this_client->addr[0x%x]\n",this_client->addr);
    dev_dbg(&this_client->dev, "ft5406_ts_probe: this_client->irq[0x%x]\n",this_client->irq);
    dev_dbg(&this_client->dev, "ft5406_ts_probe: this_client->name[%s]\n",this_client->name);
    dev_dbg(&this_client->dev, "ft5406_ts_probe: this_client->dev[%s]\n",dev_name(&this_client->dev));
    dev_dbg(&this_client->dev, "ft5406_ts_probe: this_client->adapter->name[%s]\n",this_client->adapter->name);
    dev_dbg(&this_client->dev, "ft5406_ts_probe: this_client->adapter->id[0x%x]\n",this_client->adapter->id);


    INIT_WORK(&ft5406_ts->pen_event_work, ft5406_ts_pen_irq_work);


    ft5406_ts->ts_workqueue = create_singlethread_workqueue(dev_name(&client->dev));
    if (!ft5406_ts->ts_workqueue) 
    {
        err = -ESRCH;
        dev_err(&this_client->dev,"==singlethread error==\n");
        goto exit_create_singlethread;
    }

    this_client->irq = IRQ_EINT(8);
    err = request_irq(this_client->irq, ft5406_ts_interrupt, IRQF_TRIGGER_FALLING, FT5406_NAME, ft5406_ts);
    if (err < 0) 
    {
        dev_err(&client->dev, "ft5406_probe: request irq failed\n");
        goto exit_irq_request_failed;
    }
    disable_irq(this_client->irq);


    input_dev = input_allocate_device();
    if (!input_dev) 
    {
        err = -ENOMEM;
        dev_err(&client->dev, "failed to allocate input device\n");
        goto exit_input_dev_alloc_failed;
    }    

    ft5406_ts->input_dev = input_dev;
    input_dev->name = FT5406_NAME;

#ifdef CONFIG_TOUCHSCREEN_FT5406_ICS
    set_bit(TS_TOUCH_BTN, input_dev->keybit);
#endif

    set_bit(TS_PRESSURE, input_dev->absbit);
    set_bit(TS_POSITION_X, input_dev->absbit);
    set_bit(TS_POSITION_Y, input_dev->absbit);
    set_bit(TS_TOUCH_SIZE, input_dev->absbit);

    input_set_abs_params(input_dev, TS_POSITION_X, 0, SCREEN_MAX_X, 0, 0);
    input_set_abs_params(input_dev, TS_POSITION_Y, 0, SCREEN_MAX_Y, 0, 0);
    input_set_abs_params(input_dev, TS_PRESSURE, 0, PRESS_MAX, 0, 0);
    input_set_abs_params(input_dev, TS_TOUCH_SIZE, 0, 200, 0, 0);

    set_bit(EV_ABS, input_dev->evbit);
    set_bit(EV_KEY, input_dev->evbit);



    /*this creates the virtual i2c bus master device 
    there will be an event<x> file in /dev/input/  */
    err = input_register_device(input_dev);
    if (err) 
    {
        dev_err(&client->dev,"ft5406_ts_probe: failed to register input device: %s\n", dev_name(&client->dev));
        goto exit_input_register_device_failed;
    }

#ifdef CONFIG_HAS_EARLYSUSPEND
    dev_dbg(&this_client->dev, "==register_early_suspend =\n");
    ft5406_ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
    ft5406_ts->early_suspend.suspend = ft5406_ts_suspend;
    ft5406_ts->early_suspend.resume = ft5406_ts_resume;
    register_early_suspend(&ft5406_ts->early_suspend);
#endif


        

    /*try to power-up the touchscreen*/
    nGPIOPowerUp=S5PV210_GPH2(3);
    if ((err=gpio_request(nGPIOPowerUp, "ft5406 power-up")) < 0)
    {
        dev_err(&client->dev,"gpio_request failed for nGPIOPowerUp[%d]\n",nGPIOPowerUp);
        goto exit_gpio_power_up_failed;
    }    
    else if ((err=gpio_direction_output(nGPIOPowerUp,1)) < 0)
    {
        dev_err(&client->dev,"gpio_direction_output failed for nGPIOPowerUp[%d]\n",nGPIOPowerUp);
        gpio_free(nGPIOPowerUp);
        goto exit_gpio_power_up_failed;
    }
    else
    {
        dev_dbg(&this_client->dev, "setting nGPIOPowerUp[%d]\n\n",nGPIOPowerUp);
        gpio_set_value(nGPIOPowerUp, 1);
        msleep(300);
        gpio_free(nGPIOPowerUp);
    }
   
    
    /*try to wake-up the touchscreen*/
    nGPIOWake=S5PV210_GPH2(1);
    if ((err=gpio_request(nGPIOWake, "ft5406 wake-up")) < 0)
    {
        dev_err(&client->dev,"gpio_request failed for nGPIOWake[%d]\n\n",nGPIOWake);
        goto exit_gpio_wake_up_failed;
    }    
    else if ((err=gpio_direction_output(nGPIOWake,0)) < 0)
    {
        dev_err(&client->dev,"gpio_direction_output failed for nGPIOWake[%d]\n\n",nGPIOWake);
        gpio_free(nGPIOWake);
        goto exit_gpio_wake_up_failed;
    }
    else
    {
        dev_dbg(&this_client->dev, "setting nGPIOWake[%d]\n\n",nGPIOWake);
        gpio_set_value(nGPIOWake, 0);
        msleep(100);
        gpio_set_value(nGPIOWake, 1);
        msleep(100);
        gpio_free(nGPIOWake);
    }   
    msleep(350);


    dev_dbg(&this_client->dev, "attempting to activate ctpm device *work-mode with voodoos, writing 0x05 to register 0x88 and 0x1E to register 0x80\n");
    if ((err=ft5406_i2c_txdata(auOEMVoodoos1, sizeof(auOEMVoodoos1))) < 0 ||
            (err=ft5406_i2c_txdata(auOEMVoodoos2, sizeof(auOEMVoodoos2))) < 0)
    {
        dev_err(&client->dev,"attempt to activate ctpm device *work-mode with voodoos failed with err[%d]\n", err);
        goto exit_activate_ft5406_workmode_failed;
    }
    msleep(20);

        
    enable_irq(this_client->irq);

    printk("==ft5406_ts_probe complete==\n\n");
    return 0;



exit_activate_ft5406_workmode_failed:
exit_gpio_wake_up_failed:
exit_gpio_power_up_failed:


exit_input_register_device_failed:
    input_free_device(input_dev);

exit_input_dev_alloc_failed:
    free_irq(this_client->irq, ft5406_ts);

exit_irq_request_failed:
    cancel_work_sync(&ft5406_ts->pen_event_work);
    destroy_workqueue(ft5406_ts->ts_workqueue);

exit_create_singlethread:
    i2c_set_clientdata(client, NULL);
    kfree(ft5406_ts);

exit_alloc_data_failed:
exit_check_functionality_failed:

    printk("==ft5406_ts_probe failed==\n\n");
    return err;
}

/***********************************************************************************************
Name:       
Input:      
Output:    
function:   
***********************************************************************************************/
static int __devexit ft5406_ts_remove(struct i2c_client *client)
{
    struct ft5406_ts_data *ft5406_ts=0;

    printk("==ft5406_ts_remove=\n");
    
    ft5406_ts = i2c_get_clientdata(client);
    unregister_early_suspend(&ft5406_ts->early_suspend);
    free_irq(this_client->irq, ft5406_ts);

    input_unregister_device(ft5406_ts->input_dev);
    kfree(ft5406_ts);
    cancel_work_sync(&ft5406_ts->pen_event_work);
    destroy_workqueue(ft5406_ts->ts_workqueue);
    i2c_set_clientdata(client, NULL);
    return 0;
}

/***********************************************************************************************
Name:     
Input:    
Output:    
function:    
***********************************************************************************************/
static int __init ft5406_ts_init(void)
{
    int ret;

    printk("==ft5406_ts_init==\n");
    ret = i2c_add_driver(&ft5406_ts_driver);
    printk("==ft5406_ts_init returned[%d]==\n",ret);

    return ret;
}

/***********************************************************************************************
Name:     
Input:    
Output:    
function:    
***********************************************************************************************/
static void __exit ft5406_ts_exit(void)
{
    printk("==ft5406_ts_exit==\n");
    i2c_del_driver(&ft5406_ts_driver);
}


/***********************************************************************************************
***********************************************************************************************/
module_init(ft5406_ts_init);
module_exit(ft5406_ts_exit);


MODULE_DEVICE_TABLE(i2c, ft5406_ts_id);
MODULE_AUTHOR("<violet313@gmail.com>");
MODULE_DESCRIPTION("violet313 ft5406 TouchScreen driver");
MODULE_LICENSE("GPL");

/*end of file*/

