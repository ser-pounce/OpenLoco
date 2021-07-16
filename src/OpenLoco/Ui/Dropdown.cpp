#include "Dropdown.h"
#include "../CompanyManager.h"
#include "../Console.h"
#include "../Graphics/ImageIds.h"
#include "../Input.h"
#include "../Interop/Interop.hpp"
#include "../Localisation/FormatArguments.hpp"
#include "../Localisation/StringIds.h"
#include "../Objects/CompetitorObject.h"
#include "../Objects/ObjectManager.h"
#include "../Widget.h"
#include "../Window.h"

#include <cassert>
#include <cstdarg>
#include <limits>
#include <optional>

using namespace OpenLoco::Interop;

namespace
{
    namespace OL       = OpenLoco;
    namespace Gfx      = OL::Gfx;
    namespace Ui       = OL::Ui;
    namespace Dropdown = Ui::Dropdown;

    constexpr uint8_t bytes_per_item = 8;
    constexpr uint8_t max_items      = 40;

    loco_global<uint8_t[31], 0x005045FA> _colourMap1;
    loco_global<uint8_t[31], 0x00504619> _colourMap2;
    loco_global<std::uint8_t[33], 0x005046FA> _appropriateImageDropdownItemsPerRow;
    loco_global<Ui::WindowType, 0x0052336F> _pressedWindowType;
    loco_global<Ui::WindowNumber_t, 0x00523370> _pressedWindowNumber;
    loco_global<int32_t, 0x00523372> _pressedWidgetIndex;
    loco_global<int16_t, 0x112C876> _currentFontSpriteBase;
    loco_global<char[512], 0x0112CC04> _textBuffer;
    loco_global<uint8_t, 0x01136F94> _windowDropdownOnpaintCellX;
    loco_global<uint8_t, 0x01136F96> _windowDropdownOnpaintCellY;
    loco_global<uint16_t, 0x0113D84C> _dropdownItemCount;
    loco_global<uint32_t, 0x0113DC60> _dropdownDisabledItems;
    loco_global<uint32_t, 0x0113DC68> _dropdownItemHeight;
    loco_global<uint32_t, 0x0113DC6C> _dropdownItemWidth;
    loco_global<uint32_t, 0x0113DC70> _dropdownColumnCount;
    loco_global<uint32_t, 0x0113DC74> _dropdownRowCount;
    loco_global<uint16_t, 0x0113DC78> _word_113DC78;
    loco_global<int16_t, 0x0113D84E> _dropdownHighlightedIndex;
    loco_global<uint32_t, 0x0113DC64> _dropdownSelection;
    loco_global<OL::string_id[max_items], 0x0113D850> _dropdownItemFormats;
    loco_global<std::byte[max_items][bytes_per_item], 0x0113D8A0> _dropdownItemArgs;
    loco_global<std::byte[max_items][bytes_per_item], 0x0113D9E0> _dropdownItemArgs2;
    loco_global<uint8_t[max_items], 0x00113DB20> _menuOptions;
} // namespace

Dropdown::Index::Index(size_t index)
    : _index{ static_cast<uint8_t>(index) }
{
    assert(_index < max_items);
}

Dropdown::Index::operator uint8_t() const
{
    return _index;
}

Dropdown::Index& Dropdown::Index::operator++()
{
    ++_index;
    assert(_index < max_items);
    return *this;
}

void Dropdown::add(Index index, string_id format)
{
    _dropdownItemFormats[index] = format;
}

void Dropdown::add(Index index, string_id title, std::initializer_list<format_arg> l)
{
    add(index, title);

    auto dest = _dropdownItemArgs[index];

    for (auto arg : l)
    {
        std::visit([index, &dest](auto value) {
            assert(dest + sizeof value <= _dropdownItemArgs[index + 1]);
            auto src = reinterpret_cast<std::byte const*>(&value);
            dest = std::copy(src, src + sizeof value, dest);
        }, arg);
    }
}

