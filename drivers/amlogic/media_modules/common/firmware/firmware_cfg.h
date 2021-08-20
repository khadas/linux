/*
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Description:
 */
/*all firmwares in one bin.*/
{VIDEO_MISC,	VIDEO_PACKAGE,	"video_ucode.bin"},

/* Note: if the addition of new package has the same name */
/* as the firmware in the video_ucode.bin, the firmware */
/* in the video_ucode.bin will be ignored yet, because the */
/* video_ucode.bin will always be processed in the end */
{VIDEO_ENCODE,	VIDEO_PACKAGE,	"h264_enc.bin"},


/*firmware for a special format, to replace the format in the package.*/
/*{VIDEO_DECODE,	VIDEO_FW_FILE,	"h265.bin"},*/
/*{VIDEO_DECODE,	VIDEO_FW_FILE,	"h264.bin"},*/
/*{VIDEO_DECODE,	VIDEO_FW_FILE,	"h264_multi.bin"},*/

