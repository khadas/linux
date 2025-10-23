/*
 * Command line options parser.
 *
 * Copyright (C) 2025 Synaptics Incorporated. All rights reserved.
 *
 * This software is licensed to you under the terms of the
 * GNU General Public License version 2 (the "GPL") with Broadcom special exception.
 *
 * INFORMATION CONTAINED IN THIS DOCUMENT IS PROVIDED "AS-IS," AND SYNAPTICS
 * EXPRESSLY DISCLAIMS ALL EXPRESS AND IMPLIED WARRANTIES, INCLUDING ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE,
 * AND ANY WARRANTIES OF NON-INFRINGEMENT OF ANY INTELLECTUAL PROPERTY RIGHTS.
 * IN NO EVENT SHALL SYNAPTICS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, PUNITIVE, OR CONSEQUENTIAL DAMAGES ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OF THE INFORMATION CONTAINED IN THIS DOCUMENT, HOWEVER CAUSED
 * AND BASED ON ANY THEORY OF LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, AND EVEN IF SYNAPTICS WAS ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE. IF A TRIBUNAL OF COMPETENT JURISDICTION
 * DOES NOT PERMIT THE DISCLAIMER OF DIRECT DAMAGES OR ANY OTHER DAMAGES,
 * SYNAPTICS' TOTAL CUMULATIVE LIABILITY TO ANY PARTY SHALL NOT
 * EXCEED ONE HUNDRED U.S. DOLLARS
 *
 * Copyright (C) 2025, Broadcom.
 *
 *      Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 *
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions of
 * the license of that module.  An independent module is a module which is not
 * derived from this software.  The special exception does not apply to any
 * modifications of the software.
 *
 *
 * <<Broadcom-WL-IPTag/Dual:>>
 */

#ifndef MINI_OPT_H
#define MINI_OPT_H

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Include Files ---------------------------------------------------- */

/* ---- Constants and Types ---------------------------------------------- */

#define MINIOPT_MAXKEY	128	/* Max options */
typedef struct miniopt {

	/* These are persistent after miniopt_init() */
	const char* name;		/* name for prompt in error strings */
	const char* flags;		/* option chars that take no args */
	bool longflags;		/* long options may be flags */
	bool opt_end;		/* at end of options (passed a "--") */

	/* These are per-call to miniopt() */

	int consumed;		/* number of argv entries cosumed in
				 * the most recent call to miniopt()
				 */
	bool positional;
	bool good_int;		/* 'val' member is the result of a sucessful
				 * strtol conversion of the option value
				 */
	char opt;
	char key[MINIOPT_MAXKEY];
	char* valstr;		/* positional param, or value for the option,
				 * or null if the option had
				 * no accompanying value
				 */
	uint uval;		/* strtol translation of valstr */
	int  val;		/* strtol translation of valstr */
} miniopt_t;

void miniopt_init(miniopt_t *t, const char* name, const char* flags, bool longflags);
int miniopt(miniopt_t *t, char **argv);

/* ---- Variable Externs ------------------------------------------------- */
/* ---- Function Prototypes ---------------------------------------------- */

#ifdef __cplusplus
	}
#endif

#endif  /* MINI_OPT_H  */