void Dropdown::add(Index index, string_id title, FormatArguments& fArgs)
{
    add(index, title);

    auto argsLength = static_cast<uint8_t>(fArgs.getLength());
    assert(argsLength <= 2 * bytes_per_item);

    auto start = static_cast<std::byte const*>(&fArgs);
    auto mid   = start + std::min(argsLength, bytes_per_item);
    auto end   = mid   + std::max(static_cast<int>(argsLength) - bytes_per_item, 0);

    std::copy(start, mid, _dropdownItemArgs[index]);
    std::copy(mid, end, _dropdownItemArgs2[index]);
}

void Dropdown::add(Index index, string_id title, format_arg l)
{
    add(index, title, { l });
}

void Dropdown::setItemDisabled(size_t index)
{
    assert(index < CHAR_BIT * sizeof(uint32_t));
    _dropdownDisabledItems |= (1U << index);
}

int16_t Dropdown::getHighlightedItem()
{
    return _dropdownHighlightedIndex;
}

void Dropdown::setHighlightedItem(size_t index)
{
    assert(index < max_items);
    _dropdownHighlightedIndex = static_cast<int16_t>(index);
}

void Dropdown::clearHighlightedItem()
{
    _dropdownHighlightedIndex = -1;
}

void Dropdown::setItemSelected(size_t index)
{
    assert(index < CHAR_BIT * sizeof(uint32_t));
    _dropdownSelection |= (1U << index);
}

namespace
{
    Ui::Widget widgets[] = {
        makeWidget({ 0, 0 }, { 1, 1 }, Ui::WidgetType::wt_3, Ui::WindowColour::primary),
        Ui::widgetEnd()
    };

    Ui::WindowEventList events;

    // 0x004CD015
    void onUpdate(Ui::Window* self)
    {
        self->invalidate();
    }

    OL::FormatArguments getFormatArgs(Dropdown::Index index)
    {
        auto args = OL::FormatArguments();

        args.pushBytes(_dropdownItemArgs[index], std::end(_dropdownItemArgs[index]));
        args.pushBytes(_dropdownItemArgs2[index], std::end(_dropdownItemArgs2[index]));

        return args;
    }

    OL::Colour_t getPrimaryColour(Ui::Window const* self)
    {
        return self->getColour(Ui::WindowColour::primary);
    }

    bool isTranslucent(OL::Colour_t colour)
    {
        return colour & OL::Colour::translucent_flag;
    }

    bool isTranslucent(Ui::Window const* self)
    {
        return isTranslucent(getPrimaryColour(self));
    }

    auto getShadeFromPrimary(Ui::Window const* self, uint8_t shade)
    {
        return OL::Colour::getShade(getPrimaryColour(self), shade);
    }

    auto getOpaqueFromPrimary(Ui::Window const* self)
    {
        return OL::Colour::opaque(getPrimaryColour(self));
    }

    void resetCellPosition()
    {
        _windowDropdownOnpaintCellX = 0;
        _windowDropdownOnpaintCellY = 0;
    }

    void advanceCellPosition()
    {
        if (++_windowDropdownOnpaintCellX == _dropdownColumnCount)
        {
            _windowDropdownOnpaintCellX = 0;
            ++_windowDropdownOnpaintCellY;
        }
    }

    //TODO Replace with Gfx::point_t?
    auto getCellCoords(Ui::Window const* self)
    {
        return std::make_pair(
            _windowDropdownOnpaintCellX * _dropdownItemWidth  + self->x,
            _windowDropdownOnpaintCellY * _dropdownItemHeight + self->y);
    }

    bool isHighlighted(Dropdown::Index index)
    {
        return index == static_cast<size_t>(Dropdown::getHighlightedItem());
    }

    auto getItemFormat(Dropdown::Index index)
    {
        return _dropdownItemFormats[index];
    }

    auto getItemCount()
    {
        return _dropdownItemCount;
    }

    auto setItemCount(Dropdown::Index index)
    {
        _dropdownItemCount = index;
    }

    bool isSelectable(size_t index)
    {
        auto format = getItemFormat(index);
        return format == OL::StringIds::carrying_cargoid_sprite
            || format == OL::StringIds::dropdown_stringid
            || format == OL::StringIds::dropdown_without_checkmark;
    }

    bool isSelected(size_t index)
    {
        assert(index < CHAR_BIT * sizeof(uint32_t));
        return isSelectable(index) && _dropdownSelection & (1U << index);
    }

