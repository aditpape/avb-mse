/*************************************************************************/ /*
 avb-mse

 Copyright (C) 2015-2016 Renesas Electronics Corporation

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

#ifndef __MSE_CORE_H__
#define __MSE_CORE_H__

#include "ravb_eavb.h"

/**
 * @brief Resistered adapter name length
 */
#define MSE_NAME_LEN_MAX	(64)
/**
 * @brief MAC address length
 */
#define MSE_MAC_LEN_MAX		(6)

/**
 * @brief Unresistered device index
 */
#define MSE_INDEX_UNDEFINED	(-1)

/**
 * @brief direction of MSE
 */
enum MSE_DIRECTION {
	/** @brief Input */
	MSE_DIRECTION_INPUT,
	/** @brief Output */
	MSE_DIRECTION_OUTPUT,
};

/**
 * @brief Get kind of MSE
 */
#define MSE_TYPE_KING_GET(type) ((type) & 0xFFFF0000)

/**
 * @brief type of MSE
 */
enum MSE_TYPE {
	/** @brief Audio Adapter */
	MSE_TYPE_ADAPTER_AUDIO = 0x00000000,
	/** @brief Adapter type for PCM */
	MSE_TYPE_ADAPTER_AUDIO_PCM,
	/** @brief Video Adapter */
	MSE_TYPE_ADAPTER_VIDEO = 0x00010000,
	/** @brief Adapter type for H.264 */
	MSE_TYPE_ADAPTER_VIDEO_H264,
	/** @brief Network Adapter */
	MSE_TYPE_ADAPTER_NETWORK = 0x000A0000,
	/** @brief Special type for EAVB Adapter */
	MSE_TYPE_ADAPTER_NETWORK_EAVB,
	/** @brief Packetizer */
	MSE_TYPE_PACKETIZER = 0x000B0000,
	/** @brief Packetizer for Audio PCM */
	MSE_TYPE_PACKETIZER_AUDIO_PCM,
	/** @brief Packetizer for Video H.264 */
	MSE_TYPE_PACKETIZER_VIDEO_H264,
};

/**
 * @brief network configuration
 */
struct mse_network_config {
	/** @brief destination MAC address */
	char dest_addr[MSE_MAC_LEN_MAX];
	/** @brief source MAC address */
	char source_addr[MSE_MAC_LEN_MAX];
	/** @brief priority */
	int priority;
	/** @brief VLAN ID */
	int vlanid;
	/** @brief unique ID */
	int uniqueid;
	/** @brief AVTP timestamp offset */
	unsigned long ts_offset;
	/* if need, add more parameters */
};

/**
 * @brief aduio stream configuration
 */
struct mse_audio_config {
	/** @brief sampling rate */
	int sample_rate;
	/** @brief sample format */
	int sample_format;
	/** @brief channels */
	int channels;
	/** @brief period_size */
	int period_size;
	/** @brief samples per frame */
	int bytes_par_sample;
	/* if need, add more parameters */
};

/**
 * @brief video stream configuration
 */
struct mse_video_config {
	/** @brief video format */
	int format;
	/** @brief bitrate [Mbps] */
	int bitrate;
	/** @brief framerate */
	struct {
		/** @brief seconds [sec]  */
		int n;
		/** @brief frame number */
		int m;
	} fps;
	/** @brief buffer num  */
	int buffers;
	/** @brief height of the frame */
	int height;
	/** @brief width of the frame */
	int width;
	/** @brief color_space */
	int color_space;
	/** @brief interlaced */
	int interlaced;
	/** @brief bytes per frame is data size in 1 ether frame */
	int bytes_per_frame;
	/* ... add more config */
};

/**
 * @brief DMA buffer for Adapter
 */
struct mse_packet {
	/** @brief packet size */
	unsigned int len;
	/** @brief physical address for DMA */
	dma_addr_t paddr;
	/** @brief virtual address for driver */
	void *vaddr;
};

/**
 * @brief main data for Adapter
 */
struct mse_adapter {
	/** @brief instance used flag */
	bool used_f;
	/** @brief adapter name */
	char name[MSE_NAME_LEN_MAX];

