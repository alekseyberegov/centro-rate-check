#include "woff.hpp"
#include "types.hpp"
#include "io.hpp"


Woff::Woff(char* b, size_t l):
	orig_buf(b), orig_len(l),
	header(NULL),
	ntables(0), tables(NULL), table_data(NULL),
	indexToLocFormat(0), nloca(0), loca(NULL) {
}


Woff::~Woff() {
	free((void*)orig_buf); // const-cast
	free(header);
	free(tables);
	for (unsigned i=0; i<ntables; ++i) {
		free(table_data[i]);
	}
	free(table_data);
	free(loca);
}


wuint32_t Woff::checksum(const wuint32_t* table, size_t len, wuint32_t sum) {
	assert(table && len);
	while (len >= sizeof(wuint32_t)) {
		sum = uint2w32(w2uint32(sum) + w2uint32(*table++));
		len -= sizeof(wuint32_t);
	}
	if (len) { // in case it's not fully initialized/padded (yet)
		wuint32_t rem = 0;
		memcpy(&rem, table, len);
		sum = uint2w32(w2uint32(sum) + w2uint32(rem));
	}
	return sum;
}


wuint32_t Woff::sfnt_checksum() const {
	wuint32_t csum = 0;

	SfntHeader sfnt_header;
	sfnt_header.flavor = header->flavor;
	sfnt_header.numTables = header->numTables;
	sfnt_header.searchRange = 1;
	sfnt_header.entrySelector = 0;
	while (sfnt_header.searchRange <= ntables) {
		sfnt_header.searchRange = sfnt_header.searchRange << 1;
		sfnt_header.entrySelector++;
	}
	sfnt_header.searchRange = uint2w16((sfnt_header.searchRange >> 1) * 16);
	sfnt_header.entrySelector = uint2w16(sfnt_header.entrySelector - 1);
	sfnt_header.rangeShift = uint2w16(ntables * 16 - w2uint16(sfnt_header.searchRange));
	csum = checksum((wuint32_t*)&sfnt_header, sizeof(sfnt_header), csum);

	wuint32_t orig_offset = uint2w32(PAD4(sizeof(SfntHeader)) + PAD4(ntables*sizeof(SfntTableDirectoryEntry)));
	for (unsigned i=0; i<ntables; ++i) {
		SfntTableDirectoryEntry entry;
		entry.tag = tables[i].tag;
		entry.checkSum = tables[i].origChecksum;
		entry.offset = orig_offset;
		entry.length = tables[i].origLength;
		csum = checksum((wuint32_t*)&entry, sizeof(entry), csum);
		csum = checksum(&tables[i].origChecksum, sizeof(tables[i].origChecksum), csum);
		orig_offset = uint2w32(w2uint32(orig_offset) + PAD4(w2uint32(tables[i].origLength)));
	}

	return uint2w32(0xB1B0AFBAu - w2uint32(csum));
}


bool Woff::update_sfnt_checksum() {
	char* headdata = NULL;
	WoffTableDirectoryEntry* head = get_table("head", &headdata);
	if (!head) return false;

	assert(w2uint32(head->origLength) == sizeof(WoffTableHead));
	((WoffTableHead*)headdata)->checkSumAdjustment = sfnt_checksum();
	if (!set_table("head", headdata, sizeof(WoffTableHead), false)) {
		free(headdata);
		return false;
	}

	LOG_INFO("updated full header checksum: %08x", w2uint32(((WoffTableHead*)headdata)->checkSumAdjustment));
	free(headdata);
	return true;
}


wuint32_t Woff::table_checksum(const char* name, const char* data, size_t len) {
	wuint32_t csum;
	if (strcmp(name, "head") == 0) {
		WoffTableHead head;
		assert(len == sizeof(head));
		memcpy(&head, data, sizeof(head));
		head.checkSumAdjustment = 0; // To calculate the checkSum for the 'head' table which itself includes the checkSumAdjustment entry for the entire font, do the following: Set the checkSumAdjustment to 0.
		csum = checksum((wuint32_t*)&head, sizeof(head));
	} else {
		csum = checksum((wuint32_t*)data, len);
	}
	return csum;
}


int Woff::get_table_index(const char* name) const {
	for (unsigned i=0; i<ntables; ++i) {
		if (strcmp(name, w2str32(tables[i].tag)) == 0) return i;
	}
	return -1;
}


