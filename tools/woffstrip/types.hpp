#pragma once
#include "main.hpp"
#include <arpa/inet.h> // [nh]to[hn][ls]


#define PACKED __attribute__((packed))
#define STRUCT struct PACKED

typedef uint32_t wuint32_t; // not strongly typed enough to enforce, but at lest for little/big endianness documentation..
typedef uint16_t wuint16_t;
typedef int16_t wint16_t; // woff/sfnt "Fword"
#define w2uint32(w) ntohl(w)
#define w2uint16(w) ntohs(w)
#define w2int16(w) ((int16_t)ntohs(w))
const char* w2str32(wuint32_t);
#define uint2w32(u) htonl(u)
#define uint2w16(u) htons(u)
#define int2w16(u) ((wint16_t)htons(u))
typedef uint32_t char_t; // utf-8
typedef unsigned index_t;

typedef struct {
	char_t from, to;
} char_range_t;
typedef struct {
	size_t from, to;
} range_t;

// https://www.w3.org/TR/2012/REC-WOFF-20121213/
// https://developer.apple.com/fonts/TrueType-Reference-Manual/
// https://docs.microsoft.com/en-us/typography/opentype/spec/font-file

STRUCT WoffHeader {
	wuint32_t signature;      // 0x774F4646 'wOFF'
	wuint32_t flavor;         // The "sfnt version" of the input font.
	wuint32_t length;         // Total size of the WOFF file.
	wuint16_t numTables;      // Number of entries in directory of font tables.
	wuint16_t reserved;       // Reserved; set to zero.
	wuint32_t totalSfntSize;  // Total size needed for the uncompressed font data, including the sfnt header, directory, and font tables (including padding).
	wuint16_t majorVersion;   // Major version of the WOFF file.
	wuint16_t minorVersion;   // Minor version of the WOFF file.
	wuint32_t metaOffset;     // Offset to metadata block, from beginning of WOFF file.
	wuint32_t metaLength;     // Length of compressed metadata block.
	wuint32_t metaOrigLength; // Uncompressed size of metadata block.
	wuint32_t privOffset;     // Offset to private data block, from beginning of WOFF file.
	wuint32_t privLength;     // Length of private data block.
	void print(const char* prefix=NULL, const char* head=NULL) const;
};

STRUCT WoffTableDirectoryEntry {
	wuint32_t tag;          // 4-byte sfnt table identifier.
	wuint32_t offset;       // Offset to the data, from beginning of WOFF file.
	wuint32_t compLength;   // Length of the compressed data, excluding padding.
	wuint32_t origLength;   // Length of the uncompressed table, excluding padding.
	wuint32_t origChecksum; // Checksum of the uncompressed table.
	void print(const char* prefix=NULL, const char* head=NULL) const;
};

STRUCT SfntHeader {
	wuint32_t flavor;        // The "sfnt version" of the input font.
	wuint16_t numTables;     // number of tables
	wuint16_t searchRange;   // (maximum power of 2 <= numTables)*16
	wuint16_t entrySelector; // log2(maximum power of 2 <= numTables)
	wuint16_t rangeShift;    // numTables*16-searchRange
};

STRUCT SfntTableDirectoryEntry {
	wuint32_t tag;      // 4-byte identifier
	wuint32_t checkSum; // checksum for this table
	wuint32_t offset;   // offset from beginning of sfnt
	wuint32_t length;   // length of this table in byte (actual length not padded length)
};

STRUCT WoffTableHead {
	PADMEMB[2+2 + 2+2];
	wuint32_t checkSumAdjustment;    // To compute: set it to 0, calculate the checksum for the 'head' table and put it in the table directory, sum the entire font as a uint32_t, then store 0xB1B0AFBA - sum. (The checksum for the 'head' table will be wrong as a result. That is OK; do not reset it.)
	PADMEMB[4];
	wuint16_t flags;                 // bit 0 - y value of 0 specifies baseline
	PADMEMB[2 + 8+8];
	wint16_t xMin, yMin, xMax, yMax; // for all glyph bounding boxes
	PADMEMB[2 + 2 + 2];
	wuint16_t indexToLocFormat;      // signed actually but only 0/1
	PADMEMB[2];
	void print(const char* prefix=NULL, const char* head=NULL) const;
};

STRUCT WoffCmapIndex {
	wuint16_t version;         // Version number (Set to zero)
	wuint16_t numberSubtables; // Number of encoding subtables
	void print(const char* prefix=NULL, const char* head=NULL) const;
};

STRUCT WoffCmapSubtable {
	wuint16_t platformID;         // Platform identifier
	wuint16_t platformSpecificID; // Platform-specific encoding identifier
	wuint32_t offset;             // Offset of the mapping table
	void print(const char* prefix=NULL, const char* head=NULL) const;
};

STRUCT WoffCmap0 {
	wuint16_t format;   // Set to 0
	wuint16_t length;   // Length in bytes of the subtable (set to 262 for format 0)
	wuint16_t language; // Language code (see above)
	uint8_t glyphIndexArray[256]; // An array that maps character codes to glyph index values
	void print(const char* prefix=NULL, const char* head=NULL) const;
};

STRUCT WoffCmap4 {
	wuint16_t format;        // Format number is set to 4
	wuint16_t length;        // Length of subtable in bytes
	wuint16_t language;      // Language code (see above)
	wuint16_t segCountX2;    // 2 * segCount
	wuint16_t searchRange;   // 2 * (2**FLOOR(log2(segCount)))
	wuint16_t entrySelector; // log2(searchRange/2)
	wuint16_t rangeShift;    // (2 * segCount) - searchRange
	// wuint16_t endCode[segCount];         // Ending character code for each segment, last = 0xFFFF.
	// wuint16_t reservedPad;               // This value should be zero
	// wuint16_t startCode[segCount];       // Starting character code for each segment
	// wuint16_t idDelta[segCount];         // Delta for all character codes in segment
	// wuint16_t idRangeOffset[segCount];   // Offset in bytes to glyph indexArray, or 0
	// wuint16_t glyphIndexArray[variable]; // Glyph index array
	void print(const char* prefix=NULL, const char* head=NULL) const;
};

STRUCT WoffCmap12 {
	wuint16_t format;   // Subtable format; set to 12.0
	PADMEMB[2];
	wuint32_t length;   // Byte length of this subtable (including the header)
	wuint32_t language; // Language code (see above)
	wuint32_t nGroups;  // Number of groupings which follow
	void print(const char* prefix=NULL, const char* head=NULL) const;
};
STRUCT WoffCmap12Group {
	wuint32_t startCharCode;  // First character code in this group
	wuint32_t endCharCode;    // Last character code in this group
	wuint32_t startGlyphCode; // Glyph index corresponding to the starting character code; subsequent charcters are mapped to sequential glyphs
	void print(const char* prefix=NULL, const char* head=NULL) const;
};

STRUCT WoffGlyph {
	wuint16_t numberOfContours; // If the number of contours is positive or zero, it is a single glyph; If the number of contours less than zero, the glyph is compound
	wint16_t xMin; // Minimum x for coordinate data
	wint16_t yMin; // Minimum y for coordinate data
	wint16_t xMax; // Maximum x for coordinate data
	wint16_t yMax; // Maximum y for coordinate data
	void print(const char* prefix=NULL, const char* head=NULL) const;
};
