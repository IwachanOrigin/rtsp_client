
#ifndef MY_AV_PACKET_LIST_H_
#define MY_AV_PACKET_LIST_H_

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

struct MyAVPacketList
{
  AVPacket pkt;
  struct MyAVPacketList* next;
};

#endif // MY_AV_PACKET_LIST_H_


