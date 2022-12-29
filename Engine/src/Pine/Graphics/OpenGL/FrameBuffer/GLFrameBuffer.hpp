#pragma once

#include "Pine/Graphics/Interfaces/IFrameBuffer.hpp"
#include "Pine/Graphics/OpenGL/Texture/GLTexture.hpp"

namespace Pine::Graphics
{

   class GLFrameBuffer : public IFrameBuffer
   {
   private:
       std::uint32_t m_Id = 0;

       GLTexture* m_ColorBuffer = nullptr;
       GLTexture* m_DepthBuffer = nullptr;
       GLTexture* m_NormalBuffer = nullptr;

       Vector2i m_Size = Vector2i(0);
   public:
       void Bind() override;
       void Dispose() override;

       bool Create(int width, int height, Buffers buffers) override;

       Vector2i GetSize() override;

       ITexture* GetColorBuffer() override;
       ITexture* GetDepthBuffer() override;
       ITexture* GetNormalBuffer() override;
   };

}