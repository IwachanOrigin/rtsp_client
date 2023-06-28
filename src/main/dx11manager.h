
#ifndef DX11_MANAGER_H_
#define DX11_MANAGER_H_

#include "stdafx.h"
#include "pipeline.h"
#include "mesh.h"

using namespace Microsoft::WRL;

namespace manager
{

class DX11Manager
{
public:
  static DX11Manager& getInstance();

  bool init(const HWND hwnd, const uint32_t& width, const uint32_t& height, const uint32_t& fpsNum);
  bool updateTexture(const uint8_t* new_data, size_t data_size);
  bool render();

  ComPtr<ID3D11Device> getDevice() { return m_d3dDevice; }
  ComPtr<ID3D11DeviceContext> getDeviceContext() { return m_immediateContext; }

private:
  ComPtr<ID3D11Device> m_d3dDevice;
  ComPtr<ID3D11DeviceContext> m_immediateContext;
  ComPtr<ID3D11RenderTargetView> m_renderTargetView;
  ComPtr<IDXGISwapChain> m_swapchain;
  ComPtr<ID3D11SamplerState> m_samplerClampLinear;
  ComPtr<ID3D11ShaderResourceView> m_srv;
  ComPtr<ID3D11Resource> m_texture;
  uint32_t m_renderWidth;
  uint32_t m_renderHeight;
  uint32_t m_textureWidth;
  uint32_t m_textureHeight;
  Pipeline m_pipeline;
  Mesh m_quad;

  explicit DX11Manager();
  ~DX11Manager();
  explicit DX11Manager(const DX11Manager &);
  DX11Manager &operator=(const DX11Manager &);
  bool createTexture();
  bool createPipeline();
  bool createMesh();
};

} // manager

#endif // DX11_MANAGER_H_
