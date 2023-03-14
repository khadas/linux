#include <linux/version.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <media/media-entity.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-subdev.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>

#define lt6911c_ID          0x0500
#define FREQ_INDEX_1080P    0
#define AML_SENSOR_NAME     "lt6911c-%u"

#define V4L2_CID_AML_BASE            (V4L2_CID_BASE + 0x1000)
#define V4L2_CID_AML_ORIG_FPS        (V4L2_CID_AML_BASE + 0x000)
#define V4L2_CID_AML_USER_FPS        (V4L2_CID_AML_BASE + 0x001)
#define V4L2_CID_AML_ROLE            (V4L2_CID_AML_BASE + 0x002)
#define V4L2_CID_AML_STROBE          (V4L2_CID_AML_BASE + 0x003)
#define V4L2_CID_AML_MODE            (V4L2_CID_AML_BASE + 0x004)



struct lt6911c_regval {
	u16 reg;
	u8 val;
};
struct lt6911c_mode {
	u32 width;
	u32 height;
	u32 hmax;
	u32 link_freq_index;

	const struct lt6911c_regval *data;
	u32 data_size;
};

struct lt6911c {
	int index;
	struct device *dev;
	struct clk *xclk;
	struct regmap *regmap;
	u8 nlanes;
	u8 bpp;
	u32 enWDRMode;

	struct i2c_client *client;
	struct v4l2_subdev sd;
	struct v4l2_fwnode_endpoint ep;
	struct media_pad pad;
	struct v4l2_mbus_framefmt current_format;
	const struct lt6911c_mode *current_mode;

	struct gpio_desc *rst_gpio;
	struct gpio_desc *pwdn_gpio;
	struct gpio_desc *power_gpio;

	struct v4l2_ctrl_handler ctrls;
	struct v4l2_ctrl *link_freq;
	struct v4l2_ctrl *pixel_rate;
	struct v4l2_ctrl *wdr;
	struct v4l2_ctrl *fps;

	int status;
	struct mutex lock;
};

struct lt6911c_pixfmt {
	u32 code;
	u32 min_width;
	u32 max_width;
	u32 min_height;
	u32 max_height;
	u8 bpp;
};

static int lt6911c_power_on(struct lt6911c *lt6911c);
static const struct lt6911c_pixfmt lt6911c_formats[] = {
	{ MEDIA_BUS_FMT_YVYU8_2X8, 1920, 1920, 1080, 1080, 8 },
};

static const struct regmap_config lt6911c_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
};

static const s64 lt6911c_link_freq_2lanes[] = {
	[FREQ_INDEX_1080P] = 672000000,
};

static inline const s64 *lt6911c_link_freqs_ptr(const struct lt6911c *lt6911c)
{
	return lt6911c_link_freq_2lanes;
}
static inline int lt6911c_link_freqs_num(const struct lt6911c *lt6911c)
{
	int lt6911c_link_freqs_num = 1;
	return lt6911c_link_freqs_num;
}

static const struct lt6911c_mode lt6911c_modes_2lanes[] = {
	{
		.width = 1920,
		.height = 1080,
		.hmax  = 0x1130,
		.data = NULL,
		.data_size = 0,
		.link_freq_index = FREQ_INDEX_1080P,
	},
};

static int lt6911c_power_off(struct lt6911c *lt6911c) {
	return 0;
}

static int lt6911c_stop_streaming(struct lt6911c *lt6911c)
{
	return 0;
}
static inline const struct lt6911c_mode *lt6911c_modes_ptr(const struct lt6911c *lt6911c)
{
	return lt6911c_modes_2lanes;
}
static inline int lt6911c_modes_num(const struct lt6911c *lt6911c)
{
	return ARRAY_SIZE(lt6911c_modes_2lanes);
}

static inline struct lt6911c *to_lt6911c(struct v4l2_subdev *_sd)
{
	return container_of(_sd, struct lt6911c, sd);
}

static int lt6911c_set_ctrl(struct v4l2_ctrl *ctrl) {
	switch (ctrl->id) {
	case V4L2_CID_AML_USER_FPS:
		break;
	}
	return 0;
}

static const struct v4l2_ctrl_ops lt6911c_ctrl_ops = {
	.s_ctrl = lt6911c_set_ctrl,
};

int lt6911c_sbdev_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh) {
	struct lt6911c *lt6911c = to_lt6911c(sd);
	lt6911c_power_on(lt6911c);
	return 0;
}

