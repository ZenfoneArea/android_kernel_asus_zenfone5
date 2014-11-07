/*
 * drivers/misc/intel_fw_logging.c
 *
 * Copyright (C) 2011 Intel Corp
 * Author: winson.w.yung@intel.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/rpmsg.h>
#include <linux/notifier.h>
#include <linux/delay.h>
#include <linux/ctype.h>
#include <linux/intel_mid_pm.h>
#include <asm/intel_mid_rpmsg.h>

#include <linux/io.h>
#include <asm/intel_scu_ipc.h>
#include <asm/intel_scu_ipcutil.h>

#include "intel_fabricid_def.h"
#include "intel_fw_trace.h"

/*
   OSHOB - OS Handoff Buffer

   This buffer contains the 32-byte value that is persists across cold and warm
   resets only, but loses context on a cold boot.

   More info about OSHOB, OSNIB could be found in FAS Section 2.8.
   We use the first byte in OSNIB to store and pass the Reboot/boot Reason.
   The attribute of OS image is selected for Reboot/boot reason.
*/

#define RECOVERABLE_FABERR_INT		9
#define MAX_FID_REG_LEN			32

#define USE_LEGACY()							\
	(intel_mid_identify_cpu() == INTEL_MID_CPU_CHIP_PENWELL ||	\
	 intel_mid_identify_cpu() == INTEL_MID_CPU_CHIP_CLOVERVIEW)

/* The legacy fabric error logging struct (e.g. Clovertrail) takes 12 dwords
 * of basic, and 9 additional dwords of extension.
 */
#define MAX_NUM_LOGDWORDS		12
#define MAX_NUM_LOGDWORDS_EXTENDED      9
#define MAX_NUM_ALL_LOGDWORDS_LEGACY    (MAX_NUM_LOGDWORDS +		\
					 MAX_NUM_LOGDWORDS_EXTENDED)
#define SIZE_ALL_LOGDWORDS_LEGACY       (MAX_NUM_ALL_LOGDWORDS_LEGACY * \
					 sizeof(u32))

/* The new fabric error logging struct (e.g. Tangier) takes a maximum
 * of 50 dwords.
 */
#define MAX_NUM_ALL_LOGDWORDS           50
#define SIZE_ALL_LOGDWORDS              (MAX_NUM_ALL_LOGDWORDS *	\
					 sizeof(u32))

#define FABERR_INDICATOR		0x15
#define FABERR_INDICATOR1		0x0dec0ded
#define FWERR_INDICATOR			0x7
#define UNDEFLVL1ERR_IND		0x11
#define UNDEFLVL2ERR_IND		0x22
#define SWDTERR_IND			0xdd
#define MEMERR_IND			0xf501
#define INSTERR_IND			0xf502
#define ECCERR_IND			0xf504
#define FATALERR_IND			0xf505
#define INFORMATIVE_MSG_IND		0xf506
#define FLAG_HILOW_MASK			8
#define FAB_ID_MASK			7
#define MAX_AGENT_IDX			15

#define DWORDS_PER_LINE			2

/* Safety limits for SCU extra trace dump */
#define LOWEST_PHYS_SRAM_ADDRESS        0xFFFC0000
#define MAX_SCU_EXTRA_DUMP_SIZE         4096

/* Special indexes in error data */
#define FABRIC_ERR_STS_IDX		0
#define FABRIC_ERR_SIGNATURE_IDX	10

/* For new fabric error logging layout */
#define FABRIC_ERR_HEADER		0
#define FABRIC_ERR_SIGNATURE_IDX1	1
#define FABRIC_ERR_SCU_VERSIONINFO	2
#define FABRIC_ERR_ERRORTYPE		3
#define FABRIC_ERR_MAXIMUM_TXT		2048
#define FABRIC_ERR_ERRORTYPE_MASK	0xFFFF

/* Timeout in ms we wait SCU to generate dump on panic */
#define SCU_PANIC_DUMP_TOUT		1
#define SCU_PANIC_DUMP_RECHECK		5

