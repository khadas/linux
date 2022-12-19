/*
*
* SPDX-License-Identifier: GPL-2.0
*
* Copyright (C) 2020 Amlogic or its affiliates
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; version 2.
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
* or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
* for more details.
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*/

#include "aml_isp_ctrls.h"

static int v4l2_s_std_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	struct aml_control *a_ctrl = container_of(ctrl->handler,
					struct aml_control, hdl_std_ctrl);
	struct aml_video *vd = container_of(a_ctrl,
					struct aml_video, actrl);

	switch (ctrl->id) {
		case V4L2_CID_GAIN:
		case V4L2_CID_EXPOSURE:
		case V4L2_CID_BRIGHTNESS:
		break;
		case V4L2_CID_AML_ORIG_FPS:
			a_ctrl->fps_sensor = ctrl->val;
		break;
		case V4L2_CID_AML_USER_FPS:
			a_ctrl->fps_output = ctrl->val;
		break;
		default:
			dev_err(vd->dev, "Error ctrl->id %u, flag 0x%lx\n",
				ctrl->id, ctrl->flags);
			ret = -EINVAL;
		break;
	}

	return ret;
}

static const struct v4l2_ctrl_ops v4l2_ctrl_std_ops = {
	.s_ctrl = v4l2_s_std_ctrl,
};


/*--------------- custom v4l2_ctrl configs ----------------*/

static const struct v4l2_ctrl_config v4l2_ctrl_output_fps = {
	.ops = &v4l2_ctrl_std_ops,
	.id = V4L2_CID_AML_USER_FPS,
	.name = "ISP output fps",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	.min = 0,
	.max = 120,
	.step = 1,
	.def = 30,
};

static const struct v4l2_ctrl_config v4l2_ctrl_sensor_fps = {
	.ops = &v4l2_ctrl_std_ops,
	.id = V4L2_CID_AML_ORIG_FPS,
	.name = "ISP sensor input fps",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	.min = 0,
	.max = 120,
	.step = 1,
	.def = 30,
};
/*-----------------------------------------------------------*/

int isp_v4l2_ctrl_init(struct aml_video *video)
{
	int rtn = 0;
	struct v4l2_ctrl_handler *hdl_std_ctrl = &video->actrl.hdl_std_ctrl;

	v4l2_ctrl_handler_init(hdl_std_ctrl, 10);
	/*-------------------------add std ctrl --------------------------*/
	v4l2_ctrl_new_std(hdl_std_ctrl, &v4l2_ctrl_std_ops,
				V4L2_CID_GAIN, 100, 3200, 1, 100 );
	v4l2_ctrl_new_std(hdl_std_ctrl, &v4l2_ctrl_std_ops,
				V4L2_CID_EXPOSURE, 0, 0xffff, 1, 0);
	v4l2_ctrl_new_std(hdl_std_ctrl, &v4l2_ctrl_std_ops,
				V4L2_CID_BRIGHTNESS, 0, 255, 1, 128);

	/*-------------------------add cst ctrl --------------------------*/
	v4l2_ctrl_new_custom(hdl_std_ctrl, &v4l2_ctrl_sensor_fps, NULL);
	v4l2_ctrl_new_custom(hdl_std_ctrl, &v4l2_ctrl_output_fps, NULL);

	video->actrl.fps_sensor = 0;
	video->actrl.fps_output = 0;
	video->vdev.ctrl_handler = hdl_std_ctrl;
	rtn = v4l2_ctrl_handler_setup(video->vdev.ctrl_handler);
	if (rtn) {
		dev_err(video->dev, "Could not sync v4l2 controls\n");
		return rtn;
	}

	if (hdl_std_ctrl->error) {
		dev_err(video->dev, "Control initialization error %d\n",
			hdl_std_ctrl->error);
		rtn = hdl_std_ctrl->error;
	}

	return rtn;
}

void isp_v4l2_ctrl_deinit( struct aml_video *video)
{
	struct v4l2_ctrl_handler *hdl_std_ctrl = &video->actrl.hdl_std_ctrl;

	v4l2_ctrl_handler_free(hdl_std_ctrl);
}
