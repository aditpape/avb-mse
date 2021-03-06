/*************************************************************************/ /*
 avb-mse

 Copyright (C) 2015-2017 Renesas Electronics Corporation

 License        Dual MIT/GPLv2

 The contents of this file are subject to the MIT license as set out below.

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 Alternatively, the contents of this file may be used under the terms of
 the GNU General Public License Version 2 ("GPL") in which case the provisions
 of GPL are applicable instead of those above.

 If you wish to allow use of your version of this file only under the terms of
 GPL, and not to allow others to use your version of this file under the terms
 of the MIT license, indicate your decision by deleting the provisions above
 and replace them with the notice and other provisions required by GPL as set
 out in the file called "GPL-COPYING" included in this distribution. If you do
 not delete the provisions above, a recipient may use your version of this file
 under the terms of either the MIT license or GPL.

 This License is also included in this distribution in the file called
 "MIT-COPYING".

 EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
 PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
 COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


 GPLv2:
 If you wish to use this file under the terms of GPL, following terms are
 effective.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/ /*************************************************************************/

#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME "/" fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/if_vlan.h>

#include "ravb_mse_kernel.h"
#include "mse_packetizer.h"
#include "mse_packet_ctrl.h"
#include "avtp.h"

#define MSE_DMA_BUF_RECEIVE_SIZE 10
#define MSE_DMA_BUF_SEND_SIZE 10

#define MSE_PACKET_COUNT_MAX (128)

/* Checking the difference between read_p and write_p */
int mse_packet_ctrl_check_packet_remain(struct mse_packet_ctrl *dma)
{
	return (dma->write_p + dma->size - dma->read_p) % dma->size;
}

struct mse_packet_ctrl *mse_packet_ctrl_alloc(struct device *dev,
					      int max_packet,
					      int max_packet_size)
{
	struct mse_packet_ctrl *dma;
	dma_addr_t paddr, pitch;
	int i;

	mse_debug("packets=%d size=%d", max_packet, max_packet_size);

	dma = kmalloc(sizeof(*dma), GFP_KERNEL);
	if (!dma)
		return NULL;

	dma->dma_vaddr = dma_alloc_coherent(dev,
					    max_packet_size * max_packet,
					    &dma->dma_handle,
					    GFP_KERNEL);
	if (!dma->dma_vaddr) {
		mse_err("cannot dma_alloc_coherent!\n");
		kfree(dma);
		return NULL;
	}

	dma->dev = dev;
	dma->size = max_packet;
	dma->write_p = 0;
	dma->read_p = 0;
	dma->max_packet_size = max_packet_size;
	dma->packet_table = kmalloc((sizeof(struct mse_packet) * dma->size),
				    GFP_KERNEL);

	paddr = dma->dma_handle;
	for (i = 0; i < dma->size; i++) {
		pitch = dma->max_packet_size * i;
		dma->packet_table[i].len = dma->max_packet_size;
		dma->packet_table[i].paddr = paddr + pitch;
		dma->packet_table[i].vaddr = dma->dma_vaddr + pitch;
	}

	return dma;
}

void mse_packet_ctrl_free(struct mse_packet_ctrl *dma)
{
	if (!dma)
		return;

	mse_debug("START\n");

	kfree(dma->packet_table);
	dma_free_coherent(dma->dev,
			  dma->size * dma->max_packet_size,
			  dma->dma_vaddr,
			  dma->dma_handle);
	kfree(dma);
}

