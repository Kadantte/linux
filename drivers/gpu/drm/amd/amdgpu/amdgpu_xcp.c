/*
 * Copyright 2022 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 */
#include "amdgpu.h"
#include "amdgpu_xcp.h"
#include "amdgpu_drv.h"

#include <drm/drm_drv.h>
#include "../amdxcp/amdgpu_xcp_drv.h"

static void amdgpu_xcp_sysfs_entries_init(struct amdgpu_xcp_mgr *xcp_mgr);
static void amdgpu_xcp_sysfs_entries_update(struct amdgpu_xcp_mgr *xcp_mgr);

static int __amdgpu_xcp_run(struct amdgpu_xcp_mgr *xcp_mgr,
			    struct amdgpu_xcp_ip *xcp_ip, int xcp_state)
{
	int (*run_func)(void *handle, uint32_t inst_mask);
	int ret = 0;

	if (!xcp_ip || !xcp_ip->valid || !xcp_ip->ip_funcs)
		return 0;

	run_func = NULL;

	switch (xcp_state) {
	case AMDGPU_XCP_PREPARE_SUSPEND:
		run_func = xcp_ip->ip_funcs->prepare_suspend;
		break;
	case AMDGPU_XCP_SUSPEND:
		run_func = xcp_ip->ip_funcs->suspend;
		break;
	case AMDGPU_XCP_PREPARE_RESUME:
		run_func = xcp_ip->ip_funcs->prepare_resume;
		break;
	case AMDGPU_XCP_RESUME:
		run_func = xcp_ip->ip_funcs->resume;
		break;
	}

	if (run_func)
		ret = run_func(xcp_mgr->adev, xcp_ip->inst_mask);

	return ret;
}

static int amdgpu_xcp_run_transition(struct amdgpu_xcp_mgr *xcp_mgr, int xcp_id,
				     int state)
{
	struct amdgpu_xcp_ip *xcp_ip;
	struct amdgpu_xcp *xcp;
	int i, ret;

	if (xcp_id >= MAX_XCP || !xcp_mgr->xcp[xcp_id].valid)
		return -EINVAL;

	xcp = &xcp_mgr->xcp[xcp_id];
	for (i = 0; i < AMDGPU_XCP_MAX_BLOCKS; ++i) {
		xcp_ip = &xcp->ip[i];
		ret = __amdgpu_xcp_run(xcp_mgr, xcp_ip, state);
		if (ret)
			break;
	}

	return ret;
}

int amdgpu_xcp_prepare_suspend(struct amdgpu_xcp_mgr *xcp_mgr, int xcp_id)
{
	return amdgpu_xcp_run_transition(xcp_mgr, xcp_id,
					 AMDGPU_XCP_PREPARE_SUSPEND);
}

int amdgpu_xcp_suspend(struct amdgpu_xcp_mgr *xcp_mgr, int xcp_id)
{
	return amdgpu_xcp_run_transition(xcp_mgr, xcp_id, AMDGPU_XCP_SUSPEND);
}

int amdgpu_xcp_prepare_resume(struct amdgpu_xcp_mgr *xcp_mgr, int xcp_id)
{
	return amdgpu_xcp_run_transition(xcp_mgr, xcp_id,
					 AMDGPU_XCP_PREPARE_RESUME);
}

int amdgpu_xcp_resume(struct amdgpu_xcp_mgr *xcp_mgr, int xcp_id)
{
	return amdgpu_xcp_run_transition(xcp_mgr, xcp_id, AMDGPU_XCP_RESUME);
}

static void __amdgpu_xcp_add_block(struct amdgpu_xcp_mgr *xcp_mgr, int xcp_id,
				   struct amdgpu_xcp_ip *ip)
{
	struct amdgpu_xcp *xcp;

	if (!ip)
		return;

	xcp = &xcp_mgr->xcp[xcp_id];
	xcp->ip[ip->ip_id] = *ip;
	xcp->ip[ip->ip_id].valid = true;

	xcp->valid = true;
}

int amdgpu_xcp_init(struct amdgpu_xcp_mgr *xcp_mgr, int num_xcps, int mode)
{
	struct amdgpu_device *adev = xcp_mgr->adev;
	struct amdgpu_xcp_ip ip;
	uint8_t mem_id;
	int i, j, ret;

	if (!num_xcps || num_xcps > MAX_XCP)
		return -EINVAL;

	xcp_mgr->mode = mode;

	for (i = 0; i < MAX_XCP; ++i)
		xcp_mgr->xcp[i].valid = false;

	/* This is needed for figuring out memory id of xcp */
	xcp_mgr->num_xcp_per_mem_partition = num_xcps / xcp_mgr->adev->gmc.num_mem_partitions;

	for (i = 0; i < num_xcps; ++i) {
		for (j = AMDGPU_XCP_GFXHUB; j < AMDGPU_XCP_MAX_BLOCKS; ++j) {
			ret = xcp_mgr->funcs->get_ip_details(xcp_mgr, i, j,
							     &ip);
			if (ret)
				continue;

			__amdgpu_xcp_add_block(xcp_mgr, i, &ip);
		}

		xcp_mgr->xcp[i].id = i;

		if (xcp_mgr->funcs->get_xcp_mem_id) {
			ret = xcp_mgr->funcs->get_xcp_mem_id(
				xcp_mgr, &xcp_mgr->xcp[i], &mem_id);
			if (ret)
				continue;
			else
				xcp_mgr->xcp[i].mem_id = mem_id;
		}
	}

	xcp_mgr->num_xcps = num_xcps;
	amdgpu_xcp_update_partition_sched_list(adev);

	return 0;
}

