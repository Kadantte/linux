/* SPDX-License-Identifier: GPL-2.0 OR Linux-OpenIB */
/*
 * Copyright 2015-2020 Amazon.com, Inc. or its affiliates. All rights reserved.
 */

#ifndef ENA_H
#define ENA_H

#include <linux/bitops.h>
#include <linux/dim.h>
#include <linux/etherdevice.h>
#include <linux/if_vlan.h>
#include <linux/inetdevice.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <net/xdp.h>
#include <uapi/linux/bpf.h>
#include <net/devlink.h>

#include "ena_com.h"
#include "ena_eth_com.h"

#define DRV_MODULE_GEN_MAJOR	2
#define DRV_MODULE_GEN_MINOR	1
#define DRV_MODULE_GEN_SUBMINOR 0

#define DRV_MODULE_NAME		"ena"

#define DEVICE_NAME	"Elastic Network Adapter (ENA)"

/* 1 for AENQ + ADMIN */
#define ENA_ADMIN_MSIX_VEC		1
#define ENA_MAX_MSIX_VEC(io_queues)	(ENA_ADMIN_MSIX_VEC + (io_queues))

/* The ENA buffer length fields is 16 bit long. So when PAGE_SIZE == 64kB the
 * driver passes 0.
 * Since the max packet size the ENA handles is ~9kB limit the buffer length to
 * 16kB.
 */
#if PAGE_SIZE > SZ_16K
#define ENA_PAGE_SIZE (_AC(SZ_16K, UL))
#else
#define ENA_PAGE_SIZE PAGE_SIZE
#endif

#define ENA_MIN_MSIX_VEC		2

#define ENA_REG_BAR			0
#define ENA_MEM_BAR			2
#define ENA_BAR_MASK (BIT(ENA_REG_BAR) | BIT(ENA_MEM_BAR))

#define ENA_DEFAULT_RING_SIZE	(1024)
#define ENA_MIN_RING_SIZE	(256)

#define ENA_MIN_RX_BUF_SIZE (2048)

#define ENA_MIN_NUM_IO_QUEUES	(1)

#define ENA_TX_WAKEUP_THRESH		(MAX_SKB_FRAGS + 2)
#define ENA_DEFAULT_RX_COPYBREAK	(256 - NET_IP_ALIGN)

#define ENA_MIN_MTU		128

#define ENA_NAME_MAX_LEN	20
#define ENA_IRQNAME_SIZE	40

#define ENA_PKT_MAX_BUFS	19

#define ENA_RX_RSS_TABLE_LOG_SIZE  7
#define ENA_RX_RSS_TABLE_SIZE	(1 << ENA_RX_RSS_TABLE_LOG_SIZE)

/* The number of tx packet completions that will be handled each NAPI poll
 * cycle is ring_size / ENA_TX_POLL_BUDGET_DIVIDER.
 */
#define ENA_TX_POLL_BUDGET_DIVIDER	4

/* Refill Rx queue when number of required descriptors is above
 * QUEUE_SIZE / ENA_RX_REFILL_THRESH_DIVIDER or ENA_RX_REFILL_THRESH_PACKET
 */
#define ENA_RX_REFILL_THRESH_DIVIDER	8
#define ENA_RX_REFILL_THRESH_PACKET	256

/* Number of queues to check for missing queues per timer service */
#define ENA_MONITORED_TX_QUEUES	4
/* Max timeout packets before device reset */
#define MAX_NUM_OF_TIMEOUTED_PACKETS 128

#define ENA_TX_RING_IDX_NEXT(idx, ring_size) (((idx) + 1) & ((ring_size) - 1))

#define ENA_RX_RING_IDX_NEXT(idx, ring_size) (((idx) + 1) & ((ring_size) - 1))
#define ENA_RX_RING_IDX_ADD(idx, n, ring_size) \
	(((idx) + (n)) & ((ring_size) - 1))

#define ENA_IO_TXQ_IDX(q)	(2 * (q))
#define ENA_IO_RXQ_IDX(q)	(2 * (q) + 1)
#define ENA_IO_TXQ_IDX_TO_COMBINED_IDX(q)	((q) / 2)
#define ENA_IO_RXQ_IDX_TO_COMBINED_IDX(q)	(((q) - 1) / 2)

