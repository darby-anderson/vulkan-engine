//
// Created by darby on 6/29/2024.
//

#include "RingBuffer.hpp"

namespace Fairy::Application {

RingBuffer::RingBuffer(uint32_t ringSize, const Graphics::Context& context, size_t bufferSize, const std::string& name)
    : ringSize_(ringSize),
      context_(context),
      bufferSize_(bufferSize)
{
    bufferRing_.reserve(ringSize);

    VkBufferUsageFlags usage =
#if defined(VK_KHR_buffer_device_address) && defined (_WIN32)
//        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
#endif
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

    for(uint32_t i = 0; i < ringSize_; i++) {
        bufferRing_.emplace_back(context_.createPersistentBuffer(bufferSize_, usage, name + " " + std::to_string(i)));
    }
}

void RingBuffer::moveToNextBuffer() {
    ringIndex_++;
    if(ringIndex_ >= ringSize_) {
        ringIndex_ = 0;
    }
}

const Graphics::Buffer* RingBuffer::buffer() const {
    return bufferRing_[ringIndex_].get();
}

}
