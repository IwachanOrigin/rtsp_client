
#include <chrono>
#include <thread>
#include <iostream>
#include "rtspcontroller.h"

#undef main

int main(int argc, char* argv[])
{
  if (argc < 2)
  {
    std::cerr << argv[0] << " <rtsp url>" << std::endl;
    return -1;
  }

  client::RtspController controller;
  for (int i = 1; i <= argc - 1; i++)
  {
    controller.openURL(argv[0], argv[i]);
  }

  // Start to Live555 event loop.
  controller.eventloop();
  
  return 0;
}