#define ENA_MGMNT_IRQ_IDX		0
#define ENA_IO_IRQ_FIRST_IDX		1
#define ENA_IO_IRQ_IDX(q)		(ENA_IO_IRQ_FIRST_IDX + (q))

#define ENA_ADMIN_POLL_DELAY_US 100

/* ENA device should send keep alive msg every 1 sec.
 * We wait for 6 sec just to be on the safe side.
 */
#define ENA_DEVICE_KALIVE_TIMEOUT	(6 * HZ)
#define ENA_MAX_NO_INTERRUPT_ITERATIONS 3

#define ENA_MMIO_DISABLE_REG_READ	BIT(0)

struct ena_phc_info;

struct ena_irq {
	irq_handler_t handler;
	void *data;
	int cpu;
	u32 vector;
	cpumask_t affinity_hint_mask;
	char name[ENA_IRQNAME_SIZE];
};

struct ena_napi {
	u8 first_interrupt ____cacheline_aligned;
	u8 interrupts_masked;
	struct napi_struct napi;
	struct ena_ring *tx_ring;
	struct ena_ring *rx_ring;
	u32 qid;
	struct dim dim;
};

struct ena_tx_buffer {
	union {
		struct sk_buff *skb;
		/* XDP buffer structure which is used for sending packets in
		 * the xdp queues
		 */
		struct xdp_frame *xdpf;
	};
	/* num of ena desc for this specific skb
	 * (includes data desc and metadata desc)
	 */
	u32 tx_descs;
	/* num of buffers used by this skb */
	u32 num_of_bufs;

	/* Total size of all buffers in bytes */
	u32 total_tx_size;

	/* Indicate if bufs[0] map the linear data of the skb. */
	u8 map_linear_data;

	/* Used for detect missing tx packets to limit the number of prints */
	u8 print_once;
	/* Save the last jiffies to detect missing tx packets
	 *
	 * sets to non zero value on ena_start_xmit and set to zero on
	 * napi and timer_Service_routine.
	 *
	 * while this value is not protected by lock,
	 * a given packet is not expected to be handled by ena_start_xmit
	 * and by napi/timer_service at the same time.
	 */
	unsigned long last_jiffies;
	struct ena_com_buf bufs[ENA_PKT_MAX_BUFS];
} ____cacheline_aligned;

struct ena_rx_buffer {
	struct sk_buff *skb;
	struct page *page;
	dma_addr_t dma_addr;
	u32 page_offset;
	u32 buf_offset;
	struct ena_com_buf ena_buf;
} ____cacheline_aligned;

struct ena_stats_tx {
	u64 cnt;
	u64 bytes;
	u64 queue_stop;
	u64 prepare_ctx_err;
	u64 queue_wakeup;
	u64 dma_mapping_err;
	u64 linearize;
	u64 linearize_failed;
	u64 napi_comp;
	u64 tx_poll;
	u64 doorbells;
	u64 bad_req_id;
	u64 llq_buffer_copy;
	u64 missed_tx;
	u64 unmask_interrupt;
	u64 last_napi_jiffies;
};

struct ena_stats_rx {
	u64 cnt;
	u64 bytes;
	u64 rx_copybreak_pkt;
	u64 csum_good;
	u64 refil_partial;
	u64 csum_bad;
	u64 page_alloc_fail;
	u64 skb_alloc_fail;
	u64 dma_mapping_err;
	u64 bad_desc_num;
	u64 bad_req_id;
	u64 empty_rx_ring;
	u64 csum_unchecked;
	u64 xdp_aborted;
	u64 xdp_drop;
	u64 xdp_pass;
	u64 xdp_tx;
	u64 xdp_invalid;
	u64 xdp_redirect;
};

struct ena_ring {
	/* Holds the empty requests for TX/RX
	 * out of order completions
	 */
	u16 *free_ids;

	union {
		struct ena_tx_buffer *tx_buffer_info;
		struct ena_rx_buffer *rx_buffer_info;
	};