#define output_str(ret, out, size, a...)				\
	do {								\
		if (out && (size) - (ret) > 1) {			\
			(ret) += snprintf((out) + (ret),		\
					  (size) - (ret), ## a);	\
			if ((size) - (ret) <= 0)			\
				ret = size - 1;				\
		} else {						\
			pr_info(a);					\
		}							\
	} while (0)

union error_log {
	struct {
		u32 cmd:3;
		u32 signature:5;
		u32 initid:8;
		u32 num_err_logs:4;
		u32 agent_idx:4;
		u32 err_code:4;
		u32 fw_err_ind:3;
		u32 multi_err:1;
	} fields;
	u32 data;
};

union fabric_status {
	struct {
		u32 status_has_hilo:11;
		u32 flag_status_cnt:5;
		u32 status_has_hilo1:12;
		u32 regidx:4;
	} fields;
	u32 data;
};

union flag_status_hilo {
	struct {
		/* Maps to flag_status [10..0] or [42..32] */
		u32 bits_rang0:11;
		u32 reserved1:5;

		/* Maps to flag_status [27..16] or [59..48] */
		u32 bits_rang1:12;
		u32 reserved:4;
	} fields;
	u32 data;
};

/* For new fabric error log format layout */

union error_header {
	struct {
		u32 num_of_recv_err:6;
		u32 recv_err_count_overflow:1;
		u32 logging_buf_full:1;
		u32 num_flag_regs:8;
		u32 num_err_regs:8;
		u32 checksum:8;
	} fields;
	u32 data;
};

union error_scu_version {
	struct {
		u32 scu_rt_minor_ver:16;
		u32 scu_rt_major_ver:16;
	} fields;
	u32 data;
};

union reg_ids {
	struct {
		u32 reg_id0:8;
		u32 reg_id1:8;
		u32 reg_id2:8;
		u32 reg_id3:8;
	} fields;
	u32 data;
};

static void __iomem *oshob_base;
static u32 *log_buffer;
static u32 log_buffer_sz;

static char *parsed_fab_err;
static u32 parsed_fab_err_sz;
static u32 parsed_fab_err_length;

static void __iomem *fabric_err_buf1;
static void __iomem *fabric_err_buf2;
static void __iomem *sram_trace_buf;

static struct scu_trace_hdr_t trace_hdr;
static u32 *scu_trace_buffer;

static int scu_trace_buffer_size;

static int irq;
static int recoverable_irq;

static struct rpmsg_instance *fw_logging_instance;

static char *fabric_names[] = {
	"\nFull Chip Fabric [error]\n\n",
	"\nAudio Fabric [error]\n\n",
	"\nSecondary Chip Fabric [error]\n\n",
	"\nGP Fabric [error]\n\n",
	"\nSC Fabric [error]\n\n",
	"\nSC1 Fabric [error]\n\n",
	"\nUnknown Fabric [error]\n\n"
};

static char *agent_names[] = {
	"FULLFAB_FLAG_STATUS",
	"AUDIO",
	"SECONDARY",
	"GP",
	"SC",
	"CDMI_TOCP_TA",
	"CDMI_IOCP_IA",
	"FCSF_IOCP_IA",
	"FCGF_IOCP_IA",
	"AFSF_IOCP_IA",
	"SFFC_IOCP_IA",
	"SFAF_IOCP_IA",
	"SFSC_IOCP_IA",
	"GFFC_IOCP_IA",
	"ARC_IOCP_IA",
	"SCSF_TOCP_TA"
};

static bool disable_scu_tracing;
static int set_disable_scu_tracing(const char *val,
				   const struct kernel_param *kp)
{
	int err;
	bool saved_value;

	saved_value = kp->arg;

	err = param_set_bool(val, kp);
	if (err || ((bool)kp->arg == saved_value))
		return err;

	if (disable_scu_tracing)
		disable_irq(irq);
	else
		enable_irq(irq);

	return 0;
}

static struct kernel_param_ops disable_scu_tracing_ops = {
	.set = set_disable_scu_tracing,
	.get = param_get_bool,
};
module_param_cb(disable_scu_tracing, &disable_scu_tracing_ops,
		&disable_scu_tracing,  S_IRUSR | S_IWUSR);
MODULE_PARM_DESC(disable_scu_tracing,
		"Disable scu tracing"
		 "Set to 1 to prevent SCU tracing messages in dmesg");

static irqreturn_t fw_logging_irq_thread(int irq, void *ignored)
{
	char *trace, *end, prefix[20];
	unsigned int count;
	int i, len;
	u32 size;

	i = snprintf(prefix, sizeof(prefix), "SCU TRACE ");
	switch (trace_hdr.cmd & TRACE_ID_MASK) {
	case TRACE_ID_INFO:
		i += snprintf(prefix + i, sizeof(prefix) - i, "INFO");
		break;
	case TRACE_ID_ERROR:
		i += snprintf(prefix + i, sizeof(prefix) - i, "ERROR");
		break;
	default:
		pr_err("Invalid message ID!\n");
		break;
	}
	snprintf(prefix + i, sizeof(prefix) - i, ": ");

	if (trace_hdr.cmd & TRACE_IS_ASCII) {
		size = trace_hdr.size;
		trace = (char *)scu_trace_buffer;
		end = trace + trace_hdr.size;
		while (trace < end) {
			len = strnlen(trace, size);
			if (!len) {
				trace++;
				continue;
			}
			pr_info("%s%s\n", prefix, trace);
			trace += len + 1;
			size -= len;
		}
	} else {
		count = trace_hdr.size / sizeof(u32);

		for (i = 0; i < count; i++)
			pr_info("%s[%d]:0x%08x\n", prefix, i,
				scu_trace_buffer[i]);
	}

	return IRQ_HANDLED;
}

static void read_scu_trace_hdr(struct scu_trace_hdr_t *hdr)
{
	unsigned int count;
	u32 *buf = (u32 *) hdr;
	int i;

	if (!hdr)
		return;

	count = sizeof(struct scu_trace_hdr_t) / sizeof(u32);

	if (!fabric_err_buf1) {
		pr_err("Invalid Fabric Error buf1 offset\n");
		return;
	}
	for (i = 0; i < count; i++)
		*(buf + i)  = readl(fabric_err_buf1 + i * sizeof(u32));
}

static void read_sram_trace_buf(u32 *buf, u8 *scubuffer, unsigned int size)
{
	int i;
	unsigned int count;

	if (!buf || !scubuffer || !size)
		return;

	count = size / sizeof(u32);
	for (i = 0; i < count; i++)
		buf[i] = readl(scubuffer + i * sizeof(u32));
}

static irqreturn_t fw_logging_irq(int irq, void *ignored)
{
	read_scu_trace_hdr(&trace_hdr);

	if (trace_hdr.magic != TRACE_MAGIC ||
	    trace_hdr.offset + trace_hdr.size > scu_trace_buffer_size) {
		pr_err("Invalid SCU trace!\n");
		return IRQ_HANDLED;
	}

	read_sram_trace_buf(scu_trace_buffer,
			    (u8 *) sram_trace_buf + trace_hdr.offset,
			    trace_hdr.size);

	return IRQ_WAKE_THREAD;
}

static void __iomem *get_oshob_addr(void)
{
	u32 oshob_base_addr = 0;
	u16 oshob_size;
	void __iomem *oshob_addr;

	oshob_base_addr = intel_scu_ipc_get_oshob_base();
	if (oshob_base_addr == 0) {
		pr_err("Invalid OSHOB address!!\n");
		return NULL;
	}

	oshob_size = intel_scu_ipc_get_oshob_size();
	if (oshob_size == 0) {
		pr_err("Size of oshob is null!!\n");
		return NULL;
	}

	pr_debug("OSHOB addr is 0x%x size is %d\n",
		 oshob_base_addr, oshob_size);

	oshob_addr = ioremap_nocache(
			(resource_size_t)oshob_base_addr,
			(unsigned long)oshob_size);
	if (oshob_addr == NULL) {
		pr_err("ioremap of oshob address failed!!\n");
		return NULL;
	}

	return oshob_addr; /* Return OSHOB base address */
}

static u8 caculate_checksum(u32 length)
{
	int i;
	u8 checksum = 0;
	u8 *array = (u8 *)log_buffer;

	for (i = 0; i < length; i++)
		checksum += array[i];

	return ~checksum + 1;
}

static bool fw_error_found(bool use_legacytype)
{
	u8 checksum = 0;
	union error_log err_log;
	union error_header err_header;

	if (use_legacytype) {

		err_log.data = log_buffer[FABRIC_ERR_SIGNATURE_IDX];

		/* No SCU/fabric error if tenth
		 * DW signature field is not 10101 */
		if (err_log.fields.signature != FABERR_INDICATOR)
			return false;
	} else {

		if (log_buffer[FABRIC_ERR_SIGNATURE_IDX1] != FABERR_INDICATOR1)
			return false;

		err_header.data = log_buffer[FABRIC_ERR_HEADER];
		checksum = err_header.fields.checksum;
		err_header.fields.checksum = 0;
		log_buffer[FABRIC_ERR_HEADER] = err_header.data;

		if (caculate_checksum(MAX_NUM_ALL_LOGDWORDS << 2) !=
		    checksum) {
			pr_info("fw_error_found: new checksum error\n");
			return false;
		}
	}

	return true;
}

static int get_fabric_error_cause_detail(char *buf, u32 size, u32 fabid,
					 u32 *fid_status, int ishidword)
{
	int index = 0, ret = 0;
	char *ptr;
	u32 fid_mask = 1;

	while (index < MAX_FID_REG_LEN) {

		if ((*fid_status) & fid_mask) {
			ptr = fabric_error_lookup(fabid, index, ishidword);

			if (ptr && *ptr)
				output_str(ret, buf, size, "%s\n", ptr);
		}

		index++;
		fid_mask <<= 1;
	}

	output_str(ret, buf, size, "\n");

	return ret;
}

static int get_additional_error(char *buf, int size, int num_err_log,
			u32 *faberr_dwords, int max_dw_left)
{
	int i = 0, ret = 0;
	union error_log log;

	output_str(ret, buf, size,
		   "\nAdditional logs associated with error(s): ");

	if (num_err_log) {

		while (i < num_err_log && i < max_dw_left) {

			output_str(ret, buf, size, "\nerror_log: 0x%X\n",
				*(faberr_dwords + i));

			output_str(ret, buf, size, "error_addr: 0x%X\n",
				   *(faberr_dwords + i + 1));

			log.data = *(faberr_dwords + i);

			output_str(ret, buf, size,
				   "\nDecoded error log detail\n");
			output_str(ret, buf, size,
				   "---------------------------\n\n");

			output_str(ret, buf, size, "Agent Index:");
			if (log.fields.agent_idx > MAX_AGENT_IDX) {
				output_str(ret, buf, size,
					   "Unknown agent index (%d)\n\n",
					   log.fields.agent_idx);
			} else {
				output_str(ret, buf, size, "%s\n\n",
					   agent_names[log.fields.agent_idx]);
			}

			output_str(ret, buf, size,
				   "Cmd initiator ID: %d\n",
				   log.fields.initid);

			output_str(ret, buf, size, "Command: %d\n",
				   log.fields.cmd);

			output_str(ret, buf, size, "Code: %d\n",
				   log.fields.err_code);

			if (log.fields.multi_err)
				output_str(ret, buf, size,
					   "\n Multiple errors detected!\n");
			i += 2; /* Skip one error_log/addr pair */
		}
	} else {
		output_str(ret, buf, size, "Not present\n");
	}

	return ret;
}

char *get_fabric_name(u32 fabric_idx, u32 *fab_id)
{
	switch (fabric_idx) {
	case 0: /* REGIDX [31..28] is x000 */
	case 1: /* REGIDX [31..28] is x001 */
	case 2: /* REGIDX [31..28] is x010 */
	case 3: /* REGIDX [31..28] is x011 */
	case 4: /* REGIDX [31..28] is x100 */
		*fab_id = fabric_idx;
		break;
	default:
		*fab_id = FAB_ID_UNKNOWN;
		break;
	}

	return fabric_names[*fab_id];
}

static void read_fwerr_log(u32 *buf, void __iomem *oshob_ptr)
{
	int count;
	void __iomem *fabric_err_dump_offset = oshob_ptr +
		intel_scu_ipc_get_fabricerror_buf1_offset();

	if (fabric_err_dump_offset == oshob_ptr) {
		pr_err("Invalid Fabric error buf1 offset\n");
		return;
	}
	if (!USE_LEGACY()) {
		for (count = 0; count < MAX_NUM_ALL_LOGDWORDS; count++)
			buf[count] = readl(fabric_err_dump_offset +
					   count * sizeof(u32));
	} else {
		for (count = 0; count < MAX_NUM_LOGDWORDS; count++)
			buf[count] = readl(fabric_err_dump_offset +
					   count * sizeof(u32));

		/* Get 9 additional DWORDS */
		fabric_err_dump_offset = oshob_ptr +
			intel_scu_ipc_get_fabricerror_buf2_offset();

		if (fabric_err_dump_offset == oshob_ptr) {
			/* Fabric error buf2 not available on all platforms. */
			pr_warn("No Fabric Error buf2 offset available\n");
			return;
		}

		for (count = 0; count < MAX_NUM_LOGDWORDS_EXTENDED; count++)
			buf[count + MAX_NUM_LOGDWORDS] =
				readl(fabric_err_dump_offset +
				      sizeof(u32) * count);
	}
}

static int dump_fwerr_log(char *buf, int size)
{
	char *ptr = NULL;
	union error_log err_log;
	union flag_status_hilo flag_status;
	union fabric_status err_status;
	u32 id = FAB_ID_UNKNOWN;
	int count, num_flag_status, num_err_logs;
	int prev_id = FAB_ID_UNKNOWN, offset = 0, ret = 0;

	err_status.data = log_buffer[FABRIC_ERR_STS_IDX];
	err_log.data = log_buffer[FABRIC_ERR_SIGNATURE_IDX];

	/* FW error if tenth DW reserved field is 111 */
	if ((((err_status.data & 0xFFFF) == SWDTERR_IND) ||
	     ((err_status.data & 0xFFFF) == UNDEFLVL1ERR_IND) ||
	     ((err_status.data & 0xFFFF) == UNDEFLVL2ERR_IND) ||
	     ((err_status.data & 0xFFFF) == MEMERR_IND) ||
	     ((err_status.data & 0xFFFF) == INSTERR_IND) ||
	     ((err_status.data & 0xFFFF) == ECCERR_IND) ||
	     ((err_status.data & 0xFFFF) == FATALERR_IND) ||
	     ((err_status.data & 0xFFFF) == INFORMATIVE_MSG_IND)) &&
	    (err_log.fields.fw_err_ind == FWERR_INDICATOR)) {

		output_str(ret, buf, size, "HW WDT expired");

		switch (err_status.data & 0xFFFF) {
		case SWDTERR_IND:
			output_str(ret, buf, size,
				   " without facing any exception.\n\n");
			break;
		case MEMERR_IND:
			output_str(ret, buf, size,
				   " following a Memory Error exception.\n\n");
			break;
		case INSTERR_IND:
			output_str(ret, buf, size,
				   " following an Instruction Error exception.\n\n");
			break;
		case ECCERR_IND:
			output_str(ret, buf, size,
				   " following a SRAM ECC Error exception.\n\n");
			break;
		case FATALERR_IND:
			output_str(ret, buf, size,
				   " following a FATAL Error exception.\n\n");
			break;
		case INFORMATIVE_MSG_IND:
			output_str(ret, buf, size,
				   " following an Informative Message.\n\n");
			break;
		default:
			output_str(ret, buf, size, ".\n\n");
			break;
		}
		output_str(ret, buf, size, "HW WDT debug data:\n");
		output_str(ret, buf, size, "===================\n");
		for (count = 0;
			count < MAX_NUM_LOGDWORDS + MAX_NUM_LOGDWORDS_EXTENDED;
			count++) {
			output_str(ret, buf, size, "DW%d:0x%08x\n",
				   count, log_buffer[count]);
		}
		goto out;
	}

	num_flag_status = err_status.fields.flag_status_cnt;
	/* num_err_logs indicates num of error_log/addr pairs */
	num_err_logs = err_log.fields.num_err_logs * 2;

	output_str(ret, buf, size,
		   "HW WDT fired following a Fabric Error exception.\n\n");
	output_str(ret, buf, size, "Fabric Error debug data:\n");
	output_str(ret, buf, size, "===================\n");

	for (count = 0; count < num_flag_status; count++) {

		err_status.data = log_buffer[count];
		ptr = get_fabric_name(
			err_status.fields.regidx & FAB_ID_MASK, &id);

		/*
		 * Only print the fabric name if is unknown
		 * type, or we haven't printed out it yet.
		 */

		if (prev_id != id || id == FAB_ID_UNKNOWN) {
			output_str(ret, buf, size, "%s", ptr);
			prev_id = id;
		}

		flag_status.data = 0;
		flag_status.fields.bits_rang0 =
				err_status.fields.status_has_hilo;

		flag_status.fields.bits_rang1 =
				err_status.fields.status_has_hilo1;

		/*
		 * The most significant bit in REGIDX field is set
		 * the flag_status has the high 32bit of the dword
		 * otherwise the flag_status has the low 32bit of
		 * the dword
		 */

		if (err_status.fields.regidx & FLAG_HILOW_MASK)
			ret += get_fabric_error_cause_detail(buf + ret,
							     size - ret,
							     id,
							     &flag_status.data,
							     1);
		else
			ret += get_fabric_error_cause_detail(buf + ret,
							     size - ret,
							     id,
							     &flag_status.data,
							     0);

		offset++; /* Use this to track error_log/address offset */
	}

	if (offset & 1)
		offset++; /* If offset is odd number, adjust to even offset */

	ret += get_additional_error(buf + ret, size - ret, num_err_logs,
				    &log_buffer[offset],
				    MAX_NUM_LOGDWORDS - offset);

	output_str(ret, buf, size, "\n\n\nAdditional debug data:\n\n");
	for (count = 0;
		count < MAX_NUM_LOGDWORDS + MAX_NUM_LOGDWORDS_EXTENDED;
		count++) {
		output_str(ret, buf, size, "DW%d:0x%08x\n",
			   count, log_buffer[count]);
	}
out:
	return ret;
}

static int dump_scu_extented_trace(char *buf, int size,
				   int log_offset, int *read)
{
	int ret = 0;
	int i;
	unsigned int end, start;

	*read = 0;

	/* Title for error dump */
	if ((USE_LEGACY() && (log_offset == SIZE_ALL_LOGDWORDS_LEGACY)) ||
	    (!USE_LEGACY() && (log_offset == SIZE_ALL_LOGDWORDS)))
		output_str(ret, buf, size, "SCU Extra trace\n");

	start = log_offset / sizeof(u32);
	end = log_buffer_sz / sizeof(u32);
	for (i = start; i < end; i++) {
		/* Make sure we get only full lines */
		if (buf && (size - ret < 18))
			break;
		/*
		 * "EW:" to separate lines from "DW:" lines
		 * elsewhere in this file.
		 */
		output_str(ret, buf, size, "EW%d:0x%08x\n", i,
			   *(log_buffer + i));
		*read += sizeof(u32);
	}

	return ret;
}

static char *cmd_type_str[] = {
	"Idle",
	"Write",
	"Read",
	"ReadEx",
	"ReadLinked",
	"WriteNonPost",
	"WriteConditional",
	"Broadcast"
};

static char *error_type_str[] = {
	"Unknown",
	"Unsupported Command",
	"Address Hole",
	"Unknown",
	"Inband Error",
	"Unknown",
	"Unknown",
	"Request Timeout, Not Accepted",
	"Request Timeout, No Response",
	"Request Timeout, Data not accepted",
	"Unknown",
	"Unknown",
	"Unknown",
	"Unknown",
	"Unknown",
	"Unknown"
};

#define ALLOC_UNIT_SIZE 1024
#define MAX_LINE_SIZE   132
#define MAX_ALLOC_SIZE  (40 * ALLOC_UNIT_SIZE)
#define fab_err_snprintf(str, sz, format, a...)			\
	do {								\
		char _buffer[MAX_LINE_SIZE];				\
		int  _n, _current_size;				\
		if ((str) == NULL)					\
			break;						\
		_n = snprintf(_buffer, MAX_LINE_SIZE, (format), ## a);	\
		_n = min(_n, MAX_LINE_SIZE - 1);			\
		_current_size = strlen(str);				\
		if ((_current_size + _n + 1) > (sz)) {			\
			if (((sz) + ALLOC_UNIT_SIZE) > MAX_ALLOC_SIZE)	\
				break;					\
			(str) = krealloc(				\
				(str),					\
				(sz) + ALLOC_UNIT_SIZE,		\
				GFP_KERNEL);				\
			if ((str) != NULL)				\
				(sz) += ALLOC_UNIT_SIZE;		\
			else {						\
				(sz) = 0;				\
				pr_err("krealloc failed\n");		\
			}						\
		}							\
		if ((str) != NULL) {					\
			memcpy((str)+_current_size, _buffer, _n+1);	\
		}							\
	} while (0)

static int parse_fab_err_log(
	char **parsed_fab_err_log, u32 *parsed_fab_err_log_sz)
{
	u8 id = 0;
	char *ptr;
	u32 fabric_id;
	union error_header err_header;
	union error_scu_version err_scu_ver;
	int error_type, cmd_type, init_id, is_multi, is_secondary;
	u16 scu_minor_ver, scu_major_ver;
	u32 reg_ids = 0, error_typ = log_buffer[FABRIC_ERR_ERRORTYPE];
	int i, need_new_regid, num_flag_status,
		num_err_logs, offset, total;

	err_header.data = log_buffer[FABRIC_ERR_HEADER];
	err_scu_ver.data = log_buffer[FABRIC_ERR_SCU_VERSIONINFO];

	scu_minor_ver = err_scu_ver.fields.scu_rt_minor_ver;
	scu_major_ver = err_scu_ver.fields.scu_rt_major_ver;

	error_typ &= FABRIC_ERR_ERRORTYPE_MASK;

	num_flag_status = err_header.fields.num_flag_regs;
	num_err_logs = err_header.fields.num_err_regs;

	if (*parsed_fab_err_log != NULL)
		kfree(*parsed_fab_err_log);
	*parsed_fab_err_log = kzalloc(ALLOC_UNIT_SIZE, GFP_KERNEL);
	if (*parsed_fab_err_log == NULL) {
		*parsed_fab_err_log_sz = 0;
		return 0;
	}
	*parsed_fab_err_log_sz = ALLOC_UNIT_SIZE;

	fab_err_snprintf(
		*parsed_fab_err_log, *parsed_fab_err_log_sz,
		"Fabric Error debug data:\n");
	fab_err_snprintf(
		*parsed_fab_err_log, *parsed_fab_err_log_sz,
		"========================\n");
	fab_err_snprintf(
		*parsed_fab_err_log, *parsed_fab_err_log_sz,
		"SCU runtime major version: %X\n",
		scu_major_ver);
	fab_err_snprintf(
		*parsed_fab_err_log, *parsed_fab_err_log_sz,
		"SCU runtime minor version: %X\n",
		scu_minor_ver);
	fab_err_snprintf(
		*parsed_fab_err_log, *parsed_fab_err_log_sz,
		"Total Errlog reg recorded: %d\n",
		num_err_logs);
	fab_err_snprintf(
		*parsed_fab_err_log, *parsed_fab_err_log_sz,
		"Total Flag Status reg recorded: %d\n",
		num_flag_status);
	fab_err_snprintf(
		*parsed_fab_err_log, *parsed_fab_err_log_sz,
		"Recoverable error counter overflowed: %s\n",
		err_header.fields.recv_err_count_overflow ? "Yes" : "No");
	fab_err_snprintf(
		*parsed_fab_err_log, *parsed_fab_err_log_sz,
		"Logging structure ran out of space: %s\n",
		err_header.fields.logging_buf_full ? "Yes" : "No");
	fab_err_snprintf(
		*parsed_fab_err_log, *parsed_fab_err_log_sz,
		"# of recoverable error since last fatal: %d\n",
		err_header.fields.num_of_recv_err);
	fab_err_snprintf(
		*parsed_fab_err_log, *parsed_fab_err_log_sz,
		"Fabric error type: %s\n\n",
		get_errortype_str(error_typ));
	fab_err_snprintf(
		*parsed_fab_err_log, *parsed_fab_err_log_sz,
		"Summary of Fabric Error detail:\n");
	fab_err_snprintf(
		*parsed_fab_err_log, *parsed_fab_err_log_sz,
		"-------------------------------\n");

	i = 0;
	total = 0;
	offset = 4;
	need_new_regid = 1;

	reg_ids = 0;
	while ((num_flag_status + num_err_logs) > 0) {
		if (!reg_ids)
			reg_ids = log_buffer[offset++];
		id = (reg_ids & 0xFF);
		reg_ids >>= 8;
		if (num_flag_status) {
			unsigned long flag_status;
			int hi;
			ptr = get_element_flagsts_detail(id);
			fab_err_snprintf(
				*parsed_fab_err_log, *parsed_fab_err_log_sz,
				"\n* %s\n",
				ptr);
			for (hi = 0; hi < 2; hi++) {
				flag_status = log_buffer[offset+hi];
				while (flag_status) {
					unsigned long idx =
						__ffs(flag_status);
					ptr = fabric_error_lookup(id, idx, hi);
					if (ptr && *ptr)
						fab_err_snprintf(
							*parsed_fab_err_log,
							*parsed_fab_err_log_sz,
							"%s\n", ptr);
					flag_status &= ~(1<<idx);
				}
			}
			offset += 2;
			num_flag_status--;
		} else {
			ptr = get_element_errorlog_detail(id, &fabric_id);
			fab_err_snprintf(
				*parsed_fab_err_log, *parsed_fab_err_log_sz,
				"\n* %s\n",
				ptr);
			fab_err_snprintf(
				*parsed_fab_err_log, *parsed_fab_err_log_sz,
				"Lower ErrLog DW: 0x%08X\n",
				log_buffer[offset]);

			cmd_type = log_buffer[offset] & 7;
			fab_err_snprintf(
				*parsed_fab_err_log, *parsed_fab_err_log_sz,
				"\tCommand Type: %s\n",
				cmd_type_str[cmd_type]);

			init_id = (log_buffer[offset] >> 8) & 0xFF;
			ptr = get_initiator_id_str(init_id, fabric_id);
			fab_err_snprintf(
				*parsed_fab_err_log, *parsed_fab_err_log_sz,
				"\tInit ID: %s\n", ptr);

			error_type = (log_buffer[offset] >> 24) & 0xF;
			fab_err_snprintf(
				*parsed_fab_err_log, *parsed_fab_err_log_sz,
				"\tError Type: %s\n",
				error_type_str[error_type]);

			is_secondary = (log_buffer[offset] >> 30) & 1;
			fab_err_snprintf(
				*parsed_fab_err_log, *parsed_fab_err_log_sz,
				"\tSecondary error: %s\n",
				is_secondary ? "Yes" : "No");

			is_multi = (log_buffer[offset] >> 31) & 1;
			fab_err_snprintf(
				*parsed_fab_err_log, *parsed_fab_err_log_sz,
				"\tMultiple errors: %s\n",
				is_multi ? "Yes" : "No");

			fab_err_snprintf(
				*parsed_fab_err_log, *parsed_fab_err_log_sz,
				"Upper ErrLog DW: 0x%08X\n",
				log_buffer[offset + 1]);
			fab_err_snprintf(
				*parsed_fab_err_log, *parsed_fab_err_log_sz,
				"Associated 32bit Address: 0x%08X\n",
				log_buffer[offset + 2]);

			offset += 3;
			num_err_logs--;
		}
	}

	fab_err_snprintf(
		*parsed_fab_err_log, *parsed_fab_err_log_sz,
		"\n\n");

	if (error_typ != ERR_FABRIC_ERR)
		offset = MAX_NUM_ALL_LOGDWORDS;

	for (i = 0; i < offset; i++)
		fab_err_snprintf(
			*parsed_fab_err_log, *parsed_fab_err_log_sz,
			"DW%d:0x%08x\n",
			i, log_buffer[i]);

	if (*parsed_fab_err_log != NULL) {
		char *footer = "\nLength of fabric error file: %5dB\n";
		fab_err_snprintf(
			*parsed_fab_err_log,
			*parsed_fab_err_log_sz,
			footer,
			strlen(*parsed_fab_err_log) + strlen(footer) + 2);
	}

	if (*parsed_fab_err_log != NULL)
		return strlen(*parsed_fab_err_log);
	else
		return 0;
}

#ifdef CONFIG_PROC_FS
struct proc_dir_entry *ipanic_faberr;
struct proc_dir_entry *offline_scu_log;
struct proc_dir_entry *ipanic_faberr_recoverable;

static ssize_t intel_fw_logging_recoverable_proc_read(struct file *file,
						  char __user *buffer,
						  size_t count, loff_t *ppos)
{
	if ((parsed_fab_err == NULL) || (*ppos >= parsed_fab_err_length))
		return 0; /* We have finished to read, return 0 */

	if ((*ppos + count) >= parsed_fab_err_length)
		count = parsed_fab_err_length - *ppos;

	if (copy_to_user(buffer, parsed_fab_err + *ppos, count))
		return -EFAULT;

	*ppos += count;

	return count;
}

static ssize_t intel_fw_logging_proc_read(struct file *file,
					  char __user *buffer, size_t count,
					  loff_t *ppos)
{
	int ret = 0;
	u32 read;

	if (!USE_LEGACY()) {
		return intel_fw_logging_recoverable_proc_read(file, buffer,
							      count, ppos);
	} else {
		if (!*ppos) {
			/* Fill the buffer, return the buffer size*/
			ret = dump_fwerr_log(buffer, count);
			read = SIZE_ALL_LOGDWORDS_LEGACY;
			*ppos = read;
		} else {
			if (*ppos >= SIZE_ALL_LOGDWORDS_LEGACY +
			    scu_trace_buffer_size)
				return 0;
			ret = dump_scu_extented_trace(buffer, count,
						      *ppos, &read);
			*ppos += read;
		}
	}
	return ret;
}

static ssize_t offline_scu_log_proc_read(struct file *file,
					  char __user *buffer, size_t count,
					  loff_t *ppos)
{
	if (*ppos >= scu_trace_buffer_size) {
		/* outside of offline SCU trace buffer bounds */
		return 0;
	}

	if ((*ppos + count) >= scu_trace_buffer_size)
		count = (scu_trace_buffer_size - *ppos);

	if (copy_to_user(buffer, scu_trace_buffer + *ppos, count))
		return -EFAULT;

	*ppos += count;

	return count;
}

static const struct file_operations ipanic_fabric_err_fops = {
	.read = intel_fw_logging_proc_read
};

static const struct file_operations offline_scu_log_fops = {
	.read = offline_scu_log_proc_read
};

static const struct file_operations ipanic_fab_recoverable_fops = {
	.read = intel_fw_logging_recoverable_proc_read
};
#endif /* CONFIG_PROC_FS */

static irqreturn_t recoverable_faberror_irq(int irq, void *ignored)
{
	bool use_legacytype;
	pr_err("A recoverable fabric error has been captured!!!\n");

	read_fwerr_log(log_buffer, oshob_base);

	use_legacytype = USE_LEGACY();

	if (use_legacytype || !fw_error_found(use_legacytype)) {
		pr_info("No valid stored new SCU errors found in SRAM, bogus interrupt\n");
		return IRQ_HANDLED;
	}

	parsed_fab_err_length = parse_fab_err_log(&parsed_fab_err,
						  &parsed_fab_err_sz);

	return IRQ_WAKE_THREAD;
}

static irqreturn_t recoverable_faberror_thread(int irq, void *ignored)
{
	/* Clear fabric error region inside OSHOB if neccessary */
	pr_info("recoverable_faberror_thread: IPCMSG_CLEAR_FABERROR\n");
	rpmsg_send_simple_command(fw_logging_instance,
				  IPCMSG_CLEAR_FABERROR, 0);

	return IRQ_HANDLED;
}

static int fw_logging_crash_on_boot(void)
{
	int length = 0;
	int err = 0;
	bool use_legacytype = USE_LEGACY();
	u32 read;

	if (use_legacytype)
		log_buffer_sz =
			SIZE_ALL_LOGDWORDS_LEGACY + scu_trace_buffer_size;
	else
		log_buffer_sz =
			SIZE_ALL_LOGDWORDS + scu_trace_buffer_size;

	log_buffer = kzalloc(log_buffer_sz, GFP_KERNEL);
	if (!log_buffer) {
		pr_err("Failed to allocate memory for log buffer\n");
		err = -ENOMEM;
		goto out1;
	}

	read_fwerr_log(log_buffer, oshob_base);

	if (!fw_error_found(use_legacytype)) {
		pr_info("No valid stored SCU errors found in SRAM\n");
		goto out1;
	}

	if (use_legacytype) {
		length = dump_fwerr_log(NULL, 0);

		if (sram_trace_buf) {
			/*
			 * SCU gives pointer via oshob. Address is a physical
			 * address somewhere in shared sram
			 */
			read_sram_trace_buf(
				log_buffer + MAX_NUM_ALL_LOGDWORDS_LEGACY,
				sram_trace_buf, scu_trace_buffer_size);

			length += dump_scu_extented_trace(NULL, 0,
					      SIZE_ALL_LOGDWORDS_LEGACY, &read);
		}
	} else {
		parsed_fab_err_length =
			parse_fab_err_log(&parsed_fab_err, &parsed_fab_err_sz);

		if ((sram_trace_buf != NULL) && (scu_trace_buffer != NULL))
			memcpy(scu_trace_buffer, sram_trace_buf,
			       scu_trace_buffer_size);
	}


#ifdef CONFIG_PROC_FS

	ipanic_faberr = proc_create("ipanic_fabric_err", S_IFREG | S_IRUGO,
				    NULL, &ipanic_fabric_err_fops);
	if (ipanic_faberr == 0) {
		pr_err("Fail creating procfile ipanic_fabric_err for fatal fabric err\n");
		err = -ENODEV;
		goto out1;
	}

	offline_scu_log = proc_create("offline_scu_log", S_IFREG | S_IRUGO,
				    NULL, &offline_scu_log_fops);
	if (offline_scu_log == 0) {
		pr_err("Fail creating procfile offline_scu_log for SCU log\n");
		err = -ENODEV;
		goto out2;
	}

#endif /* CONFIG_PROC_FS */

	return err;

out2:
	remove_proc_entry("ipanic_fabric_err", NULL);
out1:
	return err;
}

static int intel_fw_logging_panic_handler(struct notifier_block *this,
					  unsigned long event, void *unused)
{
	unsigned int timeout = 0, count;
	int i;

	apic_scu_panic_dump();

	do {
		mdelay(SCU_PANIC_DUMP_TOUT);
		read_scu_trace_hdr(&trace_hdr);
	} while (trace_hdr.magic != TRACE_MAGIC &&
		 timeout++ < SCU_PANIC_DUMP_RECHECK);

	if (timeout > SCU_PANIC_DUMP_RECHECK) {
		pr_info("Waiting for trace from SCU timed out!\n");
		goto out;
	}

	pr_info("SCU trace on Kernel panic:\n");

	count = scu_trace_buffer_size / sizeof(u32);
	for (i = 0; i < count; i += DWORDS_PER_LINE) {
		/* EW111:0xdeadcafe EW112:0xdeadcafe \0 */
		char dword_line[DWORDS_PER_LINE * 17 + 1] = {0};
		/* abcdefgh\0 */
		char ascii_line[DWORDS_PER_LINE * sizeof(u32) + 1] = {0};
		int ascii_offset = 0, dword_offset = 0, j;

		for (j = 0; i + j < count && j < DWORDS_PER_LINE; j++) {
			int k;
			u32 dword = readl(sram_trace_buf + (i + j) *
					  sizeof(u32));
			char *c = (char *) &dword;

			dword_offset = sprintf(dword_line + dword_offset,
					       "EW%d:0x%08x ", i + j, dword);
			for (k = 0; k < sizeof(dword); k++)
				if (isascii(*(c + k)) && isalnum(*(c + k)) &&
				    *(c + k) != 0)
					ascii_line[ascii_offset++] = *(c + k);
				else
					ascii_line[ascii_offset++] = '.';
		}
		ascii_line[ascii_offset++] = '\0';

		pr_info("%s %s\n", dword_line, ascii_line);
	}

out:
	return 0;
}

#ifdef CONFIG_ATOM_SOC_POWER
static void __iomem *ia_trace_buf;
static void intel_fw_logging_report_nc_pwr(u32 value, int reg_type)
{
	struct ia_trace_t *ia_trace = ia_trace_buf;

	switch (reg_type) {
	case APM_REG_TYPE:
		ia_trace->apm_cmd[1] = ia_trace->apm_cmd[0];
		ia_trace->apm_cmd[0] = value;
		break;
	case OSPM_REG_TYPE:
		ia_trace->ospm_pm_ssc[1] = ia_trace->ospm_pm_ssc[0];
		ia_trace->ospm_pm_ssc[0] = value;
		break;
	default:
		pr_err("Invalid reg type!\n");
		break;
	}
}

static int intel_fw_logging_start_nc_pwr_reporting(void)
{
	u32 buffer;

	if (scu_trace_buffer_size <  sizeof(struct ia_trace_t)) {
		pr_warn("Sram_buf_sz is smaller than expected\n");
		return 0;
	}

	buffer = intel_scu_ipc_get_scu_trace_buffer();
	buffer += scu_trace_buffer_size - sizeof(struct ia_trace_t);
	ia_trace_buf = ioremap_nocache(buffer, sizeof(struct ia_trace_t));
	if (!ia_trace_buf) {
		pr_err("Failed to map ia trace buffer\n");
		return -ENOMEM;
	}

	nc_report_power_state =  intel_fw_logging_report_nc_pwr;

	return 0;
}

static void intel_fw_logging_stop_nc_pwr_reporting(void)
{
	iounmap(ia_trace_buf);
	nc_report_power_state = NULL;
}

#else /* !CONFIG_ATOM_SOC_POWER */

static int intel_fw_logging_start_nc_pwr_reporting(void)
{
	return 0;
}

static void intel_fw_logging_stop_nc_pwr_reporting(void)
{
}

#endif /* CONFIG_ATOM_SOC_POWER */

static struct notifier_block fw_logging_panic_notifier = {
	.notifier_call	= intel_fw_logging_panic_handler,
	.next		= NULL,
	.priority	= INT_MAX
};

static int intel_fw_logging_probe(struct platform_device *pdev)
{
	int err = 0;

	if (!sram_trace_buf) {
		pr_err("No sram trace buffer available, skip SCU tracing init\n");
		err = -ENODEV;
		goto err1;
	}

	err = atomic_notifier_chain_register(
		&panic_notifier_list,
		&fw_logging_panic_notifier);
	if (err) {
		pr_err("Failed to register notifier!\n");
		goto err1;
	}

	err = intel_fw_logging_start_nc_pwr_reporting();
	if (err) {
		pr_err("Failed to start nc power reporting!\n");
		goto err2;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		pr_info("No irq available, SCU tracing not available\n");
		err = irq;
		goto err3;
	}

	err = request_threaded_irq(irq, fw_logging_irq,
				   fw_logging_irq_thread,
				   IRQF_ONESHOT, "fw_logging",
				   &pdev->dev);
	if (err) {
		pr_err("Requesting irq for logging trace failed\n");
		goto err3;
	}

	if (!disable_scu_tracing)
		enable_irq(irq);

	return err;

err3:
	intel_fw_logging_stop_nc_pwr_reporting();
err2:
	atomic_notifier_chain_unregister(&panic_notifier_list,
					 &fw_logging_panic_notifier);
err1:
	return err;
}

static int intel_fw_logging_remove(struct platform_device *pdev)
{
	free_irq(irq, &pdev->dev);
	free_irq(recoverable_irq, &pdev->dev);
	intel_fw_logging_stop_nc_pwr_reporting();
	return atomic_notifier_chain_unregister(&panic_notifier_list,
						&fw_logging_panic_notifier);
}

#ifdef CONFIG_PM
static int intel_fw_logging_suspend(struct platform_device *dev,
					pm_message_t state)
{
	rpmsg_send_simple_command(fw_logging_instance,
					IPCMSG_SCULOG_CTRL,
					IPC_CMD_SCU_LOG_SUSPEND);
	return 0;
}

static int intel_fw_logging_resume(struct platform_device *dev)
{
	rpmsg_send_simple_command(fw_logging_instance,
					IPCMSG_SCULOG_CTRL,
					IPC_CMD_SCU_LOG_RESUME);
	return 0;
}
#endif

static const struct platform_device_id intel_fw_logging_table[] = {
	{"scuLog", 1 },
};

static struct platform_driver intel_fw_logging_driver = {
	.driver = {
		.name = "scuLog",
		.owner = THIS_MODULE,
		},
	.probe = intel_fw_logging_probe,
	.remove = intel_fw_logging_remove,
	.id_table = intel_fw_logging_table,
	.suspend = intel_fw_logging_suspend,
	.resume = intel_fw_logging_resume,
};

static int intel_fw_logging_init(void)
{
	u32 scu_trace_buffer_addr;
	int ioapic, err = 0;
	struct io_apic_irq_attr irq_attr;

	oshob_base = get_oshob_addr();
	if (oshob_base == NULL) {
		pr_err("Failed to get OSHOB address\n");
		err = -EINVAL;
		goto err0;
	}

	scu_trace_buffer_addr = intel_scu_ipc_get_scu_trace_buffer();
	if (scu_trace_buffer_addr &&
	    (scu_trace_buffer_addr >= LOWEST_PHYS_SRAM_ADDRESS)) {
		/*
		 * Calculate size of SCU extra trace buffer. Size of the buffer
		 * is given by SCU. Make sanity check in case of incorrect data.
		 */
		scu_trace_buffer_size =
			intel_scu_ipc_get_scu_trace_buffer_size();
		if (scu_trace_buffer_size > MAX_SCU_EXTRA_DUMP_SIZE) {
			pr_err("Failed to get scu trace buffer size\n");
			err = -ENODEV;
			goto err1;
		}
		/* Looks that we have valid buffer and size. */
		sram_trace_buf =
			ioremap_nocache(scu_trace_buffer_addr,
					scu_trace_buffer_size);
		if (!sram_trace_buf) {
			pr_err("Failed to map scu trace buffer\n");
			err =  -ENOMEM;
			goto err1;
		}

		scu_trace_buffer = kzalloc(scu_trace_buffer_size, GFP_KERNEL);
		if (!scu_trace_buffer) {
			pr_err("Failed to allocate memory for trace buffer\n");
			err = -ENOMEM;
			goto err2;
		}
	} else {
		pr_info("No extended trace buffer available\n");
	}

	fabric_err_buf1 = oshob_base +
		intel_scu_ipc_get_fabricerror_buf1_offset();

	if (fabric_err_buf1 == oshob_base) {
		pr_err("OSHOB Fabric error buf1 offset NULL\n");
		goto err3;
	}

	fabric_err_buf2 = oshob_base +
		intel_scu_ipc_get_fabricerror_buf2_offset();

	if (fabric_err_buf2 == oshob_base) {
		/* Fabric buffer buf2 not available on all plaforms. */
		pr_warn("OSHOB Fabric error buf2 not present (not available on all platforms)\n");
	}

	/* Check and report existing error logs */
	err = fw_logging_crash_on_boot();
	if (err) {
		pr_err("Logging SCU errors stored in SRAM failed\n");
		goto err3;
	}

	if (USE_LEGACY()) {
		ipanic_faberr_recoverable = 0;
		goto non_recover;
	}

	ioapic = mp_find_ioapic(RECOVERABLE_FABERR_INT);
	if (ioapic < 0) {
		pr_err("Finding ioapic for recoverable fabric error interrupt failed\n");
		goto err3;
	}

	irq_attr.ioapic = ioapic;
	irq_attr.ioapic_pin = RECOVERABLE_FABERR_INT;
	irq_attr.trigger = 1;
	irq_attr.polarity = 0; /* Active High */
	io_apic_set_pci_routing(NULL, RECOVERABLE_FABERR_INT, &irq_attr);

	recoverable_irq = RECOVERABLE_FABERR_INT;
	err = request_threaded_irq(RECOVERABLE_FABERR_INT,
				   recoverable_faberror_irq,
				   recoverable_faberror_thread,
				   IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
				   "faberr_int",
				   0);
	if (err) {
		pr_err("Requesting irq for recoverable fabric error failed\n");
		goto err3;
	}

	/* Create a permanent sysfs for hosting recoverable error log */
#ifdef CONFIG_PROC_FS
	ipanic_faberr_recoverable = proc_create("ipanic_fabric_recv_err",
						S_IFREG | S_IRUGO, NULL,
						&ipanic_fab_recoverable_fops);
	if (ipanic_faberr_recoverable == 0) {
		pr_err("Fail creating procfile ipanic_fabric_recv_err for recoverable fabric err\n");
		err = -ENODEV;
		goto err3;
	}

#endif /* CONFIG_PROC_FS */

non_recover:
	/* Clear fabric error region inside OSHOB if neccessary */
	rpmsg_send_simple_command(fw_logging_instance,
				  IPCMSG_CLEAR_FABERROR, 0);

	err = platform_driver_register(&intel_fw_logging_driver);
	if (err) {
#ifdef CONFIG_PROC_FS
		if (ipanic_faberr_recoverable) {
			remove_proc_entry("ipanic_fabric_recv_err", NULL);
			ipanic_faberr_recoverable = 0;
		}
#endif /* CONFIG_PROC_FS */
		pr_err("Failed to register platform driver\n");
		goto err3;
	}
	return err;
err3:
	kfree(scu_trace_buffer);
err2:
	iounmap(sram_trace_buf);
err1:
	iounmap(oshob_base);
err0:
	return err;
}

static void intel_fw_logging_exit(void)
{
	platform_driver_unregister(&intel_fw_logging_driver);
	kfree(scu_trace_buffer);

	iounmap(oshob_base);
	iounmap(sram_trace_buf);

#ifdef CONFIG_PROC_FS
	if (ipanic_faberr)
		remove_proc_entry("ipanic_fabric_err", NULL);

	if (offline_scu_log)
		remove_proc_entry("offline_scu_log", NULL);

	if (ipanic_faberr_recoverable)
		remove_proc_entry("ipanic_fabric_recv_err", NULL);
#endif /* CONFIG_PROC_FS */

	kfree(log_buffer);
}

static int fw_logging_rpmsg_probe(struct rpmsg_channel *rpdev)
{
	int ret = 0;

	if (rpdev == NULL) {
		pr_err("fw_logging rpmsg channel not created\n");
		ret = -ENODEV;
		goto out;
	}

	dev_info(&rpdev->dev, "Probed fw_logging rpmsg device\n");

	/* Allocate rpmsg instance for fw_logging*/
	ret = alloc_rpmsg_instance(rpdev, &fw_logging_instance);
	if (!fw_logging_instance) {
		dev_err(&rpdev->dev, "kzalloc fw_logging instance failed\n");
		goto out;
	}
	/* Initialize rpmsg instance */
	init_rpmsg_instance(fw_logging_instance);

	/* Init scu fw_logging */
	ret = intel_fw_logging_init();

	if (ret)
		free_rpmsg_instance(rpdev, &fw_logging_instance);
out:
	return ret;
}

static void fw_logging_rpmsg_remove(struct rpmsg_channel *rpdev)
{
	intel_fw_logging_exit();
	free_rpmsg_instance(rpdev, &fw_logging_instance);
	dev_info(&rpdev->dev, "Removed fw_logging rpmsg device\n");
}

static void fw_logging_rpmsg_cb(struct rpmsg_channel *rpdev, void *data,
					int len, void *priv, u32 src)
{
	dev_warn(&rpdev->dev, "unexpected, message\n");

	print_hex_dump(KERN_DEBUG, __func__, DUMP_PREFIX_NONE, 16, 1,
		       data, len,  true);
}

static struct rpmsg_device_id fw_logging_rpmsg_id_table[] = {
	{ .name	= "rpmsg_fw_logging" },
	{ },
};
MODULE_DEVICE_TABLE(rpmsg, fw_logging_rpmsg_id_table);

static struct rpmsg_driver fw_logging_rpmsg = {
	.drv.name	= KBUILD_MODNAME,
	.drv.owner	= THIS_MODULE,
	.id_table	= fw_logging_rpmsg_id_table,
	.probe		= fw_logging_rpmsg_probe,
	.callback	= fw_logging_rpmsg_cb,
	.remove		= fw_logging_rpmsg_remove,
};

static int __init fw_logging_rpmsg_init(void)
{
	return register_rpmsg_driver(&fw_logging_rpmsg);
}
module_init(fw_logging_rpmsg_init);

static void __exit fw_logging_rpmsg_exit(void)
{
	return unregister_rpmsg_driver(&fw_logging_rpmsg);
}
module_exit(fw_logging_rpmsg_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Utility driver for getting intel scu fw debug info");
MODULE_AUTHOR("Winson Yung <winson.w.yung@intel.com>");
