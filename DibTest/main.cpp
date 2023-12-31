#include <iostream>
#include <sstream>
#include <vector>
#include <cstdint>
#include <bit>
#include <array>
#include <span>

#include <windows.h>

// Check for machine and header assumptions
static_assert(sizeof(BITMAPINFOHEADER) == 0x28);
static_assert(sizeof(BITMAPV5HEADER) == 0x7C);
static_assert(std::endian::native == std::endian::little);


struct Rgba {
    unsigned char b = 0xFF, g = 0xFF, r = 0xFF, a = 0xFF;

    static constexpr uint32_t A_MASK = 0xFF000000;
    static constexpr uint32_t R_MASK =   0xFF0000;
    static constexpr uint32_t G_MASK =     0xFF00;
    static constexpr uint32_t B_MASK =       0xFF;
};

[[maybe_unused]] constexpr Rgba WHITE      { .b = 0xFF, .g = 0xFF, .r = 0xFF };
[[maybe_unused]] constexpr Rgba AQUA       { .b = 0xFF, .g = 0xFF, .r = 0 };
[[maybe_unused]] constexpr Rgba MISTY      { .b = 0xE1, .g = 0xE4, .r = 0xFF };
[[maybe_unused]] constexpr Rgba SEMI_BLACK { .b = 0, .g = 0, .r = 0, .a = 0x40 };
[[maybe_unused]] constexpr Rgba SEMI_AQUA  { .b = 0xFF, .g = 0xFF, .r = 0, .a = 0x40 };
[[maybe_unused]] constexpr Rgba SEMI_PINK  { .b = 0xFF, .g = 0, .r = 0xFF, .a = 0x40 };
[[maybe_unused]] constexpr Rgba RED        { .b = 0, .g = 0, .r = 0xFF };
[[maybe_unused]] constexpr Rgba GREEN      { .b = 0, .g = 0xAA, .r = 0 };
[[maybe_unused]] constexpr Rgba BLUE       { .b = 0xFF, .g = 0, .r = 0 };
[[maybe_unused]] constexpr Rgba YELLOW     { .b = 0, .g = 0xD7, .r = 0xFF };

class Image {
public:
    Image() = default;
    Image(size_t w, size_t h, Rgba color) { resize(w, h, color); }
    void resize(size_t w, size_t h, Rgba color);
    Rgba& at(size_t y, size_t x);
    const Rgba& at(size_t y, size_t x) const;
    Rgba& operator () (size_t y, size_t x) { return at(y, x); }
    const Rgba& operator ()(size_t y, size_t x) const { return at(y, x); }
    size_t width() const { return fWidth; }
    size_t height() const { return fHeight; }
    size_t area() const { return fWidth * fHeight; }
    size_t nBytes() const { return area() * sizeof(Rgba); }
    std::span<Rgba> scanLine(size_t y);
    std::span<const Rgba> scanLine(size_t y) const;
    Rgba* data() { return fData.data(); }
    const Rgba* data() const { return fData.data(); }
private:
    size_t fWidth = 0, fHeight = 0;
    std::vector<Rgba> fData;
};


void Image::resize(size_t w, size_t h, Rgba color)
{
    fWidth = w;
    fHeight = h;
    fData.resize(w * h);
    std::fill(fData.begin(), fData.end(), color);
}


Rgba& Image::at(size_t y, size_t x)
{
    if (y > fHeight || x > fWidth)
        throw std::out_of_range("y/x out of range");
    return fData[y * fWidth + x];
}


const Rgba& Image::at(size_t y, size_t x) const
{
    if (y > fHeight || x > fWidth)
        throw std::out_of_range("y/x out of range");
    return fData[y * fWidth + x];
}


std::span<Rgba> Image::scanLine(size_t y)
{
    if (y > fHeight)
        throw std::out_of_range("y/x out of range");
    return { fData.data() + (y * fWidth), fWidth };
}


std::span<const Rgba> Image::scanLine(size_t y) const
{
    if (y > fHeight)
        throw std::out_of_range("y/x out of range");
    return { fData.data() + (y * fWidth), fWidth };
}


void writeBitPalette(std::ostream& os)
{
    std::array<uint32_t, 3> pal { Rgba::R_MASK, Rgba::G_MASK, Rgba::B_MASK };
    static_assert(sizeof(pal) == 12);
    os.write(reinterpret_cast<const char*>(&pal), sizeof(pal));
}

template <class T>
inline size_t spanBytes(const std::span<T>& data)
    { return sizeof(T) * data.size(); }

void writeImageData(std::ostream& os, const Image& im)
{
    for (size_t y = im.height(); y != 0; ) { --y;
        auto scanLine = im.scanLine(y);
        os.write(reinterpret_cast<const char*>(scanLine.data()), spanBytes(scanLine));
    }
}

