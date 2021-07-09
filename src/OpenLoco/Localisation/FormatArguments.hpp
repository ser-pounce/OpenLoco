#pragma once

#include "../Interop/Interop.hpp"
#include <cassert>

using namespace OpenLoco::Interop;

namespace OpenLoco
{

    class FormatArguments
    {
    private:
        std::byte* _buffer;
        std::byte* _bufferStart;
        size_t _length;

    public:
        FormatArguments(std::byte* buffer, size_t length)
        {
            _bufferStart = buffer;
            _buffer = _bufferStart;
            _length = length;
        }

        FormatArguments()
        {
            loco_global<std::byte[0x0112C83A - 0x0112C826], 0x0112C826> _commonFormatArgs;

            _bufferStart = _buffer = &*_commonFormatArgs;
            _length = std::size(_commonFormatArgs);
        }

        template<typename... T>
        static FormatArguments common(T&&... args)
        {
            loco_global<std::byte[0x0112C83A - 0x0112C826], 0x0112C826> _commonFormatArgs;
            FormatArguments formatter{ _commonFormatArgs.get(), std::size(_commonFormatArgs) };
            (formatter.push(args), ...);
            return formatter;
        }

        template<typename... T>
        static FormatArguments mapToolTip(T&&... args)
        {
            loco_global<std::byte[40], 0x0050A018> _mapTooltipFormatArguments;
            FormatArguments formatter{ _mapTooltipFormatArguments.get(), std::size(_mapTooltipFormatArguments) };
            (formatter.push(args), ...);
            return formatter;
        }

        // Size in bytes to skip forward the buffer
        void skip(const size_t size)
        {
            auto* const nextOffset = getNextOffset(size);

            _buffer = nextOffset;
        }

        void pushBytes(std::byte const* start, std::byte const* end)
        {
            auto len = end - start;
            assert(len % 2 == 0);
            
            auto nextOffset = getNextOffset(len);

            std::copy(start, end, _buffer);
            _buffer = nextOffset;
        }

        template<typename T>
        void push(T arg)
        {
            static_assert(sizeof(T) % 2 == 0, "Tried to push an odd number of bytes onto the format args!");
            auto start = reinterpret_cast<std::byte const*>(&arg);
            pushBytes(start, start + sizeof arg);
        }

        const void* operator&()
        {
            return _bufferStart;
        }

        size_t getLength() const
        {
            return _buffer - _bufferStart;
        }

    private:
        std::byte* getNextOffset(const size_t size) const
        {
            std::byte* const nextOffset = reinterpret_cast<std::byte*>(reinterpret_cast<std::byte*>(_buffer) + size);
            if (nextOffset > _bufferStart + _length)
                throw std::out_of_range("FormatArguments: attempting to advance outside of buffer");
            return nextOffset;
        }
    };
}
