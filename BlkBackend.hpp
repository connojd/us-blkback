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

#include <xen/be/BackendBase.hpp>
#include <xen/be/FrontendHandlerBase.hpp>
#include <xen/be/RingBufferBase.hpp>
#include "DiskImage.h"

//! [BlkOutRingBuffer]
class BlkOutRingBuffer : public XenBackend::RingBufferOutBase
								   <xenblk_event_page, xenblk_evt>
{
public:

	BlkOutRingBuffer(domid_t domId, evtchn_port_t port, grant_ref_t ref) :
		XenBackend::RingBufferOutBase<xenblk_event_page, xenblk_evt>
			(domId, port, ref, XENBLK_IN_RING_OFFS, XENBLK_IN_RING_SIZE) {}

};
//! [BlkOutRingBuffer]

//! [BlkInRingBuffer]
class BlkInRingBuffer : public XenBackend::RingBufferInBase
								  <blkif_back_ring_t, blkif_srint_t,
								   blkif_request_t, blkif_response_t>
{
public:

	BlkInRingBuffer(domid_t domId, evtchn_port_t port, grant_ref_t ref) :
		XenBackend::RingBufferInBase<blkif_back_ring_t, blkif_back_ring_t,
									 blkif_request_t, blkif_response_t>
									(domId, port, ref),
		mLog("InRingBuffer"),
        mDomId(domId)
	{
		LOG(mLog, DEBUG) << "Create out ring buffer, dom id: " << domId;
	}


private:

	// Override receiving requests
	virtual void processRequest(const blkif_request& req) override;

	// XenBackend::Log can be used by backend
	XenBackend::Log mLog;
    DiskImage &mImage;
    domid_t mDomId;
};
//! [BlkInRingBuffer]

//! [BlkFrontend]
class BlkFrontendHandler : public XenBackend::FrontendHandlerBase
{
public:

	BlkFrontendHandler(const std::string& devName, domid_t feDomId) :
		FrontendHandlerBase("FrontendHandler", "blk_dev", 0, feDomId),
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
	std::shared_ptr<BlkOutRingBuffer> mOutRingBuffer;

    std::list<DiskImage> image;
};
//! [BlkFrontend]

//! [BlkBackend]
class BlkBackend : public XenBackend::BackendBase
{
public:

	BlkBackend() :
		BackendBase("BlkBackend", "blk_dev"),
		mLog("BlkBackend")
	{
		LOG(mLog, DEBUG) << "Create blk backend";
	}

private:

	// override onNewFrontend method
	void onNewFrontend(domid_t domId, uint16_t devId) override;

	// XenBackend::Log can be used by backend
	XenBackend::Log mLog;
};
//! [BlkBackend]

#endif /* BLKBACKEND_HPP_ */
