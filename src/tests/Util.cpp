#include "Util.hpp"
#include <utility>
#include <array>
#include <ranges>
#include <utf8.h>
#include <iostream>

static constexpr std::array<std::pair<char32_t, char32_t>, 213> ranges = { // {{{
	std::make_pair<char32_t, char32_t>(0x0, 0x7F), // Basic Latin
	std::make_pair<char32_t, char32_t>(0x80, 0xFF), // Latin-1 Supplement
	std::make_pair<char32_t, char32_t>(0x100, 0x17F), // Latin Extended-A
	std::make_pair<char32_t, char32_t>(0x180, 0x24F), // Latin Extended-B
	std::make_pair<char32_t, char32_t>(0x250, 0x2AF), // IPA Extensions
	std::make_pair<char32_t, char32_t>(0x2B0, 0x2FF), // Spacing Modifier Letters
	std::make_pair<char32_t, char32_t>(0x300, 0x36F), // Combining Diacritical Marks
	std::make_pair<char32_t, char32_t>(0x370, 0x3FF), // Greek and Coptic
	std::make_pair<char32_t, char32_t>(0x400, 0x4FF), // Cyrillic
	std::make_pair<char32_t, char32_t>(0x500, 0x527), // Cyrillic Supplement
	std::make_pair<char32_t, char32_t>(0x531, 0x58A), // Armenian
	std::make_pair<char32_t, char32_t>(0x591, 0x5F4), // Hebrew
	std::make_pair<char32_t, char32_t>(0x600, 0x6FF), // Arabic
	std::make_pair<char32_t, char32_t>(0x700, 0x74F), // Syriac
	std::make_pair<char32_t, char32_t>(0x750, 0x77F), // Arabic Supplement
	std::make_pair<char32_t, char32_t>(0x780, 0x7B1), // Thaana
	std::make_pair<char32_t, char32_t>(0x7C0, 0x7FA), // NKo
	std::make_pair<char32_t, char32_t>(0x800, 0x83E), // Samaritan
	std::make_pair<char32_t, char32_t>(0x840, 0x85E), // Mandaic
	std::make_pair<char32_t, char32_t>(0x900, 0x97F), // Devanagari
	std::make_pair<char32_t, char32_t>(0x981, 0x9FB), // Bengali
	std::make_pair<char32_t, char32_t>(0xA01, 0xA75), // Gurmukhi
	std::make_pair<char32_t, char32_t>(0xA81, 0xAF1), // Gujarati
	std::make_pair<char32_t, char32_t>(0xB01, 0xB77), // Oriya
	std::make_pair<char32_t, char32_t>(0xB82, 0xBFA), // Tamil
	std::make_pair<char32_t, char32_t>(0xC01, 0xC7F), // Telugu
	std::make_pair<char32_t, char32_t>(0xC82, 0xCF2), // Kannada
	std::make_pair<char32_t, char32_t>(0xD02, 0xD7F), // Malayalam
	std::make_pair<char32_t, char32_t>(0xD82, 0xDF4), // Sinhala
	std::make_pair<char32_t, char32_t>(0xE01, 0xE5B), // Thai
	std::make_pair<char32_t, char32_t>(0xE81, 0xEDD), // Lao
	std::make_pair<char32_t, char32_t>(0xF00, 0xFDA), // Tibetan
	std::make_pair<char32_t, char32_t>(0x1000, 0x109F), // Myanmar
	std::make_pair<char32_t, char32_t>(0x10A0, 0x10FC), // Georgian
	std::make_pair<char32_t, char32_t>(0x1100, 0x11FF), // Hangul Jamo
	std::make_pair<char32_t, char32_t>(0x1200, 0x137C), // Ethiopic
	std::make_pair<char32_t, char32_t>(0x1380, 0x1399), // Ethiopic Supplement
	std::make_pair<char32_t, char32_t>(0x13A0, 0x13F4), // Cherokee
	std::make_pair<char32_t, char32_t>(0x1400, 0x167F), // Unified Canadian Aboriginal Syllabics
	std::make_pair<char32_t, char32_t>(0x1680, 0x169C), // Ogham
	std::make_pair<char32_t, char32_t>(0x16A0, 0x16F0), // Runic
	std::make_pair<char32_t, char32_t>(0x1700, 0x1714), // Tagalog
	std::make_pair<char32_t, char32_t>(0x1720, 0x1736), // Hanunoo
	std::make_pair<char32_t, char32_t>(0x1740, 0x1753), // Buhid
	std::make_pair<char32_t, char32_t>(0x1760, 0x1773), // Tagbanwa
	std::make_pair<char32_t, char32_t>(0x1780, 0x17F9), // Khmer
	std::make_pair<char32_t, char32_t>(0x1800, 0x18AA), // Mongolian
	std::make_pair<char32_t, char32_t>(0x18B0, 0x18F5), // Unified Canadian Aboriginal Syllabics Extended
	std::make_pair<char32_t, char32_t>(0x1900, 0x194F), // Limbu
	std::make_pair<char32_t, char32_t>(0x1950, 0x1974), // Tai Le
	std::make_pair<char32_t, char32_t>(0x1980, 0x19DF), // New Tai Lue
	std::make_pair<char32_t, char32_t>(0x19E0, 0x19FF), // Khmer Symbols
	std::make_pair<char32_t, char32_t>(0x1A00, 0x1A1F), // Buginese
	std::make_pair<char32_t, char32_t>(0x1A20, 0x1AAD), // Tai Tham
	std::make_pair<char32_t, char32_t>(0x1B00, 0x1B7C), // Balinese
	std::make_pair<char32_t, char32_t>(0x1B80, 0x1BB9), // Sundanese
	std::make_pair<char32_t, char32_t>(0x1BC0, 0x1BFF), // Batak
	std::make_pair<char32_t, char32_t>(0x1C00, 0x1C4F), // Lepcha
	std::make_pair<char32_t, char32_t>(0x1C50, 0x1C7F), // Ol Chiki
	std::make_pair<char32_t, char32_t>(0x1CD0, 0x1CF2), // Vedic Extensions
	std::make_pair<char32_t, char32_t>(0x1D00, 0x1D7F), // Phonetic Extensions
	std::make_pair<char32_t, char32_t>(0x1D80, 0x1DBF), // Phonetic Extensions Supplement
	std::make_pair<char32_t, char32_t>(0x1DC0, 0x1DFF), // Combining Diacritical Marks Supplement
	std::make_pair<char32_t, char32_t>(0x1E00, 0x1EFF), // Latin Extended Additional
	std::make_pair<char32_t, char32_t>(0x1F00, 0x1FFE), // Greek Extended
	std::make_pair<char32_t, char32_t>(0x2000, 0x206F), // General Punctuation
	std::make_pair<char32_t, char32_t>(0x2070, 0x209C), // Superscripts and Subscripts
	std::make_pair<char32_t, char32_t>(0x20A0, 0x20B9), // Currency Symbols
	std::make_pair<char32_t, char32_t>(0x20D0, 0x20F0), // Combining Diacritical Marks for Symbols
	std::make_pair<char32_t, char32_t>(0x2100, 0x214F), // Letterlike Symbols
	std::make_pair<char32_t, char32_t>(0x2150, 0x2189), // Number Forms
	std::make_pair<char32_t, char32_t>(0x2190, 0x21FF), // Arrows
	std::make_pair<char32_t, char32_t>(0x2200, 0x22FF), // Mathematical Operators
	std::make_pair<char32_t, char32_t>(0x2300, 0x23F3), // Miscellaneous Technical
	std::make_pair<char32_t, char32_t>(0x2400, 0x2426), // Control Pictures
	std::make_pair<char32_t, char32_t>(0x2440, 0x244A), // Optical Character Recognition
	std::make_pair<char32_t, char32_t>(0x2460, 0x24FF), // Enclosed Alphanumerics
	std::make_pair<char32_t, char32_t>(0x2500, 0x257F), // Box Drawing
	std::make_pair<char32_t, char32_t>(0x2580, 0x259F), // Block Elements
	std::make_pair<char32_t, char32_t>(0x25A0, 0x25FF), // Geometric Shapes
	std::make_pair<char32_t, char32_t>(0x2600, 0x26FF), // Miscellaneous Symbols
	std::make_pair<char32_t, char32_t>(0x2701, 0x27BF), // Dingbats
	std::make_pair<char32_t, char32_t>(0x27C0, 0x27EF), // Miscellaneous Mathematical Symbols-A
	std::make_pair<char32_t, char32_t>(0x27F0, 0x27FF), // Supplemental Arrows-A
	std::make_pair<char32_t, char32_t>(0x2800, 0x28FF), // Braille Patterns
	std::make_pair<char32_t, char32_t>(0x2900, 0x297F), // Supplemental Arrows-B
	std::make_pair<char32_t, char32_t>(0x2980, 0x29FF), // Miscellaneous Mathematical Symbols-B
	std::make_pair<char32_t, char32_t>(0x2A00, 0x2AFF), // Supplemental Mathematical Operators
	std::make_pair<char32_t, char32_t>(0x2B00, 0x2B59), // Miscellaneous Symbols and Arrows
	std::make_pair<char32_t, char32_t>(0x2C00, 0x2C5E), // Glagolitic
	std::make_pair<char32_t, char32_t>(0x2C60, 0x2C7F), // Latin Extended-C
	std::make_pair<char32_t, char32_t>(0x2C80, 0x2CFF), // Coptic
	std::make_pair<char32_t, char32_t>(0x2D00, 0x2D25), // Georgian Supplement
	std::make_pair<char32_t, char32_t>(0x2D30, 0x2D7F), // Tifinagh
	std::make_pair<char32_t, char32_t>(0x2D80, 0x2DDE), // Ethiopic Extended
	std::make_pair<char32_t, char32_t>(0x2DE0, 0x2DFF), // Cyrillic Extended-A
	std::make_pair<char32_t, char32_t>(0x2E00, 0x2E31), // Supplemental Punctuation
	std::make_pair<char32_t, char32_t>(0x2E80, 0x2EF3), // CJK Radicals Supplement
	std::make_pair<char32_t, char32_t>(0x2F00, 0x2FD5), // Kangxi Radicals
	std::make_pair<char32_t, char32_t>(0x2FF0, 0x2FFB), // Ideographic Description Characters
	std::make_pair<char32_t, char32_t>(0x3000, 0x303F), // CJK Symbols and Punctuation
	std::make_pair<char32_t, char32_t>(0x3041, 0x309F), // Hiragana
	std::make_pair<char32_t, char32_t>(0x30A0, 0x30FF), // Katakana
	std::make_pair<char32_t, char32_t>(0x3105, 0x312D), // Bopomofo
	std::make_pair<char32_t, char32_t>(0x3131, 0x318E), // Hangul Compatibility Jamo
	std::make_pair<char32_t, char32_t>(0x3190, 0x319F), // Kanbun
	std::make_pair<char32_t, char32_t>(0x31A0, 0x31BA), // Bopomofo Extended
	std::make_pair<char32_t, char32_t>(0x31C0, 0x31E3), // CJK Strokes
	std::make_pair<char32_t, char32_t>(0x31F0, 0x31FF), // Katakana Phonetic Extensions
	std::make_pair<char32_t, char32_t>(0x3200, 0x32FE), // Enclosed CJK Letters and Months
	std::make_pair<char32_t, char32_t>(0x3300, 0x33FF), // CJK Compatibility
	std::make_pair<char32_t, char32_t>(0x3400, 0x4DB5), // CJK Unified Ideographs Extension A
	std::make_pair<char32_t, char32_t>(0x4DC0, 0x4DFF), // Yijing Hexagram Symbols
	std::make_pair<char32_t, char32_t>(0x4E00, 0x9FCB), // CJK Unified Ideographs
	std::make_pair<char32_t, char32_t>(0xA000, 0xA48C), // Yi Syllables
	std::make_pair<char32_t, char32_t>(0xA490, 0xA4C6), // Yi Radicals
	std::make_pair<char32_t, char32_t>(0xA4D0, 0xA4FF), // Lisu
	std::make_pair<char32_t, char32_t>(0xA500, 0xA62B), // Vai
	std::make_pair<char32_t, char32_t>(0xA640, 0xA697), // Cyrillic Extended-B
	std::make_pair<char32_t, char32_t>(0xA6A0, 0xA6F7), // Bamum
	std::make_pair<char32_t, char32_t>(0xA700, 0xA71F), // Modifier Tone Letters
	std::make_pair<char32_t, char32_t>(0xA720, 0xA7FF), // Latin Extended-D
	std::make_pair<char32_t, char32_t>(0xA800, 0xA82B), // Syloti Nagri
	std::make_pair<char32_t, char32_t>(0xA830, 0xA839), // Common Indic Number Forms
	std::make_pair<char32_t, char32_t>(0xA840, 0xA877), // Phags-pa
	std::make_pair<char32_t, char32_t>(0xA880, 0xA8D9), // Saurashtra
	std::make_pair<char32_t, char32_t>(0xA8E0, 0xA8FB), // Devanagari Extended
	std::make_pair<char32_t, char32_t>(0xA900, 0xA92F), // Kayah Li
	std::make_pair<char32_t, char32_t>(0xA930, 0xA95F), // Rejang
	std::make_pair<char32_t, char32_t>(0xA960, 0xA97C), // Hangul Jamo Extended-A
	std::make_pair<char32_t, char32_t>(0xA980, 0xA9DF), // Javanese
	std::make_pair<char32_t, char32_t>(0xAA00, 0xAA5F), // Cham
	std::make_pair<char32_t, char32_t>(0xAA80, 0xAADF), // Tai Viet
	std::make_pair<char32_t, char32_t>(0xE000, 0xF8FF), // Private Use Area
	std::make_pair<char32_t, char32_t>(0xF900, 0xFAD9), // CJK Compatibility Ideographs
	std::make_pair<char32_t, char32_t>(0xFB00, 0xFB4F), // Alphabetic Presentation Forms
	std::make_pair<char32_t, char32_t>(0xFB50, 0xFDFD), // Arabic Presentation Forms-A
	std::make_pair<char32_t, char32_t>(0xFE00, 0xFE0F), // Variation Selectors
	std::make_pair<char32_t, char32_t>(0xFE10, 0xFE19), // Vertical Forms
	std::make_pair<char32_t, char32_t>(0xFE20, 0xFE26), // Combining Half Marks
	std::make_pair<char32_t, char32_t>(0xFE30, 0xFE4F), // CJK Compatibility Forms
	std::make_pair<char32_t, char32_t>(0xFE50, 0xFE6B), // Small Form Variants
	std::make_pair<char32_t, char32_t>(0xFE70, 0xFEFF), // Arabic Presentation Forms-B
	std::make_pair<char32_t, char32_t>(0xFF01, 0xFFEE), // Halfwidth and Fullwidth Forms
	std::make_pair<char32_t, char32_t>(0xFFF9, 0xFFFD), // Specials
	std::make_pair<char32_t, char32_t>(0x10000, 0x1005D), // Linear B Syllabary
	std::make_pair<char32_t, char32_t>(0x10080, 0x100FA), // Linear B Ideograms
	std::make_pair<char32_t, char32_t>(0x10100, 0x1013F), // Aegean Numbers
	std::make_pair<char32_t, char32_t>(0x10140, 0x1018A), // Ancient Greek Numbers
	std::make_pair<char32_t, char32_t>(0x10190, 0x1019B), // Ancient Symbols
	std::make_pair<char32_t, char32_t>(0x101D0, 0x101FD), // Phaistos Disc
	std::make_pair<char32_t, char32_t>(0x10280, 0x1029C), // Lycian
	std::make_pair<char32_t, char32_t>(0x102A0, 0x102D0), // Carian
	std::make_pair<char32_t, char32_t>(0x10300, 0x10323), // Old Italic
	std::make_pair<char32_t, char32_t>(0x10330, 0x1034A), // Gothic
	std::make_pair<char32_t, char32_t>(0x10380, 0x1039F), // Ugaritic
	std::make_pair<char32_t, char32_t>(0x103A0, 0x103D5), // Old Persian
	std::make_pair<char32_t, char32_t>(0x10400, 0x1044F), // Deseret
	std::make_pair<char32_t, char32_t>(0x10450, 0x1047F), // Shavian
	std::make_pair<char32_t, char32_t>(0x10480, 0x104A9), // Osmanya
	std::make_pair<char32_t, char32_t>(0x10800, 0x1083F), // Cypriot Syllabary
	std::make_pair<char32_t, char32_t>(0x10840, 0x1085F), // Imperial Aramaic
	std::make_pair<char32_t, char32_t>(0x10900, 0x1091F), // Phoenician
	std::make_pair<char32_t, char32_t>(0x10920, 0x1093F), // Lydian
	std::make_pair<char32_t, char32_t>(0x10A00, 0x10A58), // Kharoshthi
	std::make_pair<char32_t, char32_t>(0x10A60, 0x10A7F), // Old South Arabian
	std::make_pair<char32_t, char32_t>(0x10B00, 0x10B3F), // Avestan
	std::make_pair<char32_t, char32_t>(0x10B40, 0x10B5F), // Inscriptional Parthian
	std::make_pair<char32_t, char32_t>(0x10B60, 0x10B7F), // Inscriptional Pahlavi
	std::make_pair<char32_t, char32_t>(0x10C00, 0x10C48), // Old Turkic
	std::make_pair<char32_t, char32_t>(0x10E60, 0x10E7E), // Rumi Numeral Symbols
	std::make_pair<char32_t, char32_t>(0x11000, 0x1106F), // Brahmi
	std::make_pair<char32_t, char32_t>(0x11080, 0x110C1), // Kaithi
	std::make_pair<char32_t, char32_t>(0x12000, 0x1236E), // Cuneiform
	std::make_pair<char32_t, char32_t>(0x12400, 0x12473), // Cuneiform Numbers and Punctuation
	std::make_pair<char32_t, char32_t>(0x13000, 0x1342E), // Egyptian Hieroglyphs
	std::make_pair<char32_t, char32_t>(0x16800, 0x16A38), // Bamum Supplement
	std::make_pair<char32_t, char32_t>(0x1B000, 0x1B001), // Kana Supplement
	std::make_pair<char32_t, char32_t>(0x1D000, 0x1D0F5), // Byzantine Musical Symbols
	std::make_pair<char32_t, char32_t>(0x1D100, 0x1D1DD), // Musical Symbols
	std::make_pair<char32_t, char32_t>(0x1D200, 0x1D245), // Ancient Greek Musical Notation
	std::make_pair<char32_t, char32_t>(0x1D300, 0x1D356), // Tai Xuan Jing Symbols
	std::make_pair<char32_t, char32_t>(0x1D360, 0x1D371), // Counting Rod Numerals
	std::make_pair<char32_t, char32_t>(0x1D400, 0x1D7FF), // Mathematical Alphanumeric Symbols
	std::make_pair<char32_t, char32_t>(0x1F000, 0x1F02B), // Mahjong Tiles
	std::make_pair<char32_t, char32_t>(0x1F030, 0x1F093), // Domino Tiles
	std::make_pair<char32_t, char32_t>(0x1F0A0, 0x1F0DF), // Playing Cards
	std::make_pair<char32_t, char32_t>(0x1F100, 0x1F1FF), // Enclosed Alphanumeric Supplement
	std::make_pair<char32_t, char32_t>(0x1F200, 0x1F251), // Enclosed Ideographic Supplement
	std::make_pair<char32_t, char32_t>(0x1F300, 0x1F5FF), // Miscellaneous Symbols And Pictographs
	std::make_pair<char32_t, char32_t>(0x1F601, 0x1F64F), // Emoticons
	std::make_pair<char32_t, char32_t>(0x1F680, 0x1F6C5), // Transport And Map Symbols
	std::make_pair<char32_t, char32_t>(0x1F700, 0x1F773), // Alchemical Symbols
	std::make_pair<char32_t, char32_t>(0x20000, 0x2A6D6), // CJK Unified Ideographs Extension B
	std::make_pair<char32_t, char32_t>(0x2A700, 0x2B734), // CJK Unified Ideographs Extension C
	std::make_pair<char32_t, char32_t>(0x2B740, 0x2B81D), // CJK Unified Ideographs Extension D
	std::make_pair<char32_t, char32_t>(0x2F800, 0x2FA1D), // CJK Compatibility Ideographs Supplement
	std::make_pair<char32_t, char32_t>(0xE0001, 0xE007F), // Tags
	std::make_pair<char32_t, char32_t>(0xE0100, 0xE01EF), // Variation Selectors Supplement
	std::make_pair<char32_t, char32_t>(0xF0000, 0xFFFFD), // Supplementary Private Use Area-A
	std::make_pair<char32_t, char32_t>(0x100000, 0x10FFFD), // Supplementary Private Use Area-B
}; // }}}

std::string randomString(std::mt19937& mt, std::size_t len)
{
	auto getCodepoint = [&mt] -> char32_t
	{
		// Get random range
		std::uniform_int_distribution<std::size_t> distrib(0uz, ranges.size()-1uz);
		auto&& [lo, hi] = ranges[distrib(mt)];

		// Get random codepoint
		std::uniform_int_distribution<char32_t> codepoint(lo, hi);
		return codepoint(mt);
	};

	std::string r;
	for ([[maybe_unused]] const auto _ : std::ranges::iota_view{0uz, len+1})
	    utf8::append(getCodepoint(), r);

	return r;
}
