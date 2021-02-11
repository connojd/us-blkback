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
#include "Args.hpp"
#include "Service.hpp"

#include <csignal>
#include <cmath>
#include <cxxopts.hpp>

#ifdef _WIN32
#include <windows.h>
#else
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sched.h>
#include <unistd.h>
#endif

using XenBackend::FrontendHandlerPtr;
using XenBackend::RingBufferPtr;

// Total number of (persistent) grants that may be mapped at any given time.
// On Windows this must be less than the number of grants implied
// by the size of the FDO hole. This hole currently is 64MB which
// gives 64K grants available in total. A small (< 128) portion of
// these are reserved for the shared info and grant table pages.
// The rest are free to use here.
constexpr uint64_t MAX_PGRANTS = 8192U;

// Total number of frontends we can service. This is chosen to
// keep the maximum number of persistent grants-per-frontend around 1000.
constexpr uint64_t MAX_FRONTENDS = 8U;
constexpr uint64_t MAX_PGRANTS_PER_FRONTEND = MAX_PGRANTS / MAX_FRONTENDS;

constexpr uint64_t SECTORS_PER_PAGE = XC_PAGE_SIZE / SECTOR_SIZE;
constexpr uint64_t SEGMENTS_PER_INDIRECT_PAGE =
    XC_PAGE_SIZE / sizeof(struct blkif_request_segment);

constexpr uint64_t MAX_INDIRECT_SEGMENTS = 256U;
constexpr uint64_t MAX_INDIRECT_PAGES =
    div_round_up(MAX_INDIRECT_SEGMENTS, SEGMENTS_PER_INDIRECT_PAGE);

// Evict 5% of existing grants when the persistent limit is full
constexpr uint64_t GRANT_EVICTION_SIZE =
    div_round_up(MAX_PGRANTS_PER_FRONTEND * 5, 100);

// Make sure segments-per-page is a positive power of two
static_assert(SEGMENTS_PER_INDIRECT_PAGE > 0U);
static_assert((SEGMENTS_PER_INDIRECT_PAGE & (SEGMENTS_PER_INDIRECT_PAGE - 1U)) == 0U);

static_assert(MAX_INDIRECT_PAGES <= BLKIF_MAX_INDIRECT_PAGES_PER_REQUEST);
static_assert(MAX_PGRANTS_PER_FRONTEND > BLKIF_MAX_SEGMENTS_PER_REQUEST);
static_assert(MAX_PGRANTS_PER_FRONTEND > GRANT_EVICTION_SIZE);

static std::atomic<uint64_t> frontendCount;

static bool validSegment(const blkif_request_segment *const seg) noexcept
{
    if (seg->gref == 0U) {
        return false;
    }

    if (seg->first_sect >= SECTORS_PER_PAGE) {
        return false;
    }

    if (seg->last_sect >= SECTORS_PER_PAGE) {
        return false;
    }

    if (seg->first_sect > seg->last_sect) {
        return false;
    }

    return true;
}

void BlkCmdRingBuffer::evictGrants()
{
    while (mGntLru.size() > (MAX_PGRANTS_PER_FRONTEND - GRANT_EVICTION_SIZE)) {
        GntPage page = mGntLru.back();
        page.unmap();
        mGntMap.erase(page.gref());
        mGntLru.pop_back();
    }
}

void BlkCmdRingBuffer::freeGrants()
{
    for (auto &page : mGntLru) {
        page.unmap();
    }

    mGntLru.clear();
    mGntMap.clear();
}

void *BlkCmdRingBuffer::mapGrant(const grant_ref_t gref)
{
    GntPage page{gref};

    if (!page.map(mDomId)) {
        LOG(mLog, ERROR) << "Failed to map gref " << gref << '\n';
        return nullptr;
    }

    void *virt = page.addr();

    mGntLru.push_front(page);
    mGntMap.emplace(std::make_pair(gref, mGntLru.begin()));

    return virt;
}

void *BlkCmdRingBuffer::addGrant(const grant_ref_t gref)
{
    auto map_itr = mGntMap.find(gref);

    if (map_itr != mGntMap.end()) {
        auto lru_itr = map_itr->second;
        void *virt = lru_itr->addr();

        // Move to the front of LRU list
        mGntLru.push_front(*lru_itr);
        mGntLru.erase(lru_itr);

        // Ensure that gref points to the most-recently used node
        // on the LRU list
        map_itr->second = mGntLru.begin();

        return virt;
    }

    if (mGntLru.size() == MAX_PGRANTS_PER_FRONTEND) {
        this->evictGrants();
    }

    return this->mapGrant(gref);
}