	/** @brief direction of adapter */
	enum MSE_DIRECTION inout;
	/** @brief type of Adapter */
	enum MSE_TYPE type;
	/** @brief mutex lock */
	struct mutex lock;

	/** @brief adapter's private data */
	void *private_data;

	/** @brief sysfs index */
	int index_sysfs;
	/** @brief sysfs data */
	struct mse_sysfs_config *sysfs_config;
};

/**
 * @brief registered operations for network adapter
 */
struct mse_adapter_network_ops {
	/** @brief name */
	char *name;
	/** @brief type */
	enum MSE_TYPE type;
	/** @brief private data */
	void *priv;
	/** @brief open function pointer */
	int (*open)(char *name);
	/** @brief release function pointer */
	int (*release)(int index);
	/** @brief set_option function pointer */
	int (*set_option)(int index);
	/** @brief set CBS config function pointer */
	int (*set_cbs_param)(int index, struct eavb_cbsparam *cbs);
	/** @brief set Stream ID config function pointer */
	int (*set_streamid)(int index, u8 streamid[8]);
	/** @brief start function pointer */
	int (*start)(int index);
	/** @brief stop function pointer */
	int (*stop)(int index);
	/** @brief send function pointer */
	int (*send)(int index, struct mse_packet *packets, int num_packets);
	/** @brief receive function pointer */
	int (*receive_prepare)(int index,
			       struct mse_packet *packets,
			       int num_packets);
	/** @brief receive function pointer */
	int (*receive)(int index, int num_packets);
	/** @brief check function pointer */
	int (*check_receive)(int index);
	/** @brief cancel function pointer */
	int (*cancel)(int index);
};

/**
 * @brief registered operations for packetizer
 */
struct mse_packetizer_ops {
	/** @brief name */
	char *name;
	/** @brief type */
	enum MSE_TYPE type;
	/** @brief private data */
	void *priv;
	/** @brief open function pointer */
	int (*open)(void);
	/** @brief release function pointer */
	int (*release)(int index);
	/** @brief init function pointer */
	int (*init)(int index);
	/** @brief set network config function pointer */
	int (*set_network_config)(int index,
				  struct mse_network_config *config);
	/** @brief set audio config function pointer */
	int (*set_audio_config)(int index, struct mse_audio_config *config);
	/** @brief set video config function pointer */
	int (*set_video_config)(int index, struct mse_video_config *config);

	/** @brief calc_cbs function pointer */
	int (*calc_cbs)(int index, struct eavb_cbsparam *cbs);

	/** @brief packetize function pointer */
	int (*packetize)(int index,
			 void *packet,
			 size_t *packet_size,
			 void *buffer,
			 size_t buffer_size,
			 size_t *buffer_processed,
			 unsigned int *timestamp);
	/** @brief depacketize function pointer */
	int (*depacketize)(int index,
			   void *buffer,
			   size_t buffer_size,
			   size_t *buffer_processed,
			   unsigned int *timestamp,
			   void *packet,
			   size_t packet_size);
};

#define GET_UPPER_16BIT(id) (((id) & 0xFF00) >> 8)
#define GET_LOWER_16BIT(id) ((id) & 0x00FF)

extern inline void mse_make_streamid(u8 *streamid, char *mac, int uid)
{
	int len = MSE_MAC_LEN_MAX;

	/* Create Stream ID SrcMAC + U-ID */
	memcpy(streamid, mac, len);
	streamid[len++] = (u8)GET_UPPER_16BIT(uid);
	streamid[len] = (u8)GET_LOWER_16BIT(uid);
}

/**
 * @brief register media adapter to MSE
 *
 * @param[in] type type of adapter
 * @param[in] name of adapter
 * @param[in] data private adapter pointer
 *
 * @retval 0 MSE instance ID
 * @retval <0 Error
 */
extern int mse_register_adapter_media(enum MSE_TYPE type,
				      char *name,
				      void *data);

/**
 * @brief unregister media adapter from MSE
 *
 * @param[in] index instance ID of MSE
 *
 * @retval 0 Success
 * @retval <0 Error
 */
