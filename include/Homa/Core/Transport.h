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

#ifndef HOMA_CORE_TRANSPORT_H
#define HOMA_CORE_TRANSPORT_H

#include <Homa/Homa.h>

namespace Homa {
namespace Core {

/**
 * Interface to Homa's a transport protocol implementation.
 *
 * This is a low-level API that can be used to create Homa-based transports
 * for different runtime environments (e.g. polling, kernel threading,
 * green threads, etc).
 */
class Transport {
  public:
    /**
     * Collection of user-defined transport callbacks.
     */
    struct CallbackHandler {
        virtual void handleIncommingMessage(
            uint16_t port, Homa::unique_ptr<Homa::InMessage> inMessage) = 0;
        virtual void handleSendReady() = 0;
        virtual void handleGrantsReady() = 0;
    };

    /**
     * Return a new instance of a Homa-based transport.
     *
     * The caller is responsible for calling free() on the returned pointer.
     *
     * @param driver
     *      Driver with which this transport should send and receive packets.
     * @param transportId
     *      This transport's unique identifier in the group of transports among
     *      which this transport will communicate.
     * @param handler
     *      Option user-defined callback when a new message has been received.
     * @return
     *      Pointer to the new transport instance.
     */
    static Transport* create(Driver* driver, uint64_t transportId,
                             CallbackHandler* handler = nullptr);

    /**
     * Handle an ingress packet by running it through the transport protocol
     * stack.
     *
     * @param packet
     *      The ingress packet.
     * @param source
     *      IpAddress of the socket from which the packet is sent.
     */
    virtual void processPacket(Driver::Packet* packet, IpAddress source) = 0;

    /**
     * Make incremental progressing sending outbound messages.
     */
    virtual void trySendMessage() = 0;

    /**
     * Make incremental progress scheduling inbound messages.
     */
    virtual void trySendGrants() = 0;

    /**
     * Make incremental progress checking any expired timeouts.
     *
     * @return
     *      The tsc time before which this method should be called again.
     */
    virtual uint64_t checkTimeouts() = 0;
};

}  // namespace Core
}  // namespace Homa

#endif  // HOMA_CORE_TRANSPORT_H
