/* linux/drivers/media/video/hm2055.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define DEBUG

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-i2c-drv.h>
#include <media/utvcam_platform.h>

#ifdef CONFIG_VIDEO_SAMSUNG_V4L2
#include <linux/videodev2_samsung.h>
#endif

#include "hm2055.h"

#define HM2055_DRIVER_NAME          "hm2055"
#define DEFAULT_FMT                 V4L2_PIX_FMT_YUYV
#define DEFAULT_FREQ                24000000

/* Camera functional setting values configured by user concept */
struct hm2055_userset {
    int brightness;     /* V4L2_CID_CAMERA_BRIGHTNESS */
    int wb;             /* V4L2_CID_CAMERA_WHITE_BALANCE */
    int effect;         /* V4L2_CID_CAMERA_EFFECT */
};

struct hm2055_state {
    struct v4l2_subdev sd;
    struct v4l2_pix_format pix;
    struct hm2055_userset userset;

    int freq;
    int is_mipi;
    int framesize_index;
    int check_previewdata;
};

struct hm2055_enum_framesize {
    unsigned int index;
    unsigned int width;
    unsigned int height;
};

enum {
    HM2055_PREVIEW_VGA,
    HM2055_PREVIEW_2M,
};

struct hm2055_enum_framesize hm2055_framesize_list[] = {
    { HM2055_PREVIEW_VGA, 640, 480 },
    { HM2055_PREVIEW_2M, 1600, 1200 },
};

static inline struct hm2055_state *to_state(struct v4l2_subdev *sd) {
    return container_of(sd, struct hm2055_state, sd);
}

/*
 * HM2055 register structure : 2bytes address, 1bytes value
 * retry on write failure up-to 3 times
 */
static inline int hm2055_write(struct v4l2_subdev *sd, u16 addr, u8 val) {
    struct i2c_client *client = v4l2_get_subdevdata(sd);
    struct i2c_msg msg[1];
    unsigned char reg[3];
    int err = 0;
    int retry = 0;

    if (!client->adapter)
        return -ENODEV;

again:
    msg->addr = client->addr;
    msg->flags = 0;
    msg->len = 3;
    msg->buf = reg;

    reg[0] = addr >> 8;
    reg[1] = addr & 0xff;
    reg[2] = val & 0xff;

    err = i2c_transfer(client->adapter, msg, 1);
    if (err >= 0)
        return err;    /* Returns here on success */

    /* abnormal case: retry 3 times */
    if (retry < 3) {
        dev_err(&client->dev, "%s: address: 0x%02x%02x, "
            "value: 0x%02x\n", __func__,
            reg[0], reg[1], reg[2]);
        retry++;
        goto again;
    }

    return err;
}

static int hm2055_write_regs(struct v4l2_subdev *sd, struct hm2055_reg regs[], int size) {
    int i, err;
    struct i2c_client *client = v4l2_get_subdevdata(sd);

    for (i = 0; i < size; i++)
        if ((err = hm2055_write(sd, regs[i].addr, regs[i].value)) < 0)
            v4l_info(client, "%s: register set failed\n", __func__);

    return 0;
}

static int hm2055_s_config(struct v4l2_subdev *sd, int irq, void *platform_data) {
    struct i2c_client *client = v4l2_get_subdevdata(sd);
    struct hm2055_state *state = to_state(sd);
    struct utvcam_platform_data *pdata;

    dev_info(&client->dev, "fetching platform data\n");
    pdata = client->dev.platform_data;

    if (!pdata) {
        dev_err(&client->dev, "%s: no platform data\n", __func__);
        return -ENODEV;
    }

    /*
     * Assign default format and resolution
     * Use configured default information in platform data
     * or without them, use default information in driver
     */
    if (!(pdata->default_width && pdata->default_height)) {
        state->pix.width = hm2055_framesize_list->width;
        state->pix.height = hm2055_framesize_list->height;
    } else {
        state->pix.width = pdata->default_width;
        state->pix.height = pdata->default_height;
    }

    if (!pdata->pixelformat)
        state->pix.pixelformat = DEFAULT_FMT;
    else
        state->pix.pixelformat = pdata->pixelformat;

    if (!pdata->freq)
        state->freq = DEFAULT_FREQ;
    else
        state->freq = pdata->freq;

    if (!pdata->is_mipi) {
        dev_info(&client->dev, "parallel mode\n");
        state->is_mipi = 0;
    } else
        state->is_mipi = pdata->is_mipi;

    return 0;
}

