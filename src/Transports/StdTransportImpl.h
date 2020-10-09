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

#ifndef HOMA_TRANSPORTS_STDTRANSPORT_H
#define HOMA_TRANSPORTS_STDTRANSPORT_H

#include <Homa/Core/Transport.h>
#include <Homa/Transports/StdTransport.h>

#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace Homa {

/**
 * A std::thread based Homa transport implementation.
 */
class StdTransportImpl : public Homa::StdTransport,
                         public Homa::Core::Transport::CallbackHandler {
  public:
    class Socket : public Homa::Socket {
      public:
        virtual Homa::unique_ptr<Homa::OutMessage> alloc();
        virtual Homa::unique_ptr<Homa::InMessage> receive(bool blocking);
        virtual void shutdown();
        virtual bool isShutdown() const;
        virtual Socket::Address getLocalAddress() const;

        void handleIncomingMessage(Homa::unique_ptr<Homa::InMessage> inMessage);

      protected:
        virtual void close();

      private:
        std::mutex m;
        std::condition_variable cv;
        std::deque<Homa::unique_ptr<Homa::InMessage>> messages;
    };

    StdTransportImpl(Driver* driver, uint64_t transportId);
    virtual Homa::unique_ptr<Socket> open(uint16_t port);
    virtual Driver* getDriver();
    virtual uint64_t getId();

    virtual void handleIncommingMessage(
        uint16_t port, Homa::unique_ptr<Homa::InMessage> inMessage);
    virtual void handleSendReady();
    virtual void handleGrantsReady();

  private:
    void receiverMain();
    void pacerMain();
    void granterMain();
    void timeoutMain();

    Core::Transport* const core;
    Driver* const driver;
    std::mutex mutex;
    std::condition_variable sendReady;
    std::condition_variable grantsReady;
    std::thread pacer;
    std::thread granter;
    std::thread timeoutHandler;
    std::unordered_map<uint16_t, Socket> sockets;
};

}  // namespace Homa

#endif  // HOMA_TRANSPORTS_STDTRANSPORT_H
