/*
 * Misc system wide definitions
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
 *
 * Edited with the help of GENAI.
 */

#ifndef	_bcmdefs_h_
#define	_bcmdefs_h_

#ifdef CONFIG_UBSAN
/* Redefine the BCM_FLEX_ARRAY as empty for Linux Undefined Behavior Sanitizer
 * checking. (CONFIG_UBSAN)
 */
#ifdef BCM_FLEX_ARRAY
#undef BCM_FLEX_ARRAY
#endif /* BCM_FLEX_ARRAY */
#define BCM_FLEX_ARRAY
#else
#ifndef BCM_FLEX_ARRAY
#define BCM_FLEX_ARRAY  (1)
#endif /* BCM_FLEX_ARRAY */
#endif /* CONFIG_UBSAN */

/*
 * One doesn't need to include this file explicitly, gets included automatically if
 * typedefs.h is included.
 */

/* For all the corerevs of the chip being built */
#ifdef VLSI_COREREVS
#include <vlsi_chip_defs.h>	/* auto-generated definitions from vlsi_data */
#endif /* VLSI_COREREVS */

/* Use BCM_REFERENCE to suppress warnings about intentionally-unused function
 * arguments or local variables.
 */
#define BCM_REFERENCE(data)	((void)(data))

/* Allow for suppressing unused variable warnings. */
#ifdef __GNUC__
#define BCM_UNUSED_VAR     __attribute__ ((unused))
#else
#define BCM_UNUSED_VAR
#endif

/* Allow for suppressing pedantic warnings. */
#ifdef __GNUC__
#define BCM_EXTENSION	__extension__
#else
#define BCM_EXTENSION
#endif

/* Allow for suppressing a warning for a switch case without break */
#ifdef __GNUC__
#define GCC_SUPPRESS_FALLTHROUGH_WARNING     __attribute__ ((fallthrough))
#else
#define GCC_SUPPRESS_FALLTHROUGH_WARNING
#endif

/* Linux kenrel already defined "fallthrough" as macro
 * so, GCC_SUPPRESS_FALLTHROUGH_WARNIN macro could not be used in driver
 * GCC supports the __fallthrough__ attribute since 7.1.
 * Clang supports the __fallthrough__ Statement Attributes since 10.0.0
 */
#if (defined(__GNUC__) && (__GNUC__ > 7 || (__GNUC__ == 7 && __GNUC_MINOR__ >= 1)) || \
	(defined(__clang__) && __clang_major__ >= 10))
#define BCM_FALLTHROUGH __attribute__ ((__fallthrough__))
#else
#define BCM_FALLTHROUGH
#endif /* __GNUC__ */

/* GNU GCC 4.6+ supports selectively turning off a warning.
 * Define these diagnostic macros to help suppress cast-qual warning
 * until all the work can be done to fix the casting issues.
 */
#if (defined(__GNUC__) && defined(STRICT_GCC_WARNINGS) && (__GNUC__ > 4 || (__GNUC__ == \
	4 && __GNUC_MINOR__ >= 6)) || defined(__clang__))

#define GCC_DIAGNOSTIC_PUSH_SUPPRESS_CAST()              \
	_Pragma("GCC diagnostic push")			\
	_Pragma("GCC diagnostic ignored \"-Wcast-qual\"")

#define GCC_DIAGNOSTIC_PUSH_SUPPRESS_NULL_DEREF()	\
	_Pragma("GCC diagnostic push")			\
	_Pragma("GCC diagnostic ignored \"-Wnull-dereference\"")

#define GCC_DIAGNOSTIC_POP()                             \
	_Pragma("GCC diagnostic pop")

#elif defined(_MSC_VER)

#define GCC_DIAGNOSTIC_PUSH_SUPPRESS_CAST()              \
	__pragma(warning(push))                          \
	__pragma(warning(disable:4090))
#define GCC_DIAGNOSTIC_PUSH_SUPPRESS_NULL_DEREF()	\
	__pragma(warning(push))
#define GCC_DIAGNOSTIC_POP()                             \
	__pragma(warning(pop))
#else
#define GCC_DIAGNOSTIC_PUSH_SUPPRESS_CAST()
#define GCC_DIAGNOSTIC_PUSH_SUPPRESS_NULL_DEREF()
#define GCC_DIAGNOSTIC_POP()
#endif   /* Diagnostic macros not defined */

#if (defined(__GNUC__) && defined(STRICT_GCC_WARNINGS) && (__GNUC__ > 8 || (__GNUC__ == \
	8 && __GNUC_MINOR__ >= 1)) || defined(__clang__))

#if !defined(__clang__) || __clang_major__ >= 13
#define GCC_DIAGNOSTIC_PUSH_SUPPRESS_FN_TYPE()           \
	_Pragma("GCC diagnostic push")			\
	_Pragma("GCC diagnostic ignored \"-Wcast-function-type\"")
#else
#define GCC_DIAGNOSTIC_PUSH_SUPPRESS_FN_TYPE()           \
	_Pragma("GCC diagnostic push")
#endif // !__clang__ || __clang_major__ >= 13

#elif defined(_MSC_VER)

#define GCC_DIAGNOSTIC_PUSH_SUPPRESS_FN_TYPE()           \
	__pragma(warning(push))
#else
#define GCC_DIAGNOSTIC_PUSH_SUPPRESS_FN_TYPE()
#endif   /* Diagnostic macros not defined */

/* Macros to allow Coverity modeling contructs in source code */
#if defined(__COVERITY__)

/* Coverity Doc:
 * Indicates to the TAINTED_SCALAR checker and the INTEGER_OVERFLOW checker
 * that a function taints its argument
 */
#define COV_TAINTED_DATA_ARG(arg)  __coverity_tainted_data_argument__(arg)

/* Coverity Doc:
 * Indicates to the TAINTED_SCALAR checker and the INTEGER_OVERFLOW checker
 * that a function is a tainted data sink for an argument.
 */
#define COV_TAINTED_DATA_SINK(arg) __coverity_tainted_data_sink__(arg)

/* Coverity Doc:
 * Models a function that cannot take a negative number as an argument. Used in
 * conjunction with other models to indicate that negative arguments are invalid.
 */
#define COV_NEG_SINK(arg)          __coverity_negative_sink__(arg)

#else

#define COV_TAINTED_DATA_ARG(arg)  do { } while (0)
#define COV_TAINTED_DATA_SINK(arg) do { } while (0)
#define COV_NEG_SINK(arg)          do { } while (0)

#endif /* __COVERITY__ */

/* Compile-time assert can be used in place of ASSERT if the expression evaluates
 * to a constant at compile time.
 */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
/* _Static_assert() is supported in ISO C from C11. */
#define STATIC_ASSERT(expr) _Static_assert(expr, "Static ASSERT failure")
#else
#define STATIC_ASSERT(expr) { \
	/* Make sure the expression is constant. */ \
	typedef enum { _STATIC_ASSERT_NOT_CONSTANT = (expr) } _static_assert_e BCM_UNUSED_VAR; \
	/* Make sure the expression is true. */ \
	typedef char STATIC_ASSERT_FAIL[(expr) ? 1 : -1] BCM_UNUSED_VAR; \
}
#endif /* __STDC_VERSION__ >= 201112L */

