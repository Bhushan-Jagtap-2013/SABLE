#include "types.h"
#include "smx.h"
#include "processor.h"
#include "util.h"
#include "config_regs.h"
#include "msr.h"
#include "uuid.h"
#include "config.h"
#include "mle.h"
#include "acmod.h"
#include "page.h"
#include "mtrrs.h"
//#include "hash.h"
#include "intel_tpm.h"
#include "arch-intel/heap.h"
#include "acpi.h"
#include "atomic.h"
#include "keyboard.h"
#include "mutex.h"

#define ACM_MEM_TYPE_UC                 0x0100
#define ACM_MEM_TYPE_WC                 0x0200
#define ACM_MEM_TYPE_WT                 0x1000
#define ACM_MEM_TYPE_WP                 0x2000
#define ACM_MEM_TYPE_WB                 0x4000
 
#define DEF_ACM_MAX_SIZE                0x8000
#define DEF_ACM_VER_MASK                0xffffffff
#define DEF_ACM_VER_SUPPORTED           0x00
#define DEF_ACM_MEM_TYPES               ACM_MEM_TYPE_UC
#define DEF_SENTER_CTRLS                0x00
 
extern acm_hdr_t *g_sinit;

extern int get_ram_ranges(uint64_t *min_lo_ram, uint64_t *max_lo_ram, uint64_t *min_hi_ram, uint64_t *max_hi_ram);

int get_parameters(getsec_parameters_t *params)
{
	unsigned long cr4;
	uint32_t index, eax, ebx, ecx;
	int param_type;


	// START 
	// this code is placed here to avoid SMX not enabled error and should be removed once
	// testing is done for loading SINIT module

	/* testing for chipset support requires enabling SMX on the processor */
	write_cr4(read_cr4() | CR4_SMXE);
	out_info("Remove this code : SMX is enabled");

	// END

	/* sanity check because GETSEC[PARAMETERS] will fail if not set */
	cr4 = read_cr4();
	if ( !(cr4 & CR4_SMXE) ) {
		out_info("SMXE not enabled, can't read parameters");
		return 0;
	}

	memset(params, 0, sizeof(*params));
	params->acm_max_size = DEF_ACM_MAX_SIZE;
	params->acm_mem_types = DEF_ACM_MEM_TYPES;
	params->senter_controls = DEF_SENTER_CTRLS;
	params->proc_based_scrtm = 0;
	params->preserve_mce = 0;

	index = 0;
	do {
		__getsec_parameters(index++, &param_type, &eax, &ebx, &ecx);

		/* the code generated for a 'switch' statement doesn't work in this */
		/* environment, so use if/else blocks instead */

		/* NULL - all reserved */
		if (param_type == 0)
		;
		/* supported ACM versions */
		else if (param_type == 1) {
			if (params->n_versions == MAX_SUPPORTED_ACM_VERSIONS )
				out_info("number of supported ACM version exceeds MAX_SUPPORTED_ACM_VERSIONS");
			else {
				params->acm_versions[params->n_versions].mask = ebx;
				params->acm_versions[params->n_versions].version = ecx;
				params->n_versions++;
			}
		}
		/* max size AC execution area */
		else if (param_type == 2)
			params->acm_max_size = eax & 0xffffffe0;
		/* supported non-AC mem types */
		else if (param_type == 3)
			params->acm_mem_types = eax & 0xffffffe0;
		/* SENTER controls */
		else if (param_type == 4)
			params->senter_controls = (eax & 0x00007fff) >> 8;
		/* TXT extensions support */
		else if (param_type == 5) {
			params->proc_based_scrtm = (eax & 0x00000020) ? 1 : 0;
			params->preserve_mce = (eax & 0x00000040) ? 1 : 0;
		}
		else {
			out_description("unknown GETSEC[PARAMETERS] type", param_type);
			param_type = 0;    /* set so that we break out of the loop */
		}
	} while (param_type != 0);

	if (params->n_versions == 0) {
		params->acm_versions[0].mask = DEF_ACM_VER_MASK;
		params->acm_versions[0].version = DEF_ACM_VER_SUPPORTED;
		params->n_versions = 1;
	}

	return 1;
}
///*
// * txt.c: Intel(r) TXT support functions, including initiating measured
// *        launch, post-launch, AP wakeup, etc.
// *
// * Copyright (c) 2003-2011, Intel Corporation
// * All rights reserved.
// *
// * Redistribution and use in source and binary forms, with or without
// * modification, are permitted provided that the following conditions
// * are met:
// *
// *   * Redistributions of source code must retain the above copyright
// *     notice, this list of conditions and the following disclaimer.
// *   * Redistributions in binary form must reproduce the above
// *     copyright notice, this list of conditions and the following
// *     disclaimer in the documentation and/or other materials provided
// *     with the distribution.
// *   * Neither the name of the Intel Corporation nor the names of its
// *     contributors may be used to endorse or promote products derived
// *     from this software without specific prior written permission.
// *
// * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
// * OF THE POSSIBILITY OF SUCH DAMAGE.
// *
// */
//
//#include <config.h>
//#include <stdbool.h>
//#include <types.h>
//#include <tb_error.h>
//#include <msr.h>
//#include <compiler.h>
//#include <string.h>
//#include <misc.h>
//#include <page.h>
//#include <processor.h>
//#include <printk.h>
//#include <atomic.h>
//#include <mutex.h>
//#include <tpm.h>
//#include <uuid.h>
//#include <loader.h>
//#include <e820.h>
//#include <tboot.h>
//#include <mle.h>
//#include <hash.h>
//#include <cmdline.h>
//#include <acpi.h>
//#include <txt/txt.h>
//#include <txt/config_regs.h>
//#include <txt/mtrrs.h>
//#include <txt/heap.h>
//#include <txt/acmod.h>
//#include <txt/smx.h>
//#include <txt/verify.h>
//#include <txt/vmcs.h>
//#include <io.h>

/* counter timeout for waiting for all APs to enter wait-for-sipi */
#define AP_WFS_TIMEOUT     0x10000000

__data struct acpi_rsdp g_rsdp;
extern char __start[];		/* start of module */
extern char _end[];		/* end of module */
extern char _mle_start[];	/* start of text section */
extern char _mle_end[];		/* end of text section */
extern char _post_launch_entry[]; /* entry point post SENTER, in boot.S */
extern char _txt_wakeup[];        /* RLP join address for GETSEC[WAKEUP] */



/*
 * Bhushan : I think g_cmdline is not needed for sable but 
 * keeping it as a placeholder. we can remove it later
 * copy of original command line
 * part of tboot measurement (hence in .text section)
 */

#define CMDLINE_SIZE	512
extern char g_cmdline[CMDLINE_SIZE];



//extern long s3_flag;
//
//extern struct mutex ap_lock;
//
///* MLE/kernel shared data page (in boot.S) */
//extern tboot_shared_t _tboot_shared;
//extern void apply_policy(tb_error_t error);
//extern void cpu_wakeup(uint32_t cpuid, uint32_t sipi_vec);
//extern void print_event(const tpm12_pcr_event_t *evt);
//extern void print_event_2(void *evt, uint16_t alg);
//
//
///*
// * this is the structure whose addr we'll put in TXT heap
// * it needs to be within the MLE pages, so force it to the .text section
// */

/* 
 * Bhushan : 
 * we need to replace &_skinit with &_post_launch_entry once its implementation ready
 */

static __text const mle_hdr_t g_mle_hdr = {
	uuid			:	MLE_HDR_UUID,
	length			:	sizeof(mle_hdr_t),
	version			:	MLE_HDR_VER,
	entry_point		:	(uint32_t)&_post_launch_entry - TBOOT_START,
	first_valid_page	:	0,
	mle_start_off		:	(uint32_t)&_mle_start - TBOOT_BASE_ADDR,
	mle_end_off		:	(uint32_t)&_mle_end - TBOOT_BASE_ADDR,
	capabilities		:	{ MLE_HDR_CAPS },
	cmdline_start_off	:	(uint32_t)g_cmdline - TBOOT_BASE_ADDR,
	cmdline_end_off		:	(uint32_t)g_cmdline + CMDLINE_SIZE - 1 - TBOOT_BASE_ADDR,
};