int BlkCmdRingBuffer::processSegment(const blkif_request_segment *const seg,
                                     const blkif_sector_t start_sector,
                                     const uint32_t nr_sectors,
                                     const bool write)
{
    auto buffer = reinterpret_cast<uint8_t *>(this->addGrant(seg->gref));

    if (!buffer) {
        LOG(mLog, ERROR) << "Failed to add grant with gref " << seg->gref;
        return BLKIF_RSP_ERROR;
    }

    buffer += SECTOR_SIZE * seg->first_sect;

    if (write) {
        return mImage->writeSectors(start_sector, nr_sectors, buffer);
    } else {
        return mImage->readSectors(start_sector, nr_sectors, buffer);
    }
}

int BlkCmdRingBuffer::handleReadWrite(const blkif_request_t &req)
{
    const bool write = req.operation == BLKIF_OP_WRITE;
    const uint8_t nr_segs = req.nr_segments;
    uint64_t start_sector = req.sector_number;

    if (nr_segs == 0U || nr_segs > BLKIF_MAX_SEGMENTS_PER_REQUEST) {
        return BLKIF_RSP_ERROR;
    }

    for (uint32_t i = 0U; i < nr_segs; i++) {
        const blkif_request_segment *const seg = &req.seg[i];
        const uint32_t nr_sectors = seg->last_sect - seg->first_sect + 1U;

        const int rc = this->processSegment(seg, start_sector, nr_sectors, write);
        if (rc != BLKIF_RSP_OKAY) {
            return rc;
        }

        start_sector += nr_sectors;
    }

    return BLKIF_RSP_OKAY;
}

int BlkCmdRingBuffer::handleIndirect(const blkif_request_indirect_t *indirect)
{
    const uint16_t op = indirect->indirect_op;
    const uint16_t total_segments = indirect->nr_segments;

    if (op != BLKIF_OP_READ && op != BLKIF_OP_WRITE) {
        LOG(mLog, ERROR) << "Indirect request has invalid op (" << op << ")";
        return BLKIF_RSP_ERROR;
    }

    if (total_segments == 0U) {
        LOG(mLog, ERROR) << "Indirect request has 0 segments";
        return BLKIF_RSP_ERROR;
    }

    if (total_segments > MAX_INDIRECT_SEGMENTS) {
        LOG(mLog, ERROR) << "Indirect request has too many segments ("
                         << total_segments << ")";
        return BLKIF_RSP_ERROR;
    }

    blkif_sector_t start_sector = indirect->sector_number;
    uint64_t segments_done = 0U;
    const bool write = (op == BLKIF_OP_WRITE);
    const uint64_t nr_indirect_grefs = div_round_up(total_segments,
                                                    SEGMENTS_PER_INDIRECT_PAGE);

    for (uint64_t i = 0U; i < nr_indirect_grefs; i++) {
        const grant_ref_t gref = indirect->indirect_grefs[i];
        auto seg = reinterpret_cast<const blkif_request_segment *const>(this->addGrant(gref));

        if (!seg) {
            return BLKIF_RSP_ERROR;
        }

        // How many segments are on this indirect page?
        const uint64_t nr_segs = minimum(total_segments - segments_done,
                                         SEGMENTS_PER_INDIRECT_PAGE);

        for (uint64_t n = 0U; n < nr_segs; n++) {
            const uint32_t nr_sectors = seg[n].last_sect - seg[n].first_sect + 1U;
            const int rc = this->processSegment(&seg[n],
                                                start_sector,
                                                nr_sectors,
                                                write);
            if (rc != BLKIF_RSP_OKAY) {
                return rc;
            }

            start_sector += nr_sectors;
        }

        segments_done += nr_segs;
    }

    return BLKIF_RSP_OKAY;
}

static uint64_t cmd_count = 0;