static int __amdgpu_xcp_switch_partition_mode(struct amdgpu_xcp_mgr *xcp_mgr,
					      int mode)
{
	int ret, curr_mode, num_xcps = 0;

	if (!xcp_mgr->funcs || !xcp_mgr->funcs->switch_partition_mode)
		return 0;

	mutex_lock(&xcp_mgr->xcp_lock);

	curr_mode = xcp_mgr->mode;
	/* State set to transient mode */
	xcp_mgr->mode = AMDGPU_XCP_MODE_TRANS;

	ret = xcp_mgr->funcs->switch_partition_mode(xcp_mgr, mode, &num_xcps);

	if (ret) {
		/* Failed, get whatever mode it's at now */
		if (xcp_mgr->funcs->query_partition_mode)
			xcp_mgr->mode = amdgpu_xcp_query_partition_mode(
				xcp_mgr, AMDGPU_XCP_FL_LOCKED);
		else
			xcp_mgr->mode = curr_mode;

		goto out;
	}
	amdgpu_xcp_sysfs_entries_update(xcp_mgr);
out:
	mutex_unlock(&xcp_mgr->xcp_lock);

	return ret;
}

int amdgpu_xcp_switch_partition_mode(struct amdgpu_xcp_mgr *xcp_mgr, int mode)
{
	if (!xcp_mgr || mode == AMDGPU_XCP_MODE_NONE)
		return -EINVAL;

	if (xcp_mgr->mode == mode)
		return 0;

	return __amdgpu_xcp_switch_partition_mode(xcp_mgr, mode);
}

int amdgpu_xcp_restore_partition_mode(struct amdgpu_xcp_mgr *xcp_mgr)
{
	if (!xcp_mgr || xcp_mgr->mode == AMDGPU_XCP_MODE_NONE)
		return 0;

	return __amdgpu_xcp_switch_partition_mode(xcp_mgr, xcp_mgr->mode);
}

static bool __amdgpu_xcp_is_cached_mode_valid(struct amdgpu_xcp_mgr *xcp_mgr)
{
	if (!xcp_mgr->funcs || !xcp_mgr->funcs->query_partition_mode)
		return true;

	if (!amdgpu_sriov_vf(xcp_mgr->adev) &&
	    xcp_mgr->mode == AMDGPU_XCP_MODE_NONE)
		return true;

	if (xcp_mgr->mode != AMDGPU_XCP_MODE_NONE &&
	    xcp_mgr->mode != AMDGPU_XCP_MODE_TRANS)
		return true;

	return false;
}

int amdgpu_xcp_query_partition_mode(struct amdgpu_xcp_mgr *xcp_mgr, u32 flags)
{
	int mode;

	if (__amdgpu_xcp_is_cached_mode_valid(xcp_mgr))
		return xcp_mgr->mode;

	if (!(flags & AMDGPU_XCP_FL_LOCKED))
		mutex_lock(&xcp_mgr->xcp_lock);
	mode = xcp_mgr->funcs->query_partition_mode(xcp_mgr);

	/* First time query for VF, set the mode here */
	if (amdgpu_sriov_vf(xcp_mgr->adev) &&
	    xcp_mgr->mode == AMDGPU_XCP_MODE_NONE)
		xcp_mgr->mode = mode;

	if (xcp_mgr->mode != AMDGPU_XCP_MODE_TRANS && mode != xcp_mgr->mode)
		dev_WARN(
			xcp_mgr->adev->dev,
			"Cached partition mode %d not matching with device mode %d",
			xcp_mgr->mode, mode);

	if (!(flags & AMDGPU_XCP_FL_LOCKED))
		mutex_unlock(&xcp_mgr->xcp_lock);

	return mode;
}

static int amdgpu_xcp_dev_alloc(struct amdgpu_device *adev)
{
	struct drm_device *p_ddev;
	struct drm_device *ddev;
	int i, ret;

	ddev = adev_to_drm(adev);

	/* xcp #0 shares drm device setting with adev */
	adev->xcp_mgr->xcp->ddev = ddev;

	for (i = 1; i < MAX_XCP; i++) {
		ret = amdgpu_xcp_drm_dev_alloc(&p_ddev);
		if (ret == -ENOSPC) {
			dev_warn(adev->dev,
			"Skip xcp node #%d when out of drm node resource.", i);
			ret = 0;
			goto out;
		} else if (ret) {
			goto out;
		}

		/* Redirect all IOCTLs to the primary device */
		adev->xcp_mgr->xcp[i].rdev = p_ddev->render->dev;
		adev->xcp_mgr->xcp[i].pdev = p_ddev->primary->dev;
		adev->xcp_mgr->xcp[i].driver = (struct drm_driver *)p_ddev->driver;
		adev->xcp_mgr->xcp[i].vma_offset_manager = p_ddev->vma_offset_manager;
		p_ddev->render->dev = ddev;
		p_ddev->primary->dev = ddev;
		p_ddev->vma_offset_manager = ddev->vma_offset_manager;
		p_ddev->driver = &amdgpu_partition_driver;
		adev->xcp_mgr->xcp[i].ddev = p_ddev;

		dev_set_drvdata(p_ddev->dev, &adev->xcp_mgr->xcp[i]);
	}
	ret = 0;
out:
	amdgpu_xcp_sysfs_entries_init(adev->xcp_mgr);

	return ret;
}