    bool empty(Dropdown::Index index)
    {
        return getItemFormat(index) == OL::StringIds::empty;
    }

    bool isText(Dropdown::Index index)
    {
        return getItemFormat(index) != static_cast<OL::string_id>(-2)
            && getItemFormat(index) != OL::StringIds::null;
    }

    void enableAllItems()
    {
        _dropdownDisabledItems = 0;
    }

    bool isDisabled(size_t index)
    {
        assert(index < CHAR_BIT * sizeof(uint32_t));
        return _dropdownDisabledItems & (1U << index);
    }

    void deselectAllItems()
    {
        _dropdownSelection = 0;
    }

    uint32_t makeTransparent(uint32_t colour)
    {
        return (1 << 25) | colour;
    }

    void drawHighlightedBackground(Ui::Window const* self, Gfx::Context* context)
    {
        auto [x, y] = getCellCoords(self);
        x += 2;
        y += 2;
        Gfx::drawRect(context, x, y, _dropdownItemWidth, _dropdownItemHeight, makeTransparent(OL::PaletteIndex::index_2E));
    }

    void formatString(Dropdown::Index index)
    {
        auto args = getFormatArgs(index);
        OL::StringManager::formatString(_textBuffer, getItemFormat(index), &args);
    }

    void addCheckmarkIfSelected(Dropdown::Index index)
    {
        if (isSelected(index))
        {
            Dropdown::add(index, getItemFormat(index) + 1);
        }
    }

    void drawString(Ui::Window const* self, Gfx::Context* context, Dropdown::Index index)
    {
        auto colour = getOpaqueFromPrimary(self);

        addCheckmarkIfSelected(index);

        if (isDisabled(index))
        {
            colour = OL::Colour::inset(colour);
        }

        if (isHighlighted(index))
        {
            colour = OL::Colour::white;
        }

        formatString(index);

        _currentFontSpriteBase = OL::Font::medium_bold;
        Gfx::clipString(self->width - 5, _textBuffer);

        auto [x, y] = getCellCoords(self);
        x += 2;
        y += 1;
        _currentFontSpriteBase = OL::Font::m1;
        Gfx::drawString(context, x, y, colour, _textBuffer);
    }

    void drawSeparator(Ui::Window const* self, Gfx::Context* context)
    {
        uint32_t colour1, colour2;

        if (isTranslucent(self))
        {
            colour1 = makeTransparent(_colourMap1[getOpaqueFromPrimary(self)]) + 1;
            colour2 = colour1 + 1;
        }
        else
        {
            colour1 = getShadeFromPrimary(self, 3);
            colour2 = getShadeFromPrimary(self, 7);
        }

        auto [x, y] = getCellCoords(self);
        x += 2;
        y += 1 + _dropdownItemHeight / 2;

        Gfx::drawRect(context, x, y,     _dropdownItemWidth - 1, 1, colour1);
        Gfx::drawRect(context, x, y + 1, _dropdownItemWidth - 1, 1, colour2);
    }

    auto getImageId(Dropdown::Index index)
    {
        uint32_t id;
        auto src = static_cast<std::byte const*>(_dropdownItemArgs[index]);
        std::copy(src, src + sizeof id, reinterpret_cast<std::byte*>(&id));
        return id;
    }

    void drawImage(Ui::Window const* self, Gfx::Context* context, Dropdown::Index index)
    {
        auto id = getImageId(index);

        if (getItemFormat(index) == static_cast<OL::string_id>(-2) && isHighlighted(index))
        {
            ++id;
        }

        auto [x, y] = getCellCoords(self);
        x += 2;
        y += 2;
        Gfx::drawImage(context, x, y, id);
    }

    void drawContent(Ui::Window const* self, Gfx::Context* context, Dropdown::Index index)
    {
        if (isHighlighted(index))
        {
            drawHighlightedBackground(self, context);
        }

        if (isText(index))
        {
            drawString(self, context, index);
        }
        else
        {
            drawImage(self, context, index);
        }
    }

    // 0x004CD00E
    void draw(Ui::Window* self, Gfx::Context* context)
    {
        self->draw(context);
        resetCellPosition();

        for (Dropdown::Index index; index < _dropdownItemCount; ++index)
        {
            if (!empty(index))
            {
                drawContent(self, context, index);
            }
            else
            {
                drawSeparator(self, context);
            }

            advanceCellPosition();
        }
    }

