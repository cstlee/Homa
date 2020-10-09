/* Copyright (c) 2020, Stanford University
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "StdTransportImpl.h"

namespace Homa {

Homa::unique_ptr<Homa::InMessage>
StdTransportImpl::Socket::receive(bool blocking)
{
    std::unique_lock<std::mutex> lock(m);
    while (blocking && messages.empty()) {
        cv.wait(lock);
    }
    Homa::unique_ptr<Homa::InMessage> message;
    if (!messages.empty()) {
        message = std::move(messages.front());
        messages.pop_front();
    }
    return message;
}

void
StdTransportImpl::Socket::handleIncomingMessage(
    Homa::unique_ptr<Homa::InMessage> inMessage)
{
    std::unique_lock<std::mutex> lock(m);
    messages.push_back(std::move(inMessage));
    cv.notify_one();
}

StdTransportImpl::StdTransportImpl(Driver* driver, uint64_t transportId)
    : core(Homa::Core::Transport::create(driver, transportId, this))
    , driver(driver)
    , mutex()
    , sendReady()
    , grantsReady()
    , pacer(&StdTransportImpl::pacerMain, this)
    , granter(&StdTransportImpl::granterMain, this)
    , timeoutHandler(&StdTransportImpl::timeoutMain, this)
    , sockets()
{}

void
StdTransportImpl::handleIncommingMessage(
    uint16_t port, Homa::unique_ptr<Homa::InMessage> inMessage)
{
    std::lock_guard<std::mutex> lock(mutex);
    if (sockets.find(port) != sockets.end()) {
        sockets.at(port).handleIncomingMessage(std::move(inMessage));
    }
}

void
StdTransportImpl::handleSendReady()
{
    sendReady.notify_one();
}

void
StdTransportImpl::handleGrantsReady()
{
    grantsReady.notify_one();
}

void
StdTransportImpl::receiverMain()
{
    while (true) {
        const int MAX_BURST = 32;
        Driver::Packet* packets[MAX_BURST];
        IpAddress srcAddrs[MAX_BURST];
        int numPackets = driver->receivePackets(MAX_BURST, packets, srcAddrs);
        for (int i = 0; i < numPackets; ++i) {
            core->processPacket(packets[i], srcAddrs[i]);
        }
    }
}

void
StdTransportImpl::pacerMain()
{
    while (true) {
        std::unique_lock<std::mutex> lock(mutex);
        sendReady.wait(lock);
        core->trySendMessage();
    }
}

void
StdTransportImpl::granterMain()
{
    while (true) {
        std::unique_lock<std::mutex> lock(mutex);
        grantsReady.wait(lock);
        core->trySendGrants();
    }
}

void
StdTransportImpl::timeoutMain()
{
    while (true) {
        uint64_t sleep = core->checkTimeouts();
        // sleep_until(sleep);
    }
}

}  // namespace Homa