int amdgpu_xcp_mgr_init(struct amdgpu_device *adev, int init_mode,
			int init_num_xcps,
			struct amdgpu_xcp_mgr_funcs *xcp_funcs)
{
	struct amdgpu_xcp_mgr *xcp_mgr;
	int i;

	if (!xcp_funcs || !xcp_funcs->get_ip_details)
		return -EINVAL;

	xcp_mgr = kzalloc(sizeof(*xcp_mgr), GFP_KERNEL);

	if (!xcp_mgr)
		return -ENOMEM;

	xcp_mgr->adev = adev;
	xcp_mgr->funcs = xcp_funcs;
	xcp_mgr->mode = init_mode;
	mutex_init(&xcp_mgr->xcp_lock);

	if (init_mode != AMDGPU_XCP_MODE_NONE)
		amdgpu_xcp_init(xcp_mgr, init_num_xcps, init_mode);

	adev->xcp_mgr = xcp_mgr;
	for (i = 0; i < MAX_XCP; ++i)
		xcp_mgr->xcp[i].xcp_mgr = xcp_mgr;

	return amdgpu_xcp_dev_alloc(adev);
}

int amdgpu_xcp_get_partition(struct amdgpu_xcp_mgr *xcp_mgr,
			     enum AMDGPU_XCP_IP_BLOCK ip, int instance)
{
	struct amdgpu_xcp *xcp;
	int i, id_mask = 0;

	if (ip >= AMDGPU_XCP_MAX_BLOCKS)
		return -EINVAL;

	for (i = 0; i < xcp_mgr->num_xcps; ++i) {
		xcp = &xcp_mgr->xcp[i];
		if ((xcp->valid) && (xcp->ip[ip].valid) &&
		    (xcp->ip[ip].inst_mask & BIT(instance)))
			id_mask |= BIT(i);
	}

	if (!id_mask)
		id_mask = -ENXIO;

	return id_mask;
}

int amdgpu_xcp_get_inst_details(struct amdgpu_xcp *xcp,
				enum AMDGPU_XCP_IP_BLOCK ip,
				uint32_t *inst_mask)
{
	if (!xcp->valid || !inst_mask || !(xcp->ip[ip].valid))
		return -EINVAL;

	*inst_mask = xcp->ip[ip].inst_mask;

	return 0;
}

int amdgpu_xcp_dev_register(struct amdgpu_device *adev,
			const struct pci_device_id *ent)
{
	int i, ret;

	if (!adev->xcp_mgr)
		return 0;

	for (i = 1; i < MAX_XCP; i++) {
		if (!adev->xcp_mgr->xcp[i].ddev)
			break;

		ret = drm_dev_register(adev->xcp_mgr->xcp[i].ddev, ent->driver_data);
		if (ret)
			return ret;
	}

	return 0;
}

void amdgpu_xcp_dev_unplug(struct amdgpu_device *adev)
{
	struct drm_device *p_ddev;
	int i;

	if (!adev->xcp_mgr)
		return;

	for (i = 1; i < MAX_XCP; i++) {
		if (!adev->xcp_mgr->xcp[i].ddev)
			break;

		p_ddev = adev->xcp_mgr->xcp[i].ddev;
		drm_dev_unplug(p_ddev);
		p_ddev->render->dev = adev->xcp_mgr->xcp[i].rdev;
		p_ddev->primary->dev = adev->xcp_mgr->xcp[i].pdev;
		p_ddev->driver =  adev->xcp_mgr->xcp[i].driver;
		p_ddev->vma_offset_manager = adev->xcp_mgr->xcp[i].vma_offset_manager;
	}
}

int amdgpu_xcp_open_device(struct amdgpu_device *adev,
			   struct amdgpu_fpriv *fpriv,
			   struct drm_file *file_priv)
{
	int i;

	if (!adev->xcp_mgr)
		return 0;

	fpriv->xcp_id = AMDGPU_XCP_NO_PARTITION;
	for (i = 0; i < MAX_XCP; ++i) {
		if (!adev->xcp_mgr->xcp[i].ddev)
			break;

		if (file_priv->minor == adev->xcp_mgr->xcp[i].ddev->render) {
			if (adev->xcp_mgr->xcp[i].valid == FALSE) {
				dev_err(adev->dev, "renderD%d partition %d not valid!",
						file_priv->minor->index, i);
				return -ENOENT;
			}
			dev_dbg(adev->dev, "renderD%d partition %d opened!",
					file_priv->minor->index, i);
			fpriv->xcp_id = i;
			break;
		}
	}

