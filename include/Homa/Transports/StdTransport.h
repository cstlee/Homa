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

#include <Homa/Homa.h>

namespace Homa {

/**
 * An std::thread based Homa transport implementation.
 */
class StdTransport : public Transport {
  public:
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
     * @return
     *      Pointer to the new transport instance.
     */
    static StdTransport* create(Driver* driver, uint64_t transportId);
};

}  // namespace Homa

#endif  // HOMA_TRANSPORTS_STDTRANSPORT_H