///*
// * counts of APs going into wait-for-sipi
// */
///* count of APs in WAIT-FOR-SIPI */
atomic_t ap_wfs_count;
/*
 static inline void print_uuid(const uuid_t *uuid)
 {
	out_info("UUID");
	out_description("uuid->data1", uuid->data1);
	out_description("uuid->data2", (uint32_t)uuid->data2);
	out_description("uuid->data3", (uint32_t)uuid->data3);
	out_description("uuid->data4", (uint32_t)uuid->data4);
	out_description("uuid->data5[0]", uuid->data5[0]);
	out_description("uuid->data5[1]", (uint32_t)uuid->data5[1]);
	out_description("uuid->data5[2]", (uint32_t)uuid->data5[2]);
	out_description("uuid->data5[3]", (uint32_t)uuid->data5[3]);
	out_description("uuid->data5[4]", (uint32_t)uuid->data5[4]);
	out_description("uuid->data5[5]", (uint32_t)uuid->data5[5]);
 }
*/

static void print_file_info(void)
{
	out_info("file addresses:");
	out_description("&_start=", (unsigned int)&__start);
	out_description("&_end=", (unsigned int)&_end);
	out_description("&_mle_start=", (unsigned int)&_mle_start);
	out_description("&_mle_end=", (unsigned int)&_mle_end);
	out_description("&_post_launch_entry=", (unsigned int)&_post_launch_entry);

	/* 
	 * Bhushan
	 * _skinit is just place holder to avoid build failure
	 * replace it with &_post_launch_entry when required
	 */
	out_description("&_txt_wakeup=", (unsigned int)&_txt_wakeup);
	out_description("&g_mle_hdr=", (unsigned int)&g_mle_hdr);
	wait(4000);
}

static void print_mle_hdr(const mle_hdr_t *mle_hdr)
{
	out_info("MLE header:");
	out_info("uuid=");
	print_uuid(&mle_hdr->uuid); 
	out_description("length :", mle_hdr->length);
	out_description("version :", mle_hdr->version);
	out_description("entry_point :", mle_hdr->entry_point);
	out_description("first_valid_page :", mle_hdr->first_valid_page);
	out_description("mle_start_off :", mle_hdr->mle_start_off);
	out_description("mle_end_off :", mle_hdr->mle_end_off);
	print_txt_caps(mle_hdr->capabilities);
}

/*
 * build_mle_pagetable()
 */

/* page dir/table entry is phys addr + P + R/W + PWT */
#define MAKE_PDTE(addr)  (((uint64_t)(unsigned long)(addr) & PAGE_MASK) | 0x01)

/* we assume/know that our image is <2MB and thus fits w/in a single */
/* PT (512*4KB = 2MB) and thus fixed to 1 pg dir ptr and 1 pgdir and */
/* 1 ptable = 3 pages and just 1 loop loop for ptable MLE page table */
/* can only contain 4k pages */

static __mlept uint8_t g_mle_pt[3 * PAGE_SIZE];  
/* pgdir ptr + pgdir + ptab = 3 */

static void *build_mle_pagetable(uint32_t mle_start, uint32_t mle_size)
{
	void *ptab_base;
	uint32_t ptab_size, mle_off;
	void *pg_dir_ptr_tab, *pg_dir, *pg_tab;
	uint64_t *pte;

	out_info("MLE information : Creating pages for MLE");
	out_description("Start", mle_start);
	out_description("End", mle_start + mle_size);
	out_description("Size", mle_size);

	if (mle_size > 512 * PAGE_SIZE ) {
		out_info("MLE size too big for single page table");
		return NULL;
	}

	/* should start on page boundary */
	if (mle_start & ~PAGE_MASK) {
		out_info("MLE start is not page-aligned");
		return NULL;
	}

	/* place ptab_base below MLE */
	ptab_size = sizeof(g_mle_pt);
	ptab_base = &g_mle_pt;
	memset(ptab_base, 0, ptab_size);

	out_info("Page table information");
	out_description("ptab_size=", ptab_size);
	out_description("ptab_base=", (unsigned int)ptab_base);

	pg_dir_ptr_tab	= ptab_base;
	pg_dir		= pg_dir_ptr_tab + PAGE_SIZE;
	pg_tab		= pg_dir + PAGE_SIZE;


	/* only use first entry in page dir ptr table */
	*(uint64_t *)pg_dir_ptr_tab = MAKE_PDTE(pg_dir);

	/* only use first entry in page dir */
	*(uint64_t *)pg_dir = MAKE_PDTE(pg_tab);

	pte = pg_tab;
	mle_off = 0;
	do {
		*pte = MAKE_PDTE(mle_start + mle_off);
		pte++;
		mle_off += PAGE_SIZE;
	} while (mle_off < mle_size);

	return ptab_base;
}


static __data event_log_container_t *g_elog = NULL;
//static __data heap_event_log_ptr_elt2_t *g_elog_2 = NULL;
//static __data heap_event_log_ptr_elt2_1_t *g_elog_2_1 = NULL;

/* should be called after os_mle_data initialized */
static void *init_event_log(void)
{
	os_mle_data_t *os_mle_data = get_os_mle_data_start(get_txt_heap());
	g_elog = (event_log_container_t *)&os_mle_data->event_log_buffer;

	memcpy((void *)g_elog->signature, EVTLOG_SIGNATURE, sizeof(g_elog->signature));
	g_elog->container_ver_major = EVTLOG_CNTNR_MAJOR_VER;
	g_elog->container_ver_minor = EVTLOG_CNTNR_MINOR_VER;
	g_elog->pcr_event_ver_major = EVTLOG_EVT_MAJOR_VER;
	g_elog->pcr_event_ver_minor = EVTLOG_EVT_MINOR_VER;
	g_elog->size = sizeof(os_mle_data->event_log_buffer);
	g_elog->pcr_events_offset = sizeof(*g_elog);
	g_elog->next_event_offset = sizeof(*g_elog);

	return (void *)g_elog;
}
//
///* initialize TCG compliant TPM 2.0 event log descriptor */
//static void init_evtlog_desc_1(heap_event_log_ptr_elt2_1_t *evt_log)
//{
//    os_mle_data_t *os_mle_data = get_os_mle_data_start(get_txt_heap());
//   
//    evt_log->phys_addr = (uint64_t)(unsigned long)(os_mle_data->event_log_buffer);
//    evt_log->allcoated_event_container_size = 2*PAGE_SIZE;
//    evt_log->first_record_offset = 0;
//    evt_log->next_record_offset = 0;
//    printk(TBOOT_DETA"TCG compliant TPM 2.0 event log descriptor:\n");
//    printk(TBOOT_DETA"\t phys_addr = 0x%LX\n",  evt_log->phys_addr);
//    printk(TBOOT_DETA"\t allcoated_event_container_size = 0x%x \n", evt_log->allcoated_event_container_size);
//    printk(TBOOT_DETA"\t first_record_offset = 0x%x \n", evt_log->first_record_offset);
//    printk(TBOOT_DETA"\t next_record_offset = 0x%x \n", evt_log->next_record_offset);
//}
//
//static void init_evtlog_desc(heap_event_log_ptr_elt2_t *evt_log)
//{
//    unsigned int i;
//    os_mle_data_t *os_mle_data = get_os_mle_data_start(get_txt_heap());
//    switch (g_tpm->extpol) {
//    case TB_EXTPOL_AGILE:
//        for (i=0; i<evt_log->count; i++) {
//            evt_log->event_log_descr[i].alg = g_tpm->algs_banks[i];
//            evt_log->event_log_descr[i].phys_addr =
//                    (uint64_t)(unsigned long)(os_mle_data->event_log_buffer + i*4096);
//            evt_log->event_log_descr[i].size = 4096;
//            evt_log->event_log_descr[i].pcr_events_offset = 0;
//            evt_log->event_log_descr[i].next_event_offset = 0;
//        }
//        break;
//    case TB_EXTPOL_EMBEDDED:
//        for (i=0; i<evt_log->count; i++) {
//            evt_log->event_log_descr[i].alg = g_tpm->algs[i];
//            evt_log->event_log_descr[i].phys_addr =
//                    (uint64_t)(unsigned long)(os_mle_data->event_log_buffer + i*4096);
//            evt_log->event_log_descr[i].size = 4096;
//            evt_log->event_log_descr[i].pcr_events_offset = 0;
//            evt_log->event_log_descr[i].next_event_offset = 0;
//        }
//        break;
//    case TB_EXTPOL_FIXED:
//        evt_log->event_log_descr[0].alg = g_tpm->cur_alg;
//        evt_log->event_log_descr[0].phys_addr =
//                    (uint64_t)(unsigned long)os_mle_data->event_log_buffer;
//        evt_log->event_log_descr[0].size = 4096;
//        evt_log->event_log_descr[0].pcr_events_offset = 0;
//        evt_log->event_log_descr[0].next_event_offset = 0;
//        break;
//    default:
//        return;
//    }
//}
//
static void init_os_sinit_ext_data(heap_ext_data_element_t* elts)
{
//	txt_caps_t			sinit_caps;
	heap_event_log_ptr_elt_t*	evt_log;
	heap_ext_data_element_t*	elt = elts;

	if (g_tpm->major == TPM12_VER_MAJOR) {
		evt_log = (heap_event_log_ptr_elt_t *)elt->data;
		evt_log->event_log_phys_addr = (uint64_t)(unsigned long)init_event_log();
		elt->type = HEAP_EXTDATA_TYPE_TPM_EVENT_LOG_PTR;
		elt->size = sizeof(*elt) + sizeof(*evt_log);
	} 
	else if ( g_tpm->major == TPM20_VER_MAJOR ) {
		out_info("We dont expect to be here: init_os_sinit_ext_data");
		while(1);
//		if (g_sinit != NULL) {
//			sinit_caps = get_sinit_capabilities(g_sinit);
//		}
//		else 
//			return;
//		if (sinit_caps.tcg_event_log_format) {
//			g_elog_2_1 = (heap_event_log_ptr_elt2_1_t *)elt->data;
//			init_evtlog_desc_1(g_elog_2_1);
//			elt->type = HEAP_EXTDATA_TYPE_TPM_EVENT_LOG_PTR_2_1;
//			elt->size = sizeof(*elt) + sizeof(heap_event_log_ptr_elt2_1_t);
//			printk(TBOOT_DETA"heap_ext_data_element TYPE = %d \n", elt->type);
//			printk(TBOOT_DETA"heap_ext_data_element SIZE = %d \n", elt->size);
//		} else {
//			g_elog_2 = (heap_event_log_ptr_elt2_t *)elt->data;
//			if (g_tpm->extpol == TB_EXTPOL_AGILE)
//				g_elog_2->count = g_tpm->banks;
//			else 
//				if ( g_tpm->extpol == TB_EXTPOL_EMBEDDED )
//					g_elog_2->count = g_tpm->alg_count;
//				else
//					g_elog_2->count = 1;
//			init_evtlog_desc(g_elog_2);
//			elt->type = HEAP_EXTDATA_TYPE_TPM_EVENT_LOG_PTR_2;
//			elt->size = sizeof(*elt) + sizeof(u32) +
//			g_elog_2->count * sizeof(heap_event_log_descr_t);
//			printk(TBOOT_DETA"INTEL TXT LOG elt SIZE = %d \n", elt->size);
//		}
	}
	elt = (void *)elt + elt->size;
	elt->type = HEAP_EXTDATA_TYPE_END;
	elt->size = sizeof(*elt);
}