	fpriv->vm.mem_id = fpriv->xcp_id == AMDGPU_XCP_NO_PARTITION ? -1 :
				adev->xcp_mgr->xcp[fpriv->xcp_id].mem_id;
	return 0;
}

void amdgpu_xcp_release_sched(struct amdgpu_device *adev,
				  struct amdgpu_ctx_entity *entity)
{
	struct drm_gpu_scheduler *sched;
	struct amdgpu_ring *ring;

	if (!adev->xcp_mgr)
		return;

	sched = entity->entity.rq->sched;
	if (drm_sched_wqueue_ready(sched)) {
		ring = to_amdgpu_ring(entity->entity.rq->sched);
		atomic_dec(&adev->xcp_mgr->xcp[ring->xcp_id].ref_cnt);
	}
}

int amdgpu_xcp_select_scheds(struct amdgpu_device *adev,
			     u32 hw_ip, u32 hw_prio,
			     struct amdgpu_fpriv *fpriv,
			     unsigned int *num_scheds,
			     struct drm_gpu_scheduler ***scheds)
{
	u32 sel_xcp_id;
	int i;
	struct amdgpu_xcp_mgr *xcp_mgr = adev->xcp_mgr;

	if (fpriv->xcp_id == AMDGPU_XCP_NO_PARTITION) {
		u32 least_ref_cnt = ~0;

		fpriv->xcp_id = 0;
		for (i = 0; i < xcp_mgr->num_xcps; i++) {
			u32 total_ref_cnt;

			total_ref_cnt = atomic_read(&xcp_mgr->xcp[i].ref_cnt);
			if (total_ref_cnt < least_ref_cnt) {
				fpriv->xcp_id = i;
				least_ref_cnt = total_ref_cnt;
			}
		}
	}
	sel_xcp_id = fpriv->xcp_id;

	if (xcp_mgr->xcp[sel_xcp_id].gpu_sched[hw_ip][hw_prio].num_scheds) {
		*num_scheds =
			xcp_mgr->xcp[fpriv->xcp_id].gpu_sched[hw_ip][hw_prio].num_scheds;
		*scheds =
			xcp_mgr->xcp[fpriv->xcp_id].gpu_sched[hw_ip][hw_prio].sched;
		atomic_inc(&adev->xcp_mgr->xcp[sel_xcp_id].ref_cnt);
		dev_dbg(adev->dev, "Selected partition #%d", sel_xcp_id);
	} else {
		dev_err(adev->dev, "Failed to schedule partition #%d.", sel_xcp_id);
		return -ENOENT;
	}

	return 0;
}

static void amdgpu_set_xcp_id(struct amdgpu_device *adev,
			      uint32_t inst_idx,
			      struct amdgpu_ring *ring)
{
	int xcp_id;
	enum AMDGPU_XCP_IP_BLOCK ip_blk;
	uint32_t inst_mask;

	ring->xcp_id = AMDGPU_XCP_NO_PARTITION;
	if (ring->funcs->type == AMDGPU_RING_TYPE_COMPUTE)
		adev->gfx.enforce_isolation[0].xcp_id = ring->xcp_id;
	if ((adev->xcp_mgr->mode == AMDGPU_XCP_MODE_NONE) ||
	    (ring->funcs->type == AMDGPU_RING_TYPE_CPER))
		return;

	inst_mask = 1 << inst_idx;

	switch (ring->funcs->type) {
	case AMDGPU_HW_IP_GFX:
	case AMDGPU_RING_TYPE_COMPUTE:
	case AMDGPU_RING_TYPE_KIQ:
		ip_blk = AMDGPU_XCP_GFX;
		break;
	case AMDGPU_RING_TYPE_SDMA:
		ip_blk = AMDGPU_XCP_SDMA;
		break;
	case AMDGPU_RING_TYPE_VCN_ENC:
	case AMDGPU_RING_TYPE_VCN_JPEG:
		ip_blk = AMDGPU_XCP_VCN;
		break;
	default:
		dev_err(adev->dev, "Not support ring type %d!", ring->funcs->type);
		return;
	}

	for (xcp_id = 0; xcp_id < adev->xcp_mgr->num_xcps; xcp_id++) {
		if (adev->xcp_mgr->xcp[xcp_id].ip[ip_blk].inst_mask & inst_mask) {
			ring->xcp_id = xcp_id;
			dev_dbg(adev->dev, "ring:%s xcp_id :%u", ring->name,
				ring->xcp_id);
			if (ring->funcs->type == AMDGPU_RING_TYPE_COMPUTE)
				adev->gfx.enforce_isolation[xcp_id].xcp_id = xcp_id;
			break;
		}
	}
}

static void amdgpu_xcp_gpu_sched_update(struct amdgpu_device *adev,
					struct amdgpu_ring *ring,
					unsigned int sel_xcp_id)
{
	unsigned int *num_gpu_sched;

	num_gpu_sched = &adev->xcp_mgr->xcp[sel_xcp_id]
			.gpu_sched[ring->funcs->type][ring->hw_prio].num_scheds;
	adev->xcp_mgr->xcp[sel_xcp_id].gpu_sched[ring->funcs->type][ring->hw_prio]
			.sched[(*num_gpu_sched)++] = &ring->sched;
	dev_dbg(adev->dev, "%s :[%d] gpu_sched[%d][%d] = %d",
		ring->name, sel_xcp_id, ring->funcs->type,
		ring->hw_prio, *num_gpu_sched);
}