	/* cache ptr to avoid using the adapter */
	struct device *dev;
	struct pci_dev *pdev;
	struct napi_struct *napi;
	struct net_device *netdev;
	struct ena_com_dev *ena_dev;
	struct ena_adapter *adapter;
	struct ena_com_io_cq *ena_com_io_cq;
	struct ena_com_io_sq *ena_com_io_sq;
	struct bpf_prog *xdp_bpf_prog;
	struct xdp_rxq_info xdp_rxq;
	spinlock_t xdp_tx_lock;	/* synchronize XDP TX/Redirect traffic */
	/* Used for rx queues only to point to the xdp tx ring, to
	 * which traffic should be redirected from this rx ring.
	 */
	struct ena_ring *xdp_ring;

	u16 next_to_use;
	u16 next_to_clean;
	u16 rx_copybreak;
	u16 rx_headroom;
	u16 qid;
	u16 mtu;
	u16 sgl_size;

	/* The maximum header length the device can handle */
	u8 tx_max_header_size;

	bool disable_meta_caching;
	u16 no_interrupt_event_cnt;

	/* cpu and NUMA for TPH */
	int cpu;
	int numa_node;

	/* number of tx/rx_buffer_info's entries */
	int ring_size;

	enum ena_admin_placement_policy_type tx_mem_queue_type;

	struct ena_com_rx_buf_info ena_bufs[ENA_PKT_MAX_BUFS];
	u32  smoothed_interval;
	u32  per_napi_packets;
	u16 non_empty_napi_events;
	struct u64_stats_sync syncp;
	union {
		struct ena_stats_tx tx_stats;
		struct ena_stats_rx rx_stats;
	};

	u8 *push_buf_intermediate_buf;
	int empty_rx_queue;
} ____cacheline_aligned;

struct ena_stats_dev {
	u64 tx_timeout;
	u64 suspend;
	u64 resume;
	u64 wd_expired;
	u64 interface_up;
	u64 interface_down;
	u64 admin_q_pause;
	u64 rx_drops;
	u64 tx_drops;
	u64 reset_fail;
};

enum ena_flags_t {
	ENA_FLAG_DEVICE_RUNNING,
	ENA_FLAG_DEV_UP,
	ENA_FLAG_LINK_UP,
	ENA_FLAG_MSIX_ENABLED,
	ENA_FLAG_TRIGGER_RESET,
	ENA_FLAG_ONGOING_RESET
};

/* adapter specific private data structure */
struct ena_adapter {
	struct ena_com_dev *ena_dev;
	/* OS defined structs */
	struct net_device *netdev;
	struct pci_dev *pdev;

	/* rx packets that shorter that this len will be copied to the skb
	 * header
	 */
	u32 rx_copybreak;
	u32 max_mtu;

	u32 num_io_queues;
	u32 max_num_io_queues;

	int msix_vecs;

	u32 missing_tx_completion_threshold;

	u32 requested_tx_ring_size;
	u32 requested_rx_ring_size;

	u32 max_tx_ring_size;
	u32 max_rx_ring_size;

	u32 msg_enable;

	/* large_llq_header_enabled is used for two purposes:
	 * 1. Indicates that large LLQ has been requested.
	 * 2. Indicates whether large LLQ is set or not after device
	 *    initialization / configuration.
	 */
	bool large_llq_header_enabled;
	bool large_llq_header_supported;

	u16 max_tx_sgl_size;
	u16 max_rx_sgl_size;

	u8 mac_addr[ETH_ALEN];

	unsigned long keep_alive_timeout;
	unsigned long missing_tx_completion_to;

	char name[ENA_NAME_MAX_LEN];

	struct ena_phc_info *phc_info;

	unsigned long flags;
	/* TX */
	struct ena_ring tx_ring[ENA_MAX_NUM_IO_QUEUES]
		____cacheline_aligned_in_smp;

	/* RX */
	struct ena_ring rx_ring[ENA_MAX_NUM_IO_QUEUES]
		____cacheline_aligned_in_smp;

	struct ena_napi ena_napi[ENA_MAX_NUM_IO_QUEUES];

	struct ena_irq irq_tbl[ENA_MAX_MSIX_VEC(ENA_MAX_NUM_IO_QUEUES)];