    void initEvents()
    {
        events.on_update = onUpdate;
        events.draw = draw;
    }

    void setTransparent(Ui::Window* window)
    {
        window->flags |= Ui::WindowFlags::transparent;
    }

    void createWindow(Gfx::point_t origin, Gfx::ui_size_t size, OL::Colour_t colour)
    {
        auto window = Ui::WindowManager::createWindow(Ui::WindowType::dropdown, origin, size, Ui::WindowFlags::stick_to_front, &events);

        window->setColour(Ui::WindowColour::primary, colour);
        window->widgets = widgets;

        if (isTranslucent(colour))
        {
            setTransparent(window);
        }
    }

    void resetDropdown()
    {
        Dropdown::clearHighlightedItem();
        enableAllItems();
        deselectAllItems();
    }

    // 0x004CCF1E
    void open(Gfx::point_t origin, Gfx::ui_size_t size, OL::Colour_t colour)
    {
        widgets[0].windowColour = Ui::WindowColour::primary;
        initEvents();
        resetDropdown();
        createWindow(origin, size, colour);
        OL::Input::state(OL::Input::State::dropdownActive);
    }

    // 0x004CC807 based on
    void setColourAndInputFlags(OL::Colour_t& colour, uint8_t flags)
    {
        if (isTranslucent(colour))
        {
            colour = _colourMap2[OL::Colour::opaque(colour)];
            colour = OL::Colour::translucent(colour);
        }

        OL::Input::resetFlag(OL::Input::Flags::flag1);
        OL::Input::resetFlag(OL::Input::Flags::flag2);

        if (flags & (1 << 7))
        {
            OL::Input::setFlag(OL::Input::Flags::flag1);
        }
    }

    // 0x004955BC
    uint16_t getStringWidth(char* buffer)
    {
        registers regs;
        regs.esi = reinterpret_cast<decltype(regs.esi)>(buffer);
        call(0x004955BC, regs);

        return regs.cx;
    }

    auto maxItemWidth(size_t count)
    {
        uint16_t maxStringWidth = 0;

        for (uint8_t index = 0; index < count; index++)
        {
            formatString(index);
            _currentFontSpriteBase = OL::Font::medium_bold;
            maxStringWidth = std::max(maxStringWidth, getStringWidth(_textBuffer));
        }

        return maxStringWidth + 3;
    }

    void setLayout(size_t count, uint32_t rows, uint8_t columns, uint16_t width, std::optional<uint8_t> height)
    {
        _dropdownColumnCount = columns;
        setItemCount(count);
        _dropdownRowCount = rows;
        _dropdownItemWidth = width;
        _dropdownItemHeight = height.value_or(10);

        widgets[0].right = _dropdownItemWidth * _dropdownColumnCount + 3;
        widgets[0].bottom = _dropdownItemHeight * _dropdownRowCount + 3;
    }

    auto overrideItemHeight(uint8_t flags, uint8_t height)
    {
        return flags & (1 << 6) ? std::optional{ height } : std::nullopt;
    }

    bool offScreenY(Gfx::point_t origin, Gfx::ui_size_t size)
    {
        return origin.y < 0 || (origin.y + size.height) > Ui::height();
    }

    void ensureOnScreen(Gfx::point_t& origin, Gfx::ui_size_t& size, Gfx::point_t parentOrigin, Gfx::ui_size_t parentSize)
    {
        if (origin.y + size.height > Ui::height())
        {
            origin.y = parentOrigin.y - size.height;
        }

        if (origin.y < 0)
        {
            origin = { parentOrigin.x + parentSize.width, 0 };

            if (origin.x + size.width > Ui::width())
            {
                origin.x = parentOrigin.x - size.width;
            }
        }

        origin.x = std::clamp<int16_t>(origin.x, 0, std::max(0, Ui::width() - size.width));
    }