int lt6911c_sbdev_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh) {
	struct lt6911c *lt6911c = to_lt6911c(sd);
	lt6911c_stop_streaming(lt6911c);
	lt6911c_power_off(lt6911c);
	return 0;
}
static int lt6911c_power_suspend(struct device *dev)
{
	return 0;
}
static int lt6911c_power_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops lt6911c_pm_ops = {
	SET_RUNTIME_PM_OPS(lt6911c_power_suspend, lt6911c_power_resume, NULL)
};

static int lt6911c_log_status(struct v4l2_subdev *sd)
{
	struct lt6911c *lt6911c = to_lt6911c(sd);
	dev_info(lt6911c->dev, "log status done\n");
	return 0;
}

const struct v4l2_subdev_core_ops lt6911c_core_ops = {
	.log_status = lt6911c_log_status,
};

static int lt6911c_start_streaming(struct lt6911c *lt6911c)
{
	return 0;
}

static int lt6911c_set_stream(struct v4l2_subdev *sd, int enable)
{
	struct lt6911c *lt6911c = to_lt6911c(sd);
	int ret = 0;

	if (lt6911c->status == enable)
		return ret;
	else
		lt6911c->status = enable;

	if (enable) {
		ret = lt6911c_start_streaming(lt6911c);
		if (ret) {
			dev_err(lt6911c->dev, "Start stream failed\n");
			goto unlock_and_return;
		}

		dev_info(lt6911c->dev, "stream on\n");
	} else {
		lt6911c_stop_streaming(lt6911c);

		dev_info(lt6911c->dev, "stream off\n");
	}

unlock_and_return:

	return ret;
}

static struct v4l2_ctrl_config wdr_cfg = {
	.ops = &lt6911c_ctrl_ops,
	.id = V4L2_CID_AML_MODE,
	.name = "wdr mode",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 0,
	.max = 2,
	.step = 1,
	.def = 0,
};
static struct v4l2_ctrl_config v4l2_ctrl_output_fps = {
	.ops = &lt6911c_ctrl_ops,
	.id = V4L2_CID_AML_USER_FPS,
	.name = "Sensor output fps",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 0,
	.max = 120,
	.step = 1,
	.def = 60,
};

static const struct v4l2_subdev_video_ops lt6911c_video_ops = {
	.s_stream = lt6911c_set_stream,
};

static const struct media_entity_operations lt6911c_subdev_entity_ops = {
	.link_validate = v4l2_subdev_link_validate,
};

static struct v4l2_subdev_internal_ops lt6911c_internal_ops = {
	.open = lt6911c_sbdev_open,
	.close = lt6911c_sbdev_close,
};

static inline u8 lt6911c_get_link_freq_index(struct lt6911c *lt6911c)
{
	return lt6911c->current_mode->link_freq_index;
}
static s64 lt6911c_get_link_freq(struct lt6911c *lt6911c)
{
	u8 index = lt6911c_get_link_freq_index(lt6911c);
	return *(lt6911c_link_freqs_ptr(lt6911c) + index);
}

static u64 lt6911c_calc_pixel_rate(struct lt6911c *lt6911c)
{
	s64 link_freq = lt6911c_get_link_freq(lt6911c);
	u8 nlanes = lt6911c->nlanes;
	u64 pixel_rate;
	pixel_rate = link_freq * 2 * nlanes;
	do_div(pixel_rate, lt6911c->bpp);
	return pixel_rate;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static int lt6911c_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *cfg,
			  struct v4l2_subdev_format *fmt)
#else
static int lt6911c_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *fmt)
#endif
{
	struct lt6911c *lt6911c = to_lt6911c(sd);
	struct v4l2_mbus_framefmt *framefmt;

	mutex_lock(&lt6911c->lock);

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY)
		framefmt = v4l2_subdev_get_try_format(&lt6911c->sd, cfg,
							  fmt->pad);
	else
		framefmt = &lt6911c->current_format;

	fmt->format = *framefmt;

	mutex_unlock(&lt6911c->lock);

	return 0;
}


#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
int lt6911c_get_selection(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *cfg,
				 struct v4l2_subdev_selection *sel)