static int amdgpu_xcp_sched_list_update(struct amdgpu_device *adev)
{
	struct amdgpu_ring *ring;
	int i;

	for (i = 0; i < MAX_XCP; i++) {
		atomic_set(&adev->xcp_mgr->xcp[i].ref_cnt, 0);
		memset(adev->xcp_mgr->xcp[i].gpu_sched, 0, sizeof(adev->xcp_mgr->xcp->gpu_sched));
	}

	if (adev->xcp_mgr->mode == AMDGPU_XCP_MODE_NONE)
		return 0;

	for (i = 0; i < AMDGPU_MAX_RINGS; i++) {
		ring = adev->rings[i];
		if (!ring || !ring->sched.ready || ring->no_scheduler)
			continue;

		amdgpu_xcp_gpu_sched_update(adev, ring, ring->xcp_id);

		/* VCN may be shared by two partitions under CPX MODE in certain
		 * configs.
		 */
		if ((ring->funcs->type == AMDGPU_RING_TYPE_VCN_ENC ||
		     ring->funcs->type == AMDGPU_RING_TYPE_VCN_JPEG) &&
		    (adev->xcp_mgr->num_xcps > adev->vcn.num_vcn_inst))
			amdgpu_xcp_gpu_sched_update(adev, ring, ring->xcp_id + 1);
	}

	return 0;
}

int amdgpu_xcp_update_partition_sched_list(struct amdgpu_device *adev)
{
	int i;

	for (i = 0; i < adev->num_rings; i++) {
		struct amdgpu_ring *ring = adev->rings[i];

		if (ring->funcs->type == AMDGPU_RING_TYPE_COMPUTE ||
			ring->funcs->type == AMDGPU_RING_TYPE_KIQ)
			amdgpu_set_xcp_id(adev, ring->xcc_id, ring);
		else
			amdgpu_set_xcp_id(adev, ring->me, ring);
	}

	return amdgpu_xcp_sched_list_update(adev);
}

void amdgpu_xcp_update_supported_modes(struct amdgpu_xcp_mgr *xcp_mgr)
{
	struct amdgpu_device *adev = xcp_mgr->adev;

	xcp_mgr->supp_xcp_modes = 0;

	switch (NUM_XCC(adev->gfx.xcc_mask)) {
	case 8:
		xcp_mgr->supp_xcp_modes = BIT(AMDGPU_SPX_PARTITION_MODE) |
					  BIT(AMDGPU_DPX_PARTITION_MODE) |
					  BIT(AMDGPU_QPX_PARTITION_MODE) |
					  BIT(AMDGPU_CPX_PARTITION_MODE);
		break;
	case 6:
		xcp_mgr->supp_xcp_modes = BIT(AMDGPU_SPX_PARTITION_MODE) |
					  BIT(AMDGPU_TPX_PARTITION_MODE) |
					  BIT(AMDGPU_CPX_PARTITION_MODE);
		break;
	case 4:
		xcp_mgr->supp_xcp_modes = BIT(AMDGPU_SPX_PARTITION_MODE) |
					  BIT(AMDGPU_DPX_PARTITION_MODE) |
					  BIT(AMDGPU_CPX_PARTITION_MODE);
		break;
	case 2:
		xcp_mgr->supp_xcp_modes = BIT(AMDGPU_SPX_PARTITION_MODE) |
					  BIT(AMDGPU_CPX_PARTITION_MODE);
		break;
	case 1:
		xcp_mgr->supp_xcp_modes = BIT(AMDGPU_SPX_PARTITION_MODE) |
					  BIT(AMDGPU_CPX_PARTITION_MODE);
		break;

	default:
		break;
	}
}

int amdgpu_xcp_pre_partition_switch(struct amdgpu_xcp_mgr *xcp_mgr, u32 flags)
{
	/* TODO:
	 * Stop user queues and threads, and make sure GPU is empty of work.
	 */

	if (flags & AMDGPU_XCP_OPS_KFD)
		amdgpu_amdkfd_device_fini_sw(xcp_mgr->adev);

	return 0;
}

int amdgpu_xcp_post_partition_switch(struct amdgpu_xcp_mgr *xcp_mgr, u32 flags)
{
	int ret = 0;

	if (flags & AMDGPU_XCP_OPS_KFD) {
		amdgpu_amdkfd_device_probe(xcp_mgr->adev);
		amdgpu_amdkfd_device_init(xcp_mgr->adev);
		/* If KFD init failed, return failure */
		if (!xcp_mgr->adev->kfd.init_complete)
			ret = -EIO;
	}

	return ret;
}

/*====================== xcp sysfs - configuration ======================*/
#define XCP_CFG_SYSFS_RES_ATTR_SHOW(_name)                         \
	static ssize_t amdgpu_xcp_res_sysfs_##_name##_show(        \
		struct amdgpu_xcp_res_details *xcp_res, char *buf) \
	{                                                          \
		return sysfs_emit(buf, "%d\n", xcp_res->_name);    \
	}

