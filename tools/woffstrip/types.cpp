#include "types.hpp"


const char* w2str32(wuint32_t w) {
	static union {
		char s[5];
		wuint32_t w;
	} s;
	s.w = w;
	for (size_t i=0; i<4; ++i) {
		if (s.s[i] <= ' ' || s.s[i] >= '~') s.s[i] = '*';
	}
	s.s[4] = '\0';
	return s.s;
}

void WoffHeader::print(const char* prefix, const char* head) const {
	if (!config.dump) return;
	if (head) puts(head);
	prefix = prefix?:"";
	printf("%ssignature:      %s\n", prefix, w2str32(signature));
	printf("%sflavor:         %u\n", prefix, w2uint32(flavor));
	printf("%slength:         %u\n", prefix, w2uint32(length));
	printf("%snumTables:      %u\n", prefix, w2uint16(numTables));
	printf("%sreserved:       %u\n", prefix, w2uint16(reserved));
	printf("%stotalSfntSize:  %u\n", prefix, w2uint32(totalSfntSize));
	printf("%smajorVersion:   %u\n", prefix, w2uint16(majorVersion));
	printf("%sminorVersion:   %u\n", prefix, w2uint16(minorVersion));
	printf("%smetaOffset:     %u\n", prefix, w2uint32(metaOffset));
	printf("%smetaLength:     %u\n", prefix, w2uint32(metaLength));
	printf("%smetaOrigLength: %u\n", prefix, w2uint32(metaOrigLength));
	printf("%sprivOffset:     %u\n", prefix, w2uint32(privOffset));
	printf("%sprivLength:     %u\n", prefix, w2uint32(privLength));
}

void WoffTableDirectoryEntry::print(const char* prefix, const char* head) const {
	if (!config.dump) return;
	if (head) puts(head);
	prefix = prefix?:"";
	printf("%stag:          %s\n", prefix, w2str32(tag));
	printf("%soffset:       %u\n", prefix, w2uint32(offset));
	printf("%scompLength:   %u\n", prefix, w2uint32(compLength));
	printf("%sorigLength:   %u\n", prefix, w2uint32(origLength));
	printf("%sorigChecksum: %08x\n", prefix, w2uint32(origChecksum));
}

void WoffTableHead::print(const char* prefix, const char* head) const {
	if (!config.dump) return;
	if (head) puts(head);
	prefix = prefix?:"";
	printf("%scheckSumAdjustment: %08x\n", prefix, w2uint32(indexToLocFormat));
	printf("%sflags:              %04x\n", prefix, w2uint16(flags));
	printf("%smin:                %d/%d\n", prefix, w2int16(xMin), w2int16(yMin));
	printf("%smax:                %d/%d\n", prefix, w2int16(xMax), w2int16(yMax));
	printf("%sindexToLocFormat:   %u\n", prefix, w2uint16(indexToLocFormat));
}

void WoffCmapIndex::print(const char* prefix, const char* head) const {
	if (!config.dump) return;
	if (head) puts(head);
	prefix = prefix?:"";
	printf("%sversion:         %u\n", prefix, w2uint16(version));
	printf("%snumberSubtables: %u\n", prefix, w2uint16(numberSubtables));
}

void WoffCmapSubtable::print(const char* prefix, const char* head) const {
	if (!config.dump) return;
	if (head) puts(head);
	prefix = prefix?:"";
	printf("%splatformID:         %u\n", prefix, w2uint16(platformID));
	printf("%splatformSpecificID: %u\n", prefix, w2uint16(platformSpecificID));
	printf("%soffset:             %u\n", prefix, w2uint32(offset));
}

void WoffCmap0::print(const char* prefix, const char* head) const {
	if (!config.dump) return;
	if (head) puts(head);
	prefix = prefix?:"";
	printf("%sformat:   %u\n", prefix, w2uint16(format));
	printf("%slength:   %u\n", prefix, w2uint16(length));
	printf("%slanguage: %u\n", prefix, w2uint16(language));
	for (unsigned c=0; c<256; ++c) {
		if (!glyphIndexArray[c]) continue;
		printf("%s  %04x: %u\n", prefix, c, glyphIndexArray[c]);
	}
}

void WoffCmap4::print(const char* prefix, const char* head) const {
	if (!config.dump) return;
	if (head) puts(head);
	prefix = prefix?:"";
	printf("%sformat:        %u\n", prefix, w2uint16(format));
	printf("%slength:        %u\n", prefix, w2uint16(length));
	printf("%slanguage:      %u\n", prefix, w2uint16(language));
	printf("%ssegCountX2:    %u\n", prefix, w2uint16(segCountX2));
	printf("%ssearchRange:   %u\n", prefix, w2uint16(searchRange));
	printf("%sentrySelector: %u\n", prefix, w2uint16(entrySelector));
	printf("%srangeShift:    %u\n", prefix, w2uint16(rangeShift));
}

void WoffCmap12::print(const char* prefix, const char* head) const {
	if (!config.dump) return;
	if (head) puts(head);
	prefix = prefix?:"";
	printf("%sformat:   %u\n", prefix, w2uint16(format));
	printf("%slength:   %u\n", prefix, w2uint32(length));
	printf("%slanguage: %u\n", prefix, w2uint32(language));
	printf("%snGroups:  %u\n", prefix, w2uint32(nGroups));
}

void WoffCmap12Group::print(const char* prefix, const char* head) const {
	if (!config.dump) return;
	if (head) puts(head);
	prefix = prefix?:"";
	printf("%s%04x-%04x (@ %u)\n", prefix, w2uint32(startCharCode), w2uint32(endCharCode), w2uint32(startGlyphCode));
}

void WoffGlyph::print(const char* prefix, const char* head) const {
	if (!config.dump) return;
	if (head) puts(head);
	prefix = prefix?:"";
	printf("%snumberOfContours %u\n", prefix, w2uint16(numberOfContours));
	printf("%smin:             %d/%d\n", prefix, w2int16(xMin), w2int16(yMin));
	printf("%smax:             %d/%d\n", prefix, w2int16(xMax), w2int16(yMax));
}
