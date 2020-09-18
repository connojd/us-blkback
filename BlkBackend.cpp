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
 */

#include "BlkBackend.hpp"
#include <csignal>
#include <cmath>
#include <cxxopts.hpp>

#ifdef _WIN32

#include <windows.h>

static inline int
set_affinity(uint64_t core)
{
    if (SetProcessAffinityMask(GetCurrentProcess(), 1ULL << core) == 0) {
        return -1;
    }

    return 0;
}

#else

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <sched.h>

static inline int
set_affinity(uint64_t core)
{
    cpu_set_t  mask;

    CPU_ZERO(&mask);
    CPU_SET(core, &mask);

    if (sched_setaffinity(0, sizeof(mask), &mask) != 0) {
        return -1;
    }

    return 0;
}

#endif

using XenBackend::FrontendHandlerPtr;
using XenBackend::RingBufferPtr;

void dump_sector(const void *target, uint64_t sector_size)
{
    std::cerr << '\n';
    for(uint64_t i = 0; i < sector_size; i++) {
        if ( i % 16 == 0 )
            std::cerr << '\n';
        unsigned char *ptr = (unsigned char*)target;
        std::cerr << std::hex << std::setfill('0') << std::setw(2) << (int)ptr[i] << " " << std::dec;
    }
    std::cerr << '\n';
}


std::vector<grant_ref_t>
gatherGrantRefs(const struct blkif_request_segment *segments, uint32_t nr_segments)
{
    std::vector<grant_ref_t> grefs;
    
    for (uint64_t i = 0; i < nr_segments; i++) {
        grefs.push_back(segments[i].gref);
    }

    return grefs;
}

std::vector<grant_ref_t>
gatherIndirectGrantRefs(const blkif_request_indirect_t *req)
{
    std::vector<grant_ref_t> grefs;

    uint64_t page_count = std::ceil(req->nr_segments / (4096 / (1.0*sizeof(struct blkif_request_segment))));

    for(uint64_t i = 0; i < page_count; i++) {
        grefs.push_back(req->indirect_grefs[i]);
    }

    return grefs;
}


int BlkCmdRingBuffer::processSegments(const struct blkif_request_segment *segments,
                                      const uint32_t nr_segments,
                                      const blkif_sector_t sector_number,
                                      const bool write)
{
    constexpr uintptr_t sectors_per_page = XC_PAGE_SIZE / SECTOR_SIZE;
    int rc = 0;
    blkif_sector_t nr_sectors = 0;
    std::vector<grant_ref_t> grefs = gatherGrantRefs(segments, nr_segments);
    XenBackend::XenGnttabBuffer buffer(mDomId, grefs.data(), grefs.size());

    for(uint64_t i = 0; i < nr_segments; i++) {
        for(uint64_t sect_offset = segments[i].first_sect;
            sect_offset <= segments[i].last_sect;
            sect_offset++) {

            blkif_sector_t target_sector = sector_number + nr_sectors;
            uintptr_t byte_offset = (sect_offset + (i * sectors_per_page)) * SECTOR_SIZE;
            char *target_buffer = (char *)((uintptr_t)buffer.get() + byte_offset);

            if(write) {
                rc = mImage->writeSector(target_sector,
                                         target_buffer,
                                         SECTOR_SIZE);
            } else {
                rc = mImage->readSector(target_sector,
                                        target_buffer,
                                        SECTOR_SIZE);
            }

            nr_sectors++;

            if(rc) {
                return rc;
            }
        }
    }

    return rc;
}

int BlkCmdRingBuffer::performRead(const blkif_request_t &req)
{
    if(req.nr_segments == 0) {
        return BLKIF_RSP_ERROR;
    }
    
    int rc = processSegments(req.seg, req.nr_segments, req.sector_number, false);
    if(rc) {
        LOG(mLog, INFO) << "Read failed: " << req.sector_number;
        return BLKIF_RSP_ERROR;
    }

    return BLKIF_RSP_OKAY;
}

int BlkCmdRingBuffer::performWrite(const blkif_request_t &req)
{
    int rc = processSegments(req.seg, req.nr_segments, req.sector_number, true);
    if(rc) {
        LOG(mLog, INFO) << "Write failed: " << req.sector_number;
        return BLKIF_RSP_ERROR;
    }

    return BLKIF_RSP_OKAY;
}

int BlkCmdRingBuffer::handleIndirectRequest(const blkif_request_indirect_t *indirect)
{
    std::vector<grant_ref_t> grefs = gatherIndirectGrantRefs(indirect);
    XenBackend::XenGnttabBuffer buffer(mDomId, grefs.data(), grefs.size());
    struct blkif_request_segment *seg = (struct blkif_request_segment *)buffer.get();
    int rc = processSegments(seg, indirect->nr_segments, indirect->sector_number, indirect->indirect_op == BLKIF_OP_WRITE);

    if(rc) {
        LOG(mLog, INFO) << "Indirect op failed: " << indirect->sector_number;
        return BLKIF_RSP_ERROR;
    }

    return BLKIF_RSP_OKAY;
}

static uint64_t cmd_count = 0;