    // 0x004CCAB2
    void showDropdown(Gfx::point_t parentOrigin, Gfx::ui_size_t parentSize, OL::Colour_t colour, size_t count, std::optional<uint8_t> itemHeight)
    {
        setLayout(count, count, 1, maxItemWidth(count), itemHeight);

        Gfx::ui_size_t size = { widgets[0].width(), widgets[0].height() };
        Gfx::point_t origin = { parentOrigin.x, parentOrigin.y + parentSize.height };

        ensureOnScreen(origin, size, parentOrigin, parentSize);

        open(origin, size, colour);
    }
}

namespace
{
    void invalidateItems()
    {
        for (auto i = 0; i < getItemCount(); i++)
        {
            Dropdown::add(i, OL::StringIds::empty);
        }
    }
}
/**
    * 0x004CC807
    *
    * @param x
    * @param y
    * @param width
    * @param height
    * @param colour
    * @param count
    * @param itemHeight
    * @param flags
    * Custom Dropdown height if flags & (1<<6) is true
    */
void Dropdown::show(int16_t x, int16_t y, int16_t width, int16_t height, Colour_t colour, size_t count, uint8_t itemHeight, uint8_t flags)
{
    WindowManager::close(WindowType::dropdown, 0);
    setColourAndInputFlags(colour, flags);
    invalidateItems();
    _word_113DC78 = 0;

    setLayout(count, count, 1, width, overrideItemHeight(flags, itemHeight));
    Gfx::ui_size_t size = { static_cast<uint16_t>(width), static_cast<uint16_t>(count * _dropdownItemHeight + 3) };
    Gfx::point_t origin = { x, y + height };

    ensureOnScreen(origin, size, { x, y }, { static_cast<uint16_t>(width), static_cast<uint16_t>(height) });
    open(origin, size, colour);
}

/**
    * 0x004CC807
    *
    * @param x
    * @param y
    * @param width
    * @param height
    * @param colour
    * @param count
    * @param flags
    */
void Dropdown::show(int16_t x, int16_t y, int16_t width, int16_t height, Colour_t colour, size_t count, uint8_t flags)
{
    show(x, y, width, height, colour, count, 0, flags & ~(1 << 6));
}

/**
    * 0x004CCDE7
    *
    * @param x
    * @param y
    * @param width
    * @param height
    * @param heightOffset
    * @param colour
    * @param columnCount
    * @param count
    * @param flags
    */
void Dropdown::showImage(int16_t x, int16_t y, int16_t width, int16_t height, int16_t heightOffset, Colour_t colour, uint8_t columnCount, uint8_t count, uint8_t flags)
{
    assert(count < std::size(_appropriateImageDropdownItemsPerRow));

    WindowManager::close(WindowType::dropdown, 0);
    setColourAndInputFlags(colour, flags);
    invalidateItems();
    _word_113DC78 = 0;

    auto rows = count / columnCount + ((count % columnCount) ? 1 : 0);
    setLayout(count, rows, columnCount, width, height);


    Gfx::ui_size_t size{ static_cast<uint16_t>(widgets[0].width() + 1),
                         static_cast<uint16_t>(widgets[0].height() + 1) };
    Gfx::point_t origin{ x, y + heightOffset };

    if (origin.y + size.height > Ui::height())
    {
        origin.y = y - height - size.height;
    }

    if (origin.y < 0)
    {
        origin = { x + widgets[0].width(), 0 };
    }

    origin.x = std::clamp<int16_t>(origin.x, 0, std::max(0, Ui::width() - size.width));
    

    open(origin, size, colour);
}

// 0x004CC989
void Dropdown::showBelow(Window* window, WidgetIndex_t widgetIndex, size_t count, int8_t itemHeight, uint8_t flags)
{
    WindowManager::close(WindowType::dropdown, 0);
    _word_113DC78 = 0;

    if (Input::state() != Input::State::widgetPressed || Input::hasFlag(Input::Flags::widgetPressed))
    {
        _word_113DC78 = _word_113DC78 | 1;
    }

    if (_pressedWindowType != WindowType::undefined)
    {
        WindowManager::invalidateWidget(_pressedWindowType, _pressedWindowNumber, _pressedWidgetIndex);
    }

    _pressedWidgetIndex = widgetIndex;
    _pressedWindowType = window->type;
    _pressedWindowNumber = window->number;
    WindowManager::invalidateWidget(_pressedWindowType, _pressedWindowNumber, _pressedWidgetIndex);

    auto widget = window->widgets[widgetIndex];
    auto colour = window->getColour(widget.windowColour);
    colour = Colour::translucent(colour);

    int16_t x = widget.left + window->x;
    int16_t y = widget.top + window->y;

    if (isTranslucent(colour))
    {
        colour = Colour::translucent(_colourMap2[Colour::opaque(colour)]);
    }

    Input::resetFlag(Input::Flags::flag1);
    Input::resetFlag(Input::Flags::flag2);

    if (flags & (1 << 7))
    {
        Input::setFlag(Input::Flags::flag1);
    }

    showDropdown({ x, y }, { widget.width(), widget.height() }, colour, count, overrideItemHeight(flags, itemHeight));
}

