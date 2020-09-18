/*
 *  Implementation of block backend
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 * Copyright (C) 2016 EPAM Systems Inc.
 */

#ifndef BLKBACKEND_HPP_
#define BLKBACKEND_HPP_

#include <memory>
#define _WINDLL 1
#define __x86_64__ 1
//#define __XEN_TOOLS__ 1
#include <xen/be/BackendBase.hpp>
#include <xen/be/FrontendHandlerBase.hpp>
#include <xen/be/RingBufferBase.hpp>
#include <xen/be/XenGnttab.hpp>
#include "DiskImage.h"

#define XENBLK_IN_RING_OFFS 0
#define XENBLK_IN_RING_SIZE 4096

//! [BlkCmdRingBuffer]
class BlkCmdRingBuffer : public XenBackend::RingBufferInBase<blkif_back_ring_t, blkif_sring_t,
							     blkif_request_t, blkif_response_t>
{
public:

	BlkCmdRingBuffer(domid_t domId,
			 evtchn_port_t port,
			 grant_ref_t ref,
			 std::shared_ptr<DiskImage> diskImage) :
	  XenBackend::RingBufferInBase<blkif_back_ring_t,
				       blkif_sring_t,
				       blkif_request_t,
				       blkif_response_t>(domId, port, ref,
                                                         __CONST_RING_SIZE(blkif, XC_PAGE_SIZE),
                                                         XC_PAGE_SIZE),
	  mLog("InRingBuffer"),
	  mDomId(domId),
	  mImage(diskImage)
	{
		LOG(mLog, INFO) << "Create out ring buffer, dom id: " << domId;
	}


        ~BlkCmdRingBuffer() = default;

private:


        int processSegments(const struct blkif_request_segment *segments,
			    uint32_t nr_segments,
			    blkif_sector_t sector_number,
			    bool write);
        int performRead(const blkif_request_t &req);
        int performWrite(const blkif_request_t &req);
        int handleIndirectRequest(const blkif_request_indirect_t *indirect);
	// Override receiving requests
	virtual void processRequest(const blkif_request& req) override;

	// XenBackend::Log can be used by backend
	XenBackend::Log mLog;
  
        domid_t mDomId;
        std::shared_ptr<DiskImage> mImage{nullptr};
};
//! [BlkInRingBuffer]

//! [BlkFrontend]
class BlkFrontendHandler : public XenBackend::FrontendHandlerBase
{
public:
  BlkFrontendHandler(const std::string& devName, 
		     domid_t feDomId, 
		     uint16_t devId) : FrontendHandlerBase("FrontendHandler",
							   "vbd",
							   feDomId, 
							   devId),
				       mLog("FrontendHandler")
  {
    LOG(mLog, DEBUG) << "Create blk frontend handler, dom id: "
		     << feDomId;
  }
  
private:

	// Override onBind method
	void onBind() override;

	// Override onClosing method
	void onClosing() override;

	// XenBackend::Log can be used by backend
	XenBackend::Log mLog;

	// Store out ring buffer
    std::shared_ptr<BlkCmdRingBuffer> mCmdRingBuffer{nullptr};

    // The backing store for this device
    std::shared_ptr<DiskImage> mImage{nullptr};
};
//! [BlkFrontend]

//! [BlkBackend]
class BlkBackend : public XenBackend::BackendBase
{
public:

	BlkBackend(bool wait = false) :
		BackendBase("BlkBackend", "vbd", wait),
		mLog("BlkBackend")
	{
		LOG(mLog, DEBUG) << "Create vbd backend";
	}

private:

	// override onNewFrontend method
	void onNewFrontend(domid_t domId, uint16_t devId) override;

	// XenBackend::Log can be used by backend
	XenBackend::Log mLog;
};
//! [BlkBackend]

#endif /* BLKBACKEND_HPP_ */