//bool evtlog_append_tpm12(uint8_t pcr, tb_hash_t *hash, uint32_t type)
//{
//    if ( g_elog == NULL )
//        return true;
//
//    tpm12_pcr_event_t *next = (tpm12_pcr_event_t *)
//                              ((void*)g_elog + g_elog->next_event_offset);
//    
//    if ( g_elog->next_event_offset + sizeof(*next) > g_elog->size )
//        return false;
//
//    next->pcr_index = pcr;
//    next->type = type;
//    memcpy(next->digest, hash, sizeof(next->digest));
//    next->data_size = 0;
//
//    g_elog->next_event_offset += sizeof(*next) + next->data_size;
//
//    print_event(next);
//    return true;
//}
//
//void dump_event_2(void)
//{
//    heap_event_log_descr_t *log_descr;
//
//    for ( unsigned int i=0; i<g_elog_2->count; i++ ) {
//        log_descr = &g_elog_2->event_log_descr[i];
//        printk(TBOOT_DETA"\t\t\t Log Descrption:\n");
//        printk(TBOOT_DETA"\t\t\t             Alg: %u\n", log_descr->alg);
//        printk(TBOOT_DETA"\t\t\t            Size: %u\n", log_descr->size);
//        printk(TBOOT_DETA"\t\t\t    EventsOffset: [%u,%u)\n",
//                log_descr->pcr_events_offset,
//                log_descr->next_event_offset);
//
//        uint32_t hash_size, data_size; 
//        hash_size = get_hash_size(log_descr->alg); 
//        if ( hash_size == 0 )
//            return;
//
//        void *curr, *next;
//        *((u64 *)(&curr)) = log_descr->phys_addr +
//                log_descr->pcr_events_offset;
//        *((u64 *)(&next)) = log_descr->phys_addr +
//                log_descr->next_event_offset;
//
//        if ( log_descr->alg != TB_HALG_SHA1 ) {
//            print_event_2(curr, TB_HALG_SHA1);
//            curr += sizeof(tpm12_pcr_event_t) + sizeof(tpm20_log_descr_t);
//        }
//
//        while ( curr < next ) {
//            print_event_2(curr, log_descr->alg);
//            data_size = *(uint32_t *)(curr + 2*sizeof(uint32_t) + hash_size);
//            curr += 3*sizeof(uint32_t) + hash_size + data_size;
//        }
//    }
//}
//
//bool evtlog_append_tpm20(uint8_t pcr, uint16_t alg, tb_hash_t *hash, uint32_t type)
//{
//    heap_event_log_descr_t *cur_desc = NULL;
//    uint32_t hash_size; 
//    void *cur, *next;
//
//    for ( unsigned int i=0; i<g_elog_2->count; i++ ) {
//        if ( g_elog_2->event_log_descr[i].alg == alg ) {
//            cur_desc = &g_elog_2->event_log_descr[i];
//            break;
//        }
//    }
//    if ( !cur_desc )
//        return false;
//
//    hash_size = get_hash_size(alg); 
//    if ( hash_size == 0 )
//        return false;
//
//    if ( cur_desc->next_event_offset + 32 > cur_desc->size )
//        return false;
//
//    cur = next = (void *)(unsigned long)cur_desc->phys_addr +
//                     cur_desc->next_event_offset;
//    *((u32 *)next) = pcr;
//    next += sizeof(u32);
//    *((u32 *)next) = type;
//    next += sizeof(u32);
//    memcpy((uint8_t *)next, hash, hash_size);
//    next += hash_size;
//    *((u32 *)next) = 0;
//    cur_desc->next_event_offset += 3*sizeof(uint32_t) + hash_size; 
//
//    print_event_2(cur, alg);
//    return true;
//}
//
//bool evtlog_append(uint8_t pcr, hash_list_t *hl, uint32_t type)
//{
//    switch (g_tpm->major) {
//    case TPM12_VER_MAJOR:
//        evtlog_append_tpm12(pcr, &hl->entries[0].hash, type);
//        break;
//    case TPM20_VER_MAJOR:
//        for (unsigned int i=0; i<hl->count; i++)
//            evtlog_append_tpm20(pcr, hl->entries[i].alg,
//                    &hl->entries[i].hash, type);
//        break;
//    default:
//        return false;
//    }
//
//    return true;
//}
//
__data uint32_t g_using_da = 0;
//__data acm_hdr_t *g_sinit = 0;
//

/*
 * sets up TXT heap
 */

void set_vtd_pmrs(os_sinit_data_t *os_sinit_data, uint64_t min_lo_ram, uint64_t max_lo_ram, uint64_t min_hi_ram, uint64_t max_hi_ram)
{    
	out_description64("min_lo_ram", min_lo_ram);
	out_description64("max_lo_ram", max_lo_ram);
	out_description64("min_hi_ram", min_hi_ram);
	out_description64("max_hi_ram", max_hi_ram);
	wait(6000);

	/*
	 * base must be 2M-aligned and size must be multiple of 2M
	 * (so round bases and sizes down--rounding size up might conflict
	 * with a BIOS-reserved region and cause problems; in practice, rounding
	 * base down doesn't)
	 * we want to protect all of usable mem so that any kernel allocations
	 * before VT-d remapping is enabled are protected
	 */

	min_lo_ram &= ~0x1fffffULL;
	uint64_t lo_size = (max_lo_ram - min_lo_ram) & ~0x1fffffULL;
	os_sinit_data->vtd_pmr_lo_base = min_lo_ram;
	os_sinit_data->vtd_pmr_lo_size = lo_size;

	min_hi_ram &= ~0x1fffffULL;
	uint64_t hi_size = (max_hi_ram - min_hi_ram) & ~0x1fffffULL;
	os_sinit_data->vtd_pmr_hi_base = min_hi_ram;
	os_sinit_data->vtd_pmr_hi_size = hi_size;
}