WoffTableDirectoryEntry* Woff::get_table(const char* name, char** data) {
	int i = get_table_index(name);
	if (i < 0) {
		LOG("table '%s' not found", name);
		if (data) *data = NULL;
		return NULL;
	}
	WoffTableDirectoryEntry* table = &tables[i];

	if (data) {
		if (!decompress(table_data[i], w2uint32(table->compLength), *data, w2uint32(table->origLength))) {
			*data = NULL;
			return NULL;
		}
		if (table->origChecksum != table_checksum(name, *data, w2uint32(table->origLength))) {
			LOG_INFO("table '%s' checksum mismatch", name);
		}
	}

	return table;
}


bool Woff::set_table(const char* name, const char* data, size_t len, bool update_csum) {
	WoffTableDirectoryEntry* table = get_table(name);
	if (!table) return false;
	int index = get_table_index(name);
	assert(index >= 0);

	if (update_csum) {
		wuint32_t csum = table_checksum(name, data, len);
		if (csum == table->origChecksum) {
			LOG("checksum for '%s' has not changed", name);
			return false;
		}
		table->origChecksum = csum;
		LOG_INFO("updated checksum for '%s': %08x", name, w2uint32(csum));
	}

	char* cdata;
	size_t clen;
	if (!docompress(data, len, cdata, &clen)) return false;
	free(table_data[index]);
	table_data[index] = cdata;
	table->compLength = uint2w32(clen);
	table->origLength = uint2w32(len);
	LOG_INFO("updated data for '%s'", name);

	return true;
}


bool Woff::finalize() {
	if (!update_offsets()) return false;
	if (!update_sfnt_checksum()) return false;
	return true;
}


WoffGlyph* Woff::get_glyph(size_t start, size_t end, char*& buf, size_t& len) {
	buf = NULL;
	WoffTableDirectoryEntry* glyf = get_table("glyf", &buf);
	if (!glyf) return NULL;
	len = (size_t)w2uint32(glyf->origLength);

	assert(end > start && end-start >= sizeof(WoffGlyph));
	if (len < end) {
		free(buf);
		buf = NULL;
		return NULL;
	}
	return (WoffGlyph*)(buf+start);
}


