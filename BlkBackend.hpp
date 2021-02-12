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

static constexpr inline uint64_t minimum(uint64_t left, uint64_t right) noexcept
{
    return (left < right) ? left : right;
}

static constexpr inline uint64_t div_round_up(uint64_t n, uint64_t d) noexcept
{
    return (n + d - 1U) / d;
}

struct GntPage {
    void *mAddr{nullptr};
    grant_ref_t mGref{0};

    GntPage(grant_ref_t gref) noexcept :
        mAddr{nullptr},
        mGref{gref}
    { }

    GntPage(GntPage &&other) :
        mAddr{other.mAddr},
        mGref{other.mGref}
    { }

    GntPage(const GntPage &other) :
        mAddr{other.mAddr},
        mGref{other.mGref}
    { }

    void *map(const domid_t domid)
    {
        constexpr int prot = PROT_READ | PROT_WRITE;
        constexpr uint32_t count = 1U;

        mAddr = xengnttab_map_domain_grant_refs(nullptr,
                                                count,
                                                domid,
                                                &mGref,
                                                prot);
        return mAddr;
    }

    void unmap()
    {
        xengnttab_unmap(nullptr, mAddr, 1);
    }

    grant_ref_t gref() const noexcept
    {
        return mGref;
    }

    void *addr() noexcept
    {
        return mAddr;
    }
};


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
		LOG(mLog, DEBUG) << "Created blkif ring: frontend: " << domId
                                 << ", ring size: "
                                 << __CONST_RING_SIZE(blkif, XC_PAGE_SIZE);
	}

        ~BlkCmdRingBuffer()
        {
            this->freeGrants();
        }

private:

        int processSegments(const struct blkif_request_segment *segments,
			    uint32_t nr_segments,
			    blkif_sector_t sector_number,
			    bool write);

        int processSegment(const blkif_request_segment *const seg,
			   const blkif_sector_t start_sector,
			   const uint32_t nr_sectors,
			   bool write);

        int handleReadWrite(const blkif_request_t &req);
        int handleIndirect(const blkif_request_indirect_t *indirect);

        void freeGrants();
        void evictGrants();
        void *mapGrant(const grant_ref_t gref);
        void *addGrant(const grant_ref_t gref);

	// Override receiving requests
	virtual void processRequest(const blkif_request& req) override;

	// XenBackend::Log can be used by backend
	XenBackend::Log mLog;

        domid_t mDomId;
        std::shared_ptr<DiskImage> mImage{nullptr};
        std::list<GntPage> mGntLru;
        std::unordered_map<grant_ref_t, decltype(mGntLru)::iterator> mGntMap;
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