// 0x004CC989
void Dropdown::showBelow(Window* window, WidgetIndex_t widgetIndex, size_t count, uint8_t flags)
{
    showBelow(window, widgetIndex, count, 0, flags & ~(1 << 6));
}

/**
    * 0x004CCA6D
    * x @<cx>
    * y @<dx>
    * width @<bp>
    * height @<di>
    * colour @<al>
    * itemHeight @ <ah>
    * count @<bl>
    * flags @<bh>
    * Custom Dropdown height if flags & (1<<6) is true
    */
void Dropdown::showText(int16_t x, int16_t y, uint16_t width, uint16_t height, uint8_t itemHeight, Colour_t colour, size_t count, uint8_t flags)
{
    assert(count < std::numeric_limits<uint8_t>::max());

    setColourAndInputFlags(colour, flags);

    WindowManager::close(WindowType::dropdown, 0);
    _word_113DC78 = 0;

    showDropdown({ x, y }, { width, height }, colour, count, overrideItemHeight(flags, itemHeight));
}

// 0x004CCA6D
void Dropdown::showText(int16_t x, int16_t y, int16_t width, int16_t height, Colour_t colour, size_t count, uint8_t flags)
{
    showText(x, y, width, height, 0, colour, count, flags & ~(1 << 6));
}

/**
    * 0x004CCC7C
    * x @<cx>
    * y @<dx>
    * width @<bp>
    * height @<di>
    * colour @<al>
    * count @<bl>
    * flags @<bh>
    */
void Dropdown::showText2(int16_t x, int16_t y, int16_t width, int16_t height, uint8_t itemHeight, Colour_t colour, size_t count, uint8_t flags)
{
    assert(count < std::numeric_limits<uint8_t>::max());

    setColourAndInputFlags(colour, flags);

    WindowManager::close(WindowType::dropdown, 0);
    _word_113DC78 = 0;

    _dropdownColumnCount = 1;
    _dropdownItemWidth = width;
    _dropdownItemHeight = 10;

    if (flags & (1 << 6))
    {
        _dropdownItemHeight = itemHeight;
    }

    flags &= ~(1 << 6);

    _dropdownItemCount = static_cast<uint16_t>(count);
    _dropdownRowCount = static_cast<uint32_t>(count);
    uint16_t dropdownHeight = static_cast<uint16_t>(count) * _dropdownItemHeight + 3;
    widgets[0].bottom = dropdownHeight;
    dropdownHeight++;

    Gfx::ui_size_t size = { static_cast<uint16_t>(width), static_cast<uint16_t>(height) };
    Gfx::point_t origin = { x, y };
    origin.y += height;

    size.height = dropdownHeight;
    if ((size.height + origin.y) > Ui::height() || origin.y < 0)
    {
        origin.y -= (height + dropdownHeight);
        auto dropdownBottom = origin.y;

        if (origin.y >= 0)
        {
            dropdownBottom = origin.y + dropdownHeight;
        }

        if (origin.y < 0 || dropdownBottom > Ui::height())
        {
            origin.x += width + 3;
            origin.y = 0;
        }
    }

    size.width = width + 3;
    widgets[0].right = size.width;
    size.width++;

    if (origin.x < 0)
    {
        origin.x = 0;
    }

    origin.x += size.width;

    if (origin.x > Ui::width())
    {
        origin.x = Ui::width();
    }

    origin.x -= size.width;

    open(origin, size, colour);
}

void Dropdown::showText2(int16_t x, int16_t y, int16_t width, int16_t height, Colour_t colour, size_t count, uint8_t flags)
{
    showText2(x, y, width, height, 0, colour, count, flags & ~(1 << 6));
}