#else
int lt6911c_get_selection(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_selection *sel)
#endif
{
	int rtn = 0;
	struct lt6911c *lt6911c = to_lt6911c(sd);
	const struct lt6911c_mode *mode = lt6911c->current_mode;

	switch (sel->target) {
	case V4L2_SEL_TGT_CROP_DEFAULT:
		sel->r.left = 0;
		sel->r.top = 0;
		sel->r.width = mode->width;
		sel->r.height = mode->height;
	break;
	case V4L2_SEL_TGT_CROP:
		sel->r.left = 0;
		sel->r.top = 0;
		sel->r.width = mode->width;
		sel->r.height = mode->height;
	break;
	default:
		rtn = -EINVAL;
		dev_err(lt6911c->dev, "Error support target: 0x%x\n", sel->target);
	break;
	}

	return rtn;
}


#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static int lt6911c_enum_frame_size(struct v4l2_subdev *sd,
					struct v4l2_subdev_state *cfg,
				   struct v4l2_subdev_frame_size_enum *fse)
#else
static int lt6911c_enum_frame_size(struct v4l2_subdev *sd,
				   struct v4l2_subdev_pad_config *cfg,
				   struct v4l2_subdev_frame_size_enum *fse)
#endif
{
	if (fse->index >= ARRAY_SIZE(lt6911c_formats))
		return -EINVAL;

	fse->min_width = lt6911c_formats[fse->index].min_width;
	fse->min_height = lt6911c_formats[fse->index].min_height;;
	fse->max_width = lt6911c_formats[fse->index].max_width;
	fse->max_height = lt6911c_formats[fse->index].max_height;
	return 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static int lt6911c_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *cfg,
				 struct v4l2_subdev_mbus_code_enum *code)
#else
static int lt6911c_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_mbus_code_enum *code)
#endif
{
	if (code->index >= ARRAY_SIZE(lt6911c_formats))
		return -EINVAL;
	code->code = lt6911c_formats[code->index].code;
	return 0;
}

static s64 lt6911c_check_link_freqs(const struct lt6911c *lt6911c,
				   const struct v4l2_fwnode_endpoint *ep)
{
	int i, j;
	const s64 *freqs = lt6911c_link_freqs_ptr(lt6911c);
	int freqs_count = lt6911c_link_freqs_num(lt6911c);

	for (i = 0; i < freqs_count; i++) {
		for (j = 0; j < ep->nr_of_link_frequencies; j++) {
			if (freqs[i] == ep->link_frequencies[j]) {
				return 0;
			}
		}
		if (j == ep->nr_of_link_frequencies)
			return freqs[i];
	}
	return 0;
}

static int lt6911c_parse_endpoint(struct lt6911c *lt6911c) {
	int rtn = 0;
	s64 fq;
	struct fwnode_handle *endpoint = NULL;
	struct device_node *node = NULL;
	for_each_endpoint_of_node(lt6911c->dev->of_node, node) {
		if (strstr(node->name, "lt6911c")) {
			endpoint = of_fwnode_handle(node);
			break;
		}
	}
	rtn = v4l2_fwnode_endpoint_alloc_parse(endpoint, &lt6911c->ep);
	fwnode_handle_put(endpoint);
	if (rtn) {
		dev_err(lt6911c->dev, "Parsing endpoint node failed\n");
		rtn = -EINVAL;
		goto err_return;
	}
	if (lt6911c->ep.bus_type != V4L2_MBUS_CSI2_DPHY) {
		dev_err(lt6911c->dev, "Unsupported bus type, should be CSI2\n");
		rtn = -EINVAL;
		goto err_free;
	}
	lt6911c->nlanes = lt6911c->ep.bus.mipi_csi2.num_data_lanes;
	if (lt6911c->nlanes != 2 && lt6911c->nlanes != 4) {
		dev_err(lt6911c->dev, "Invalid data lanes: %d\n", lt6911c->nlanes);
		rtn = -EINVAL;
		goto err_free;
	}
	dev_info(lt6911c->dev, "Using %u data lanes\n", lt6911c->nlanes);
	if (!lt6911c->ep.nr_of_link_frequencies) {
		dev_err(lt6911c->dev, "link-frequency property not found in DT\n");
		rtn = -EINVAL;
		goto err_free;
	}
	fq = lt6911c_check_link_freqs(lt6911c, &lt6911c->ep);
	if (fq) {
		dev_err(lt6911c->dev, "Link frequency of %lld is not supported\n", fq);
		rtn = -EINVAL;
		goto err_free;
	}

	return rtn;

err_free:
	v4l2_fwnode_endpoint_free(&lt6911c->ep);
err_return:
	return rtn;
}