	/* timer service */
	struct work_struct reset_task;
	struct timer_list timer_service;

	bool wd_state;
	bool dev_up_before_reset;
	bool disable_meta_caching;
	unsigned long last_keep_alive_jiffies;

	struct u64_stats_sync syncp;
	struct ena_stats_dev dev_stats;
	struct ena_admin_eni_stats eni_stats;
	struct ena_admin_ena_srd_info ena_srd_info;

	/* last queue index that was checked for uncompleted tx packets */
	u32 last_monitored_tx_qid;

	enum ena_regs_reset_reason_types reset_reason;

	struct bpf_prog *xdp_bpf_prog;
	u32 xdp_first_ring;
	u32 xdp_num_queues;

	struct devlink *devlink;
	struct devlink_port devlink_port;
#ifdef CONFIG_DEBUG_FS

	struct dentry *debugfs_base;
#endif /* CONFIG_DEBUG_FS */
};

void ena_set_ethtool_ops(struct net_device *netdev);

void ena_dump_stats_to_dmesg(struct ena_adapter *adapter);

void ena_dump_stats_to_buf(struct ena_adapter *adapter, u8 *buf);


int ena_update_queue_params(struct ena_adapter *adapter,
			    u32 new_tx_size,
			    u32 new_rx_size,
			    u32 new_llq_header_len);

int ena_update_queue_count(struct ena_adapter *adapter, u32 new_channel_count);

int ena_set_rx_copybreak(struct ena_adapter *adapter, u32 rx_copybreak);

int ena_get_sset_count(struct net_device *netdev, int sset);

static inline void ena_reset_device(struct ena_adapter *adapter,
				    enum ena_regs_reset_reason_types reset_reason)
{
	adapter->reset_reason = reset_reason;
	/* Make sure reset reason is set before triggering the reset */
	smp_mb__before_atomic();
	set_bit(ENA_FLAG_TRIGGER_RESET, &adapter->flags);
}

int ena_destroy_device(struct ena_adapter *adapter, bool graceful);
int ena_restore_device(struct ena_adapter *adapter);
int handle_invalid_req_id(struct ena_ring *ring, u16 req_id,
			  struct ena_tx_buffer *tx_info, bool is_xdp);

/* Increase a stat by cnt while holding syncp seqlock on 32bit machines */
static inline void ena_increase_stat(u64 *statp, u64 cnt,
				     struct u64_stats_sync *syncp)
{
	u64_stats_update_begin(syncp);
	(*statp) += cnt;
	u64_stats_update_end(syncp);
}

static inline void ena_ring_tx_doorbell(struct ena_ring *tx_ring)
{
	ena_com_write_sq_doorbell(tx_ring->ena_com_io_sq);
	ena_increase_stat(&tx_ring->tx_stats.doorbells, 1, &tx_ring->syncp);
}

int ena_xmit_common(struct ena_adapter *adapter,
		    struct ena_ring *ring,
		    struct ena_tx_buffer *tx_info,
		    struct ena_com_tx_ctx *ena_tx_ctx,
		    u16 next_to_use,
		    u32 bytes);
void ena_unmap_tx_buff(struct ena_ring *tx_ring,
		       struct ena_tx_buffer *tx_info);
void ena_init_io_rings(struct ena_adapter *adapter,
		       int first_index, int count);
int ena_create_io_tx_queues_in_range(struct ena_adapter *adapter,
				     int first_index, int count);
int ena_setup_tx_resources_in_range(struct ena_adapter *adapter,
				    int first_index, int count);
void ena_free_all_io_tx_resources_in_range(struct ena_adapter *adapter,
					   int first_index, int count);
void ena_free_all_io_tx_resources(struct ena_adapter *adapter);
void ena_down(struct ena_adapter *adapter);
int ena_up(struct ena_adapter *adapter);
void ena_unmask_interrupt(struct ena_ring *tx_ring, struct ena_ring *rx_ring);
void ena_update_ring_numa_node(struct ena_ring *tx_ring,
			       struct ena_ring *rx_ring);
#endif /* !(ENA_H) */