int mse_packet_ctrl_make_packet(int index,
				void *data,
				size_t size,
				int ptp_clock,
				int *current_timestamp,
				int tstamp_size,
				unsigned int *tstamp,
				struct mse_packet_ctrl *dma,
				struct mse_packetizer_ops *ops,
				size_t *processed)
{
	int ret = MSE_PACKETIZE_STATUS_CONTINUE;
	size_t packet_size = 0;
	int new_write_p;
	int pcount = 0, pcount_max;
	unsigned int timestamp;
	int packetized;

	packetized = mse_packet_ctrl_check_packet_remain(dma);
	pcount_max = min(dma->size - packetized - 1, MSE_PACKET_COUNT_MAX);
	while ((ret == MSE_PACKETIZE_STATUS_CONTINUE) &&
	       (pcount < pcount_max)) {
		new_write_p = (dma->write_p + 1) % dma->size;
		if (new_write_p == dma->read_p) {
			mse_debug("make overrun r=%d w=%d nw=%d p=%zu/%zu\n",
				  dma->read_p, dma->write_p, new_write_p,
				  *processed, size);
			return *processed;
		}
		memset(dma->packet_table[dma->write_p].vaddr, 0,
		       AVTP_FRAME_SIZE_MIN);
		if (tstamp_size == 1) {               /* video */
			timestamp = tstamp[0];
		} else {                              /* audio */
			if (*current_timestamp < tstamp_size) {
				timestamp = tstamp[(*current_timestamp)++];
			} else if (*current_timestamp == tstamp_size) {
				timestamp = 0;      /* dummy, not used */
				(*current_timestamp)++;
			} else {
				mse_err("not enough timestamp %d", tstamp_size);
				return -EINVAL;
			}
		}

		ret = ops->packetize(index,
				     dma->packet_table[dma->write_p].vaddr,
				     &packet_size,
				     data,
				     size,
				     processed,
				     &timestamp);

		if (ret >= 0 && ret != MSE_PACKETIZE_STATUS_NOT_ENOUGH) {
			pcount++;
			if (packet_size < AVTP_FRAME_SIZE_MIN)
				packet_size = AVTP_FRAME_SIZE_MIN;
			dma->packet_table[dma->write_p].len = packet_size;

			dma->write_p = new_write_p;
		} else if (ret < 0) {
			return ret;
		} else {
			break;
		}
	}
	mse_debug("packetize %d %zu/%zu\n", pcount, *processed, size);

	return *processed;
}

int mse_packet_ctrl_make_packet_crf(int index,
				    u64 *timestamps,
				    int count,
				    struct mse_packet_ctrl *dma)
{
	int ret = MSE_PACKETIZE_STATUS_CONTINUE;
	size_t packet_size = 0;
	int new_write_p;

	new_write_p = (dma->write_p + 1) % dma->size;
	if (new_write_p == dma->read_p) {
		mse_err("make overrun r=%d w=%d nw=%d\n",
			dma->read_p, dma->write_p, new_write_p);
		return -ENOSPC;
	}

	memset(dma->packet_table[dma->write_p].vaddr, 0, AVTP_FRAME_SIZE_MIN);

	/* CRF packetizer */
	ret = mse_packetizer_crf_tstamp_audio_ops.packetize(
		index,
		dma->packet_table[dma->write_p].vaddr,
		&packet_size,
		timestamps,
		count * sizeof(*timestamps),
		NULL,
		NULL);

	if (ret >= 0) {
		if (packet_size < AVTP_FRAME_SIZE_MIN)
			packet_size = AVTP_FRAME_SIZE_MIN;
		dma->packet_table[dma->write_p].len = packet_size;

		dma->write_p = new_write_p;
	}

	return MSE_PACKETIZE_STATUS_COMPLETE;
}

int mse_packet_ctrl_send_prepare_packet(
				int index,
				struct mse_packet_ctrl *dma,
				struct mse_adapter_network_ops *ops)
{
	return ops->send_prepare(index,
				 dma->packet_table,
				 dma->size);
}

int mse_packet_ctrl_send_packet(int index,
				struct mse_packet_ctrl *dma,
				struct mse_adapter_network_ops *ops)
{
	int read_p, ret, send_size;
	int packetized;

	packetized = mse_packet_ctrl_check_packet_remain(dma);
	send_size = min(packetized, MSE_PACKET_COUNT_MAX);

	if (!send_size)
		return 0;

	/* send packets */
	ret = ops->send(index, dma->packet_table, send_size);
	if (ret < 0)
		return -EPERM;

	read_p = dma->read_p; /* for debug */
	dma->read_p = (dma->read_p + ret) % dma->size;

	mse_debug("%d packtets w=%d r=%d->%d\n",
		  ret, dma->write_p, read_p, dma->read_p);

	return 0;
}

int mse_packet_ctrl_receive_prepare_packet(
				int index,
				struct mse_packet_ctrl *dma,
				struct mse_adapter_network_ops *ops)
{
	return ops->receive_prepare(index,
				    dma->packet_table,
				    dma->size);
}

int mse_packet_ctrl_receive_packet(int index,
				   int max_size,
				   struct mse_packet_ctrl *dma,
				   struct mse_adapter_network_ops *ops)
{
	int write_p, ret;
	int received = 0, empty_slot;
	int size = min(max_size, MSE_PACKET_COUNT_MAX);

	mse_debug("network adapter=%s r=%d w=%d\n",
		  ops->name, dma->read_p, dma->write_p);

	received = mse_packet_ctrl_check_packet_remain(dma);
	empty_slot = (dma->size - 1) - received;