int Woff::align_glyph(WoffGlyph* g, int setTo) {
	if (!w2uint16(g->numberOfContours)) {
		return 0; // empty/deleted
	} else if ((int16_t)w2uint16(g->numberOfContours) < 0) {
		return -1; // compound glyph
	}

	if (w2int16(g->yMin) <= setTo) {
		return 0;
	}
	const int32_t adjust = -(w2int16(g->yMin) - (int)setTo);

	uint16_t numberOfContours = w2uint16(g->numberOfContours);
	wuint16_t* endPtsOfContours = (wuint16_t*)(g + 1); // Array of last points of each contour; n is the number of contours; array entries are point indices
	for (unsigned i=0; i<numberOfContours; ++i) {
		LOG_DUMP("  endPtsOfContours %u: %u", i, w2uint16(endPtsOfContours[i]));
	}
	uint16_t coords = w2uint16(endPtsOfContours[numberOfContours-1])+1;
	if (!coords) {
		return -1;
	}

	wuint16_t* instructionLength = endPtsOfContours + numberOfContours;
	uint8_t* instructions = (uint8_t*)(instructionLength + 1);

	uint8_t* flagptr = instructions + w2uint16(*instructionLength);
	uint8_t** flags = (uint8_t**)calloc(coords, sizeof(uint8_t*));
	uint16_t flaglen = 0;
	for (uint16_t f=0; f<coords;) {
		assert((flagptr[flaglen] & 0xC0) == 0); // reserved
		if (flagptr[flaglen] & 0x08) { // If set, the next byte specifies the number of additional times this set of flags is to be repeated. In this way, the number of flags listed can be smaller than the number of points in a character.
			if (flaglen+1 >= coords || !flagptr[flaglen+1] || f+flagptr[flaglen+1] >= coords || flaglen+flagptr[flaglen+1] >= coords) {
				LOG("Bogus repeat flag: %u %u %u %u", flagptr[flaglen+1], f, flaglen, coords);
				free(flags);
				return -1;
			}
			for (uint16_t ff=0; ff<flagptr[flaglen+1]+1; ++ff) {
				flags[f++] = &flagptr[flaglen]; // duplicate - keeps repeat flag
			}
			flaglen+=2;
		} else {
			flags[f++] = &flagptr[flaglen++];
		}
	}
	LOG_DUMP("  %u of %u flags given", flaglen, coords);

	uint8_t* x_start = &flagptr[flaglen];
	uint8_t* y_start = x_start;
	for (uint16_t i=0; i<coords; ++i) {
		if (*flags[i] & 0x02) { // X_SHORT_VECTOR: the corresponding coordinate is 1 byte long
			y_start++;
		} else if (*flags[i] & 0x10) { // X_IS_SAME_OR_POSITIVE_X_SHORT_VECTOR: the current coordinate is the same as the previous coordinate
		} else { // the current coordinate is a signed 16-bit delta vector
			y_start+=2;
		}
	}

	int32_t val;
	bool fixed = false;
	assert(adjust < 0);
	if (*flags[0] & 0x04) { // Y_SHORT_VECTOR
		if (*flags[0] & 0x20) { // positive
			val = *y_start;
			if (val + adjust >= 0) {
				*y_start = val + adjust;
				fixed = true;
			} else if (abs(val + adjust) <= UINT8_MAX) {
				*y_start = abs(val + adjust);
				*flags[0] &= ~0x20; // set to negative
				fixed = true;
			}
		} else { // negative
			val = -((int32_t)(*y_start));
			if (abs(val + adjust) <= UINT8_MAX) {
				*y_start = abs(val + adjust);
				fixed = true;
			}
		}
	} else if ((*flags[0] & 0x20) == 0) { // !Y_IS_SAME_OR_POSITIVE_X_SHORT_VECTOR: the current coordinate is a signed 16-bit delta vector
		val = (int32_t)(w2uint16(*((wuint16_t*)y_start))); // signed 16bit
		if (val + adjust >= INT16_MIN) {
			*((int16_t*)y_start) = (int16_t)uint2w16((int16_t)(val + adjust));
			fixed = true;
		}
	}

	if (!fixed) {
		LOG("TODO: could not adjust coordinates, going on anyways.."); // TODO: memmove + insert needed
		free(flags);
		return 0; // not fatal
	}
	free(flags);

	assert(w2int16(g->yMin) + adjust == setTo);
	g->yMin = int2w16(w2int16(g->yMin) + adjust);
	g->yMax = int2w16(w2int16(g->yMax) + adjust);
	g->print("  ", "aligned:");

	return setTo;
}


size_t Woff::delete_glyph(size_t start, size_t end) {
	char* glyfbuf = NULL;
	size_t glyflen;
	WoffGlyph* g = get_glyph(start, end, glyfbuf, glyflen);
	if (!g) return false;

	// TODO: completely removing should be legit, as: "If a glyph has no outline, then loca[n] = loca [n+1]." - but this gave display issues?..
	memmove(glyfbuf+start+sizeof(WoffGlyph), glyfbuf+end, glyflen-end);
	glyflen -= end-start-sizeof(WoffGlyph);

	g->print("  ", "deleting glyph contours");
	g->numberOfContours = uint2w16(0); // keep min/max bounding box

	if (!set_table("glyf", glyfbuf, glyflen)) {
		free(glyfbuf);
		return 0;
	}
	free(glyfbuf);
	return end-start-sizeof(WoffGlyph);
}


bool Woff::update_offsets() {
	uint32_t sfntlen = PAD4(sizeof(SfntHeader)) + PAD4(ntables * sizeof(SfntTableDirectoryEntry));
	uint32_t offset = PAD4(sizeof(WoffHeader)) + PAD4(ntables * sizeof(WoffTableDirectoryEntry));
	for (unsigned i=0; i<ntables; ++i) {
		WoffTableDirectoryEntry* table = &tables[i];
		table->offset = uint2w32(offset);
		sfntlen += PAD4(w2uint32(table->origLength));
		offset += PAD4(w2uint32(table->compLength));
	}
	header->length = uint2w32(offset);
	header->totalSfntSize = uint2w32(sfntlen);
	return true;
}


bool Woff::parseHeader() {
	if (orig_len < sizeof(WoffHeader)) return false;
	header = (WoffHeader*)memcpy(calloc(1, sizeof(WoffHeader)+4), orig_buf, sizeof(WoffHeader));
	header->print("  ", "WOFF header");

	if (w2uint32(header->length) != orig_len) { // TODO: more checks
		LOG("header length mismatch");
		return false;
	}
	if (w2uint32(header->metaLength) || w2uint32(header->privLength)) {
		LOG("unknown metadata present");
		return false;
	}
	return true;
}