/* Reclaiming text and data :
 * The following macros specify special linker sections that can be reclaimed
 * after a system is considered 'up'.
 * BCMATTACHFN is also used for detach functions (it's not worth having a BCMDETACHFN,
 * as in most cases, the attach function calls the detach function to clean up on error).
 */
#if defined(BCM_RECLAIM)

extern bool bcm_reclaimed;
extern bool bcm_attach_part_reclaimed;
extern bool bcm_preattach_part_reclaimed;
extern bool bcm_postattach_part_reclaimed;

#define RECLAIMED()			(bcm_reclaimed)
#define ATTACH_PART_RECLAIMED()		(bcm_attach_part_reclaimed)
#define PREATTACH_PART_RECLAIMED()	(bcm_preattach_part_reclaimed)
#define POSTATTACH_PART_RECLAIMED()	(bcm_postattach_part_reclaimed)

/* Place _fn/_data symbols in various reclaimed output sections */
#define BCMATTACHDATA(_data)	__attribute__ ((__section__ (".dataini2." #_data))) _data
#define BCMATTACHFN(_fn)	__attribute__ ((__section__ (".textini2." #_fn), noinline)) _fn
#define BCMPREATTACHDATA(_data)	__attribute__ ((__section__ (".dataini3." #_data))) _data
#define BCMPREATTACHFN(_fn)	__attribute__ ((__section__ (".textini3." #_fn), noinline)) _fn
#define BCMPOSTATTACHDATA(_data)	__attribute__ ((__section__ (".dataini5." #_data))) _data
#define BCMPOSTATTACHFN(_fn)	__attribute__ ((__section__ (".textini5." #_fn), noinline)) _fn

/* Relocate attach symbols to save-restore region to increase pre-reclaim heap size. */
#define BCM_SRM_ATTACH_DATA(_data)    __attribute__ ((__section__ (".datasrm." #_data))) _data
#define BCM_SRM_ATTACH_FN(_fn)        __attribute__ ((__section__ (".textsrm." #_fn), noinline)) _fn

/* Explicitly place data in .rodata section so it can be write-protected after attach */
#define BCMRODATA(_data)	__attribute__ ((__section__ (".shrodata." #_data))) _data

#ifdef BCMDBG_SR
/*
 * Don't reclaim so we can compare SR ASM
 */
#define BCMPREATTACHDATASR(_data)	_data
#define BCMPREATTACHFNSR(_fn)		_fn
#define BCMATTACHDATASR(_data)		_data
#define BCMATTACHFNSR(_fn)		_fn
#else
#define BCMPREATTACHDATASR(_data)	BCMPREATTACHDATA(_data)
#define BCMPREATTACHFNSR(_fn)		BCMPREATTACHFN(_fn)
#define BCMATTACHDATASR(_data)		BCMATTACHDATA(_data)
#define BCMATTACHFNSR(_fn)		BCMATTACHFN(_fn)
#endif

/* In case of coex cpu reinit, we should not relcaim the functions that are needed for reinit */
#if defined(COEX_CPU_REINIT) && !defined(COEX_CPU_REINIT_DISABLED)
#define BCMCOEXCPUATTACHDATA(_data)	_data
#define BCMCOEXCPUATTACHFN(_fn)		_fn
#define BCMCOEXCPUPREATTACHDATA(_data)	_data
#define BCMCOEXCPUPREATTACHFN(_fn)	_fn
#else
#define BCMCOEXCPUATTACHDATA(_data)	BCMATTACHDATA(_data)
#define BCMCOEXCPUATTACHFN(_fn)		BCMATTACHFN(_fn)
#define BCMCOEXCPUPREATTACHDATA(_data)	BCMPREATTACHDATA(_data)
#define BCMCOEXCPUPREATTACHFN(_fn)	BCMPREATTACHFN(_fn)
#endif /* COEX_CPU_REINIT && !COEX_CPU_REINIT_DISABLED */

#define BCMINITDATA(_data)	_data
#define BCMINITFN(_fn)		_fn
#ifndef CONST
#define CONST	const
#endif

/* Non-manufacture or internal attach function/dat */
#if !(defined(WLTEST) || defined(ATE_BUILD))
#define	BCMNMIATTACHFN(_fn)	BCMATTACHFN(_fn)
#define	BCMNMIATTACHDATA(_data)	BCMATTACHDATA(_data)
#else
#define	BCMNMIATTACHFN(_fn)	_fn
#define	BCMNMIATTACHDATA(_data)	_data
#endif	/* WLTEST || ATE_BUILD */

#if !defined(ATE_BUILD) && defined(BCM_CISDUMP_NO_RECLAIM)
#define	BCMCISDUMPATTACHFN(_fn)		_fn
#define	BCMCISDUMPATTACHDATA(_data)	_data
#else
#define	BCMCISDUMPATTACHFN(_fn)		BCMNMIATTACHFN(_fn)
#define	BCMCISDUMPATTACHDATA(_data)	BCMNMIATTACHDATA(_data)
#endif /* !ATE_BUILD && BCM_CISDUMP_NO_RECLAIM */

/* SROM with OTP support */
#if defined(BCMOTPSROM)
#define	BCMSROMATTACHFN(_fn)		_fn
#define	BCMSROMATTACHDATA(_data)	_data
#else
#define	BCMSROMATTACHFN(_fn)		BCMNMIATTACHFN(_fn)
#define	BCMSROMATTACHDATA(_data)	BCMNMIATTACHFN(_data)
#endif	/* BCMOTPSROM */

#if defined(BCM_CISDUMP_NO_RECLAIM)
#define	BCMSROMCISDUMPATTACHFN(_fn)	_fn
#define	BCMSROMCISDUMPATTACHDATA(_data)	_data
#else
#define	BCMSROMCISDUMPATTACHFN(_fn)	BCMSROMATTACHFN(_fn)
#define	BCMSROMCISDUMPATTACHDATA(_data)	BCMSROMATTACHDATA(_data)
#endif /* BCM_CISDUMP_NO_RECLAIM */

#define BCMUNINITFN(_fn)	_fn

#else /* BCM_RECLAIM */

#define bcm_reclaimed			(TRUE)
#define bcm_attach_part_reclaimed	(TRUE)
#define bcm_preattach_part_reclaimed	(TRUE)
#define bcm_postattach_part_reclaimed	(TRUE)
#define BCMATTACHDATA(_data)		_data
#define BCMATTACHFN(_fn)		_fn
#define BCM_SRM_ATTACH_DATA(_data)	_data
#define BCM_SRM_ATTACH_FN(_fn)		_fn
/* BCMRODATA data is written into at attach time so it cannot be in .rodata */
#define BCMRODATA(_data)	__attribute__ ((__section__ (".data." #_data))) _data
#define BCMPREATTACHDATA(_data)		_data
#define BCMPREATTACHFN(_fn)		_fn
#define BCMPOSTATTACHDATA(_data)	_data
#define BCMPOSTATTACHFN(_fn)		_fn
#define BCMINITDATA(_data)		_data
#define BCMINITFN(_fn)			_fn
#define BCMUNINITFN(_fn)		_fn
#define	BCMNMIATTACHFN(_fn)		_fn
#define	BCMNMIATTACHDATA(_data)		_data
#define	BCMSROMATTACHFN(_fn)		_fn
#define	BCMSROMATTACHDATA(_data)	_data
#define BCMPREATTACHFNSR(_fn)		_fn
#define BCMPREATTACHDATASR(_data)	_data
#define BCMATTACHFNSR(_fn)		_fn
#define BCMATTACHDATASR(_data)		_data
#define	BCMSROMATTACHFN(_fn)		_fn
#define	BCMSROMATTACHDATA(_data)	_data
#define	BCMCISDUMPATTACHFN(_fn)		_fn
#define	BCMCISDUMPATTACHDATA(_data)	_data
#define	BCMSROMCISDUMPATTACHFN(_fn)	_fn
#define	BCMSROMCISDUMPATTACHDATA(_data)	_data
#define CONST				const

