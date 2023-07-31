
#include <iostream>
#include "packetqueue.h"

PacketQueue::PacketQueue()
  : quit(0)
  , first_pkt(nullptr)
  , last_pkt(nullptr)
  , nb_packets(0)
  , size(0)
{
}

PacketQueue::~PacketQueue()
{
}

void PacketQueue::init()
{
  quit = 0;
  this->clear();

  mutex = SDL_CreateMutex();
  if (!mutex)
  {
    return;
  }
  cond = SDL_CreateCond();
  if (!cond)
  {
    return;
  }
}

int PacketQueue::put(AVPacket *packet)
{
  MyAVPacketList* avPacketList;
  avPacketList = (MyAVPacketList*)av_malloc(sizeof(MyAVPacketList));

  // check the AVPacketList was allocated
  if (!avPacketList)
  {
    return -1;
  }

  // add reference to the given AVPacket
  avPacketList->pkt = *packet;

  // the new AVPacketList will be inserted at the end of the queue
  avPacketList->next = NULL;

  // lock mutex
  SDL_LockMutex(mutex);

  // check the queue is empty
  if (!last_pkt)
  {
    // if it is, insert as first
    first_pkt = avPacketList;
  }
  else
  {
    // if not, insert as last
    last_pkt->next = avPacketList;
  }

  // point the last AVPacketList in the queue to the newly created AVPacketList
  last_pkt = avPacketList;

  // increase by 1 the number of AVPackets in the queue
  nb_packets++;

  // increase queue size by adding the size of the newly inserted AVPacket
  size += avPacketList->pkt.size;

  // notify packet_queue_get which is waiting that a new packet is available
  SDL_CondSignal(cond);

  // unlock mutex
  SDL_UnlockMutex(mutex);

  return 0;
}

int PacketQueue::get(AVPacket *pkt, int block)
{
  int ret = 0;
  MyAVPacketList *avPacketList = nullptr;

  // lock mutex
  SDL_LockMutex(mutex);

  for (;;)
  {
    // check quit flag
    if (quit)
    {
      ret = -1;
      break;
    }

    // point to the first AVPacketList in the queue
    avPacketList = first_pkt;

    // if the first packet is not NULL, the queue is not empty
    if (avPacketList)
    {
      // place the second packet in the queue at first position
      first_pkt = avPacketList->next;

      // check if queue is empty after removal
      if (!first_pkt)
      {
        // first_pkt = last_pkt = NULL = empty queue
        last_pkt = NULL;
      }

      // decrease the number of packets in the queue
      nb_packets--;

      // decrease the size of the packets in the queue
      size -= avPacketList->pkt.size;

      // point pkt to the extracted packet, this will return to the calling function
      *pkt = avPacketList->pkt;

      // free memory
      av_free(avPacketList);

      ret = 1;
      break;
    }
    else if (!block)
    {
      ret = 0;
      break;
    }
    else
    {
      // unlock mutex and wait for cond signal, then lock mutex again
      SDL_CondWait(cond, mutex);
    }
  }

  // unlock mutex
  SDL_UnlockMutex(mutex);

  return ret;
}

void PacketQueue::clear()
{
  while (nb_packets > 0)
  {
    MyAVPacketList *avPacketList;
    // point to the first AVPacketList in the queue
    avPacketList = first_pkt;

    // if the first packet is not NULL, the queue is not empty
    if (avPacketList)
    {
      // place the second packet in the queue at first position
      first_pkt = avPacketList->next;

      // check if queue is empty after removal
      if (!first_pkt)
      {
        // first_pkt = last_pkt = NULL = empty queue
        last_pkt = NULL;
      }

      // decrease the number of packets in the queue
      nb_packets--;

      // decrease the size of the packets in the queue
      size -= avPacketList->pkt.size;

      // free packet memory
      av_packet_unref(&avPacketList->pkt);

      // free memory
      av_free(avPacketList);
    }
  }
}

void PacketQueue::flush()
{
  MyAVPacketList *pkt = nullptr, *pkt1 = nullptr;
  SDL_LockMutex(mutex);

  for (pkt = first_pkt; pkt != nullptr; pkt = pkt1)
  {
    pkt1 = pkt->next;
    av_packet_unref(&pkt->pkt);
    av_freep(&pkt);
  }

  last_pkt = nullptr;
  first_pkt = nullptr;
  nb_packets = 0;
  size = 0;

  SDL_UnlockMutex(mutex);
}
