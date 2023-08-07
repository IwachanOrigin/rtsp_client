
#ifndef PACKET_QUEUE_H_
#define PACKET_QUEUE_H_

extern "C"
{
#include <SDL.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include "myavpacketlist.h"

class PacketQueue
{
public:
  explicit PacketQueue();
  ~PacketQueue();

  void init();
  int put(AVPacket *packet);
  int get(AVPacket *pkt, int block);
  void clear();
  void flush();

  int size;
  int nb_packets;
  int quit;
  SDL_cond *cond;

private:
  MyAVPacketList* first_pkt;
  MyAVPacketList* last_pkt;
  SDL_mutex *mutex;
};

#endif // PACKET_QUEUE_H_
