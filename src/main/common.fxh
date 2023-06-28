
struct VS_INPUT {
  float4 Pos   : POSITION;
  float2 Tex   : TEXCOORD0;
};

struct v2f {
  float4 Pos   : SV_POSITION;
  float2 Tex   : TEXCOORD0;
};