static int hm2055_init(struct v4l2_subdev *sd, u32 val) {
    int err = -EINVAL;

    struct i2c_client *client = v4l2_get_subdevdata(sd);
    struct hm2055_state *state = to_state(sd);

    v4l_info(client, "%s: camera initialization start\n", __func__);

    if ((err = hm2055_write_regs(sd, hm2055_init_reg, ARRAY_SIZE(hm2055_init_reg)))) {
        state->check_previewdata = 100;
        v4l_err(client, "%s: camera initialization failed. err(%d)\n",
            __func__, state->check_previewdata);
        return -EIO;
    }

    state->check_previewdata = 0;
    return 0;
}

static int hm2055_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl) {
    int err = 0;
    struct i2c_client *client = v4l2_get_subdevdata(sd);
    struct hm2055_state *state = to_state(sd);
    struct hm2055_userset *userset = &state->userset;

    switch (ctrl->id) {
    case V4L2_CID_CAMERA_BRIGHTNESS:
        ctrl->value = userset->brightness;
        break;
    case V4L2_CID_CAMERA_WHITE_BALANCE:
        ctrl->value = userset->wb;
        break;
    case V4L2_CID_CAMERA_EFFECT:
        ctrl->value = userset->effect;
        break;
    case V4L2_CID_WHITE_BALANCE_PRESET:
    case V4L2_CID_CAMERA_CONTRAST:
    case V4L2_CID_CAMERA_SATURATION:
    case V4L2_CID_CAMERA_SHARPNESS:
    case V4L2_CID_CAM_JPEG_MAIN_SIZE:
    case V4L2_CID_CAM_JPEG_MAIN_OFFSET:
    case V4L2_CID_CAM_JPEG_THUMB_SIZE:
    case V4L2_CID_CAM_JPEG_THUMB_OFFSET:
    case V4L2_CID_CAM_JPEG_POSTVIEW_OFFSET:
    case V4L2_CID_CAM_JPEG_MEMSIZE:
    case V4L2_CID_CAM_JPEG_QUALITY:
    case V4L2_CID_CAMERA_AUTO_FOCUS_RESULT:
    case V4L2_CID_CAM_DATE_INFO_YEAR:
    case V4L2_CID_CAM_DATE_INFO_MONTH:
    case V4L2_CID_CAM_DATE_INFO_DATE:
    case V4L2_CID_CAM_SENSOR_VER:
    case V4L2_CID_CAM_FW_MINOR_VER:
    case V4L2_CID_CAM_FW_MAJOR_VER:
    case V4L2_CID_CAM_PRM_MINOR_VER:
    case V4L2_CID_CAM_PRM_MAJOR_VER:
    case V4L2_CID_ESD_INT:
    case V4L2_CID_CAMERA_GET_ISO:
    case V4L2_CID_CAMERA_GET_SHT_TIME:
    case V4L2_CID_CAMERA_OBJ_TRACKING_STATUS:
    case V4L2_CID_CAMERA_SMART_AUTO_STATUS:
    case V4L2_CID_EXPOSURE:
        break;
    default:
        dev_err(&client->dev, "%s: no such ctrl\n", __func__);
        break;
    }

    return err;
}