	/* receive overrun */
	if (empty_slot == 0)
		return received;

	if (size > empty_slot)
		size = empty_slot;

	ret = ops->receive(index, size);
	if (ret < 0)
		return ret;

	write_p = dma->write_p; /* for debug */
	dma->write_p = (write_p + ret) % dma->size;

	mse_debug("%d packtets r=%d w=%d->%d\n",
		  ret, dma->read_p, write_p, dma->write_p);

	return mse_packet_ctrl_check_packet_remain(dma);
}

/* TODO: Remove. it is same as mse_packet_ctrl_receive_packet */
int mse_packet_ctrl_receive_packet_crf(int index,
				       int max_size,
				       struct mse_packet_ctrl *dma,
				       struct mse_adapter_network_ops *ops)
{
	int write_p, ret;
	int size = max_size;

	mse_debug("network adapter=%s r=%d w=%d\n",
		  ops->name, dma->read_p, dma->write_p);

	if (dma->read_p > dma->write_p &&
	    dma->read_p < dma->write_p + size) {
		mse_info("receive overrun r=%d w=%d size=%d/%d\n",
			 dma->read_p, dma->write_p, size, max_size);
		return -ENOSPC;
	}

	ret = ops->receive(index, size);
	if (ret < 0) {
		mse_err("receive error %d\n", ret);
		return -EPERM;
	}

	write_p = dma->write_p; /* for debug */
	dma->write_p = (write_p + ret) % dma->size;

	mse_debug("%d packtets r=%d w=%d->%d\n",
		  ret, dma->read_p, write_p, dma->write_p);

	if (ret != size)
		return -EINTR; /* for cancel */
	else
		return 0;
}

int mse_packet_ctrl_take_out_packet(int index,
				    void *data,
				    size_t size,
				    unsigned int *timestamps,
				    int t_size,
				    int *t_stored,
				    struct mse_packet_ctrl *dma,
				    struct mse_packetizer_ops *ops,
				    size_t *processed)
{
	int ret = MSE_PACKETIZE_STATUS_CONTINUE;
	unsigned int recv_time;
	int pcount = 0;
	int received;

	mse_debug("r=%d w=%d s=%d v=%p\n",
		  dma->read_p, dma->write_p, dma->size,
		  dma->packet_table[dma->read_p].vaddr);

	*t_stored = 0;

	received = mse_packet_ctrl_check_packet_remain(dma);
	while (received-- > 0) {
		ret = ops->depacketize(index,
				       data,
				       size,
				       processed,
				       &recv_time,
				       dma->packet_table[dma->read_p].vaddr,
				       dma->packet_table[dma->read_p].len);
		if (ret == MSE_PACKETIZE_STATUS_SKIP)
			break;

		dma->read_p = (dma->read_p + 1) % dma->size;

		if (ret < 0)
			return -EIO;

		pcount++;
		if (ret >= 0 && *t_stored < t_size) {
			*timestamps++ = recv_time;
			(*t_stored)++;
		}

		if (ret == MSE_PACKETIZE_STATUS_COMPLETE)
			break;

		/* update received count */
		if (received <= 0)
			received = mse_packet_ctrl_check_packet_remain(dma);
	}

	if (ret == MSE_PACKETIZE_STATUS_CONTINUE &&
	    (pcount > 0 || received <= 0)) {
		mse_debug("depacketize not enough. processed packet=%d(processed=%zu, ret=%d)\n",
			  pcount, *processed, ret);
		return -EAGAIN;
	}

	return *processed;
}

int mse_packet_ctrl_take_out_packet_crf(
	int index,
	u64 *timestamp,
	int t_size,
	struct mse_packet_ctrl *dma)
{
	int ret;
	int count = 0;
	size_t crf_len;

	mse_debug("r=%d w=%d s=%d v=%p\n",
		  dma->read_p, dma->write_p, dma->size,
		  dma->packet_table[dma->read_p].vaddr);

	if (dma->read_p == dma->write_p)
		return 0;

	ret = mse_packetizer_crf_tstamp_audio_ops.depacketize(
		index, timestamp, t_size * sizeof(*timestamp),
		&crf_len,
		NULL,
		dma->packet_table[dma->read_p].vaddr,
		dma->packet_table[dma->read_p].len);

	dma->read_p = (dma->read_p + 1) % dma->size;

	if (ret < 0)
		return -EIO;

	if (ret > 0) {
		count = crf_len / sizeof(u64);
		timestamp += count;
	}

	/* return the number of timestamp */
	return count;
}