//! [processRequest]
void BlkCmdRingBuffer::processRequest(const blkif_request& req)
{
    blkif_response rsp;
    memset(&rsp, 0x00, sizeof(rsp));

    rsp.id = req.id;
    rsp.operation = req.operation;
    rsp.status = BLKIF_RSP_OKAY;
      
    // process commands
    switch(req.operation)
    {
    case BLKIF_OP_READ:
    {       
        rsp.status = performRead(req);
        break;
    }
    case BLKIF_OP_WRITE:
    {
        rsp.status = performWrite(req);
        break;
    }
    case BLKIF_OP_WRITE_BARRIER:
    case BLKIF_OP_FLUSH_DISKCACHE:
    {
        mImage->flushBackingFile();
        break;
    }
    case BLKIF_OP_DISCARD:
    {
        const blkif_request_discard_t *discard = reinterpret_cast<const blkif_request_discard_t *>(&req);
        rsp.status = mImage->discard(discard->sector_number, discard->nr_sectors);
        break;
    }
    case BLKIF_OP_INDIRECT:
    {
        const blkif_request_indirect_t *indirect =
            reinterpret_cast<const blkif_request_indirect_t *>(&req);
        rsp.status = handleIndirectRequest(indirect);
        rsp.operation = indirect->indirect_op;
        break;
    }

    default:
    {
        LOG(mLog, INFO) << "Unimplemented blkif op: " << req.id << " cmd count: " << cmd_count;        
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
    // get out ring buffer event channel port
    evtchn_port_t port = getXenStore().readInt(getXsFrontendPath() + "/event-channel");

    // get out ring buffer grant table reference
    uint32_t ref = getXenStore().readInt(getXsFrontendPath() + "/ring-ref");

    std::string path = getXenStore().readString(getXsBackendPath() + "/params");
    LOG(mLog, INFO) << "open image file: " << path;

    if (path.front() == '\'' && path.back() == '\'') {
        if (path.size() > 2) {
            path = path.substr(1, path.size() - 2);
        }
    }

    mImage = std::make_shared<DiskImage>(path);

    if(!mImage) {
        LOG(mLog, ERROR) << "Failed to open image file: " << path;
        throw;
    }

    getXenStore().writeInt(getXsBackendPath() + "/feature-max-indirect-segments", 32);
    getXenStore().writeInt(getXsBackendPath() + "/feature-discard", 0);
    getXenStore().writeInt(getXsBackendPath() + "/feature-persistent", 0);
    getXenStore().writeInt(getXsBackendPath() + "/feature-flush-cache", 1);
    getXenStore().writeInt(getXsBackendPath() + "/feature-barrier", 1);
	
    getXenStore().writeInt(getXsBackendPath() + "/sectors", mImage->getSectorCount());
    getXenStore().writeInt(getXsBackendPath() + "/sector-size", mImage->getSectorSize());
    getXenStore().writeInt(getXsBackendPath() + "/info", 0);

    // create command ring buffer
    mCmdRingBuffer.reset(new BlkCmdRingBuffer(getDomId(), port, ref, mImage));
	  
    // add ring buffer
    addRingBuffer(mCmdRingBuffer);
}
//! [onBind]

//! [onClosing]
void BlkFrontendHandler::onClosing()
{
    // free allocate on bind resources
    mCmdRingBuffer.reset();
}

//! [onNewFrontend]
void BlkBackend::onNewFrontend(domid_t domId, uint16_t devId)
{
    LOG(mLog, INFO) << "New frontend, dom id: " << domId;

    // create new blk frontend handler
    addFrontendHandler(FrontendHandlerPtr(new BlkFrontendHandler(getDeviceName(), domId, devId)));
}
//! [onNewFrontend]

void waitSignals()

{
#ifndef _WIN32
    sigset_t set;
    int signal;

    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    sigprocmask(SIG_BLOCK, &set, nullptr);

    sigwait(&set,&signal);
#else
    HANDLE h = CreateEvent(NULL, FALSE, FALSE, TEXT("STOPTHREAD"));
    WaitForSingleObject(h, INFINITE);
#endif
}

//! [main]
int main(int argc, char *argv[])
{
    try
    {
        using namespace cxxopts;

        cxxopts::Options options("us-blkback - xen blkback in userspace");

        options.add_options()
        ("h,help", "Print this help menu")
        ("a,affinity", "Run on a specific cpu", cxxopts::value<uint64_t>(), "[cpu #]")
        ("w,wait", "Wait for xeniface driver");

        auto args = options.parse(argc, argv);
        if (args.count("help")) {
            std::cout << options.help() << '\n';
            exit(EXIT_SUCCESS);
        }

        if (args.count("affinity")) {
                uint64_t cpu = args["affinity"].as<uint64_t>();

                if (set_affinity(cpu)) {
                        LOG("Main", ERROR) << "Failed to set affinity to cpu "
                                           << cpu;
                        throw;
                }
        }

        // Spin until XcOpen succeeds. Useful if this program may
        // be started before the xeniface driver is loaded.
        bool wait = args.count("wait") != 0;

        // Create backend
        BlkBackend blkBackend(wait);
        LOG("Main", INFO) << "Starting block backend";
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
/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