static txt_heap_t *init_txt_heap(void *ptab_base, acm_hdr_t *sinit)
{
	txt_heap_t *txt_heap;
	uint64_t *size;

	txt_heap = get_txt_heap();

	/*
	 * BIOS data already setup by BIOS
	*/
	if (!verify_txt_heap(txt_heap, 1)) {
		return NULL;
		out_info("EORROR : bios_data init has some problem");
		wait(3000);
	}

	/*
	 * OS/loader to MLE data
	*/

	out_info("bios_data init is done");
	wait(4000);

	os_mle_data_t *os_mle_data = get_os_mle_data_start(txt_heap);
	size = (uint64_t *)((uint32_t)os_mle_data - sizeof(uint64_t));
	*size = sizeof(*os_mle_data) + sizeof(uint64_t);
	memset(os_mle_data, 0, sizeof(*os_mle_data));
	os_mle_data->version = 3;

	/*
	 * Bhushan: os_mle_data is vendor specific.
	 * we can store/initialize whatever value we want.
	 * Make sure to calculate value accordinly.
	 */

	// os_mle_data->lctx_addr = lctx->addr;
	os_mle_data->lctx_addr = NULL;
	os_mle_data->saved_misc_enable_msr = rdmsr(MSR_IA32_MISC_ENABLE);

	/*
	 * OS/loader to SINIT data
	 */
	/* check sinit supported os_sinit_data version */
	uint32_t version = get_supported_os_sinit_data_ver(sinit);
	if (version < MIN_OS_SINIT_DATA_VER) {
		out_description("ERROR: unsupported OS to SINIT data version in sinit", version);
		return NULL;
	}
	if (version > MAX_OS_SINIT_DATA_VER) {
		version = MAX_OS_SINIT_DATA_VER;
	}
	out_description("OS to SINIT data version in sinit", version);

	os_sinit_data_t *os_sinit_data = get_os_sinit_data_start(txt_heap);
	size = (uint64_t *)((uint32_t)os_sinit_data - sizeof(uint64_t));
	*size = calc_os_sinit_data_size(version);
	memset(os_sinit_data, 0, *size);
	os_sinit_data->version = version;

	/* this is phys addr */
	os_sinit_data->mle_ptab = (uint64_t)(unsigned long)ptab_base;
	os_sinit_data->mle_size = g_mle_hdr.mle_end_off - g_mle_hdr.mle_start_off;
	/* this is linear addr (offset from MLE base) of mle header */
	os_sinit_data->mle_hdr_base = (uint64_t)(unsigned long)&g_mle_hdr - (uint64_t)(unsigned long)&_mle_start;
	/* VT-d PMRs */
	uint64_t min_lo_ram, max_lo_ram, min_hi_ram, max_hi_ram;

	if (!get_ram_ranges(&min_lo_ram, &max_lo_ram, &min_hi_ram, &max_hi_ram)) {
		return NULL;
	}

	set_vtd_pmrs(os_sinit_data, min_lo_ram, max_lo_ram, min_hi_ram, max_hi_ram);
//    /* LCP owner policy data */
//    void *lcp_base = NULL;
//    uint32_t lcp_size = 0;

//    if ( find_lcp_module(lctx, &lcp_base, &lcp_size) && lcp_size > 0 ) {
//        /* copy to heap */
//        if ( lcp_size > sizeof(os_mle_data->lcp_po_data) ) {
//            printk(TBOOT_ERR"LCP owner policy data file is too large (%u)\n",
//                   lcp_size);
//            return NULL;
//        }
//        memcpy(os_mle_data->lcp_po_data, lcp_base, lcp_size);
//        os_sinit_data->lcp_po_base = (unsigned long)&os_mle_data->lcp_po_data;
//        os_sinit_data->lcp_po_size = lcp_size;
//    }


	/* capabilities : choose monitor wake mechanism first */
	txt_caps_t sinit_caps = get_sinit_capabilities(sinit);
	txt_caps_t caps_mask = { 0 };
	caps_mask.rlp_wake_getsec = 1;
	caps_mask.rlp_wake_monitor = 1;
	caps_mask.pcr_map_da = 1;
	os_sinit_data->capabilities._raw = MLE_HDR_CAPS & ~caps_mask._raw;
	if (sinit_caps.rlp_wake_monitor)
		os_sinit_data->capabilities.rlp_wake_monitor = 1;
	else if (sinit_caps.rlp_wake_getsec)
		os_sinit_data->capabilities.rlp_wake_getsec = 1;
	else {     /* should have been detected in verify_acmod() */
		out_description("SINIT capabilities are incompatible", sinit_caps._raw);
		return NULL;
	}
   
	if (sinit_caps.tcg_event_log_format) {
		out_description("SINIT ACM supports TCG compliant TPM 2.0 event log format, tcg_event_log_format", sinit_caps.tcg_event_log_format);
		os_sinit_data->capabilities.tcg_event_log_format = 1;
	}

	/* capabilities : require MLE pagetable in ECX on launch */
	/* TODO: when SINIT ready
	 * os_sinit_data->capabilities.ecx_pgtbl = 1;
	 */

	os_sinit_data->capabilities.ecx_pgtbl = 0;
//    if (is_loader_launch_efi(lctx)){
	/* we were launched EFI, set efi_rsdt_ptr */
	struct acpi_rsdp *rsdp = get_rsdp(); //get_rsdp(lctx);
	if (rsdp != NULL){
		if (version < 6){
			/* rsdt */
			/* NOTE: Winston Wang says this doesn't work for v5 */
			os_sinit_data->efi_rsdt_ptr = (uint64_t) rsdp->rsdp1.rsdt;
		} else {
			/* rsdp */
			memcpy((void *)&g_rsdp, rsdp, sizeof(struct acpi_rsdp));
			os_sinit_data->efi_rsdt_ptr = (uint64_t)((uint32_t)&g_rsdp);
		}
	} else {
		/* per discussions--if we don't have an ACPI pointer, die */
		out_info("ERROR: Failed to find RSDP for EFI launch\n");
		while(1);
		return NULL;
	}
//    }

	/* capabilities : choose DA/LG */
	os_sinit_data->capabilities.pcr_map_no_legacy = 1;
	if(sinit_caps.pcr_map_da && 0 ) //&& get_tboot_prefer_da())
		os_sinit_data->capabilities.pcr_map_da = 1;
	else if ( !sinit_caps.pcr_map_no_legacy )
		os_sinit_data->capabilities.pcr_map_no_legacy = 0;
	else if ( sinit_caps.pcr_map_da ) {
		out_info("DA is the only supported PCR mapping by SINIT, use it");
		wait(3000);
		os_sinit_data->capabilities.pcr_map_da = 1;
	} else {
		out_description("SINIT capabilities are incompatible ", sinit_caps._raw);
		return NULL;
	}
	g_using_da = os_sinit_data->capabilities.pcr_map_da;

	/* 
	 * PCR mapping selection MUST be zero in TPM2.0 mode
	 * since D/A mapping is the only supported by TPM2.0
	 */

	if ( g_tpm->major >= TPM20_VER_MAJOR ) {

		/*
		 * Bhushan :  assumption : we know our development environment is TPM 1.2
		 */

		out_info("ERROR: we dont expect to here");
		while(1);
		/*
		os_sinit_data->flags = (g_tpm->extpol == TB_EXTPOL_AGILE) ? 0 : 1;
		os_sinit_data->capabilities.pcr_map_no_legacy = 0;
		os_sinit_data->capabilities.pcr_map_da = 0;
		g_using_da = 1;
		*/
	}   


	/* Event log initialization */
	

	if (os_sinit_data->version >= 6)
		init_os_sinit_ext_data(os_sinit_data->ext_data_elts);


	print_os_sinit_data(os_sinit_data);
	wait(5000);

	/*
	 * SINIT to MLE data will be setup by SINIT
	 */

	return txt_heap;
}

/* lock that protects APs against race conditions on wakeup and shutdown */
struct mutex ap_lock;