std::string makeOldDib(const Image& im)
{
    std::ostringstream os;
    BITMAPINFOHEADER header;
    // Header
    memset(&header, 0, sizeof(header));
    header.biSize = sizeof(header);
    header.biWidth = im.width();
    header.biHeight = im.height();
    header.biPlanes = 1;
    header.biBitCount = 32;
    header.biCompression = BI_BITFIELDS;
    header.biSizeImage = im.nBytes();
    os.write(reinterpret_cast<const char*>(&header), sizeof(header));
    // Palette
    writeBitPalette(os);
    // Image data
    writeImageData(os, im);
    return os.str();
}

enum class LongDib : bool { NO, YES };

std::string makeNewDib(const Image& im, LongDib isLong)
{
    std::ostringstream os;
    BITMAPV5HEADER header;
    // Header
    memset(&header, 0, sizeof(header));
    header.bV5Size = sizeof(header);
    header.bV5Width = im.width();
    header.bV5Height = im.height();
    header.bV5Planes = 1;
    header.bV5BitCount = 32;
    header.bV5ClrUsed = 0;
    header.bV5Compression = BI_BITFIELDS;
    header.bV5SizeImage = im.nBytes();
    header.bV5RedMask = Rgba::R_MASK;
    header.bV5GreenMask = Rgba::G_MASK;
    header.bV5BlueMask = Rgba::B_MASK;
    header.bV5AlphaMask = Rgba::A_MASK;
    // No alpha for now
    header.bV5CSType = LCS_sRGB;
    header.bV5Intent = LCS_GM_IMAGES;
    os.write(reinterpret_cast<const char*>(&header), sizeof(header));
    // Palette
    if (static_cast<bool>(isLong)) {
        writeBitPalette(os);
    }
    // Image data
    writeImageData(os, im);
    return os.str();
}

enum class Format { DIB_OLD, DIB_NEW_SHORT, DIB_NEW_LONG, BITMAP };

class Clipboard
{
public:
    Clipboard();
    ~Clipboard();

    void copyRaw(uint32_t nativeFormat, std::string_view data);
    void copyBitmap(const Image& im);
    void copyImage(const Image& im, Format fmt);
private:
    void clearIf();
    bool needClear = true;
};

Clipboard::Clipboard()
{
    if (!OpenClipboard(nullptr))
        throw std::logic_error("Cannot open clipboard");
}

Clipboard::~Clipboard()
{
    CloseClipboard();
}

void Clipboard::clearIf()
{
    if (needClear) {
        EmptyClipboard();
        needClear = false;
    }
}

void Clipboard::copyRaw(uint32_t nativeFormat, std::string_view data)
{
    clearIf();

    auto globalData = GlobalAlloc(GMEM_MOVEABLE, data.size());
    if (!globalData)
        throw std::logic_error("Cannot allocate data");
    auto copyData = GlobalLock(globalData);
    memcpy(copyData, data.data(), data.size());
    GlobalUnlock(globalData);

    SetClipboardData(nativeFormat, globalData);
}

void Clipboard::copyBitmap(const Image& im)
{
    clearIf();
    HBITMAP bm = CreateBitmap(im.width(), im.height(), 1, 32, im.data());
    SetClipboardData(CF_BITMAP, bm);
}


void Clipboard::copyImage(const Image& im, Format fmt)
{
    LongDib isLong = LongDib::NO;
    switch (fmt) {
    case Format::DIB_OLD: {
            auto data = makeOldDib(im);
            copyRaw(CF_DIB, data);
        } break;
    case Format::DIB_NEW_LONG:
        isLong = LongDib::YES;
        [[fallthrough]];
    case Format::DIB_NEW_SHORT: {
            auto data = makeNewDib(im, isLong);
            copyRaw(CF_DIBV5, data);
        } break;
    case Format::BITMAP:
        copyBitmap(im);
    }
}

Image makeImage(Rgba bg)
{
    Image image(12, 10, bg);
    size_t x0 = 0, x9 = image.width() - 1;
    size_t y0 = 0, y9 = image.height() - 1;
    // Left yellow, right blue
    for (size_t y = 1; y < y9; ++y) {
        image(y, x0) = YELLOW;
        image(y, x9) = BLUE;
    }
    // Top red
    auto topScan = image.scanLine(y0);
    std::fill(topScan.begin(), topScan.end(), RED);
    // Bottom green
    auto bottomScan = image.scanLine(y9);
    std::fill(bottomScan.begin(), bottomScan.end(), GREEN);

    return image;
}

int main()
{
    try {
        Image imBlack = makeImage(SEMI_BLACK);
        Image imPink = makeImage(SEMI_PINK);
        Image imAqua = makeImage(SEMI_AQUA);
        // Copy!
        Clipboard clip;
        clip.copyImage(imAqua, Format::DIB_OLD);
        clip.copyImage(imPink, Format::DIB_NEW_LONG);
        clip.copyImage(imBlack, Format::BITMAP);
        std::cout << "Successfully copied!\n";
    } catch (const std::exception& e) {
        std::cout << "ERROR: " << e.what() << '\n';
    }
}
