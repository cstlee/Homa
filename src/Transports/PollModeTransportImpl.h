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

#ifndef HOMA_TRANSPORTS_POLLMODETRANSPORT_H
#define HOMA_TRANSPORTS_POLLMODETRANSPORT_H

#include <Homa/Core/Transport.h>
#include <Homa/Transports/PollModeTransport.h>

namespace Homa {

/**
 * A polling-based Homa transport implementation.
 */
class PollModeTransportImpl : public Homa::PollModeTransport {
  public:
    class Socket : public Homa::Socket {
      public:
        virtual Homa::unique_ptr<Homa::OutMessage> alloc();
        virtual Homa::unique_ptr<Homa::InMessage> receive(bool blocking);
        virtual void shutdown();
        virtual bool isShutdown() const;
        virtual Socket::Address getLocalAddress() const;

      protected:
        virtual void close();
    };

    PollModeTransportImpl(Driver* driver, uint64_t transportId);

    virtual Homa::unique_ptr<Socket> open(uint16_t port);
    virtual Driver* getDriver();
    virtual uint64_t getId();
    virtual void poll();

  private:
    Core::Transport* core;
};

}  // namespace Homa

#endif  // HOMA_TRANSPORTS_POLLMODETRANSPORT_H
