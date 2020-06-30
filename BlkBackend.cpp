/*
 *  Implementation of blk backend
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

#include "BlkBackend.hpp"

#include <csignal>

using XenBackend::FrontendHandlerPtr;
using XenBackend::RingBufferPtr;


uint8_t* BlkInRingBuffer::getSector(DiskImage &img, blkif_sector_t sector_number)
{
    std::vector

    uint8_t *sector = img.GetSector(sector_number);
    if (!sector) {
        return NULL;
    }

    return sector;
}

void BlkInRingBuffer::handleSegmentRequest(blkif_sector_t sector_number, blkif_request_segment &seg, blkif_response &rsp)
{
    std::unique_ptr<XenGnttabBuffer> buffer;

    try {
        buffer = std::make_unique(mDomId, &seg.gref, 1);

    } except (...) {
        rsp.status = BLKIF_RSP_ERROR;
        return;
    }


}

void BlkInRingBuffer::performRead(const blkif_request &req, blkif_response &rsp)
{
    for(int i = 0; i < req.nr_segments; i++) {
        handleSegmentRequest(req.sector_number, req.seg[i], rsp);
    }
}

//! [processRequest]
void BlkInRingBuffer::processRequest(const blkif_request& req)
{
	LOG(mLog, DEBUG) << "Receive request, id: " << req.id;

	blkif_response rsp;

	rsp.id = req.id;
	rsp.operation = req.operation;
    rsp.status = BLKIF_RSP_OKAY;
	// process commands

	switch(req.id)
	{
		case BLKIF_OP_READ:
        {
            performRead(req, rsp);
            break;
        }
		case BLKIF_OP_WRITE:
        {
            performWrite(req, rsp);
            break;
        }
		default:
        {
			// set error status
			rsp.status = BLKIF_RSP_EOPNOTSUPP;
            break;
        }
	}

	// send response
	sendResponse(rsp);
}
//! [processRequest]

//! [onBind]
void BlkFrontendHandler::onBind()
{
	LOG(mLog, DEBUG) << "Bind, dom id: " << getDomId();

	// get out ring buffer event channel port
	evtchn_port_t port = getXenStore().readInt(getXsFrontendPath() +
											   "/path/to/out/port");
	// get out ring buffer grant table reference
	uint32_t ref = getXenStore().readInt(getXsFrontendPath() +
										 "/path/to/out/ref");
	// create out ring buffer
	mOutRingBuffer.reset(new BlkOutRingBuffer(getDomId(), port, ref));
	// add ring buffer
	addRingBuffer(mOutRingBuffer);

	// get in ring buffer event channel port
	port = getXenStore().readInt(getXsFrontendPath() +
								 "/path/to/in/port");
	// get in ring buffer grant table reference
	ref = getXenStore().readInt(getXsFrontendPath() +
								"/path/to/in/ref");
	// create in ring buffer
	RingBufferPtr outRingBuffer(
			new BlkOutRingBuffer(getDomId(), port, ref));
	// add ring buffer
	addRingBuffer(outRingBuffer);
}
//! [onBind]

//! [onClosing]
void BlkFrontendHandler::onClosing()
{
	// free allocate on bind resources
	mOutRingBuffer.reset();
}

//! [onNewFrontend]
void BlkBackend::onNewFrontend(domid_t domId, uint16_t devId)
{
	LOG(mLog, DEBUG) << "New frontend, dom id: " << domId;

	// create new blk frontend handler
	addFrontendHandler(FrontendHandlerPtr(
			new BlkFrontendHandler(getDeviceName(), domId)));
}
//! [onNewFrontend]

void waitSignals()
{
	sigset_t set;
	int signal;

	sigemptyset(&set);
	sigaddset(&set, SIGINT);
	sigaddset(&set, SIGTERM);
	sigprocmask(SIG_BLOCK, &set, nullptr);

	sigwait(&set,&signal);
}

//! [main]
int main(int argc, char *argv[])
{
	try
	{
		// Create backend
		BlkBackend blkBackend;

		blkBackend.start();

		waitSignals();

		blkBackend.stop();
	}
	catch(const std::exception& e)
	{
		LOG("Main", ERROR) << e.what();
	}

	return 0;
}
//! [main]