#define RECLAIMED()			(bcm_reclaimed)
#define ATTACH_PART_RECLAIMED()		(bcm_attach_part_reclaimed)
#define PREATTACH_PART_RECLAIMED()	(bcm_preattach_part_reclaimed)
#define POSTATTACH_PART_RECLAIMED()	(bcm_postattach_part_reclaimed)

#endif /* BCM_RECLAIM */

#define BCMUCODEDATA(_data)		BCMINITDATA(_data)

#if defined(BCM_AQM_DMA_DESC) && !defined(BCM_AQM_DMA_DESC_DISABLED) && \
	!defined(DONGLEBUILD)
#define BCMUCODEFN(_fn)			BCMINITFN(_fn)
#else
#define BCMUCODEFN(_fn)			BCMATTACHFN(_fn)
#endif /* BCM_AQM_DMA_DESC */

/* This feature is for dongle builds only.
 * In Rom build use BCMFASTPATH() to mark functions that will excluded from ROM bits if
 * BCMFASTPATH_EXCLUDE_FROM_ROM flag is defined (defined by default).
 * In romoffload or ram builds all functions that marked by BCMFASTPATH() will be placed
 * in "text_fastpath" section and will be used by trap handler.
 */
#ifndef BCMFASTPATH
#if defined(DONGLEBUILD)
#if defined(BCMROMBUILD)
#if defined(BCMFASTPATH_EXCLUDE_FROM_ROM)
	#define BCMFASTPATH(_fn)	__attribute__ ((__section__ (".text_ram." #_fn))) _fn
#else /* BCMFASTPATH_EXCLUDE_FROM_ROM */
	#define BCMFASTPATH(_fn)	_fn
#endif /* BCMFASTPATH_EXCLUDE_FROM_ROM */
#else /* BCMROMBUILD */
#ifdef BCMFASTPATH_O3OPT
#ifdef ROM_ENAB_RUNTIME_CHECK
	#define BCMFASTPATH(_fn)	__attribute__ ((__section__ (".text_fastpath." #_fn)))  _fn
#else
	#define BCMFASTPATH(_fn)	__attribute__ ((__section__ (".text_fastpath." #_fn))) \
					__attribute__ ((optimize(3))) _fn
#endif /* ROM_ENAB_RUNTIME_CHECK */
#else
	#define BCMFASTPATH(_fn)	__attribute__ ((__section__ (".text_fastpath." #_fn)))  _fn
#endif /* BCMFASTPATH_O3OPT */
#endif /* BCMROMBUILD */
#else /* DONGLEBUILD */
	#define BCMFASTPATH(_fn)	_fn
#endif /* DONGLEBUILD */
#endif /* BCMFASTPATH */

/* Use the BCMRAMFN/BCMRAMDATA() macros to tag functions/data in source that must be included in RAM
 * (excluded from ROM). This should eliminate the need to manually specify these functions/data in
 * the ROM config file. It should only be used in special cases where the function must be in RAM
 * for *all* ROM-based chips.
 */
#if defined(BCMROMBUILD)
	#define BCMRAMFN(_fn)     __attribute__ ((__section__ (".text_ram." #_fn), noinline)) _fn
	#define BCMRAMDATA(_data) __attribute__ ((__section__ (".rodata_ram." #_data))) _data
#else
	#define BCMRAMFN(_fn)		_fn
	#define BCMRAMDATA(_data)	_data
#endif /* ROMBUILD */

/* This will be used by the accessor functions and other helper functions which are required
 * to ROM a given function. This function will always remain in RAM. Also a separate modifier
 * will help to differentiate these functions from standard RAM functions.
 */
#define BCMACCESSOR_RAMFN(_fn) BCMRAMFN(_fn)

/* Use BCMSPECSYM() macro to tag symbols going to a special output section in the binary. */
#define BCMSPECSYM(_sym)	__attribute__ ((__section__ (".special." #_sym))) _sym

#ifdef BCMFUZZ
#define BCM_UNROLL_LOOPS
#else
/** Use on functions with small loops with boundaries known at compile time to trade increased
 * memory usage for a few saved cycles by avoiding the branch statement caused by the loop.
 */
#define BCM_UNROLL_LOOPS	__attribute__ ((optimize("unroll-loops")))
#endif /* BCMFUZZ */

#define STATIC	static

/* functions that do not examine any values except their arguments, and have no effects except
 * the return value should use this keyword. Note that a function that has pointer arguments
 * and examines the data pointed to must not be declared as BCMCONSTFN.
 */
#ifdef __GNUC__
#define BCMCONSTFN	__attribute__ ((const))
#else
#define BCMCONSTFN
#endif /* __GNUC__ */

/* Bus types */
#define	SI_BUS			0	/* SOC Interconnect */
#define	PCI_BUS			1	/* PCI target */
#define	PCMCIA_BUS		2	/* PCMCIA target */
#define SDIO_BUS		3	/* SDIO target */
#define JTAG_BUS		4	/* JTAG */
#define USB_BUS			5	/* USB (does not support R/W REG) */
#define SPI_BUS			6	/* gSPI target */
#define RPC_BUS			7	/* RPC target */

/* Allows size optimization for single-bus image */
#ifdef BCMBUSTYPE
#define BUSTYPE(bus)	(BCMBUSTYPE)
#else
#define BUSTYPE(bus)	(bus)
#endif

#ifdef BCMBUSCORETYPE
#define BUSCORETYPE(ct)		(BCMBUSCORETYPE)
#else
#define BUSCORETYPE(ct)		(ct)
#endif

/* Allows size optimization for single-backplane image */
#ifdef BCMCHIPTYPE
#define CHIPTYPE(bus)	(BCMCHIPTYPE)
#else
#define CHIPTYPE(bus)	(bus)
#endif

/* Allows size optimization for SPROM support */
#if defined(BCMSPROMBUS)
#define SPROMBUS	(BCMSPROMBUS)
#else
#define SPROMBUS	(PCI_BUS)
#endif

/* Allows size optimization for single-chip image */
/* These macros are NOT meant to encourage writing chip-specific code.
 * Use them only when it is appropriate for example in PMU PLL/CHIP/SWREG
 * controls and in chip-specific workarounds.
 */
#ifdef BCMCHIPID
#define CHIPID(chip)	(BCMCHIPID)
#else
#define CHIPID(chip)	(chip)
#endif

#ifdef BCMCHIPREV
#define CHIPREV(rev)	(BCMCHIPREV)
#else
#define CHIPREV(rev)	(rev)
#endif

#ifdef BCMPCIEREV
#define PCIECOREREV(rev)	(BCMPCIEREV)
#elif defined(BCMPCIEGEN2REV)
#define BCMPCIEREV		(BCMPCIEGEN2REV)
#define PCIECOREREV(rev)	(BCMPCIEGEN2REV)
#else
#define PCIECOREREV(rev)	(rev)
#endif