bool Woff::parseTables() {
	ntables = w2uint16(header->numTables);
	if (!ntables || (sizeof(WoffHeader) + ntables * sizeof(WoffTableDirectoryEntry)) > orig_len) {
		return false;
	}
	tables = (WoffTableDirectoryEntry*)memcpy(
		calloc(1, ntables * sizeof(WoffTableDirectoryEntry) + 4),
		orig_buf + sizeof(WoffHeader),
		ntables * sizeof(WoffTableDirectoryEntry)
	);

	for (unsigned i=0; i<ntables; ++i) {
		tables[i].print("  ", "table");
		assert(w2uint32(tables[i].offset) % 4 == 0); // TODO: more checks
	}

	table_data = (char**)calloc(ntables, sizeof(char*));
	for (unsigned i=0; i<ntables; ++i) {
		if (w2uint32(tables[i].offset) + w2uint32(tables[i].compLength) > orig_len) {
			return false;
		}
		table_data[i] = (char*)memcpy(
			calloc(1, w2uint32(tables[i].compLength) + 4),
			orig_buf + w2uint32(tables[i].offset),
			w2uint32(tables[i].compLength)
		);
	}

	char* headdata = NULL;
	WoffTableDirectoryEntry* head = get_table("head", &headdata);
	if (!head) return false;
	((WoffTableHead*)headdata)->print("  ", "head table");
	assert(w2uint32(head->origLength) == sizeof(WoffTableHead));
	if (((WoffTableHead*)headdata)->checkSumAdjustment != sfnt_checksum()) {
		LOG_INFO("overall checksum mismatch, ignoring");
	}
	free(headdata);

	return true;
}


bool Woff::parseCharMaps() {
	char* buf = NULL;
	WoffTableDirectoryEntry* cmap = get_table("cmap", &buf);
	if (!cmap) return false;

	if (!cmaps.parse(buf, w2uint32(cmap->origLength))) {
		free(buf);
		return false;
	}
	free(buf);
	return true;
}


bool Woff::parseLoca() {
	char* locabuf = NULL;
	WoffTableDirectoryEntry* l = get_table("loca", &locabuf);
	if (!l) return false;
	const uint32_t localen = w2uint32(l->origLength);

	char* headbuf = NULL;
	WoffTableDirectoryEntry* head = get_table("head", &headbuf);
	if (!head) {
		free(locabuf);
		return false;
	}
	indexToLocFormat = w2uint16(((WoffTableHead*)headbuf)->indexToLocFormat);
	if (indexToLocFormat != 0 && indexToLocFormat != 1) {
		free(locabuf);
		free(headbuf);
		return false;
	}
	free(headbuf);

	if (indexToLocFormat) {
		assert(localen % sizeof(wuint32_t) == 0);
		nloca = localen/sizeof(wuint32_t); // TODO: look into maxp
	} else {
		assert(localen % sizeof(wuint16_t) == 0);
		nloca = localen/sizeof(wuint16_t);
	}
	assert(nloca > 1); // To make it possible to compute the length of the last glyph element, there is an extra entry after the offset that points to the last valid index. This index points to the end of the glyph data.
	nloca--;

	union {
		wuint16_t* u16;
		wuint32_t* u32;
	} offsets;
	offsets.u16 = (wuint16_t*)locabuf;

	assert(!loca);
	loca = (range_t*)calloc(nloca, sizeof(range_t));
	for (unsigned i=0; i<nloca; ++i) {
		if (indexToLocFormat) {
			loca[i].from = w2uint32(offsets.u32[i]);
			loca[i].to = w2uint32(offsets.u32[i+1]);
		} else {
			loca[i].from = w2uint16(offsets.u16[i]) * 2;
			loca[i].to = w2uint16(offsets.u16[i+1]) * 2;
		}
	}

	LOG_INFO("parsed %u loca/glyph indices", nloca);
	for (unsigned i=0; i<nloca; ++i) {
		LOG_DUMP("  glyph %u @ %zu (#%zu)", i, loca[i].from, loca[i].to-loca[i].from);
	}
	free(locabuf);
	return true;
}