struct amdgpu_xcp_res_sysfs_attribute {
	struct attribute attr;
	ssize_t (*show)(struct amdgpu_xcp_res_details *xcp_res, char *buf);
};

#define XCP_CFG_SYSFS_RES_ATTR(_name)                                        \
	struct amdgpu_xcp_res_sysfs_attribute xcp_res_sysfs_attr_##_name = { \
		.attr = { .name = __stringify(_name), .mode = 0400 },        \
		.show = amdgpu_xcp_res_sysfs_##_name##_show,                 \
	}

XCP_CFG_SYSFS_RES_ATTR_SHOW(num_inst)
XCP_CFG_SYSFS_RES_ATTR(num_inst);
XCP_CFG_SYSFS_RES_ATTR_SHOW(num_shared)
XCP_CFG_SYSFS_RES_ATTR(num_shared);

#define XCP_CFG_SYSFS_RES_ATTR_PTR(_name) xcp_res_sysfs_attr_##_name.attr

static struct attribute *xcp_cfg_res_sysfs_attrs[] = {
	&XCP_CFG_SYSFS_RES_ATTR_PTR(num_inst),
	&XCP_CFG_SYSFS_RES_ATTR_PTR(num_shared), NULL
};

static const char *xcp_desc[] = {
	[AMDGPU_SPX_PARTITION_MODE] = "SPX",
	[AMDGPU_DPX_PARTITION_MODE] = "DPX",
	[AMDGPU_TPX_PARTITION_MODE] = "TPX",
	[AMDGPU_QPX_PARTITION_MODE] = "QPX",
	[AMDGPU_CPX_PARTITION_MODE] = "CPX",
};

static const char *nps_desc[] = {
	[UNKNOWN_MEMORY_PARTITION_MODE] = "UNKNOWN",
	[AMDGPU_NPS1_PARTITION_MODE] = "NPS1",
	[AMDGPU_NPS2_PARTITION_MODE] = "NPS2",
	[AMDGPU_NPS3_PARTITION_MODE] = "NPS3",
	[AMDGPU_NPS4_PARTITION_MODE] = "NPS4",
	[AMDGPU_NPS6_PARTITION_MODE] = "NPS6",
	[AMDGPU_NPS8_PARTITION_MODE] = "NPS8",
};

ATTRIBUTE_GROUPS(xcp_cfg_res_sysfs);

#define to_xcp_attr(x) \
	container_of(x, struct amdgpu_xcp_res_sysfs_attribute, attr)
#define to_xcp_res(x) container_of(x, struct amdgpu_xcp_res_details, kobj)

static ssize_t xcp_cfg_res_sysfs_attr_show(struct kobject *kobj,
					   struct attribute *attr, char *buf)
{
	struct amdgpu_xcp_res_sysfs_attribute *attribute;
	struct amdgpu_xcp_res_details *xcp_res;

	attribute = to_xcp_attr(attr);
	xcp_res = to_xcp_res(kobj);

	if (!attribute->show)
		return -EIO;

	return attribute->show(xcp_res, buf);
}

static const struct sysfs_ops xcp_cfg_res_sysfs_ops = {
	.show = xcp_cfg_res_sysfs_attr_show,
};

static const struct kobj_type xcp_cfg_res_sysfs_ktype = {
	.sysfs_ops = &xcp_cfg_res_sysfs_ops,
	.default_groups = xcp_cfg_res_sysfs_groups,
};

const char *xcp_res_names[] = {
	[AMDGPU_XCP_RES_XCC] = "xcc",
	[AMDGPU_XCP_RES_DMA] = "dma",
	[AMDGPU_XCP_RES_DEC] = "dec",
	[AMDGPU_XCP_RES_JPEG] = "jpeg",
};

static int amdgpu_xcp_get_res_info(struct amdgpu_xcp_mgr *xcp_mgr,
				   int mode,
				   struct amdgpu_xcp_cfg *xcp_cfg)
{
	if (xcp_mgr->funcs && xcp_mgr->funcs->get_xcp_res_info)
		return xcp_mgr->funcs->get_xcp_res_info(xcp_mgr, mode, xcp_cfg);

	return -EOPNOTSUPP;
}

#define to_xcp_cfg(x) container_of(x, struct amdgpu_xcp_cfg, kobj)
static ssize_t supported_xcp_configs_show(struct kobject *kobj,
					  struct kobj_attribute *attr, char *buf)
{
	struct amdgpu_xcp_cfg *xcp_cfg = to_xcp_cfg(kobj);
	struct amdgpu_xcp_mgr *xcp_mgr = xcp_cfg->xcp_mgr;
	int size = 0, mode;
	char *sep = "";

	if (!xcp_mgr || !xcp_mgr->supp_xcp_modes)
		return sysfs_emit(buf, "Not supported\n");

	for_each_inst(mode, xcp_mgr->supp_xcp_modes) {
		size += sysfs_emit_at(buf, size, "%s%s", sep, xcp_desc[mode]);
		sep = ", ";
	}

	size += sysfs_emit_at(buf, size, "\n");

	return size;
}

