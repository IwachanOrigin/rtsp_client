
#include "packetqueue.h"

using namespace player;

PacketQueue::PacketQueue()
  : m_nbPackets(0)
  , m_size(0)
  , m_frameNumber(0)
{
}

PacketQueue::~PacketQueue()
{
  if (m_nbPackets > 0)
  {
    this->clear();
  }
}
void PacketQueue::init()
{
  this->clear();
}

int PacketQueue::push(AVPacket* packet)
{
  // Lock mutex
  std::lock_guard<std::mutex> lock(m_mutex);

  MyAVPacketList* avPacketList = new MyAVPacketList();
  avPacketList->pkt = av_packet_alloc();

  // Move reference to the given AVPacket
  av_packet_move_ref(avPacketList->pkt, packet);

  // Increase by 1 the number of frame.
  m_frameNumber++;

  // Set frame number
  avPacketList->frameNumber = m_frameNumber;

  // Push to queue
  m_myAvPacketListQueue.push(avPacketList);

  // Increase by 1 the number of AVPackets in the queue
  m_nbPackets++;

  // Increase queue size by adding the size of the newly inserted AVPacket
  m_size += avPacketList->pkt->size;

  // notify packet_queue_get which is waiting that a new packet is available
  m_cond.notify_all();

  return 0;
}

int PacketQueue::pop(AVPacket* packet)
{
  int ret = -1;

  // Lock mutex
  std::unique_lock<std::mutex> lock(m_mutex);

  if (m_myAvPacketListQueue.empty() || m_nbPackets <= 0)
  {
    return ret;
  }

  auto myAvPacketList = m_myAvPacketListQueue.front();
  if (myAvPacketList)
  {
    // Decrease the number of packets in the queue
    m_nbPackets--;

    // Decrease the size of the packets in the queue
    m_size -= myAvPacketList->pkt->size;

    // 
    av_packet_move_ref(packet, myAvPacketList->pkt);

    // Set to ret value that the return value of frame number
    ret = myAvPacketList->frameNumber;

    // Release packet
    av_packet_unref(myAvPacketList->pkt);
    av_packet_free(&myAvPacketList->pkt);

    // Release pointer
    delete myAvPacketList;

    // Pop
    m_myAvPacketListQueue.pop();
  }
  else
  {
    // unlock mutex and wait for cond signal, then lock mutex again
    m_cond.wait(lock);
  }
  return ret;
}

void PacketQueue::clear()
{
  // Lock mutex
  std::unique_lock<std::mutex> lock(m_mutex);

  while (!m_myAvPacketListQueue.empty() && m_nbPackets > 0)
  {
    MyAVPacketList* avPacketList = m_myAvPacketListQueue.front();

    // Decrease the number of packets in the queue
    m_nbPackets--;

    // Decrease the size of the packets in the queue
    m_size -= avPacketList->pkt->size;

    // Release
    av_packet_unref(avPacketList->pkt);
    av_packet_free(&avPacketList->pkt);

    // Release pointer
    delete avPacketList;

    // pop
    m_myAvPacketListQueue.pop();
  }
}

