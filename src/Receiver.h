/* Copyright (c) 2018-2019, Stanford University
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

#ifndef HOMA_CORE_RECEIVER_H
#define HOMA_CORE_RECEIVER_H

#include <atomic>
#include <deque>
#include <unordered_map>

#include <PerfUtils/TimeTrace.h>

#include <Homa/Driver.h>

#include "ControlPacket.h"
#include "Intrusive.h"
#include "Message.h"
#include "ObjectPool.h"
#include "Policy.h"
#include "Protocol.h"
#include "SpinLock.h"
#include "Timeout.h"

namespace Homa {
namespace Core {

/**
 * The Receiver processes incoming Data packets, assembling them into messages
 * and return the message to higher-level software on request.
 *
 * This class is thread-safe.
 */
class Receiver {
  private:
    // Forward declaration
    struct Peer;

  public:
    /**
     * Represents an incoming message that is being assembled or being processed
     * by the application.
     */
    class Message : public Core::Message {
      public:
        /**
         * Defines the possible states of this Message.
         */
        enum class State {
            IN_PROGRESS,  //< Receiver is in the process of receiving this
                          // message.
            COMPLETED,    //< Receiver has received the entire message.
            DROPPED,      //< Message was COMPLETED but the Receiver has lost
                          //< communication with the Sender.
        };

        explicit Message(Driver* driver, uint16_t packetHeaderLength,
                         uint32_t messageLength)
            : Core::Message(driver, packetHeaderLength, messageLength)
            , mutex()
            , id(0, 0)
            , source()
            , numExpectedPackets(0)
            , grantIndexLimit(0)
            , priority(0)
            , unreceivedBytes(messageLength)
            , state(Message::State::IN_PROGRESS)
            , op(nullptr)
            , peer(nullptr)
            , scheduledMessageNode(this)
            , receivedMessageNode(this)
            , messageTimeout(this)
            , resendTimeout(this)
        {}

        /**
         * Associate a particular Transport::Op with this Message.  Allows the
         * receiver to single the Transport about this Message when update
         * occur.
         */
        void registerOp(void* op)
        {
            SpinLock::Lock lock(mutex);
            this->op = op;
        }

        /**
         * Return the current state of this message.
         */
        State getState() const
        {
            return state.load();
        }

      private:
        /**
         * Implements a binary comparison function for the strict weak priority
         * ordering of two Message objects.
         */
        struct ComparePriority {
            bool operator()(const Message& a, const Message& b)
            {
                return a.unreceivedBytes < b.unreceivedBytes;
            }
        };

        /// Monitor style lock.
        mutable SpinLock mutex;
        /// Contains the unique identifier for this message.
        Protocol::MessageId id;
        /// Contains source address this message.
        Driver::Address source;
        /// Number of packets the message is expected to contain.
        uint16_t numExpectedPackets;
        /// The packet index up to which the Receiver as granted.
        uint16_t grantIndexLimit;
        /// The network priority at which the Receiver requests Message be sent.
        uint8_t priority;
        /// The number of bytes that still need to be received for this Message.
        std::atomic<uint32_t> unreceivedBytes;
        /// This message's current state.
        std::atomic<State> state;
        /// Transport::Op associated with this message.
        void* op;
        /// Peer object if any that holds this message (if any).
        Peer* peer;
        /// Intrusive structure used by the Receiver to keep track of when this
        /// message should be issued grants.
        Intrusive::List<Message>::Node scheduledMessageNode;
        /// Intrusive structure used by the Receiver to keep track of this
        /// message when it has been completely received.
        Intrusive::List<Message>::Node receivedMessageNode;
        /// Intrusive structure used by the Receiver to keep track when the
        /// receiving of this message should be considered failed.
        Timeout<Message> messageTimeout;
        /// Intrusive structure used by the Receiver to keep track when
        /// unreceived parts of this message should be re-requested.
        Timeout<Message> resendTimeout;

        friend class Receiver;
    };

    explicit Receiver(Transport* transport, Policy::Manager* policyManager,
                      uint64_t messageTimeoutCycles,
                      uint64_t resendIntervalCycles);
    virtual ~Receiver();
    virtual void handleDataPacket(Driver::Packet* packet, Driver* driver);
    virtual void handleBusyPacket(Driver::Packet* packet, Driver* driver);
    virtual void handlePingPacket(Driver::Packet* packet, Driver* driver);
    virtual Receiver::Message* receiveMessage();
    virtual void dropMessage(Receiver::Message* message);
    virtual void poll();

    /**
     * Send a DONE packet to the Sender of an incoming request message.
     *
     * @param message
     *      Incoming request (message) that should be acknowledged.
     * @param driver
     *      Driver with which the DONE packet should be sent.
     */
    static inline void sendDonePacket(Message* message, Driver* driver)
    {
        PerfUtils::TimeTrace::record("Receiver::sendDonePacket : START");
        SpinLock::Lock lock_message(message->mutex);
        ControlPacket::send<Protocol::Packet::DoneHeader>(
            driver, message->source, message->id);
        PerfUtils::TimeTrace::record("Receiver::sendDonePacket : DONE");
    }