void BlkCmdRingBuffer::processRequest(const blkif_request& req)
{
    blkif_response rsp;
    memset(&rsp, 0x00, sizeof(rsp));

    rsp.id = req.id;
    rsp.operation = req.operation;
    rsp.status = BLKIF_RSP_OKAY;

    switch (req.operation) {
    case BLKIF_OP_READ:
    case BLKIF_OP_WRITE:
        rsp.status = this->handleReadWrite(req);
        break;
    case BLKIF_OP_WRITE_BARRIER:
    case BLKIF_OP_FLUSH_DISKCACHE:
        mImage->flushBackingFile();
        break;
    case BLKIF_OP_DISCARD:
    {
        auto discard = reinterpret_cast<const blkif_request_discard_t *>(&req);
        rsp.status = mImage->discard(discard->sector_number, discard->nr_sectors);
        break;
    }
    case BLKIF_OP_INDIRECT:
    {
        auto indirect = reinterpret_cast<const blkif_request_indirect_t *>(&req);
        rsp.status = this->handleIndirect(indirect);
        rsp.operation = indirect->indirect_op;
        break;
    }
    default:
        LOG(mLog, INFO) << "Unimplemented blkif op: " << req.id << " cmd count: "
                        << cmd_count;

        rsp.status = BLKIF_RSP_EOPNOTSUPP;
        break;
    }

    sendResponse(rsp);
}


//! [onBind]
void BlkFrontendHandler::onBind()
{
    // get out ring buffer event channel port
    evtchn_port_t port = getXenStore().readInt(getXsFrontendPath() + "/event-channel");

    // get out ring buffer grant table reference
    uint32_t ref = getXenStore().readInt(getXsFrontendPath() + "/ring-ref");

    std::string path = getXenStore().readString(getXsBackendPath() + "/params");
    LOG(mLog, DEBUG) << "open image file: " << path;

    if (path.front() == '\'' && path.back() == '\'') {
        if (path.size() > 2) {
            path = path.substr(1, path.size() - 2);
        }
    }

    mImage = std::make_shared<DiskImage>(path);

    if (!mImage) {
        LOG(mLog, ERROR) << "Failed to open image file: " << path;
        throw;
    }

    getXenStore().writeInt(getXsBackendPath() + "/feature-max-indirect-segments", MAX_INDIRECT_SEGMENTS);
    getXenStore().writeInt(getXsBackendPath() + "/feature-discard", 0);
    getXenStore().writeInt(getXsBackendPath() + "/feature-persistent", 1);
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
    LOG(mLog, DEBUG) << "onClosing called, freeing resources...";

    // free allocate on bind resources
    mCmdRingBuffer.reset();
}

//! [onNewFrontend]
void BlkBackend::onNewFrontend(domid_t domId, uint16_t devId)
{
    if (frontendCount >= MAX_FRONTENDS) {
        LOG(mLog, ERROR) << "Unable to create new frontend (MAX_FRONTENDS="
                         << MAX_FRONTENDS << ")\n";
        return;
    } else {
        frontendCount++;
        LOG(mLog, DEBUG) << "New frontend, dom id: " << domId;
    }

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

static inline int set_affinity(uint64_t core)
{
#ifdef _WIN32
    if (SetProcessAffinityMask(GetCurrentProcess(), 1ULL << core) == 0) {
        return -1;
    }

    return 0;
#else
    cpu_set_t  mask;

    CPU_ZERO(&mask);
    CPU_SET(core, &mask);

    if (sched_setaffinity(0, sizeof(mask), &mask) != 0) {
        return -1;
    }

    return 0;
#endif
}

//! [main]
int main(int argc, char *argv[])
{
    try
    {
        frontendCount = 0;

        auto args = parseArgs(argc, argv);
        if (args.count("affinity")) {
                uint64_t cpu = args["affinity"].as<uint64_t>();

                if (set_affinity(cpu)) {
                        LOG("Main", ERROR) << "Failed to set affinity to cpu "
                                           << cpu;
                        throw;
                }
        } else {
#ifdef _WIN32
                SYSTEM_INFO info;
                ZeroMemory(&info, sizeof(SYSTEM_INFO));
                GetSystemInfo(&info);
                auto nr_cpus = info.dwNumberOfProcessors;
#else
                auto nr_cpus = sysconf(_SC_NPROCESSORS_ONLN);
#endif
                if (set_affinity(nr_cpus - 1)) {
                        LOG("Main", ERROR) << "Failed to set affinity to cpu "
                                           << nr_cpus - 1;
                        throw;
                }
        }

#ifdef _WIN32
        if (args.count("windows-svc")) {
            if (copyArgs(argc, argv)) {
                LOG("Main", ERROR) << "Failed to copy args for Windows service\n";
                exit(EXIT_FAILURE);
            }

            serviceStart();
            freeArgs();

            exit(EXIT_SUCCESS);
        }

        if (args.count("high-priority")) {
            if (!SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS)) {
                LOG("Main", INFO) << "Failed to set high priority\n";
            }
        }
#endif


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