static int hm2055_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl) {
    int err = 0;
    int value = ctrl->value;

    struct i2c_client *client = v4l2_get_subdevdata(sd);
    struct hm2055_state *state = to_state(sd);
    struct hm2055_userset *userset = &state->userset;

    switch (ctrl->id) {
    case V4L2_CID_CAMERA_BRIGHTNESS:
        dev_dbg(&client->dev, "%s: V4L2_CID_BRIGHTNESS\n", __func__);
        switch (value) {
        case EV_MINUS_2:
            err = hm2055_write_regs(sd, hm2055_exposure_m2, ARRAY_SIZE(hm2055_exposure_m2));
            break;
        case EV_MINUS_1:
            err = hm2055_write_regs(sd, hm2055_exposure_m1, ARRAY_SIZE(hm2055_exposure_m1));
            break;
        default:
            dev_err(&client->dev, "Invalid value for camera control 0x%08x.\n", ctrl->id);
            value = EV_DEFAULT;
        case EV_DEFAULT:
            err = hm2055_write_regs(sd, hm2055_exposure_0, ARRAY_SIZE(hm2055_exposure_0));
            break;
        case EV_PLUS_1:
            err = hm2055_write_regs(sd, hm2055_exposure_p1, ARRAY_SIZE(hm2055_exposure_p1));
            break;
        case EV_PLUS_2:
            err = hm2055_write_regs(sd, hm2055_exposure_p2, ARRAY_SIZE(hm2055_exposure_p2));
            break;
        }

        if (!err)
            userset->brightness = value;

        break;

    case V4L2_CID_CAMERA_WHITE_BALANCE:
        dev_dbg(&client->dev, "%s: V4L2_CID_CAMERA_WHITE_BALANCE\n", __func__);
        switch (value) {
        default:
            dev_err(&client->dev, "Invalid value for camera control 0x%08x.\n", ctrl->id);
            value = WHITE_BALANCE_AUTO;
        case WHITE_BALANCE_AUTO:
            err = hm2055_write_regs(sd, hm2055_white_balance_auto, ARRAY_SIZE(hm2055_white_balance_auto));
            break;
        case WHITE_BALANCE_TUNGSTEN:
            err = hm2055_write_regs(sd, hm2055_white_balance_incandescence, ARRAY_SIZE(hm2055_white_balance_incandescence));
            break;
        case WHITE_BALANCE_SUNNY:
            err = hm2055_write_regs(sd, hm2055_white_balance_daylight, ARRAY_SIZE(hm2055_white_balance_daylight));
            break;
        case WHITE_BALANCE_FLUORESCENT:
            err = hm2055_write_regs(sd, hm2055_white_balance_fluorescent, ARRAY_SIZE(hm2055_white_balance_fluorescent));
            break;
        case WHITE_BALANCE_CLOUDY:
            err = hm2055_write_regs(sd, hm2055_white_balance_cloud, ARRAY_SIZE(hm2055_white_balance_cloud));
            break;
        }

        if (!err)
            userset->wb = value;

        break;
    case V4L2_CID_CAMERA_EFFECT:
        dev_dbg(&client->dev, "%s: V4L2_CID_CAMERA_EFFECT\n", __func__);

        if ((err = hm2055_write_regs(sd, hm2055_effect_reset, ARRAY_SIZE(hm2055_effect_reset))))
            dev_err(&client->dev, "Writing registers for resetting camera effect returned %d.\n", err);

        switch (value) {
        default:
            dev_err(&client->dev, "Invalid value for camera control 0x%08x.\n", ctrl->id);
            value = IMAGE_EFFECT_NONE;
        case IMAGE_EFFECT_NONE:
            err = hm2055_write_regs(sd, hm2055_effect_normal, ARRAY_SIZE(hm2055_effect_normal));
            break;
        case IMAGE_EFFECT_BNW:
            err = hm2055_write_regs(sd, hm2055_effect_grayscale, ARRAY_SIZE(hm2055_effect_grayscale));
            break;
        case IMAGE_EFFECT_SEPIA:
            err = hm2055_write_regs(sd, hm2055_effect_sepia, ARRAY_SIZE(hm2055_effect_sepia));
            break;
        case IMAGE_EFFECT_NEGATIVE:
            err = hm2055_write_regs(sd, hm2055_effect_color_inv, ARRAY_SIZE(hm2055_effect_color_inv));
            break;
        }

        if (!err)
            userset->effect = value;

        break;

    case V4L2_CID_CAM_PREVIEW_ONOFF:
        dev_dbg(&client->dev, "%s: V4L2_CID_CAM_PREVIEW_ONOFF\n", __func__);

        if (state->check_previewdata == 0)
            err = 0;
        else
            err = -EIO;

        break;

    case V4L2_CID_CAMERA_RESET:
        dev_dbg(&client->dev, "%s: V4L2_CID_CAMERA_RESET\n", __func__);
        err = hm2055_init(sd, 0);

        break;

    case V4L2_CID_CAMERA_ISO:
    case V4L2_CID_CAMERA_METERING:
    case V4L2_CID_CAMERA_CONTRAST:
    case V4L2_CID_CAMERA_SATURATION:
    case V4L2_CID_CAMERA_SHARPNESS:
    case V4L2_CID_CAMERA_WDR:
    case V4L2_CID_CAMERA_FACE_DETECTION:
    case V4L2_CID_CAMERA_FOCUS_MODE:
    case V4L2_CID_CAMERA_FLASH_MODE:
    case V4L2_CID_CAMERA_SCENE_MODE:
    case V4L2_CID_CAMERA_GPS_LATITUDE:
    case V4L2_CID_CAMERA_GPS_LONGITUDE:
    case V4L2_CID_CAMERA_GPS_TIMESTAMP:
    case V4L2_CID_CAMERA_GPS_ALTITUDE:
    case V4L2_CID_CAMERA_OBJECT_POSITION_X:
    case V4L2_CID_CAMERA_OBJECT_POSITION_Y:
    case V4L2_CID_CAMERA_SET_AUTO_FOCUS:
    case V4L2_CID_CAMERA_FRAME_RATE:
    case V4L2_CID_CAMERA_CHECK_DATALINE:
    case V4L2_CID_CAMERA_CHECK_DATALINE_STOP:
    case V4L2_CID_CAM_JPEG_QUALITY:
    case V4L2_CID_EXPOSURE:
        break;
    default:
        dev_err(&client->dev, "%s: no such control 0x%08x\n", __func__, ctrl->id);
        break;
    }

    if (err < 0)
        dev_dbg(&client->dev, "%s: vidioc_s_ctrl failed\n", __func__);

    return err;
}