#ifdef BCMPMUREV
#define PMUREV(rev)	(BCMPMUREV)
#else
#define PMUREV(rev)	(rev)
#endif

#ifdef BCMSDTCREV
#define SDTCREV(rev)	(BCMSDTCREV)
#else
#define SDTCREV(rev)	(rev)
#endif

#ifdef BCMCCREV
#define CCREV(rev)	(BCMCCREV)
#elif defined(BCMCHIPCOMMONREV)
#define BCMCCREV	(BCMCHIPCOMMONREV)
#define CCREV(rev)	(BCMCHIPCOMMONREV)
#else
#define CCREV(rev)	(rev)
#endif

#ifdef BCMGCIREV
#define GCIREV(rev)	(BCMGCIREV)
#else
#define GCIREV(rev)	(rev)
#endif

#ifdef BCMCR4REV
#define CR4REV(rev)	(BCMCR4REV)
#define CR4REV_GE(rev, val)	((BCMCR4REV) >= (val))
#else
#define CR4REV(rev)	(rev)
#define CR4REV_GE(rev, val)	((rev) >= (val))
#endif

#ifdef BCMARMCA7REV
#define CA7REV(rev)		(BCMARMCA7REV)
#define CA7REV_GE(rev, val)	((BCMARMCA7REV) >= (val))
#else
#define CA7REV(rev)		(rev)
#define CA7REV_GE(rev, val)	((rev) >= (val))
#endif

#ifdef BCMLHLREV
#define LHLREV(rev)	(BCMLHLREV)
#else
#define LHLREV(rev)	(rev)
#endif

#if defined(BCMHND_OOBRREV) && !defined(BCMHNDOOBRREV)
#define BCMHNDOOBRREV	BCMHND_OOBRREV
#endif

#ifdef BCMSPMISREV
#define SPMISREV(rev)	(BCMSPMISREV)
#elif defined(BCMSPMI_SLAVEREV)
#define BCMSPMISREV	(BCMSPMI_SLAVEREV)
#define SPMISREV(rev)	(BCMSPMI_SLAVEREV)
#else
#define	SPMISREV(rev)	(rev)
#endif

/* Defines for DMA Address Width - Shared between OSL and HNDDMA */
#define DMADDR_MASK_32 0x0		/* Address mask for 32-bits */
#define DMADDR_MASK_30 0xc0000000	/* Address mask for 30-bits */
#define DMADDR_MASK_26 0xFC000000	/* Address maks for 26-bits */
#define DMADDR_MASK_0  0xffffffff	/* Address mask for 0-bits (hi-part) */

#define	DMADDRWIDTH_26  26 /* 26-bit addressing capability */
#define	DMADDRWIDTH_30  30 /* 30-bit addressing capability */
#define	DMADDRWIDTH_32  32 /* 32-bit addressing capability */
#define	DMADDRWIDTH_63  63 /* 64-bit addressing capability */
#define	DMADDRWIDTH_64  64 /* 64-bit addressing capability */

typedef struct {
	uint32 loaddr;
	uint32 hiaddr;
} dma64addr_t;

#define PHYSADDR64HI(_pa) ((_pa).hiaddr)
#define PHYSADDR64HISET(_pa, _val) \
	do { \
		(_pa).hiaddr = (_val);		\
	} while (0)
#define PHYSADDR64LO(_pa) ((_pa).loaddr)
#define PHYSADDR64LOSET(_pa, _val) \
	do { \
		(_pa).loaddr = (_val);		\
	} while (0)

#define PHYSADDR64ISZERO(_pa) (PHYSADDR64LO(_pa) == 0 && PHYSADDR64HI(_pa) == 0)

#ifdef BCMDMA64OSL
typedef dma64addr_t dmaaddr_t;
#define PHYSADDRHI(_pa) PHYSADDR64HI(_pa)
#define PHYSADDRHISET(_pa, _val) PHYSADDR64HISET(_pa, _val)
#define PHYSADDRLO(_pa)  PHYSADDR64LO(_pa)
#define PHYSADDRLOSET(_pa, _val) PHYSADDR64LOSET(_pa, _val)
#define PHYSADDRTOULONG(_pa, _ulong) \
	do { \
		_ulong = ((unsigned long long)(_pa).hiaddr << 32) | ((_pa).loaddr); \
	} while (0)

#else
typedef uint32 dmaaddr_t;
#define PHYSADDRHI(_pa) (0u)
#define PHYSADDRHISET(_pa, _val)
#define PHYSADDRLO(_pa) ((_pa))
#define PHYSADDRLOSET(_pa, _val) \
	do { \
		(_pa) = (_val);			\
	} while (0)
#endif /* BCMDMA64OSL */

#define PHYSADDRISZERO(_pa) (PHYSADDRLO(_pa) == 0 && PHYSADDRHI(_pa) == 0)

/* One physical DMA segment */
typedef struct  {
	dmaaddr_t addr;
	uint32	length;
} hnddma_seg_t;

#if defined(__linux__)
#define MAX_DMA_SEGS 8
#else
#define MAX_DMA_SEGS 4
#endif

typedef struct {
	void *oshdmah; /* Opaque handle for OSL to store its information */
	uint origsize; /* Size of the virtual packet */
	uint nsegs;
	hnddma_seg_t segs[MAX_DMA_SEGS];
} hnddma_seg_map_t;

/* packet headroom necessary to accommodate the largest header in the system, (i.e TXOFF).
 * By doing, we avoid the need  to allocate an extra buffer for the header when bridging to WL.
 * There is a compile time check in wlc.c which ensure that this value is at least as big
 * as TXOFF. This value is used in dma_rxfill (hnddma.c).
 */

#ifndef BCMEXTRAHDROOM
#define BCMEXTRAHDROOM 204
#endif

/* Packet alignment for most efficient SDIO (can change based on platform) */
#ifndef SDALIGN
#define SDALIGN	32
#endif

/* Headroom required for dongle-to-host communication.  Packets allocated
 * locally in the dongle (e.g. for CDC ioctls or RNDIS messages) should
 * leave this much room in front for low-level message headers which may
 * be needed to get across the dongle bus to the host.  (These messages
 * don't go over the network, so room for the full WL header above would
 * be a waste.).
*/
/*
 * set the numbers to be MAX of all the devices, to avoid problems with ROM builds
 * USB BCMDONGLEHDRSZ and BCMDONGLEPADSZ is 0
 * SDIO BCMDONGLEHDRSZ 12 and BCMDONGLEPADSZ 16
*/
#define BCMDONGLEHDRSZ 12
#define BCMDONGLEPADSZ 16

#define BCMDONGLEOVERHEAD	(BCMDONGLEHDRSZ + BCMDONGLEPADSZ)

#ifdef BCMDBG

#ifndef BCMDBG_ERR
#define BCMDBG_ERR
#endif /* BCMDBG_ERR */

#ifndef BCMDBG_ASSERT
#define BCMDBG_ASSERT
#endif /* BCMDBG_ASSERT */

#endif /* BCMDBG */

#if defined(NO_BCMDBG_ASSERT)
	#undef BCMDBG_ASSERT
	#undef BCMASSERT_LOG
#endif

#if defined(BCMDBG_ASSERT) || defined(BCMASSERT_LOG)
#define BCMASSERT_SUPPORT
#endif /* BCMDBG_ASSERT || BCMASSERT_LOG */

