
#ifndef PIPELINE_H_
#define PIPELINE_H_

#include "stdafx.h"

using namespace Microsoft::WRL;

namespace manager
{

class Pipeline
{
public:
  explicit Pipeline();
  ~Pipeline();

  bool create(D3D11_INPUT_ELEMENT_DESC* input_elements, uint32_t ninput_elements);
  void activate() const;
  void destroy();

private:
  ComPtr<ID3D11VertexShader> m_vs;
  ComPtr<ID3D11InputLayout>  m_inputLayout;
  ComPtr<ID3D11PixelShader>  m_ps;

  bool compileShaderFromFile(const char* szFileName, const char* szEntryPoint, const char* szShaderModel, ID3DBlob** ppBlobOut);
};

} // manager

#endif // PIPELINE_H_