static int hm2055_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc) {
    int i;

    for (i = 0; i < ARRAY_SIZE(hm2055_controls); i++)
        if (hm2055_controls[i].id == qc->id) {
            memcpy(qc, &hm2055_controls[i], sizeof(struct v4l2_queryctrl));
            return 0;
        }

    return -EINVAL;
}

static int hm2055_s_crystal_freq(struct v4l2_subdev *sd, u32  freq, u32 flags) {
    int err = -EINVAL;
    return err;
}

static int hm2055_enum_fmt(struct v4l2_subdev *sd, struct v4l2_fmtdesc *fmtdesc) {
    int err = 0;
    return err;
}

static int hm2055_g_fmt(struct v4l2_subdev *sd, struct v4l2_format *fmt) {
    struct hm2055_state *state = to_state(sd);
    fmt->fmt.pix = state->pix;
    return 0;
}

static int hm2055_try_fmt(struct v4l2_subdev *sd, struct v4l2_format *fmt) {
    struct i2c_client *client = v4l2_get_subdevdata(sd);

    if (fmt->fmt.pix.width == 640 && fmt->fmt.pix.height == 480)
        return 0;

    if (fmt->fmt.pix.width == 1600 && fmt->fmt.pix.height == 1200)
        return 0;

    dev_err(&client->dev, "%s: Unsupported resolution: %dx%d\n", __func__,
        fmt->fmt.pix.width, fmt->fmt.pix.height);

    return -EINVAL;
}

static int hm2055_s_fmt(struct v4l2_subdev *sd, struct v4l2_format *fmt) {
    struct hm2055_state *state = to_state(sd);
    struct i2c_client *client = v4l2_get_subdevdata(sd);

    int ret = 0;

    dev_dbg(&client->dev, "%s: pixelformat = 0x%x (%c%c%c%c),"
        " colorspace = 0x%x, width = %d, height = %d\n",
        __func__, fmt->fmt.pix.pixelformat,
        (fmt->fmt.pix.pixelformat) & 0xFF,
        (fmt->fmt.pix.pixelformat >> 8) & 0xFF,
        (fmt->fmt.pix.pixelformat >> 16) & 0xFF,
        (fmt->fmt.pix.pixelformat >> 24) & 0xFF,
        fmt->fmt.pix.colorspace,
        fmt->fmt.pix.width, fmt->fmt.pix.height);

    if ((ret = hm2055_try_fmt(sd, fmt)))
        return ret;

    switch (fmt->fmt.pix.width) {
    case 640:
        if ((ret = hm2055_write_regs(sd, hm2055_640_Preview, ARRAY_SIZE(hm2055_640_Preview))))
            goto fail_write_regs;

        break;

    case 1600:
        if ((ret = hm2055_write_regs(sd, hm2055_1600_Preview, ARRAY_SIZE(hm2055_1600_Preview))))
            goto fail_write_regs;

        break;
    }

    state->pix = fmt->fmt.pix;
    return 0;

fail_write_regs:
    dev_err(&client->dev, "%s: Error while setting resolution: %dx%d\n",
        __func__, fmt->fmt.pix.width, fmt->fmt.pix.height);

    return ret;
}