static ssize_t supported_nps_configs_show(struct kobject *kobj,
					  struct kobj_attribute *attr, char *buf)
{
	struct amdgpu_xcp_cfg *xcp_cfg = to_xcp_cfg(kobj);
	int size = 0, mode;
	char *sep = "";

	if (!xcp_cfg || !xcp_cfg->compatible_nps_modes)
		return sysfs_emit(buf, "Not supported\n");

	for_each_inst(mode, xcp_cfg->compatible_nps_modes) {
		size += sysfs_emit_at(buf, size, "%s%s", sep, nps_desc[mode]);
		sep = ", ";
	}

	size += sysfs_emit_at(buf, size, "\n");

	return size;
}

static ssize_t xcp_config_show(struct kobject *kobj,
			       struct kobj_attribute *attr, char *buf)
{
	struct amdgpu_xcp_cfg *xcp_cfg = to_xcp_cfg(kobj);

	return sysfs_emit(buf, "%s\n",
			  amdgpu_gfx_compute_mode_desc(xcp_cfg->mode));
}

static ssize_t xcp_config_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf, size_t size)
{
	struct amdgpu_xcp_cfg *xcp_cfg = to_xcp_cfg(kobj);
	int mode, r;

	if (!strncasecmp("SPX", buf, strlen("SPX")))
		mode = AMDGPU_SPX_PARTITION_MODE;
	else if (!strncasecmp("DPX", buf, strlen("DPX")))
		mode = AMDGPU_DPX_PARTITION_MODE;
	else if (!strncasecmp("TPX", buf, strlen("TPX")))
		mode = AMDGPU_TPX_PARTITION_MODE;
	else if (!strncasecmp("QPX", buf, strlen("QPX")))
		mode = AMDGPU_QPX_PARTITION_MODE;
	else if (!strncasecmp("CPX", buf, strlen("CPX")))
		mode = AMDGPU_CPX_PARTITION_MODE;
	else
		return -EINVAL;

	r = amdgpu_xcp_get_res_info(xcp_cfg->xcp_mgr, mode, xcp_cfg);

	if (r)
		return r;

	xcp_cfg->mode = mode;
	return size;
}

static struct kobj_attribute xcp_cfg_sysfs_mode =
	__ATTR_RW_MODE(xcp_config, 0644);

static void xcp_cfg_sysfs_release(struct kobject *kobj)
{
	struct amdgpu_xcp_cfg *xcp_cfg = to_xcp_cfg(kobj);

	kfree(xcp_cfg);
}

static const struct kobj_type xcp_cfg_sysfs_ktype = {
	.release = xcp_cfg_sysfs_release,
	.sysfs_ops = &kobj_sysfs_ops,
};

static struct kobj_attribute supp_part_sysfs_mode =
	__ATTR_RO(supported_xcp_configs);

static struct kobj_attribute supp_nps_sysfs_mode =
	__ATTR_RO(supported_nps_configs);

static const struct attribute *xcp_attrs[] = {
	&supp_part_sysfs_mode.attr,
	&xcp_cfg_sysfs_mode.attr,
	NULL,
};

static void amdgpu_xcp_cfg_sysfs_init(struct amdgpu_device *adev)
{
	struct amdgpu_xcp_res_details *xcp_res;
	struct amdgpu_xcp_cfg *xcp_cfg;
	int i, r, j, rid, mode;

	if (!adev->xcp_mgr)
		return;

	xcp_cfg = kzalloc(sizeof(*xcp_cfg), GFP_KERNEL);
	if (!xcp_cfg)
		return;
	xcp_cfg->xcp_mgr = adev->xcp_mgr;

	r = kobject_init_and_add(&xcp_cfg->kobj, &xcp_cfg_sysfs_ktype,
				 &adev->dev->kobj, "compute_partition_config");
	if (r)
		goto err1;

	r = sysfs_create_files(&xcp_cfg->kobj, xcp_attrs);
	if (r)
		goto err1;

	if (adev->gmc.supported_nps_modes != 0) {
		r = sysfs_create_file(&xcp_cfg->kobj, &supp_nps_sysfs_mode.attr);
		if (r) {
			sysfs_remove_files(&xcp_cfg->kobj, xcp_attrs);
			goto err1;
		}
	}

	mode = (xcp_cfg->xcp_mgr->mode ==
		AMDGPU_UNKNOWN_COMPUTE_PARTITION_MODE) ?
		       AMDGPU_SPX_PARTITION_MODE :
		       xcp_cfg->xcp_mgr->mode;
	r = amdgpu_xcp_get_res_info(xcp_cfg->xcp_mgr, mode, xcp_cfg);
	if (r) {
		sysfs_remove_file(&xcp_cfg->kobj, &supp_nps_sysfs_mode.attr);
		sysfs_remove_files(&xcp_cfg->kobj, xcp_attrs);
		goto err1;
	}

	xcp_cfg->mode = mode;
	for (i = 0; i < xcp_cfg->num_res; i++) {
		xcp_res = &xcp_cfg->xcp_res[i];
		rid = xcp_res->id;
		r = kobject_init_and_add(&xcp_res->kobj,
					 &xcp_cfg_res_sysfs_ktype,
					 &xcp_cfg->kobj, "%s",
					 xcp_res_names[rid]);
		if (r)
			goto err;
	}

	adev->xcp_mgr->xcp_cfg = xcp_cfg;
	return;
err:
	for (j = 0; j < i; j++) {
		xcp_res = &xcp_cfg->xcp_res[i];
		kobject_put(&xcp_res->kobj);
	}

	sysfs_remove_file(&xcp_cfg->kobj, &supp_nps_sysfs_mode.attr);
	sysfs_remove_files(&xcp_cfg->kobj, xcp_attrs);
err1:
	kobject_put(&xcp_cfg->kobj);
}