extern int mse_unregister_adapter_media(int index);

/**
 * @brief register network adapter to MSE
 *
 * @param[in] type type of adapter
 * @param[in] name of adapter
 * @param[in] data private adapter pointer
 * @param[in] ops adapter operations
 *
 * @retval 0 MSE instance ID
 * @retval <0 Error
 */
extern int mse_register_adapter_network(struct mse_adapter_network_ops *ops);

/**
 * @brief unregister network adapter from MSE
 *
 * @param[in] index instance ID of MSE
 *
 * @retval 0 Success
 * @retval <0 Error
 */
extern int mse_unregister_adapter_network(int index);

/**
 * @brief register packetizer to MSE
 *
 * @param[in] type type of adapter
 * @param[in] name of packetizer
 * @param[in] data private pointer
 * @param[in] ops packetizer operations
 *
 * @retval 0 MSE adapter ID
 * @retval <0 Error
 */
extern int mse_register_packetizer(struct mse_packetizer_ops *ops);

/**
 * @brief unregister packetizer from MSE
 *
 * @param[in] index MSE adapter ID
 *
 * @retval 0 Success
 * @retval <0 Error
 */
extern int mse_unregister_packetizer(int index);

/**
 * @brief get audio configuration
 *
 * @param[in] index instance ID of MSE
 * @param[out] config audio configuration
 *
 * @retval 0 Success
 * @retval <0 Error
 */
extern int mse_get_audio_config(int index, struct mse_audio_config *config);

/**
 * @brief set audio configuration
 *
 * @param[in] index instance ID of MSE
 * @param[in] config audio configuration
 *
 * @retval 0 Success
 * @retval <0 Error
 */
extern int mse_set_audio_config(int index, struct mse_audio_config *config);

/**
 * @brief get video configuration
 *
 * @param[in] index instance ID of MSE
 * @param[out] config video configuration
 *
 * @retval 0 Success
 * @retval <0 Error
 */
extern int mse_get_video_config(int index, struct mse_video_config *config);

/**
 * @brief set video configuration
 *
 * @param[in] index instance ID of MSE
 * @param[in] config video configuration
 *
 * @retval 0 Success
 * @retval <0 Error
 */
extern int mse_set_video_config(int index, struct mse_video_config *config);

/**
 * @brief MSE open
 *
 * @param[in] index adapter ID of MSE
 * @param[in] type direction of adapter
 *
 * @retval >=0 instance ID of MSE
 * @retval <0 Error
 */
extern int mse_open(int index_media, enum MSE_DIRECTION inout);

/**
 * @brief MSE close
 *
 * @param[in] index instance ID of MSE
 *
 * @retval 0 Success
 * @retval <0 Error
 */
extern int mse_close(int index);

/**
 * @brief MSE streaming on
 *
 * @param[in] index instance ID of MSE
 *
 * @retval 0 Success
 * @retval <0 Error
 */
extern int mse_start_streaming(int index);

/**
 * @brief MSE streaming off
 *
 * @param[in] index instance ID of MSE
 *
 * @retval 0 Success
 * @retval <0 Error
 */
extern int mse_stop_streaming(int index);

/**
 * @brief MSE start transmission
 *
 * @param[in] index instance ID of MSE
 * @param[in] buffer send data
 * @param[in] buffer_size buffer size
 * @param[in] mse_completion callback function pointer
 *
 * @retval 0 Success
 * @retval <0 Error
 */
extern int mse_start_transmission(int index,
				  void *buffer,
				  size_t buffer_size,
				  int (*mse_completion)(int index, int size));

/**
 * @brief get private data
 *
 * @param[in] index instance ID of MSE
 * @param[out] private_data pointer
 *
 * @retval 0 Success
 */
extern int mse_get_private_data(int index, void **private_data);

/**
 * @brief get input output info
 *
 * @param[in] adapter
 *
 * @retval MSE_DIRECTION_INPUT input
 * @retval MSE_DIRECTION_OUTPUT output
 */
extern enum MSE_DIRECTION mse_get_inout(int index);

#endif /* __MSE_CORE_H__ */