static int hm2055_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param) {
    int err = 0;
    struct i2c_client *client = v4l2_get_subdevdata(sd);
    
    dev_dbg(&client->dev, "%s\n", __func__);
    
    return err;
}

static int hm2055_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param) {
    int err = 0;
    struct i2c_client *client = v4l2_get_subdevdata(sd);

    dev_dbg(&client->dev, "%s: numerator %d, denominator: %d\n",
        __func__, param->parm.capture.timeperframe.numerator,
        param->parm.capture.timeperframe.denominator);

    return err;
}

static int hm2055_enum_framesizes(struct v4l2_subdev *sd,
        struct v4l2_frmsizeenum *fsize) {

    struct hm2055_state *state = to_state(sd);
    struct hm2055_enum_framesize *elem;

    int i = 0;
    int index = 0;
    int num_entries = sizeof(hm2055_framesize_list) / 
        sizeof(struct hm2055_enum_framesize);

    /* The camera interface should read this value, this is the resolution
     * at which the sensor would provide framedata to the camera i/f
     *
     * In case of image capture,
     * this returns the default camera resolution (VGA)
     */
    fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;

    index = state->framesize_index;

    for (i = 0; i < num_entries; i++) {
        elem = &hm2055_framesize_list[i];
        if (elem->index == index) {
            fsize->discrete.width = hm2055_framesize_list[index].width;
            fsize->discrete.height = hm2055_framesize_list[index].height;
            return 0;
        }
    }

    return -EINVAL;
}

static const struct v4l2_subdev_core_ops hm2055_core_ops = {
    .init = hm2055_init,
    .s_config = hm2055_s_config,
    .queryctrl = hm2055_queryctrl,
    .g_ctrl = hm2055_g_ctrl,
    .s_ctrl = hm2055_s_ctrl,
};

static const struct v4l2_subdev_video_ops hm2055_video_ops = {
    .s_crystal_freq = hm2055_s_crystal_freq,
    .enum_fmt = hm2055_enum_fmt,
    .g_fmt = hm2055_g_fmt,
    .try_fmt = hm2055_try_fmt,
    .s_fmt = hm2055_s_fmt,
    .g_parm = hm2055_g_parm,
    .s_parm = hm2055_s_parm,    
    .enum_framesizes = hm2055_enum_framesizes,
};

static const struct v4l2_subdev_ops hm2055_ops = {
    .core = &hm2055_core_ops,
    .video = &hm2055_video_ops,
};

/*
 * hm2055_probe
 * Fetching platform data is being done with s_config subdev call.
 * In probe routine, we just register subdev device
 */
static int hm2055_probe(struct i2c_client *client,
        const struct i2c_device_id *id) {

    struct hm2055_state *state;
    struct v4l2_subdev *sd;

    if ((state = kzalloc(sizeof(struct hm2055_state), GFP_KERNEL)) == NULL)
        return -ENOMEM;

    sd = &state->sd;
    strcpy(sd->name, HM2055_DRIVER_NAME);

    /* Registering subdev */
    v4l2_i2c_subdev_init(sd, client, &hm2055_ops);
    dev_info(&client->dev, "hm2055 has been probed\n");
    return 0;
}

static int hm2055_remove(struct i2c_client *client) {
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    v4l2_device_unregister_subdev(sd);
    kfree(to_state(sd));
    return 0;
}

static const struct i2c_device_id hm2055_id[] = {
    { HM2055_DRIVER_NAME, 0 },
    { },
};

MODULE_DEVICE_TABLE(i2c, hm2055_id);

static struct v4l2_i2c_driver_data v4l2_i2c_data = {
    .name = HM2055_DRIVER_NAME,
    .probe = hm2055_probe,
    .remove = __devexit_p(hm2055_remove),
    .id_table = hm2055_id,
};

MODULE_DESCRIPTION("Samsung Electronics HM2055 UXGA camera driver");
MODULE_AUTHOR("Jinsung Yang <jsgood.yang@samsung.com>");
MODULE_LICENSE("GPL");