/**
    * 0x004CCF8C
    * window @ <esi>
    * widget @ <edi>
    * availableColours @<ebp>
    * dropdownColour @<al>
    * selectedColour @<ah>
    */
void Dropdown::showColour(const Window* window, const Widget* widget, uint32_t availableColours, Colour_t selectedColour, Colour_t dropdownColour)
{
    uint8_t count = 0;
    for (uint8_t i = 0; i < 32; i++)
    {
        if (availableColours & (1 << i))
            count++;
    }

    const uint8_t columnCount = getItemsPerRow(count);
    const uint8_t flags = 0x80;
    const uint8_t itemWidth = 16;
    const uint8_t itemHeight = 16;
    const int16_t x = window->x + widget->left;
    const int16_t y = window->y + widget->top;
    const int16_t heightOffset = widget->height() + 1;

    showImage(x, y, itemWidth, itemHeight, heightOffset, dropdownColour, columnCount, count, flags);

    uint8_t currentIndex = 0;
    for (uint8_t i = 0; i < 32; i++)
    {
        if (!(availableColours & (1 << i)))
            continue;

        if (i == selectedColour)
            Dropdown::setHighlightedItem(currentIndex);

        auto args = FormatArguments();
        args.push(Gfx::recolour(ImageIds::colour_swatch_recolourable_raised, i));
        args.push<uint16_t>(i);

        Dropdown::add(currentIndex, 0xFFFE, args);

        currentIndex++;
    }
}

// 0x004CF2B3
void Dropdown::populateCompanySelect(Window* window, Widget* widget)
{
    std::array<bool, 16> companyOrdered = {};

    CompanyId_t companyId = CompanyId::null;

    size_t index = 0;
    for (; index < CompanyManager::max_companies; index++)
    {
        int16_t maxPerformanceIndex = -1;
        for (const auto& company : CompanyManager::companies())
        {
            if (companyOrdered[company.id()] & 1)
                continue;

            if (maxPerformanceIndex < company.performance_index)
            {
                maxPerformanceIndex = company.performance_index;
                companyId = company.id();
            }
        }

        if (maxPerformanceIndex == -1)
            break;

        companyOrdered[companyId] |= 1;
        _dropdownItemFormats[index] = StringIds::dropdown_company_select;
        _menuOptions[index] = companyId;

        auto company = CompanyManager::get(companyId);
        auto competitorObj = ObjectManager::get<CompetitorObject>(company->competitor_id);
        auto ownerEmotion = company->owner_emotion;
        auto imageId = competitorObj->images[ownerEmotion];
        imageId = Gfx::recolour(imageId, company->mainColours.primary);

        add(index, StringIds::dropdown_company_select, { imageId, company->name });
    }
    auto x = widget->left + window->x;
    auto y = widget->top + window->y;
    auto colour = Colour::translucent(window->getColour(widget->windowColour));

    showText(x, y, widget->width(), widget->height(), 25, colour, index, (1 << 6));

    
    _word_113DC78 = _word_113DC78 | (1 << 1);

    size_t highlightedIndex = 0;

    while (window->owner != _menuOptions[highlightedIndex])
    {
        highlightedIndex++;

        if (highlightedIndex > CompanyManager::max_companies)
        {
            clearHighlightedItem();
            return;
        }
    }

    setHighlightedItem(highlightedIndex);
}

// 0x004CF284
OL::CompanyId_t Dropdown::getCompanyIdFromSelection(int16_t itemIndex)
{
    if (itemIndex == -1)
    {
        itemIndex = _dropdownHighlightedIndex;
    }

    auto companyId = _menuOptions[itemIndex];
    auto company = CompanyManager::get(companyId);

    if (company->empty())
    {
        companyId = CompanyId::null;
    }

    return companyId;
}

uint16_t Dropdown::getItemArgument(Index index, uint8_t argument)
{
    return reinterpret_cast<uint16_t*>(_dropdownItemArgs[index])[argument];
}

uint16_t Dropdown::getItemsPerRow(uint8_t itemCount)
{
    return _appropriateImageDropdownItemsPerRow[itemCount];
}