unsigned Woff::getMinAlignment(const std::vector<char_range_t>& v, unsigned usermin) {
	char* buf = NULL;
	size_t len;
	int headermin;
	{
		if (!get_table("head", &buf)) return -1;
		WoffTableHead* head = (WoffTableHead*)buf;
		if ((w2uint16(head->flags) & 0x01) != 1) {
			LOG("baseline is not at y=0");
			free(buf);
			return 0;
		}
		headermin = w2int16(head->yMin);
		LOG_INFO("global bounding box minimum y is %d (baseline 0)", headermin);
		free(buf);
		buf = NULL;
	}

	if (usermin) {
		if ((int)usermin < headermin) {
			LOG("given alignment %u is less than global bounding box minimum y %d", usermin, headermin);
			return 0;
		} else {
			return usermin;
		}
	}

	unsigned min = 0;
	for (std::vector<char_range_t>::const_iterator it=v.begin(); it!=v.end(); ++it) {
		for (char_t c=it->from; c<=it->to; ++c) {
			index_t i = cmaps.find(c);
			if (!i) continue;
			assert(i<nloca);
			if (loca[i].from == loca[i].to) continue;
			const WoffGlyph* g = get_glyph(loca[i].from, loca[i].to, buf, len);
			if (!g) continue;
			if (w2int16(g->yMin) > 0 && w2int16(g->yMin) < (int)min) {
				min = (unsigned)w2int16(g->yMin);
			}
			free(buf);
		}
	}
	return MAX(MAX(1, headermin), min);
}


bool Woff::alignCharIndex(index_t index, unsigned align) {
	assert(index > 0 && index < nloca);
	assert(align > 0); // as 0 is baseline and seems to break everything
	if (loca[index].from == loca[index].to) return true;

	char* gbuf;
	size_t glen;
	WoffGlyph* g = get_glyph(loca[index].from, loca[index].to, gbuf, glen);
	if (!g) return false;
	g->print("  ", "aligning glyph");

	int aligned = align_glyph(g, align);
	if (aligned < 0) {
		free(gbuf);
		return false;
	} else if (aligned > 0) {
		if (!set_table("glyf", gbuf, glen)) {
			free(gbuf);
			return false;
		}
	}

	free(gbuf);
	return true;
}


bool Woff::deleteCharIndex(index_t index) {
	assert(nloca && loca);
	if (!index) return true; // seems to mess up some things?
	if (index >= nloca) return false;
	if (index == nloca-1) return true; // keep last one as loca/glyf end marker

	if (loca[index].from == loca[index].to) {
		return true; // already nothing here
	}
	size_t dellen = delete_glyph(loca[index].from, loca[index].to);
	if (!dellen) {
		LOG_INFO("kept character #%u, is already stripped?", index);
		return true;
	}

	char* locabuf = NULL;
	WoffTableDirectoryEntry* l = get_table("loca", &locabuf);
	if (!l) return false;
	const uint32_t localen = w2uint32(l->origLength);
	union {
		wuint16_t* u16;
		wuint32_t* u32;
	} offsets;
	offsets.u16 = (wuint16_t*)locabuf;

	loca[index].to -= dellen;
	for (unsigned i=index+1; i<nloca+1; ++i) {
		if (i<nloca) {
			loca[i].from -= dellen;
			loca[i].to -= dellen;
		}

		if (indexToLocFormat) {
			offsets.u32[i] = uint2w32(w2uint32(offsets.u32[i]) - dellen); // ascending and/or != 0 seems to be expected
		} else {
			assert(dellen % 2 == 0);
			offsets.u16[i] = uint2w16(w2uint16(offsets.u16[i]) - (dellen/2));
		}
	}

	if (!set_table("loca", locabuf, localen)) {
		free(locabuf);
		return false;
	}
	free(locabuf);

	LOG_INFO("replaced character #%u with dummy value", index);
	return true;
}


char* Woff::toBuf(size_t& len) {
	len = (size_t)w2uint32(header->length);
	char* buf = (char*)memset(malloc(len), 0, len);

	memcpy(buf, header, sizeof(WoffHeader));
	assert(sizeof(WoffTableDirectoryEntry) % 4 == 0);
	memcpy(buf + PAD4(sizeof(WoffHeader)), tables, ntables * sizeof(WoffTableDirectoryEntry));
	for (unsigned i=0; i<ntables; ++i) {
		assert(w2uint32(tables[i].offset) % 4 == 0);
		memcpy(buf + w2uint32(tables[i].offset), table_data[i], w2uint32(tables[i].compLength));
	}

	return buf;
}