    /**
     * Send an ERROR packet to the Sender of an incoming request message.
     *
     * @param message
     *      Incoming request (message) that should be failed.
     * @param driver
     *      Driver with which the ERROR packet should be sent.
     */
    static inline void sendErrorPacket(Message* message, Driver* driver)
    {
        PerfUtils::TimeTrace::record("Receiver::sendErrorPacket : START");
        SpinLock::Lock lock_message(message->mutex);
        ControlPacket::send<Protocol::Packet::ErrorHeader>(
            driver, message->source, message->id);
        PerfUtils::TimeTrace::record("Receiver::sendErrorPacket : DONE");
    }

  private:
    /**
     * Holds the incoming scheduled messages from another transport.
     */
    struct Peer {
        /**
         * Peer constructor.
         */
        Peer()
            : scheduledMessages()
            , scheduledPeerNode(this)
        {}

        /**
         * Peer destructor.
         */
        ~Peer()
        {
            scheduledMessages.clear();
        }

        /**
         * Implements a binary comparison function for the strict weak priority
         * ordering of two Peer objects.
         */
        struct ComparePriority {
            bool operator()(const Peer& a, const Peer& b)
            {
                assert(!a.scheduledMessages.empty());
                assert(!b.scheduledMessages.empty());
                Message::ComparePriority comp;
                return comp(a.scheduledMessages.front(),
                            b.scheduledMessages.front());
            }
        };

        /// Contains all the scheduled messages coming from a single transport.
        Intrusive::List<Message> scheduledMessages;
        /// Intrusive structure to track all Peers with scheduled messages.
        Intrusive::List<Peer>::Node scheduledPeerNode;
    };

    void checkMessageTimeouts();
    void checkResendTimeouts();
    void runScheduler();
    static Peer* schedule(Message* message,
                          std::unordered_map<Driver::Address, Peer>* peerTable,
                          Intrusive::List<Peer>* scheduledPeers);
    static Intrusive::List<Peer>::Iterator unschedule(
        Message* message, Receiver::Peer* peer,
        Intrusive::List<Peer>* scheduledPeers);
    static void updateSchedule(Message* message, Peer* peer,
                               Intrusive::List<Peer>* scheduledPeers);

    /**
     * Given an element in a list, move the element forward in the list until
     * the preceding element compares less than or equal to the given element.
     *
     * This function will call:
     *      bool ElementType::operator<(const ElementType&) const;
     *
     * @tparam ElementType
     *      Type of the element held in the Intrusive::List.
     * @tparam Compare
     *      A weak strict ordering binary comparator for objects of ElementType.
     * @param list
     *      List that contains the element.
     * @parma node
     *      Intrusive list node for the element that should be prioritized.
     * @param comp
     *      Comparison function object which returns true when the first
     *      argument should be ordered before the second.  The signature should
     *      be equivalent to the following:
     *          bool comp(const ElementType& a, const ElementType& b);
     */
    template <typename ElementType, typename Compare>
    static void prioritize(Intrusive::List<ElementType>* list,
                           typename Intrusive::List<ElementType>::Node* node,
                           Compare comp)
    {
        assert(list->contains(node));
        auto it_node = list->get(node);
        auto it_pos = it_node;
        while (it_pos != list->begin()) {
            if (!comp(*it_node, *std::prev(it_pos))) {
                // Found the correct location; just before it_pos.
                break;
            }
            --it_pos;
        }
        if (it_pos == it_node) {
            // Do nothing if the node is already in the right spot.
        } else {
            // Move the node to the new position in the list.
            list->remove(it_node);
            list->insert(it_pos, node);
        }
    }

    /// Mutex for monitor-style locking of Receiver state.
    SpinLock mutex;

    /// Transport of which this Receiver is a part.
    Transport* transport;

    /// Provider of network packet priority and grant policy decisions.
    Policy::Manager* policyManager;

    /// Tracks the set of inbound messages being received by this Receiver.
    std::unordered_map<Protocol::MessageId, Message*,
                       Protocol::MessageId::Hasher>
        inboundMessages;

    /// Collection of all peers; used for fast access.
    std::unordered_map<Driver::Address, Peer> peerTable;

    /// List of peers with inbound messages that require grants to complete.
    Intrusive::List<Peer> scheduledPeers;

    /// Message objects to be processed by the transport.
    struct {
        /// Monitor style lock.
        SpinLock mutex;
        /// List of completely received messages.
        Intrusive::List<Message> queue;
    } receivedMessages;

    /// Used to allocate Message objects.
    ObjectPool<Message> messagePool;

    /// Maintains Message objects in increasing order of timeout.
    TimeoutManager<Message> messageTimeouts;

    /// Maintains Message object in increase order of resend timeout.
    TimeoutManager<Message> resendTimeouts;

    /// True if the Receiver is executing schedule(); false, otherwise. Use to
    /// prevent concurrent calls to trySend() from blocking on each other.
    std::atomic_flag scheduling = ATOMIC_FLAG_INIT;
};

}  // namespace Core
}  // namespace Homa

#endif  // HOMA_CORE_RECEIVER_H
