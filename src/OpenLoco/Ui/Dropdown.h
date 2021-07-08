#include "../Company.h"
#include "../Graphics/Colour.h"
#include "../Localisation/StringManager.h"
#include "../Window.h"
#include <cstdlib>
#include <variant>
#include <vector>

class OpenLoco::FormatArguments;

namespace OpenLoco::Ui::Dropdown
{
    using format_arg = std::variant<uint16_t, uint32_t, char const*>;

    class Index
    {
    public:
        Index(size_t);
        operator size_t() const;

    private:
        uint8_t _index;
    };

    void add(Index index, string_id title);
    void add(Index index, string_id title, std::initializer_list<format_arg> l);
    void add(Index index, string_id title, format_arg l);
    void add(Index index, string_id title, FormatArguments& fArgs);

    void setItemDisabled(size_t index);
    void setHighlightedItem(size_t index);
    void setItemSelected(size_t index);

    void show(int16_t x, int16_t y, int16_t width, int16_t height, Colour_t colour, size_t count, uint8_t itemHeight, uint8_t flags);
    void show(int16_t x, int16_t y, int16_t width, int16_t height, Colour_t colour, size_t count, uint8_t flags);
    void showImage(int16_t x, int16_t y, int16_t width, int16_t height, int16_t heightOffset, Colour_t colour, uint8_t columnCount, uint8_t count, uint8_t flags = 0);
    void showBelow(Window* window, WidgetIndex_t widgetIndex, size_t count, uint8_t flags);
    void showBelow(Window* window, WidgetIndex_t widgetIndex, size_t count, int8_t itemHeight, uint8_t flags);
    void showText(int16_t x, int16_t y, int16_t width, int16_t height, uint8_t itemHeight, Colour_t colour, size_t count, uint8_t flags);
    void showText(int16_t x, int16_t y, int16_t width, int16_t height, Colour_t colour, size_t count, uint8_t flags);
    void showText2(int16_t x, int16_t y, int16_t width, int16_t height, uint8_t itemHeight, Colour_t colour, size_t count, uint8_t flags);
    void showText2(int16_t x, int16_t y, int16_t width, int16_t height, Colour_t colour, size_t count, uint8_t flags);
    void showColour(const Window* window, const Widget* widget, uint32_t availableColours, Colour_t selectedColour, Colour_t dropdownColour);

    void populateCompanySelect(Window* window, Widget* widget);

    int16_t getHighlightedItem();
    CompanyId_t getCompanyIdFromSelection(int16_t itemIndex);
    uint16_t getItemArgument(Index index, uint8_t argument);
    uint16_t getItemsPerRow(uint8_t itemCount);
}
