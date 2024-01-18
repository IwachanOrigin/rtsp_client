
#ifndef PACKET_QUEUE_H_
#define PACKET_QUEUE_H_

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include "myavpacketlist.h"
#include <queue>
#include <mutex>
#include <condition_variable>

namespace player
{

class PacketQueue
{
public:
  explicit PacketQueue();
  ~PacketQueue();

  void init();
  int push(AVPacket* packet);
  int pop(AVPacket* packet);
  void clear();

  int size() const { return m_size; }
  int nbPackets() const { return m_nbPackets; }

private:
  std::queue<MyAVPacketList*> m_myAvPacketListQueue;
  int m_frameNumber;
  int m_size;
  int m_nbPackets;
  std::mutex m_mutex;
  std::condition_variable m_cond;
};

} // player

#endif // PACKET_QUEUE_H_