static void txt_wakeup_cpus(void)
{
	uint16_t cs;
	mle_join_t mle_join;
	unsigned int ap_wakeup_count;

//	if (!verify_stm(get_apicid()) )
//        apply_policy(TB_ERR_POST_LAUNCH_VERIFICATION);

	/*
	 * enable SMIs on BSP before waking APs (which will enable them on APs)
	 * because some SMM may take immediate SMI and hang if AP gets in first
	 */

	out_info("enabling SMIs on BSP");
	__getsec_smctrl();


	return;			/// BHSUHAN TESTING REMOVE


	atomic_set(&ap_wfs_count, 0);

	/* RLPs will use our GDT and CS */
	extern char gdt[], end_gdt[];
	__asm__ __volatile__ ("mov %%cs, %0\n" : "=r"(cs));

	mle_join.entry_point = (uint32_t)(unsigned long)&_txt_wakeup;
	mle_join.seg_sel = cs;

//	mle_join.gdt_base = (uint32_t)gdt_table; // extra
//	mle_join.gdt_limit = gdt_table_end - gdt_table - 1; // extra

	mle_join.gdt_base = (uint32_t) gdt;
	mle_join.gdt_limit = end_gdt - gdt - 1;

//	WAIT_FOR_INPUT();
	WAIT_FOR_INPUT();
	
	out_description("mle_join.entry_point ", (unsigned int)mle_join.entry_point);
	out_description("mle_join.seg_sel ", mle_join.seg_sel);
	out_description("mle_join.gdt_base ", mle_join.gdt_base);
	out_description("mle_join.gdt_limit ", mle_join.gdt_limit);

	write_priv_config_reg(TXTCR_MLE_JOIN, (uint64_t)(unsigned long)&mle_join);
	
	WAIT_FOR_INPUT();

	mtx_init(&ap_lock);

	txt_heap_t *txt_heap = get_txt_heap();
	sinit_mle_data_t *sinit_mle_data = get_sinit_mle_data_start(txt_heap);
	os_sinit_data_t *os_sinit_data = get_os_sinit_data_start(txt_heap);

	/* choose wakeup mechanism based on capabilities used */
	if (os_sinit_data->capabilities.rlp_wake_monitor) {
		out_info("joining RLPs to MLE with MONITOR wakeup");
		WAIT_FOR_INPUT();
		out_description("rlp_wakeup_addr ", sinit_mle_data->rlp_wakeup_addr);
		WAIT_FOR_INPUT();
		*((uint32_t *)(unsigned long)(sinit_mle_data->rlp_wakeup_addr)) = 0x01;
	}
	else {
		out_info("joining RLPs to MLE with GETSEC[WAKEUP]");
		WAIT_FOR_INPUT();
		__getsec_wakeup();
		out_info("GETSEC[WAKEUP] completed");
	}

	/* assume BIOS isn't lying to us about # CPUs, else some CPUS may not */
	/* have entered wait-for-sipi before we launch *or* we have to wait */
	/* for timeout before launching */
	/* (all TXT-capable CPUs have at least 2 cores) */

	bios_data_t *bios_data = get_bios_data_start(txt_heap);
	ap_wakeup_count = bios_data->num_logical_procs - 1;
	if ( ap_wakeup_count >= NR_CPUS ) {
		out_description("there are too many CPUs ", ap_wakeup_count);
		ap_wakeup_count = NR_CPUS - 1;
	}

	out_description("waiting for all APs to enter wait-for-sipi... count : ", ap_wakeup_count);
	/* wait for all APs that woke up to have entered wait-for-sipi */
//	WAIT_FOR_INPUT();
	uint32_t timeout = AP_WFS_TIMEOUT;
	do {
		if (timeout % 0x8000 == 0)
			out_info(".");
		else
			cpu_relax();
		if (timeout % 0x200000 == 0)
			out_info("\n");
		timeout--;
	} while ((atomic_read(&ap_wfs_count) < ap_wakeup_count) && timeout > 0);
	out_info("\n");
	if (timeout == 0)
		out_info("wait-for-sipi loop timed-out");
	else
		out_info("all APs in wait-for-sipi");
	WAIT_FOR_INPUT();
}

int txt_is_launched(void)
{
	txt_sts_t sts;

	sts._raw = read_pub_config_reg(TXTCR_STS);

	return sts.senter_done_sts;
}

int txt_launch_environment()
{
	void	*mle_ptab_base;
	os_mle_data_t *os_mle_data;
	txt_heap_t *txt_heap;

	/* print some debug info */
	print_file_info();
//	wait(3000);
	print_mle_hdr(&g_mle_hdr);
//	wait(3000);
	/* create MLE page table */
	mle_ptab_base = build_mle_pagetable(g_mle_hdr.mle_start_off + TBOOT_BASE_ADDR, g_mle_hdr.mle_end_off - g_mle_hdr.mle_start_off);
	if (mle_ptab_base == NULL) {
		out_info("Failed to create pages");
//		wait(3000);
		return 0;
	}

	out_info("Initializing Heap .....");
//	wait(3000);

	/* initialize TXT heap */
	txt_heap = init_txt_heap(mle_ptab_base, g_sinit);
	if (txt_heap == NULL) {
		out_info("Failed to initialize heap");
//		wait(3000);
		return 0;
	}

	/* save MTRRs before we alter them for SINIT launch */
	os_mle_data = get_os_mle_data_start(txt_heap);
	save_mtrrs(&(os_mle_data->saved_mtrr_state));

	/* set MTRRs properly for AC module (SINIT) */
	if (!set_mtrrs_for_acmod(g_sinit)) {
		out_info("seting MTRRs for SINIT ACM failed");
		while(1);
		return 0;
	}

	/* deactivate current locality */
	if (g_tpm_family == TPM_IF_20_CRB ) {
		out_info("We dont expect to be here");
		while(1);
//		printk(TBOOT_INFO"Relinquish CRB localility 0 before executing GETSEC[SENTER]...\n");
//		if (!tpm_relinquish_locality_crb(0)){
//			printk(TBOOT_INFO"Relinquish CRB locality 0 failed...\n");
//			apply_policy(TB_ERR_TPM_NOT_READY) ;
//		}
	}

	out_info("executing GETSEC[SENTER]...\n");

	out_description("SINIT BASE BASE :", (unsigned int) g_sinit);
	out_description("SINIT SIZE :", (unsigned int) (g_sinit->size)*4);

	wait(4000);
	__getsec_senter((uint32_t)g_sinit, (g_sinit->size)*4);
	out_info("ERROR--we should not get here!\n");
	return 0;
}