static int lt6911c_power_on(struct lt6911c *lt6911c)
{
	return 0;
}
static inline int lt6911c_read_reg(struct lt6911c *lt6911c, u16 addr, u8 *value) {
	unsigned int regval;
	int i, ret;
	for (i = 0; i < 3; ++i) {
		ret = regmap_read(lt6911c->regmap, addr, &regval);
		if (0 == ret ) {
			break;
		}
	}
	if (ret)
		dev_err(lt6911c->dev, "I2C read with i2c transfer failed for addr: %x, ret %d\n", addr, ret);
	*value = regval & 0xff;
	return 0;
}
static int lt6911c_write_reg(struct lt6911c *lt6911c, u16 addr, u8 value)
{
	int i, ret;
	for (i = 0; i < 3; i++) {
		ret = regmap_write(lt6911c->regmap, addr, value);
		if (0 == ret) {
			break;
		}
	}
	if (ret)
		dev_err(lt6911c->dev, "I2C write failed for addr: %x, ret %d\n", addr, ret);
	return ret;
}

static int lt6911c_set_register_array(struct lt6911c *lt6911c,
					 const struct lt6911c_regval *settings,
					 unsigned int num_settings)
{
	unsigned int i;
	int ret = 0;

	for (i = 0; i < num_settings; ++i, ++settings) {
		ret = lt6911c_write_reg(lt6911c, settings->reg, settings->val);
		if (ret < 0)
			return ret;
	}
	return 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static int lt6911c_set_fmt(struct v4l2_subdev *sd,
			struct v4l2_subdev_state *cfg,
			struct v4l2_subdev_format *fmt)
#else
static int lt6911c_set_fmt(struct v4l2_subdev *sd,
			struct v4l2_subdev_pad_config *cfg,
			struct v4l2_subdev_format *fmt)
#endif
{
	struct lt6911c *lt6911c = to_lt6911c(sd);
	const struct lt6911c_mode *mode;
	struct v4l2_mbus_framefmt *format;
	unsigned int i,ret;

	mutex_lock(&lt6911c->lock);

	mode = v4l2_find_nearest_size(lt6911c_modes_ptr(lt6911c),
				 lt6911c_modes_num(lt6911c),
				width, height,
				fmt->format.width, fmt->format.height);

	fmt->format.width = mode->width;
	fmt->format.height = mode->height;

	for (i = 0; i < ARRAY_SIZE(lt6911c_formats); i++) {
		if (lt6911c_formats[i].code == fmt->format.code) {
			dev_err(lt6911c->dev, " zzw find proper format %d \n",i);
			break;
		}
	}

	if (i >= ARRAY_SIZE(lt6911c_formats)) {
		i = 0;
		dev_err(lt6911c->dev, " zzw No format. reset i = 0 \n");
	}

	fmt->format.code = lt6911c_formats[i].code;
	fmt->format.field = V4L2_FIELD_NONE;

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		dev_err(lt6911c->dev, " zzw try format \n");
		format = v4l2_subdev_get_try_format(sd, cfg, fmt->pad);
		mutex_unlock(&lt6911c->lock);
		return 0;
	} else {
		dev_err(lt6911c->dev, " zzw set format, w %d, h %d, code 0x%x \n",
			fmt->format.width, fmt->format.height,
			fmt->format.code);
		format = &lt6911c->current_format;
		lt6911c->current_mode = mode;
		lt6911c->bpp = lt6911c_formats[i].bpp;

		if (lt6911c->link_freq)
			__v4l2_ctrl_s_ctrl(lt6911c->link_freq, lt6911c_get_link_freq_index(lt6911c) );
		if (lt6911c->pixel_rate)
			__v4l2_ctrl_s_ctrl_int64(lt6911c->pixel_rate, lt6911c_calc_pixel_rate(lt6911c) );
	}
	*format = fmt->format;
	lt6911c->status = 0;
	mutex_unlock(&lt6911c->lock);
	/* Set init register settings */
	ret = lt6911c_set_register_array(lt6911c, NULL, 0);
	ret = 1;
	return ret;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static int lt6911c_entity_init_cfg(struct v4l2_subdev *subdev,
				  struct v4l2_subdev_state *cfg)
#else
static int lt6911c_entity_init_cfg(struct v4l2_subdev *subdev,
				  struct v4l2_subdev_pad_config *cfg)
#endif
{
	struct v4l2_subdev_format fmt = { 0 };

	fmt.which = cfg ? V4L2_SUBDEV_FORMAT_TRY : V4L2_SUBDEV_FORMAT_ACTIVE;
	fmt.format.width = 1920;
	fmt.format.height = 1080;

	lt6911c_set_fmt(subdev, cfg, &fmt);

	return 0;
}

static const struct v4l2_subdev_pad_ops lt6911c_pad_ops = {
	.init_cfg = lt6911c_entity_init_cfg,
	.enum_mbus_code = lt6911c_enum_mbus_code,
	.enum_frame_size = lt6911c_enum_frame_size,
	.get_selection = lt6911c_get_selection,
	.get_fmt = lt6911c_get_fmt,
	.set_fmt = lt6911c_set_fmt,
};
static const struct v4l2_subdev_ops lt6911c_subdev_ops = {
	.core = &lt6911c_core_ops,
	.video = &lt6911c_video_ops,
	.pad = &lt6911c_pad_ops,
};

static int lt6911c_get_id(struct lt6911c *lt6911c)
{
	uint32_t sensor_id = 0;
	u8 val = 0;

	lt6911c_read_reg(lt6911c, 0, &val);
	sensor_id |= (val << 8);
	lt6911c_read_reg(lt6911c, 1, &val);
	sensor_id |= val;

	if (sensor_id != lt6911c_ID) {
		dev_err(lt6911c->dev, "Failed to get lt6911c id: 0x%x\n", sensor_id);
		return 0;
	} else {
		dev_err(lt6911c->dev, "success get lt6911c id 0x%x", sensor_id);
	}

	return 0;
}

static int lt6911c_ctrls_init(struct lt6911c *lt6911c) {
	int rtn = 0;
	v4l2_ctrl_handler_init(&lt6911c->ctrls, 4);
	v4l2_ctrl_new_std(&lt6911c->ctrls, &lt6911c_ctrl_ops,
				V4L2_CID_GAIN, 0, 0xF0, 1, 0);
	v4l2_ctrl_new_std(&lt6911c->ctrls, &lt6911c_ctrl_ops,
				V4L2_CID_EXPOSURE, 0, 0xffff, 1, 0);
	lt6911c->link_freq = v4l2_ctrl_new_int_menu(&lt6911c->ctrls,
						   &lt6911c_ctrl_ops,
						   V4L2_CID_LINK_FREQ,
						   lt6911c_link_freqs_num(lt6911c) - 1,
						   0, lt6911c_link_freqs_ptr(lt6911c) );
	if (lt6911c->link_freq)
			lt6911c->link_freq->flags |= V4L2_CTRL_FLAG_READ_ONLY;
	lt6911c->pixel_rate = v4l2_ctrl_new_std(&lt6911c->ctrls,
						   &lt6911c_ctrl_ops,
						   V4L2_CID_PIXEL_RATE,
						   1, INT_MAX, 1,
						   lt6911c_calc_pixel_rate(lt6911c));
	lt6911c->wdr = v4l2_ctrl_new_custom(&lt6911c->ctrls, &wdr_cfg, NULL);
	lt6911c->fps = v4l2_ctrl_new_custom(&lt6911c->ctrls, &v4l2_ctrl_output_fps, NULL);

	lt6911c->sd.ctrl_handler = &lt6911c->ctrls;

	if (lt6911c->ctrls.error) {
		dev_err(lt6911c->dev, "Control initialization a error  %d\n",
			lt6911c->ctrls.error);
		rtn = lt6911c->ctrls.error;
	}
	rtn = 0;
	return rtn;
}

static int lt6911c_register_subdev(struct lt6911c *lt6911c)
{
	int rtn = 0;

	v4l2_i2c_subdev_init(&lt6911c->sd, lt6911c->client, &lt6911c_subdev_ops);

	lt6911c->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	lt6911c->sd.dev = &lt6911c->client->dev;
	lt6911c->sd.internal_ops = &lt6911c_internal_ops;
	lt6911c->sd.entity.ops = &lt6911c_subdev_entity_ops;
	lt6911c->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;
	snprintf(lt6911c->sd.name, sizeof(lt6911c->sd.name), AML_SENSOR_NAME, lt6911c->index);

	lt6911c->pad.flags = MEDIA_PAD_FL_SOURCE;
	rtn = media_entity_pads_init(&lt6911c->sd.entity, 1, &lt6911c->pad);
	if (rtn < 0) {
		dev_err(lt6911c->dev, "Could not register media entity\n");
		goto err_return;
	}
	rtn = v4l2_async_register_subdev(&lt6911c->sd);
	if (rtn < 0) {
		dev_err(lt6911c->dev, "Could not register v4l2 device\n");
		goto err_return;
	}

err_return:
	return rtn;
}

static int lt6911c_parse_power(struct lt6911c *lt6911c) {
	int rtn = 0;
	lt6911c->rst_gpio = devm_gpiod_get_optional(lt6911c->dev,
						"reset",
						GPIOD_OUT_LOW);
	if (IS_ERR(lt6911c->rst_gpio)) {
		dev_err(lt6911c->dev, "Cannot get reset gpio\n");
		rtn = PTR_ERR(lt6911c->rst_gpio);
		goto err_return;
	}

err_return:
	return rtn;
}

static int lt6911c_probe(struct i2c_client *client) {
	struct device *dev = &client->dev;
	struct lt6911c *lt6911c;
	int ret = -EINVAL;
	dev_info(dev,"enter lt6911c_probe\n");
	lt6911c = devm_kzalloc(dev, sizeof(*lt6911c), GFP_KERNEL);
	if (!lt6911c)
		return -ENOMEM;
	dev_err(dev, "i2c dev addr 0x%x, name %s \n", client->addr, client->name);
	lt6911c->dev = dev;
	lt6911c->client = client;
	lt6911c->regmap = devm_regmap_init_i2c(client, &lt6911c_regmap_config);
	if (IS_ERR(lt6911c->regmap)) {
		dev_err(dev, "Unable to initialize I2C\n");
		return -ENODEV;
	}
	if (of_property_read_u32(dev->of_node, "index", &lt6911c->index)) {
		dev_err(dev, "Failed to read sensor index. default to 0\n");
		lt6911c->index = 0;
	}
	ret = lt6911c_parse_endpoint(lt6911c);
	if (ret) {
		dev_err(lt6911c->dev, "Error parse endpoint\n");
		goto return_err;
	}
	ret = lt6911c_parse_power(lt6911c);
	if (ret) {
		dev_err(lt6911c->dev, "Error parse power ctrls\n");
		goto free_err;
	}
	mutex_init(&lt6911c->lock);
	dev_err(dev, "bef get id. pwdn -0, reset - 1\n");
	ret = lt6911c_power_on(lt6911c);
		if (ret < 0) {
			dev_err(dev, "Could not power on the device\n");
			goto free_err;
		}
	ret = lt6911c_get_id(lt6911c);
	if (ret) {
		dev_err(dev, "Could not get id\n");
		lt6911c_power_off(lt6911c);
		goto free_err;
	}
	lt6911c_entity_init_cfg(&lt6911c->sd, NULL);
	ret = lt6911c_ctrls_init(lt6911c);
	if (ret) {
		dev_err(lt6911c->dev, "Error ctrls init\n");
		goto free_ctrl;
	}
	ret = lt6911c_register_subdev(lt6911c);
	if (ret) {
		dev_err(lt6911c->dev, "Error register subdev\n");
		goto free_entity;
	}
	v4l2_fwnode_endpoint_free(&lt6911c->ep);

	dev_info(lt6911c->dev, "probe done \n");

	return 0;

free_entity:
	media_entity_cleanup(&lt6911c->sd.entity);
free_ctrl:
	v4l2_ctrl_handler_free(&lt6911c->ctrls);
	mutex_destroy(&lt6911c->lock);
free_err:
	v4l2_fwnode_endpoint_free(&lt6911c->ep);
return_err:
	return ret;
}

static int lt6911c_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct lt6911c *lt6911c = to_lt6911c(sd);

	v4l2_async_unregister_subdev(sd);
	media_entity_cleanup(&sd->entity);
	v4l2_ctrl_handler_free(sd->ctrl_handler);

	mutex_destroy(&lt6911c->lock);

	lt6911c_power_off(lt6911c);

	return 0;
}

static const struct of_device_id lt6911c_of_match[] = {
	{ .compatible = "amlogic, lt6911c" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, lt6911c_of_match);

static struct i2c_driver lt6911c_i2c_driver = {
	.probe_new  = lt6911c_probe,
	.remove = lt6911c_remove,
	.driver = {
		.name  = "lt6911c",
		.pm = &lt6911c_pm_ops,
		.of_match_table = of_match_ptr(lt6911c_of_match),
	},
};
module_i2c_driver(lt6911c_i2c_driver);
MODULE_DESCRIPTION("LT6911C Image Sensor Driver");
MODULE_AUTHOR("open source");
MODULE_AUTHOR("@amlogic.com");
MODULE_LICENSE("GPL v2");