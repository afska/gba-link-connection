#include "SceneUtils.h"

#include <libgba-sprite-engine/gba_engine.h>

int DEBULOG_LINE = 2;

void DEBULOG(std::string string) {
  TextStream::instance().setText(string, DEBULOG_LINE, -3);
  DEBULOG_LINE++;
  for (u32 i = DEBULOG_LINE; i < 20; i++)
    TextStream::instance().setText("                              ", i, -3);
  if (DEBULOG_LINE >= 20)
    DEBULOG_LINE = 2;
}