//bool txt_s3_launch_environment(void)
//{
//    /* initial launch's TXT heap data is still in place and assumed valid */
//    /* so don't re-create; this is OK because it was untrusted initially */
//    /* and would be untrusted now */
//	txt_caps_t sinit_caps;
//
//    /* get sinit binary loaded */
//    g_sinit = (acm_hdr_t *)(uint32_t)read_pub_config_reg(TXTCR_SINIT_BASE);
//    if ( g_sinit == NULL ){
//        return false;
//    }
//	/* initialize event log in os_sinit_data, so that events will not */
//	/* repeat when s3 */
//	if ( g_tpm->major == TPM12_VER_MAJOR && g_elog )
//		g_elog = (event_log_container_t *)init_event_log();
//	else 
//		if ( g_tpm->major == TPM20_VER_MAJOR ){
//			sinit_caps = get_sinit_capabilities(g_sinit);		
//			if (sinit_caps.tcg_event_log_format && g_elog_2_1) 
//				init_evtlog_desc_1(g_elog_2_1);
//			if(!sinit_caps.tcg_event_log_format && g_elog_2) 
//				init_evtlog_desc(g_elog_2);
//		}
//
//    /* set MTRRs properly for AC module (SINIT) */
//    set_mtrrs_for_acmod(g_sinit);
//
//    printk(TBOOT_INFO"executing GETSEC[SENTER]...\n");
//    /* (optionally) pause before executing GETSEC[SENTER] */
//    if ( g_vga_delay > 0 )
//        delay(g_vga_delay * 1000);
//    __getsec_senter((uint32_t)g_sinit, (g_sinit->size)*4);
//    printk(TBOOT_ERR"ERROR--we should not get here!\n");
//    return false;
//}
//
//tb_error_t txt_launch_racm(loader_ctx *lctx)
//{
//    acm_hdr_t *racm = NULL;
//
//    /*
//     * find correct revocation AC module in modules list
//     */
//    find_platform_racm(lctx, (void **)&racm, NULL);
//    /* copy it to a 32KB aligned memory address */
//    racm = copy_racm(racm);
//    if ( racm == NULL )
//        return TB_ERR_SINIT_NOT_PRESENT;
//    /* do some checks on it */
//    if ( !verify_racm(racm) )
//        return TB_ERR_ACMOD_VERIFY_FAILED;
//
//    /* save MTRRs before we alter them for RACM launch */
//    /*  - not needed by far since always reboot after RACM launch */
//    //save_mtrrs(...);
//
//    /* set MTRRs properly for AC module (RACM) */
//    if ( !set_mtrrs_for_acmod(racm) )
//        return TB_ERR_FATAL;
//
//    /* clear MSEG_BASE/SIZE registers */
//    write_pub_config_reg(TXTCR_MSEG_BASE, 0);
//    write_pub_config_reg(TXTCR_MSEG_SIZE, 0);
//
//    printk(TBOOT_INFO"executing GETSEC[ENTERACCS]...\n");
//    /* (optionally) pause before executing GETSEC[ENTERACCS] */
//    if ( g_vga_delay > 0 )
//        delay(g_vga_delay * 1000);
//    __getsec_enteraccs((uint32_t)racm, (racm->size)*4, 0xF0);
//    /* powercycle by writing 0x0a+0x0e to port 0xcf9, */
//    /* warm reset by write 0x06 to port 0xcf9 */
//    //outb(0xcf9, 0x0a);
//    //outb(0xcf9, 0x0e);
//    outb(0xcf9, 0x06);
//    
//    printk(TBOOT_ERR"ERROR--we should not get here!\n");
//    return TB_ERR_FATAL;
//}
//
int txt_prepare_cpu(void)
{
	unsigned long eflags, cr0;
	uint64_t mcg_cap, mcg_stat;

	/* must be running at CPL 0 => this is implicit in even getting this far */
	/* since our bootstrap code loads a GDT, etc. */

	cr0 = read_cr0();

	/* must be in protected mode */
	if (!(cr0 & CR0_PE)) {
		out_info("ERR: not in protected mode\n");
		return 0;
	}

	/* cache must be enabled (CR0.CD = CR0.NW = 0) */
	if (cr0 & CR0_CD) {
		out_info("CR0.CD set\n");
		cr0 &= ~CR0_CD;
	}
	if ( cr0 & CR0_NW ) {
		out_info("CR0.NW set\n");
		cr0 &= ~CR0_NW;
	}

	/* native FPU error reporting must be enabled for proper */
	/* interaction behavior */
	if (!(cr0 & CR0_NE)) {
		out_info("CR0.NE not set\n");
		cr0 |= CR0_NE;
	}

	write_cr0(cr0);

	/* cannot be in virtual-8086 mode (EFLAGS.VM=1) */
	eflags = read_eflags();
	if (eflags & X86_EFLAGS_VM) {
		out_info("EFLAGS.VM set");
		write_eflags(eflags | ~X86_EFLAGS_VM);
	}

	out_info("CR0 and EFLAGS OK");

	/*
	 * verify that we're not already in a protected environment
	 */

	if (txt_is_launched()) {
		out_info("already in protected environment");
		return 0;
	}

	/*
	 * verify all machine check status registers are clear (unless
	 * support preserving them)
	 */

	/* no machine check in progress (IA32_MCG_STATUS.MCIP=1) */
	mcg_stat = rdmsr(MSR_MCG_STATUS);
	if (mcg_stat & 0x04) {
		out_info("machine check in progress");
		return 0;
	}

	getsec_parameters_t params;
	if (!get_parameters(&params)) {
		out_info("get_parameters() failed\n");
		return 0;
	}

	/* check if all machine check regs are clear */
	mcg_cap = rdmsr(MSR_MCG_CAP);
	for (unsigned int i = 0; i < (mcg_cap & 0xff); i++) {
		mcg_stat = rdmsr(MSR_MC0_STATUS + 4*i);
		if (mcg_stat & (1ULL << 63)) {
			out_description("MCG[index] =", i);
			out_description("ERROR =", mcg_stat);
		if (!params.preserve_mce)
			return 0;
		}
	}

	if (params.preserve_mce)
		out_info("supports preserving machine check errors");
	else
		out_info("no machine check errors");

	if (params.proc_based_scrtm)
		out_info("CPU support processor-based S-CRTM");

	/* all is well with the processor state */
	out_info("CPU is ready for SENTER");

	return 1;
}


static int verify_saved_mtrrs(txt_heap_t *txt_heap)
{   
	os_mle_data_t *os_mle_data;
	os_mle_data = get_os_mle_data_start(txt_heap);

	return validate_mtrrs(&(os_mle_data->saved_mtrr_state));
}   


int e820_reserve_ram(uint64_t base, uint64_t length);

static int reserve_vtd_delta_mem(uint64_t min_lo_ram, uint64_t max_lo_ram, uint64_t min_hi_ram, uint64_t max_hi_ram)
{
	uint64_t base, length;
	(void)min_lo_ram; (void)min_hi_ram;/* portably suppress compiler warning */
 
	txt_heap_t* txt_heap = get_txt_heap();
	os_sinit_data_t *os_sinit_data = get_os_sinit_data_start(txt_heap);
 
	if (max_lo_ram != (os_sinit_data->vtd_pmr_lo_base + os_sinit_data->vtd_pmr_lo_size) ) {
		base = os_sinit_data->vtd_pmr_lo_base + os_sinit_data->vtd_pmr_lo_size;
		length = max_lo_ram - base;
		out_info("reserving memory  which was truncated for VT-d");
		out_description("base", base);
		out_description("base + length", base + length);
		if (!e820_reserve_ram(base, length))
			return 0;
	}
	if (max_hi_ram != (os_sinit_data->vtd_pmr_hi_base + os_sinit_data->vtd_pmr_hi_size)) {
		base = os_sinit_data->vtd_pmr_hi_base + os_sinit_data->vtd_pmr_hi_size;
		length = max_hi_ram - base;
		out_info("reserving memory  which was truncated for VT-d");
		out_description("base", base);
		out_description("base + length", base + length);
		if (!e820_reserve_ram(base, length))
			return 0;
	}
 
	return 1;
}


static int verify_vtd_pmrs(txt_heap_t *txt_heap)
{
	os_sinit_data_t *os_sinit_data, tmp_os_sinit_data;
	uint64_t min_lo_ram, max_lo_ram, min_hi_ram, max_hi_ram;

	os_sinit_data = get_os_sinit_data_start(txt_heap);

	/*
	 * make sure the VT-d PMRs were actually set to cover what
	 * we expect
	 */
 
	/* calculate what they should have been */
	/* no e820 table on S3 resume, so use saved (sealed) values */

/*
285     if ( s3_flag ) {
286         min_lo_ram = g_pre_k_s3_state.vtd_pmr_lo_base;
287         max_lo_ram = min_lo_ram + g_pre_k_s3_state.vtd_pmr_lo_size;
288         min_hi_ram = g_pre_k_s3_state.vtd_pmr_hi_base;
289         max_hi_ram = min_hi_ram + g_pre_k_s3_state.vtd_pmr_hi_size;
	}
291     else {
*/
		if (!get_ram_ranges(&min_lo_ram, &max_lo_ram, &min_hi_ram, &max_hi_ram))
			return 0;
 
		/* if vtd_pmr_lo/hi sizes rounded to 2MB granularity are less than the
		   max_lo/hi_ram values determined from the e820 table, then we must
		   reserve the differences in e820 table so that unprotected memory is
		   not used by the kernel */

		if (!reserve_vtd_delta_mem(min_lo_ram, max_lo_ram, min_hi_ram, max_hi_ram) ) {
			out_info("failed to reserve VT-d PMR delta memory");
			return 0;
		}
//305     }
 
	/* compare to current values */
	memset(&tmp_os_sinit_data, 0, sizeof(tmp_os_sinit_data));
	tmp_os_sinit_data.version = os_sinit_data->version;
	set_vtd_pmrs(&tmp_os_sinit_data, min_lo_ram, max_lo_ram, min_hi_ram, max_hi_ram);

	if ((tmp_os_sinit_data.vtd_pmr_lo_base != os_sinit_data->vtd_pmr_lo_base)
	    || (tmp_os_sinit_data.vtd_pmr_lo_size != os_sinit_data->vtd_pmr_lo_size)
	    || (tmp_os_sinit_data.vtd_pmr_hi_base != os_sinit_data->vtd_pmr_hi_base)
	    || (tmp_os_sinit_data.vtd_pmr_hi_size != os_sinit_data->vtd_pmr_hi_size) ) {
		out_info("OS to SINIT data VT-d PMR settings do not match");
			print_os_sinit_data(&tmp_os_sinit_data);
			print_os_sinit_data(os_sinit_data);
			return 0;
	}


//326     if ( !s3_flag ) {
//327         /* save the verified values so that they can be sealed for S3 */
//328         g_pre_k_s3_state.vtd_pmr_lo_base = os_sinit_data->vtd_pmr_lo_base;
//329         g_pre_k_s3_state.vtd_pmr_lo_size = os_sinit_data->vtd_pmr_lo_size;
//330         g_pre_k_s3_state.vtd_pmr_hi_base = os_sinit_data->vtd_pmr_hi_base;
//331         g_pre_k_s3_state.vtd_pmr_hi_size = os_sinit_data->vtd_pmr_hi_size;
//332     }
 
	return 1;
}