static void amdgpu_xcp_cfg_sysfs_fini(struct amdgpu_device *adev)
{
	struct amdgpu_xcp_res_details *xcp_res;
	struct amdgpu_xcp_cfg *xcp_cfg;
	int i;

	if (!adev->xcp_mgr || !adev->xcp_mgr->xcp_cfg)
		return;

	xcp_cfg = adev->xcp_mgr->xcp_cfg;
	for (i = 0; i < xcp_cfg->num_res; i++) {
		xcp_res = &xcp_cfg->xcp_res[i];
		kobject_put(&xcp_res->kobj);
	}

	sysfs_remove_file(&xcp_cfg->kobj, &supp_nps_sysfs_mode.attr);
	sysfs_remove_files(&xcp_cfg->kobj, xcp_attrs);
	kobject_put(&xcp_cfg->kobj);
}

/*====================== xcp sysfs - data entries ======================*/

#define to_xcp(x) container_of(x, struct amdgpu_xcp, kobj)

static ssize_t xcp_metrics_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	struct amdgpu_xcp *xcp = to_xcp(kobj);
	struct amdgpu_xcp_mgr *xcp_mgr;
	ssize_t size;

	xcp_mgr = xcp->xcp_mgr;
	size = amdgpu_dpm_get_xcp_metrics(xcp_mgr->adev, xcp->id, NULL);
	if (size <= 0)
		return size;

	if (size > PAGE_SIZE)
		return -ENOSPC;

	return amdgpu_dpm_get_xcp_metrics(xcp_mgr->adev, xcp->id, buf);
}

static umode_t amdgpu_xcp_attrs_is_visible(struct kobject *kobj,
					   struct attribute *attr, int n)
{
	struct amdgpu_xcp *xcp = to_xcp(kobj);

	if (!xcp || !xcp->valid)
		return 0;

	return attr->mode;
}

static struct kobj_attribute xcp_sysfs_metrics = __ATTR_RO(xcp_metrics);

static struct attribute *amdgpu_xcp_attrs[] = {
	&xcp_sysfs_metrics.attr,
	NULL,
};

static const struct attribute_group amdgpu_xcp_attrs_group = {
	.attrs = amdgpu_xcp_attrs,
	.is_visible = amdgpu_xcp_attrs_is_visible
};

static const struct kobj_type xcp_sysfs_ktype = {
	.sysfs_ops = &kobj_sysfs_ops,
};

static void amdgpu_xcp_sysfs_entries_fini(struct amdgpu_xcp_mgr *xcp_mgr, int n)
{
	struct amdgpu_xcp *xcp;

	for (n--; n >= 0; n--) {
		xcp = &xcp_mgr->xcp[n];
		if (!xcp->ddev || !xcp->valid)
			continue;
		sysfs_remove_group(&xcp->kobj, &amdgpu_xcp_attrs_group);
		kobject_put(&xcp->kobj);
	}
}

static void amdgpu_xcp_sysfs_entries_init(struct amdgpu_xcp_mgr *xcp_mgr)
{
	struct amdgpu_xcp *xcp;
	int i, r;

	for (i = 0; i < MAX_XCP; i++) {
		/* Redirect all IOCTLs to the primary device */
		xcp = &xcp_mgr->xcp[i];
		if (!xcp->ddev)
			break;
		r = kobject_init_and_add(&xcp->kobj, &xcp_sysfs_ktype,
					 &xcp->ddev->dev->kobj, "xcp");
		if (r)
			goto out;

		r = sysfs_create_group(&xcp->kobj, &amdgpu_xcp_attrs_group);
		if (r)
			goto out;
	}

	return;
out:
	kobject_put(&xcp->kobj);
}

static void amdgpu_xcp_sysfs_entries_update(struct amdgpu_xcp_mgr *xcp_mgr)
{
	struct amdgpu_xcp *xcp;
	int i;

	for (i = 0; i < MAX_XCP; i++) {
		/* Redirect all IOCTLs to the primary device */
		xcp = &xcp_mgr->xcp[i];
		if (!xcp->ddev)
			continue;
		sysfs_update_group(&xcp->kobj, &amdgpu_xcp_attrs_group);
	}

	return;
}

void amdgpu_xcp_sysfs_init(struct amdgpu_device *adev)
{
	if (!adev->xcp_mgr)
		return;

	amdgpu_xcp_cfg_sysfs_init(adev);

	return;
}

void amdgpu_xcp_sysfs_fini(struct amdgpu_device *adev)
{
	if (!adev->xcp_mgr)
		return;
	amdgpu_xcp_sysfs_entries_fini(adev->xcp_mgr, MAX_XCP);
	amdgpu_xcp_cfg_sysfs_fini(adev);
}
