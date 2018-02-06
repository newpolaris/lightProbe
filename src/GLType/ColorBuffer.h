#pragma once

#include <cstdint>

class ColorBuffer
{
public:
    ColorBuffer();
    virtual ~ColorBuffer();

    bool create(uint32_t Width, uint32_t Height);
};