int txt_post_launch_verify_platform(void)
{
	txt_heap_t *txt_heap;

	/*
	 * verify some of the heap structures
	 */

	txt_heap = get_txt_heap();


	if (!verify_txt_heap(txt_heap, 0))
		return 1;

	/* verify the saved MTRRs */
	if (!verify_saved_mtrrs(txt_heap))
		return 1;

	/* verify that VT-d PMRs were really set as required */
	if (!verify_vtd_pmrs(txt_heap)) 
		return 1;

	return 0;
}                        


void txt_post_launch(void)
{
	txt_heap_t *txt_heap;
	os_mle_data_t *os_mle_data;
	int err;

	/* verify MTRRs, VT-d settings, TXT heap, etc. */
	err = txt_post_launch_verify_platform();

	/* don't return the error yet, because we need to restore settings */
	if (err != 0) {
		out_info("failed to verify platform");
		while(1);
	}
	out_info("Platform verification done");
	wait(3000);

	/* get saved OS state (os_mvmm_data_t) from LT heap */
	txt_heap = get_txt_heap();
	os_mle_data = get_os_mle_data_start(txt_heap);

	/* clear error registers so that we start fresh */
	write_priv_config_reg(TXTCR_ERRORCODE, 0x00000000);
	write_priv_config_reg(TXTCR_ESTS, 0xffffffff);  /* write 1's to clear */

	/* bring RLPs into environment (do this before restoring MTRRs to ensure */
	/* SINIT area is mapped WB for MONITOR-based RLP wakeup) */
	txt_wakeup_cpus();

	/* restore pre-SENTER IA32_MISC_ENABLE_MSR (no verification needed)
	   (do after AP wakeup so that if restored MSR has MWAIT clear it won't
	   prevent wakeup) */
	out_description("saved IA32_MISC_ENABLE", os_mle_data->saved_misc_enable_msr);
	wrmsr(MSR_IA32_MISC_ENABLE, os_mle_data->saved_misc_enable_msr);
//    if ( use_mwait() ) {
//        /* set MONITOR/MWAIT support */
//        uint64_t misc;
//        misc = rdmsr(MSR_IA32_MISC_ENABLE);
//        misc |= MSR_IA32_MISC_ENABLE_MONITOR_FSM;
//        wrmsr(MSR_IA32_MISC_ENABLE, misc);
//    }
//
	/* restore pre-SENTER MTRRs that were overwritten for SINIT launch */
	restore_mtrrs(&(os_mle_data->saved_mtrr_state));

	/* now, if there was an error, apply policy */
//    apply_policy(err);
//
	/* always set the TXT.CMD.SECRETS flag */
	write_priv_config_reg(TXTCR_CMD_SECRETS, 0x01);
	read_priv_config_reg(TXTCR_E2STS);   /* just a fence, so ignore return */
	out_info("set TXT.CMD.SECRETS flag");

	/* open TPM locality 1 */
	write_priv_config_reg(TXTCR_CMD_OPEN_LOCALITY1, 0x01);
	read_priv_config_reg(TXTCR_E2STS);   /* just a fence, so ignore return */
	out_info("opened TPM locality 1\n");
}

//void ap_wait(unsigned int cpuid)
//{
//    if ( cpuid >= NR_CPUS ) {
//        printk(TBOOT_ERR"cpuid (%u) exceeds # supported CPUs\n", cpuid);
//        apply_policy(TB_ERR_FATAL);
//        mtx_leave(&ap_lock);
//        return;
//    }
//
//    /* ensure MONITOR/MWAIT support is set */
//    uint64_t misc;
//    misc = rdmsr(MSR_IA32_MISC_ENABLE);
//    misc |= MSR_IA32_MISC_ENABLE_MONITOR_FSM;
//    wrmsr(MSR_IA32_MISC_ENABLE, misc);
//
//    /* this is close enough to entering monitor/mwait loop, so inc counter */
//    atomic_inc((atomic_t *)&_tboot_shared.num_in_wfs);
//    mtx_leave(&ap_lock);
//
//    printk(TBOOT_INFO"cpu %u mwait'ing\n", cpuid);
//    while ( _tboot_shared.ap_wake_trigger != cpuid ) {
//        cpu_monitor(&_tboot_shared.ap_wake_trigger, 0, 0);
//        mb();
//        if ( _tboot_shared.ap_wake_trigger == cpuid )
//            break;
//        cpu_mwait(0, 0);
//    }
//
//    uint32_t sipi_vec = (uint32_t)_tboot_shared.ap_wake_addr;
//    atomic_dec(&ap_wfs_count);
//    atomic_dec((atomic_t *)&_tboot_shared.num_in_wfs);
//    cpu_wakeup(cpuid, sipi_vec);
//}

void txt_cpu_wakeup(void)
{
	txt_heap_t *txt_heap;
	os_mle_data_t *os_mle_data;
	uint64_t madt_apicbase, msr_apicbase;
	unsigned int cpuid = get_apicid();

	if (cpuid >= NR_CPUS) {
		out_description("cpuid exceeds # supported CPUs. id", cpuid);
		return;
	}

	mtx_enter(&ap_lock);

	out_description("cpu waking up from TXT sleep :", cpuid);
	//WAIT_FOR_INPUT();

	/* restore LAPIC base address for AP */
	madt_apicbase = (uint64_t)get_madt_apic_base();
	if (madt_apicbase == 0) {
		out_info("not able to get apci base from MADT\n");
//        apply_policy(TB_ERR_FATAL);
		return;
	}
	msr_apicbase = rdmsr(MSR_APICBASE);
	if (madt_apicbase != (msr_apicbase & ~0xFFFULL)) {
		out_description("cpu restore apic base to of ", cpuid);
		out_description64("to base", madt_apicbase);
		wrmsr(MSR_APICBASE, (msr_apicbase & 0xFFFULL) | madt_apicbase);
	}

	txt_heap = get_txt_heap();
	os_mle_data = get_os_mle_data_start(txt_heap);

	/* apply (validated) (pre-SENTER) MTRRs from BSP to each AP */
	restore_mtrrs(&(os_mle_data->saved_mtrr_state));

	/* restore pre-SENTER IA32_MISC_ENABLE_MSR */
	wrmsr(MSR_IA32_MISC_ENABLE, os_mle_data->saved_misc_enable_msr);

//    if ( !verify_stm(cpuid) )
//        apply_policy(TB_ERR_POST_LAUNCH_VERIFICATION);

	/* enable SMIs */
	out_description("enabling SMIs on cpu :", cpuid);
	__getsec_smctrl();
	
	atomic_inc(&ap_wfs_count);
//    if ( use_mwait() )
//        ap_wait(cpuid);
//    else
//        handle_init_sipi_sipi(cpuid);
	
	//* BHUSHAN THIS IS JUST FOR TESING */
	mtx_leave(&ap_lock);
	//* TESTING LINE END */
//	WAIT_FOR_INPUT();
}

