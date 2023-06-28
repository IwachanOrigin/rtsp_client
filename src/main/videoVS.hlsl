
#include "common.fxh"

v2f VS(VS_INPUT input)
{
  v2f output;
  output.Pos = input.Pos;
  output.Tex = input.Tex;
  return output;
}

