//
// Created by darby on 6/29/2024.
//

#pragma once

#include "Context.hpp"
#include "Buffer.hpp"

#include <cstdint>

namespace Fairy::Application {

class RingBuffer final {
public:
    RingBuffer(uint32_t ringSize, const Graphics::Context& context, size_t bufferSize, const std::string& name = "Ring Buffer");

    void moveToNextBuffer();

    [[nodiscard]] const Graphics::Buffer* buffer() const;

    [[nodiscard]] const std::shared_ptr<Graphics::Buffer>& buffer(uint32_t index) const {
        ASSERT(index < bufferRing_.size(), "index too large. index: " + std::to_string(index));
        return bufferRing_[index];
    }

private:
    uint32_t ringSize_;
    const Graphics::Context& context_;
    uint32_t ringIndex_ = 0;
    size_t bufferSize_;
    std::vector<std::shared_ptr<Graphics::Buffer>> bufferRing_;
};

}