//tb_error_t txt_protect_mem_regions(void)
//{
//    uint64_t base, size;
//
//    /*
//     * TXT has 2 regions of RAM that need to be reserved for use by only the
//     * hypervisor; not even dom0 should have access:
//     *   TXT heap, SINIT AC module
//     */
//
//    /* TXT heap */
//    base = read_pub_config_reg(TXTCR_HEAP_BASE);
//    size = read_pub_config_reg(TXTCR_HEAP_SIZE);
//    printk(TBOOT_INFO"protecting TXT heap (%Lx - %Lx) in e820 table\n", base,
//           (base + size - 1));
//    if ( !e820_protect_region(base, size, E820_RESERVED) )
//        return TB_ERR_FATAL;
//
//    /* SINIT */
//    base = read_pub_config_reg(TXTCR_SINIT_BASE);
//    size = read_pub_config_reg(TXTCR_SINIT_SIZE);
//    printk(TBOOT_INFO"protecting SINIT (%Lx - %Lx) in e820 table\n", base,
//           (base + size - 1));
//    if ( !e820_protect_region(base, size, E820_RESERVED) )
//        return TB_ERR_FATAL;
//
//    /* TXT private space */
//    base = TXT_PRIV_CONFIG_REGS_BASE;
//    size = TXT_CONFIG_REGS_SIZE;
//    printk(TBOOT_INFO
//           "protecting TXT Private Space (%Lx - %Lx) in e820 table\n",
//           base, (base + size - 1));
//    if ( !e820_protect_region(base, size, E820_RESERVED) )
//        return TB_ERR_FATAL;
//
//    /* ensure that memory not marked as good RAM by the MDRs is RESERVED in
//       the e820 table */
//    txt_heap_t* txt_heap = get_txt_heap();
//    sinit_mle_data_t *sinit_mle_data = get_sinit_mle_data_start(txt_heap);
//    uint32_t num_mdrs = sinit_mle_data->num_mdrs;
//    sinit_mdr_t *mdrs_base = (sinit_mdr_t *)(((void *)sinit_mle_data
//                                              - sizeof(uint64_t)) +
//                                             sinit_mle_data->mdrs_off);
//    printk(TBOOT_INFO"verifying e820 table against SINIT MDRs: ");
//    if ( !verify_e820_map(mdrs_base, num_mdrs) ) {
//        printk(TBOOT_ERR"verification failed.\n");
//        return TB_ERR_POST_LAUNCH_VERIFICATION;
//    }
//    printk(TBOOT_INFO"verification succeeded.\n");
//
//    return TB_ERR_NONE;
//}
//
//void txt_shutdown(void)
//{
//    unsigned long apicbase;
//
//    /* shutdown shouldn't be called on APs, but if it is then just hlt */
//    apicbase = rdmsr(MSR_APICBASE);
//    if ( !(apicbase & APICBASE_BSP) ) {
//        printk(TBOOT_INFO"calling txt_shutdown on AP\n");
//        while ( true )
//            halt();
//    }
//
//    /* set TXT.CMD.NO-SECRETS flag (i.e. clear SECRETS flag) */
//    write_priv_config_reg(TXTCR_CMD_NO_SECRETS, 0x01);
//    read_priv_config_reg(TXTCR_E2STS);   /* fence */
//    printk(TBOOT_INFO"secrets flag cleared\n");
//
//    /* unlock memory configuration */
//    write_priv_config_reg(TXTCR_CMD_UNLOCK_MEM_CONFIG, 0x01);
//    read_pub_config_reg(TXTCR_E2STS);    /* fence */
//    printk(TBOOT_INFO"memory configuration unlocked\n");
//
//    /* if some APs are still in wait-for-sipi then SEXIT will hang */
//    /* so TXT reset the platform instead, expect mwait case */
//    if ( (!use_mwait()) && atomic_read(&ap_wfs_count) > 0 ) {
//        printk(TBOOT_INFO
//               "exiting with some APs still in wait-for-sipi state (%u)\n",
//               atomic_read(&ap_wfs_count));
//        write_priv_config_reg(TXTCR_CMD_RESET, 0x01);
//    }
//
//    /* close TXT private config space */
//    /* implicitly closes TPM localities 1 + 2 */
//    read_priv_config_reg(TXTCR_E2STS);   /* fence */
//    write_priv_config_reg(TXTCR_CMD_CLOSE_PRIVATE, 0x01);
//    read_pub_config_reg(TXTCR_E2STS);    /* fence */
//    printk(TBOOT_INFO"private config space closed\n");
//
//    /* SMXE may not be enabled any more, so set it to make sure */
//    write_cr4(read_cr4() | CR4_SMXE);
//
//    /* call GETSEC[SEXIT] */
//    printk(TBOOT_INFO"executing GETSEC[SEXIT]...\n");
//    __getsec_sexit();
//    printk(TBOOT_INFO"measured environment torn down\n");
//}
//
//bool txt_is_powercycle_required(void)
//{
//    /* a powercycle is required to clear the TXT_RESET.STS flag */
//    txt_ests_t ests = (txt_ests_t)read_pub_config_reg(TXTCR_ESTS);
//    return ests.txt_reset_sts;
//}
//
//#define ACM_MEM_TYPE_UC                 0x0100
//#define ACM_MEM_TYPE_WC                 0x0200
//#define ACM_MEM_TYPE_WT                 0x1000
//#define ACM_MEM_TYPE_WP                 0x2000
//#define ACM_MEM_TYPE_WB                 0x4000
//
//#define DEF_ACM_MAX_SIZE                0x8000
//#define DEF_ACM_VER_MASK                0xffffffff
//#define DEF_ACM_VER_SUPPORTED           0x00
//#define DEF_ACM_MEM_TYPES               ACM_MEM_TYPE_UC
//#define DEF_SENTER_CTRLS                0x00
//
//bool get_parameters(getsec_parameters_t *params)
//{
//    unsigned long cr4;
//    uint32_t index, eax, ebx, ecx;
//    int param_type;
//
//    /* sanity check because GETSEC[PARAMETERS] will fail if not set */
//    cr4 = read_cr4();
//    if ( !(cr4 & CR4_SMXE) ) {
//        printk(TBOOT_ERR"SMXE not enabled, can't read parameters\n");
//        return false;
//    }
//
//    memset(params, 0, sizeof(*params));
//    params->acm_max_size = DEF_ACM_MAX_SIZE;
//    params->acm_mem_types = DEF_ACM_MEM_TYPES;
//    params->senter_controls = DEF_SENTER_CTRLS;
//    params->proc_based_scrtm = false;
//    params->preserve_mce = false;
//
//    index = 0;
//    do {
//        __getsec_parameters(index++, &param_type, &eax, &ebx, &ecx);
//        /* the code generated for a 'switch' statement doesn't work in this */
//        /* environment, so use if/else blocks instead */
//
//        /* NULL - all reserved */
//        if ( param_type == 0 )
//            ;
//        /* supported ACM versions */
//        else if ( param_type == 1 ) {
//            if ( params->n_versions == MAX_SUPPORTED_ACM_VERSIONS )
//                printk(TBOOT_WARN"number of supported ACM version exceeds "
//                       "MAX_SUPPORTED_ACM_VERSIONS\n");
//            else {
//                params->acm_versions[params->n_versions].mask = ebx;
//                params->acm_versions[params->n_versions].version = ecx;
//                params->n_versions++;
//            }
//        }
//        /* max size AC execution area */
//        else if ( param_type == 2 )
//            params->acm_max_size = eax & 0xffffffe0;
//        /* supported non-AC mem types */
//        else if ( param_type == 3 )
//            params->acm_mem_types = eax & 0xffffffe0;
//        /* SENTER controls */
//        else if ( param_type == 4 )
//            params->senter_controls = (eax & 0x00007fff) >> 8;
//        /* TXT extensions support */
//        else if ( param_type == 5 ) {
//            params->proc_based_scrtm = (eax & 0x00000020) ? true : false;
//            params->preserve_mce = (eax & 0x00000040) ? true : false;
//        }
//        else {
//            printk(TBOOT_WARN"unknown GETSEC[PARAMETERS] type: %d\n", 
//                   param_type);
//            param_type = 0;    /* set so that we break out of the loop */
//        }
//    } while ( param_type != 0 );
//
//    if ( params->n_versions == 0 ) {
//        params->acm_versions[0].mask = DEF_ACM_VER_MASK;
//        params->acm_versions[0].version = DEF_ACM_VER_SUPPORTED;
//        params->n_versions = 1;
//    }
//
//    return true;
//}
//
//
///*
// * Local variables:
// * mode: C
// * c-set-style: "BSD"
// * c-basic-offset: 4
// * tab-width: 4
// * indent-tabs-mode: nil
// * End:
// */