/* Macros for doing definition and get/set of bitfields
 * Usage example, e.g. a three-bit field (bits 4-6):
 *    #define <NAME>_M	BITFIELD_MASK(3)
 *    #define <NAME>_S	4
 * ...
 *    regval = R_REG(osh, &regs->regfoo);
 *    field = GFIELD(regval, <NAME>);
 *    regval = SFIELD(regval, <NAME>, 1);
 *    W_REG(osh, &regs->regfoo, regval);
 */
#define BITFIELD_MASK(width) \
		(((unsigned)1 << (width)) - 1)
#define GFIELD(val, field) \
		(((val) >> field ## _S) & field ## _M)
#define SFIELD(val, field, bits) \
		(((val) & (~(field ## _M << field ## _S))) | \
		((unsigned)(bits) << field ## _S))

/* define BCMSMALL to remove misc features for memory-constrained environments */
#ifdef BCMSMALL
#undef	BCMSPACE
#define bcmspace	FALSE	/* if (bcmspace) code is discarded */
#else
#define	BCMSPACE
#define bcmspace	TRUE	/* if (bcmspace) code is retained */
#endif

/* ROM_ENAB_RUNTIME_CHECK may be set based upon the #define below (for ROM builds). It may also
 * be defined via makefiles (e.g. ROM auto abandon unoptimized compiles).
 */
#if defined(BCMROMBUILD)
#ifndef ROM_ENAB_RUNTIME_CHECK
	#define ROM_ENAB_RUNTIME_CHECK
#endif
#endif /* BCMROMBUILD */

#ifdef BCM_SH_SFLASH
	extern bool _bcm_sh_sflash;
#if defined(ROM_ENAB_RUNTIME_CHECK) || !defined(DONGLEBUILD)
	#define BCM_SH_SFLASH_ENAB() (_bcm_sh_sflash)
#elif defined(BCM_SH_SFLASH_DISABLED)
	#define BCM_SH_SFLASH_ENAB() (FALSE)
#else
	#define BCM_SH_SFLASH_ENAB() (TRUE)
#endif
#else
	#define BCM_SH_SFLASH_ENAB() (FALSE)
#endif	/* BCM_SH_SFLASH */

#ifdef BCM_SFLASH
	extern bool _bcm_sflash;
#if defined(ROM_ENAB_RUNTIME_CHECK) || !defined(DONGLEBUILD)
	#define BCM_SFLASH_ENAB() (_bcm_sflash)
#elif defined(BCM_SFLASH_DISABLED)
	#define BCM_SFLASH_ENAB() (FALSE)
#else
	#define BCM_SFLASH_ENAB() (TRUE)
#endif
#else
	#define BCM_SFLASH_ENAB() (FALSE)
#endif	/* BCM_SFLASH */

#ifdef BCM_DELAY_ON_LTR
	extern bool _bcm_delay_on_ltr;
#if defined(ROM_ENAB_RUNTIME_CHECK) || !defined(DONGLEBUILD)
	#define BCM_DELAY_ON_LTR_ENAB() (_bcm_delay_on_ltr)
#elif defined(BCM_DELAY_ON_LTR_DISABLED)
	#define BCM_DELAY_ON_LTR_ENAB()	(FALSE)
#else
	#define BCM_DELAY_ON_LTR_ENAB()	(TRUE)
#endif
#else
	#define BCM_DELAY_ON_LTR_ENAB() (FALSE)
#endif	/* BCM_DELAY_ON_LTR */

/* Max. nvram variable table size */
#ifndef MAXSZ_NVRAM_VARS
#ifdef LARGE_NVRAM_MAXSZ
#define MAXSZ_NVRAM_VARS	(LARGE_NVRAM_MAXSZ * 2)
#else
#if defined(BCMROMBUILD) || defined(DONGLEBUILD)
/* SROM12 changes */
#define	MAXSZ_NVRAM_VARS	6144	/* should be reduced */
#else
#define LARGE_NVRAM_MAXSZ	8192
#define MAXSZ_NVRAM_VARS	(LARGE_NVRAM_MAXSZ * 2)
#endif /* BCMROMBUILD || DONGLEBUILD */
#endif /* LARGE_NVRAM_MAXSZ */
#endif /* !MAXSZ_NVRAM_VARS */

#ifdef ATE_BUILD
#ifndef ATE_NVRAM_MAXSIZE
#define ATE_NVRAM_MAXSIZE 32000
#endif /* ATE_NVRAM_MAXSIZE */
#endif /* ATE_BUILD */

#ifdef BCMLFRAG /* BCMLFRAG support enab macros  */
	extern bool _bcmlfrag;
#if (defined(ROM_ENAB_RUNTIME_CHECK) && !defined(BCMLFRAG_NO_RUNTIME_CHECK)) || \
	!defined(DONGLEBUILD)
	#define BCMLFRAG_ENAB() (_bcmlfrag)
#elif defined(BCMLFRAG_DISABLED)
	#define BCMLFRAG_ENAB()	(FALSE)
#else
	#define BCMLFRAG_ENAB()	(TRUE)
#endif
#else
	#define BCMLFRAG_ENAB()	(FALSE)
#endif /* BCMLFRAG_ENAB */

#ifdef BCMPCIEDEV /* BCMPCIEDEV support enab macros */
extern bool _pciedevenab;
#if defined(ROM_ENAB_RUNTIME_CHECK) && !defined(BCMPCIEDEV_NO_RUNTIME_CHECK)
	#define BCMPCIEDEV_ENAB() (_pciedevenab)
#elif defined(BCMPCIEDEV_ENABLED)
	#define BCMPCIEDEV_ENAB() (TRUE)
#else
	#define BCMPCIEDEV_ENAB() (FALSE)
#endif
#else
	#define BCMPCIEDEV_ENAB() (FALSE)
#endif /* BCMPCIEDEV */

#ifdef BCMRESVFRAGPOOL /* BCMRESVFRAGPOOL support enab macros */
extern bool _resvfragpool_enab;
#if defined(ROM_ENAB_RUNTIME_CHECK) || !defined(DONGLEBUILD)
	#define  BCMRESVFRAGPOOL_ENAB() (_resvfragpool_enab)
#elif defined(BCMRESVFRAGPOOL_DISABLED)
	#define BCMRESVFRAGPOOL_ENAB()	(FALSE)
#else
	#define BCMRESVFRAGPOOL_ENAB()	(TRUE)
#endif
#else
	#define BCMRESVFRAGPOOL_ENAB()	(FALSE)
#endif /* BCMPCIEDEV */

#ifdef BCMSDIODEV /* BCMSDIODEV support enab macros */
extern bool _sdiodevenab;
#if defined(ROM_ENAB_RUNTIME_CHECK) || !defined(DONGLEBUILD)
	#define BCMSDIODEV_ENAB() (_sdiodevenab)
#elif defined(BCMSDIODEV_ENABLED)
	#define BCMSDIODEV_ENAB() (TRUE)
#else
	#define BCMSDIODEV_ENAB() (FALSE)
#endif
#else
	#define BCMSDIODEV_ENAB() (FALSE)
#endif /* BCMSDIODEV */

#ifdef BCMSPMIS
extern bool _bcmspmi_enab;
extern bool _bcmspmi_plat_enab;
#if defined(ROM_ENAB_RUNTIME_CHECK) || !defined(DONGLEBUILD)
	#define	BCMSPMIS_ENAB()		(_bcmspmi_enab)
	#define BCMSPMIS_PLAT_ENAB()	(_bcmspmi_plat_enab)
#elif defined(BCMSPMIS_DISABLED)
	#define	BCMSPMIS_ENAB()		(FALSE)
	#define BCMSPMIS_PLAT_ENAB()	(FALSE)
#else
	#define	BCMSPMIS_ENAB()		(TRUE)
	#define BCMSPMIS_PLAT_ENAB()	(_bcmspmi_plat_enab)
#endif
#else
	#define	BCMSPMIS_ENAB()		(FALSE)
	#define BCMSPMIS_PLAT_ENAB()	(FALSE)
#endif /* BCMSPMIS */

#ifdef BCMDVFS /* BCMDVFS support enab macros */
extern bool _dvfsenab;
#if defined(ROM_ENAB_RUNTIME_CHECK)
	#define BCMDVFS_ENAB() (_dvfsenab)
#elif !defined(BCMDVFS_DISABLED)
	#define BCMDVFS_ENAB() (TRUE)
#else
	#define BCMDVFS_ENAB() (FALSE)
#endif
#else
	#define BCMDVFS_ENAB() (FALSE)
#endif /* BCMDVFS */

#ifdef BCM_HW_SFHLLC
extern bool _hw_sfhllc_enab;
#if defined(ROM_ENAB_RUNTIME_CHECK)
	#define BCM_HW_SFHLLC_ENAB() (_hw_sfhllc_enab)
#elif !defined(BCM_HW_SFHLLC_DISABLED)
	#define BCM_HW_SFHLLC_ENAB() (TRUE)
#else
	#define BCM_HW_SFHLLC_ENAB() (FALSE)
#endif
#else
	#define BCM_HW_SFHLLC_ENAB() (FALSE)
#endif /* BCMDVFS */

/* Max size for reclaimable NVRAM array */
#ifndef ATE_BUILD
#ifdef DL_NVRAM
#define NVRAM_ARRAY_MAXSIZE	DL_NVRAM
#else
#define NVRAM_ARRAY_MAXSIZE	MAXSZ_NVRAM_VARS
#endif /* DL_NVRAM */
#else
#define NVRAM_ARRAY_MAXSIZE	ATE_NVRAM_MAXSIZE
#endif /* ATE_BUILD */

extern uint32 gFWID;

#ifdef BCMFRWDPKT /* BCMFRWDPKT support enab macros  */
	extern bool _bcmfrwdpkt;
#if (defined(ROM_ENAB_RUNTIME_CHECK) && !defined(BCMFRWDPKT_NO_RUNTIME_CHECK)) || \
	!defined(DONGLEBUILD)
	#define BCMFRWDPKT_ENAB() (_bcmfrwdpkt)
#elif defined(BCMFRWDPKT_DISABLED)
	#define BCMFRWDPKT_ENAB() (FALSE)
#else
	#define BCMFRWDPKT_ENAB() (TRUE)
#endif
#else
	#define BCMFRWDPKT_ENAB() (FALSE)
#endif /* BCMFRWDPKT */

#ifdef BCMFRWDPOOLREORG /* BCMFRWDPOOLREORG support enab macros  */
	extern bool _bcmfrwdpoolreorg;
#if defined(ROM_ENAB_RUNTIME_CHECK) || !defined(DONGLEBUILD)
	#define BCMFRWDPOOLREORG_ENAB() (_bcmfrwdpoolreorg)
#elif defined(BCMFRWDPOOLREORG_DISABLED)
	#define BCMFRWDPOOLREORG_ENAB()	(FALSE)
#else
	#define BCMFRWDPOOLREORG_ENAB()	(TRUE)
#endif
#else
	#define BCMFRWDPOOLREORG_ENAB()	(FALSE)
#endif /* BCMFRWDPOOLREORG */

#ifdef BCMPOOLRECLAIM /* BCMPOOLRECLAIM support enab macros  */
	extern bool _bcmpoolreclaim;
#if defined(ROM_ENAB_RUNTIME_CHECK) || !defined(DONGLEBUILD)
	#define BCMPOOLRECLAIM_ENAB() (_bcmpoolreclaim)
#elif defined(BCMPOOLRECLAIM_DISABLED)
	#define BCMPOOLRECLAIM_ENAB() (FALSE)
#else
	#define BCMPOOLRECLAIM_ENAB() (TRUE)
#endif
#else
	#define BCMPOOLRECLAIM_ENAB() (FALSE)
#endif /* BCMPOOLRECLAIM */

#ifdef BCMRXDATAPOOL /* BCMRXDATAPOOL support enab macros  */
	extern bool _bcmrxdatapool;
#if (defined(ROM_ENAB_RUNTIME_CHECK) && !defined(BCMRXDATAPOOL_NO_RUNTIME_CHECK)) || \
	!defined(DONGLEBUILD)
	#define BCMRXDATAPOOL_ENAB() (_bcmrxdatapool)
#elif defined(BCMRXDATAPOOL_DISABLED) && defined(DONGLEBUILD)
	#define BCMRXDATAPOOL_ENAB() (FALSE)
#else
	#define BCMRXDATAPOOL_ENAB() (TRUE)
#endif
#else
	#define BCMRXDATAPOOL_ENAB() (FALSE)
#endif /* BCMRXDATAPOOL */

#ifdef URB /* URB support enab macros  */
	extern bool _urb_enab;
#if (defined(ROM_ENAB_RUNTIME_CHECK) && !defined(URB_NO_RUNTIME_CHECK)) || \
	!defined(DONGLEBUILD)
	#define URB_ENAB() (_urb_enab)
#elif defined(URB_DISABLED)
	#define URB_ENAB() (FALSE)
#else
	#define URB_ENAB() (TRUE)
#endif
#else
	#define URB_ENAB() (FALSE)
#endif /* URB */

#ifdef UDCC /* UDCC support enab macros  */
	extern bool _udcc_enab;
#if defined(ROM_ENAB_RUNTIME_CHECK) || !defined(DONGLEBUILD)
	#define UDCC_ENAB() (_udcc_enab)
#elif defined(UDCC_DISABLED)
	#define UDCC_ENAB() (FALSE)
#else
	#define UDCC_ENAB() (TRUE)
#endif
#else
	#define UDCC_ENAB() (FALSE)
#endif /* UDCC */

#ifdef URB_DBG_BUS /* URB DBG BUS enab macros  */
	extern bool _urb_dbg_bus_enab;
#if defined(ROM_ENAB_RUNTIME_CHECK) || !defined(DONGLEBUILD)
	#define URB_DBG_BUS_ENAB() (_urb_dbg_bus_enab)
#elif defined(URB_DBG_BUS_DISABLED)
	#define URB_DBG_BUS_ENAB() (FALSE)
#else
	#define URB_DBG_BUS_ENAB() (TRUE)
#endif
#else
	#define URB_DBG_BUS_ENAB() (FALSE)
#endif /* URB_DBG_BUS */

#ifdef URB_MON_GIANT_PKT /* URB Mon giant packet enab macro  */
	extern bool _urb_giantpkt_enab;
#if defined(ROM_ENAB_RUNTIME_CHECK) || !defined(DONGLEBUILD)
	#define URB_MON_GIANTPKT_ENAB() (_urb_giantpkt_enab)
#elif defined(URB_MON_GIANT_PKT_DISABLED)
	#define URB_MON_GIANTPKT_ENAB() (FALSE)
#else
	#define URB_MON_GIANTPKT_ENAB() (TRUE)
#endif
#else
	#define URB_MON_GIANTPKT_ENAB() (FALSE)
#endif /* URB_MON_GIANT_PKT */

#ifdef TX_HISTOGRAM
extern bool _tx_histogram_enabled;
#if defined(ROM_ENAB_RUNTIME_CHECK)
	#define TX_HISTOGRAM_ENAB() (_tx_histogram_enabled)
#elif defined(TX_HISTOGRAM_DISABLED)
	#define TX_HISTOGRAM_ENAB() (FALSE)
#else
	#define TX_HISTOGRAM_ENAB() (TRUE)
#endif
#else
	#define TX_HISTOGRAM_ENAB() (FALSE)
#endif /* TX_HISTOGRAM */

#ifdef SMBM /* URB DBG BUS enab macros  */
	extern bool _smbm_enab;
#if defined(ROM_ENAB_RUNTIME_CHECK) || !defined(DONGLEBUILD)
	#define SMBM_ENAB()	(_smbm_enab)
#elif defined(SMBM_DISABLED)
	#define SMBM_ENAB()	(FALSE)
#else
	#define SMBM_ENAB()	(TRUE)
#endif
#else
	#define SMBM_ENAB()	(FALSE)
#endif /* SMBM */

#ifdef BCM_8021X_RXCPLRING /* BCM_8021X_RXCPLRING support enab macros */
extern bool _bcm_8021x_rxcpl_enab;
#if defined(ROM_ENAB_RUNTIME_CHECK)
	#define BCM_8021X_RXCPLRING_ENAB() (_bcm_8021x_rxcpl_enab)
#elif defined(BCM_8021X_RXCPLRING_DISABLED)
	#define BCM_8021X_RXCPLRING_ENAB() (FALSE)
#else
	#define BCM_8021X_RXCPLRING_ENAB() (TRUE)
#endif
#else
	#define BCM_8021X_RXCPLRING_ENAB() (FALSE)
#endif /* BCM_8021X_RXCPLRING */

#ifdef BCM_ARP_RXCPLRING /* BCM_ARP_RXCPLRING support enab macros */
extern bool _bcm_arp_rxcpl_enab;
#if defined(ROM_ENAB_RUNTIME_CHECK)
	#define BCM_ARP_RXCPLRING_ENAB() (_bcm_arp_rxcpl_enab)
#elif defined(BCM_ARP_RXCPLRING_DISABLED)
	#define BCM_ARP_RXCPLRING_ENAB() (FALSE)
#else
	#define BCM_ARP_RXCPLRING_ENAB() (TRUE)
#endif
#else
	#define BCM_ARP_RXCPLRING_ENAB() (FALSE)
#endif /* BCM_ARP_RXCPLRING */

/* Chip related low power flags (lpflags) */

#ifndef PAD
#define _PADLINE(line)  pad ## line
#define _XSTR(line)     _PADLINE(line)
#define PAD             _XSTR(__LINE__)
#endif

#if defined(DONGLEBUILD) && !defined(__COVERITY__)
#define MODULE_DETACH(var, detach_func) \
	do { \
		BCM_REFERENCE(detach_func); \
		OSL_SYS_HALT(); \
	} while (0);
#define MODULE_DETACH_2(var1, var2, detach_func) \
	do { \
		BCM_REFERENCE(detach_func); \
		OSL_SYS_HALT(); \
	} while (0);
#else
#define MODULE_DETACH(var, detach_func) detach_func(var)
#define MODULE_DETACH_2(var1, var2, detach_func) detach_func(var1, var2)
#endif /* DONGLEBUILD */

/* When building ROML image use runtime conditional to cause the compiler
 * to compile everything but not to complain "defined but not used"
 * as #ifdef would cause at the callsites.
 * In the end functions called under if (0) {} will not be linked
 * into the final binary if they're not called from other places either.
 */
#if !defined(BCMROMBUILD) || defined(BCMROMSYMGEN_BUILD)
#define BCM_ATTACH_REF_DECL()
#define BCM_ATTACH_REF()	(1)
#else
#define BCM_ATTACH_REF_DECL()	static bool bcm_non_roml_build = 0;
#define BCM_ATTACH_REF()	(bcm_non_roml_build)
#endif

/* For ROM builds, keep it in const section so that it gets ROMmed. If abandoned, move it to
 * RO section but after the other ro data so that FATAL log buf doesn't use this.
 */
#if defined(ROM_ENAB_RUNTIME_CHECK) || !defined(DONGLEBUILD)
#define BCMPOST_TRAP_RODATA(_data)	_data
#else
#define BCMPOST_TRAP_RODATA(_data) __attribute__ ((__section__ (".ro_ontrap." #_data))) _data
#endif

#if defined(BCMROMBUILD)
#define BCMPOST_TRAP_RAM_RODATA(data)	BCMRAMDATA(data)
#else
#define BCMPOST_TRAP_RAM_RODATA(data)	BCMPOST_TRAP_RODATA(data)
#endif

/* Similar to RO data on trap, we want code that's used after a trap to be placed in a special area
 * as this means we can use all of the rest of the .text for post trap dumps. Functions with
 * the BCMPOSTTRAPFN macro applied will either be in ROM or this protected area.
 * For RAMFNs, the ROM build only needs to nkow that they won't be in ROM, but the -roml
 * builds need to know to protect them.
 */
#if defined(BCMROMBUILD)
#define BCMPOSTTRAPFN(_fn)		_fn
#define BCMPOSTTRAPRAMFN(_fn)	__attribute__ ((__section__ (".text_ram." #_fn))) _fn
#define BCMPOSTTRAP_ACCESSOR_RAMFN(fn)	BCMPOSTTRAPRAMFN(fn)
#if defined(BCMFASTPATH_EXCLUDE_FROM_ROM)
#define BCMPOSTTRAPFASTPATH(_fn)	__attribute__ ((__section__ (".text_ram." #_fn))) _fn
#else /* BCMFASTPATH_EXCLUDE_FROM_ROM */
#define BCMPOSTTRAPFASTPATH(fn)	BCMPOSTTRAPFN(fn)
#endif /* BCMFASTPATH_EXCLUDE_FROM_ROM */
#else
#if defined(DONGLEBUILD)
#define BCMPOSTTRAPFN(_fn)	__attribute__ ((__section__ (".text_posttrap." #_fn))) _fn
#define BCMPOSTTRAPFASTPATH(_fn)	__attribute__ ((__section__ (".text_posttrapfp." #_fn))) _fn
#else
#define BCMPOSTTRAPFN(_fn)		_fn
#define BCMPOSTTRAPFASTPATH(_fn)	_fn
#endif /* DONGLEBUILD */
#define BCMPOSTTRAPRAMFN(fn)	BCMPOSTTRAPFN(fn)
#define BCMPOSTTRAP_ACCESSOR_RAMFN(fn)	BCMPOSTTRAPRAMFN(fn)
#endif /* ROMBUILD */

typedef struct bcm_rng * bcm_rng_handle_t;

/* Explicitly locate initialized data and uninitialized data (bss) in memory regions that
 * are NOT write-protected by the BUS-MPU.
 */
#define BCM_BMPU_RW_DATA(_data)	__attribute__ ((__section__ (".data_bmpu_rw." #_data))) _data
#define BCM_BMPU_RW_BSS(_data)	__attribute__ ((__section__ (".bss_bmpu_rw." #_data))) _data

/* Use BCM_FUNC_PTR() to tag function pointers for ASLR code implementation. It will perform
 * run-time relocation of a function pointer by translating it from a physical to virtual address.
 *
 * BCM_FUNC_PTR() should only be used where the function name is referenced (corresponding to the
 * relocation entry for that symbol). It should not be used when the function pointer is invoked.
 */
void* BCM_ASLR_CODE_FNPTR_RELOCATOR(void *func_ptr);
#if defined(BCM_ASLR_CODE_FNPTR_RELOC)
	/* 'func_ptr_err_chk' performs a compile time error check to ensure that only a constant
	 * function name is passed as an argument to BCM_FUNC_PTR(). This ensures that the macro is
	 * only used for function pointer references, and not for function pointer invocations.
	 *
	 * Cast function ptr arg to avoid warnings related to conversion of function ptr to void*.
	 */
	#define BCM_FUNC_PTR(fn)  BCM_EXTENSION \
		({ static void *func_ptr_err_chk __attribute__ ((unused)) = (void *)(uintptr)(fn); \
		(__typeof__(&fn))(uintptr)BCM_ASLR_CODE_FNPTR_RELOCATOR((void *)(uintptr)(fn)); })
#else
	#define BCM_FUNC_PTR(func)         (func)
#endif /* BCM_ASLR_CODE_FNPTR_RELOC */

/*
 * Timestamps have this tag appended following a null byte which
 * helps comparison/hashing scripts find and ignore them.
 */
#define TIMESTAMP_SUFFIX "<TIMESTAMP>"

#ifdef ASLR_STACK
/* MMU main thread stack data */
#define BCM_MMU_MTH_STK_DATA(_data) __attribute__ ((__section__ (".mmu_mth_stack." #_data))) _data
#endif /* ASLR_STACK */

/* Special section for MMU page-tables. */
#define BCM_MMU_PAGE_TABLE_DATA(_data) \
	__attribute__ ((__section__ (".mmu_pagetable." #_data))) _data

/* Some phy initialization code/data can't be reclaimed in dualband mode */
#if defined(DBAND)
#define WLBANDINITDATA(_data)	_data
#define WLBANDINITFN(_fn)	_fn
#else
#define WLBANDINITDATA(_data)	BCMINITDATA(_data)
#define WLBANDINITFN(_fn)	BCMINITFN(_fn)
#endif

/* Tag struct members to make it explicitly clear that they are physical addresses. These are
 * typically used in data structs shared by the firmware and host code (or off-line utilities). The
 * use of the macro avoids customer visible API/name changes.
 */
#if defined(BCM_PHYS_ADDR_NAME_CONVERSION)
	#define PHYS_ADDR_N(name) name ## _phys
#else
	#define PHYS_ADDR_N(name) name
#endif

/* Disable function inlining. */
#define BCM_NOINLINE	__attribute__ ((__noinline__))

/* Disable compiler optimizations for a function. */
#define BCM_NO_OPTIMIZE	__attribute__ ((optimize(0)))

/*
 * A compact form for a list of valid register address offsets.
 * Used for when dumping the contents of the register set for the user.
 *
 * Note: bmp_cnt is logically a single 32-bit word but it's
 * represented as an array of uint8 to avoid padding. A uint32
 * type would lead to 2 bytes of padding which would consume
 * ~20% more space per table.
 *
 * The MSB of these 32 bits is used as a flag: "count" if set, "bitmap" if not.
 * The two bits following the MSB are effectively an enum as below:
 *
 *     #define REGLIST_BMP_SIZE_1BYTE        0x0
 *     #define REGLIST_BMP_SIZE_2BYTES       0x1
 *     #define REGLIST_BMP_SIZE_4BYTES       0x2
 *     #define REGLIST_BMP_SIZE_UNUSED       0x3
 *
 * The remaining 29 bits are either a bitmap or a count. If the
 * MSB is set they're a count, i.e the number of valid same-size
 * registers whose values are contiguous from "addr".
 * If the MSB is zero they should be considered as a bitmap
 * of 29 discrete addresses counting from "addr".
 * Whether bitmap or count, each set of registers in a given
 * bmp_cnt set must have the same width as given by the enum.
 */
typedef struct _regs_bmp_list {
	uint16 addr;		/* start address offset */

	/* bit[31] count if set, else bitmap.
	 * bit[30:29] enum describing register size.
	 * Remaining 29 bits represent count if bit[31] is set or bitmap if unset.
	 */
	uint8 bmp_cnt[4u];
} regs_list_t;

#define REGLIST_BMP_SIZE_1BYTE		0x0
#define REGLIST_BMP_SIZE_2BYTES		0x1u
#define REGLIST_BMP_SIZE_4BYTES		0x2u
#define REGLIST_BMP_SIZE_UNUSED		0x3u

#define REGLIST_SIZE_SHIFT		29
#define REGLIST_SIZE_MASK		(3u << REGLIST_SIZE_SHIFT)
#define REGLIST_COUNT_MASK		(1u << 31)
#define REGLIST_LOAD_BMP32(_reglist)	((_reglist)->bmp_cnt[0] << 24 | \
					((_reglist)->bmp_cnt[1u] << 16) | \
					((_reglist)->bmp_cnt[2u] << 8) | \
					((_reglist)->bmp_cnt[3u]))

#define REGLIST_STORE_BMP32(_reglist, bitmap) { \
					(_reglist)->bmp_cnt[3u] = (uint8)bitmap; \
					(_reglist)->bmp_cnt[2u] = (uint8)(bitmap >> 8); \
					(_reglist)->bmp_cnt[1u] = (uint8)(bitmap >> 16); \
					(_reglist)->bmp_cnt[0u] = (uint8)(bitmap >> 24); \
}

#define REGLIST_WIDTH(rlst)		(((REGLIST_LOAD_BMP32((rlst)) & REGLIST_SIZE_MASK) >> \
					REGLIST_SIZE_SHIFT))

#define REGLIST_SIZE(rlst)		((REGLIST_WIDTH(rlst) == REGLIST_BMP_SIZE_1BYTE) ? \
						sizeof(uint8) : REGLIST_WIDTH(rlst) << 1u)

#define REGLIST_BMP_IS_COUNT(rlst)	((REGLIST_LOAD_BMP32((rlst)) & REGLIST_COUNT_MASK) != 0)

#define REGLIST_GET_REG_BITMAP(rlst)	(!REGLIST_BMP_IS_COUNT(rlst) ? (REGLIST_LOAD_BMP32(rlst) \
						& ~(REGLIST_SIZE_MASK | REGLIST_COUNT_MASK)) : 0)

#define REGLIST_GET_REG_COUNT(rlst)	(REGLIST_BMP_IS_COUNT(rlst) ? (REGLIST_LOAD_BMP32(rlst) \
						& ~(REGLIST_SIZE_MASK | REGLIST_COUNT_MASK)) : 0)
#ifndef WL_UNITTEST
typedef struct d11rxhdr d11rxhdr_t;
#endif /* WL_UNITTEST */

#endif /* _bcmdefs_h_ */
