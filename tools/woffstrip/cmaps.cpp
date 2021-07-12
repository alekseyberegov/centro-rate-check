#include "cmaps.hpp"


bool Cmaps::parse0(const char* b, size_t l, std::map<char_t, index_t>& map) {
	if (l < sizeof(WoffCmap0)) return false;
	const WoffCmap0* cmap = (const WoffCmap0*)b;
	cmap->print("  ", "charmap 0");

	assert(map.empty());
	for (char_t i=0; i<sizeof(cmap->glyphIndexArray); ++i) {
		if (cmap->glyphIndexArray[i] != 0) {
			map.insert(std::pair<char_t, index_t>(i, cmap->glyphIndexArray[i]));
		}
	}
	return true;
}


bool Cmaps::parse4(const char* b, size_t l, std::map<char_t, index_t>& map) {
	if (l < sizeof(WoffCmap4)) return false;
	WoffCmap4* cmap = (WoffCmap4*)b;
	cmap->print("  ", "charmap 4");

	uint16_t segCount = w2uint16(cmap->segCountX2)/2;
	if (l < sizeof(WoffCmap4) + (segCount * sizeof(wuint16_t) * 4) + sizeof(wuint16_t)) return false;
	wuint16_t* endCode = (wuint16_t*)(cmap + 1);
	wuint16_t* startCode = (wuint16_t*)((char*)endCode + segCount*sizeof(wuint16_t) + sizeof(wuint16_t));
	wuint16_t* idDelta = (wuint16_t*)((char*)startCode + segCount*sizeof(wuint16_t));
	wuint16_t* idRangeOffset = (wuint16_t*)((char*)idDelta + segCount*sizeof(wuint16_t));
	//wuint16_t* glyphIndexArray = (wuint16_t*)((char*)idRangeOffset + segCount*sizeof(wuint16_t));

	for (uint16_t s=0; s<segCount; ++s) {
		LOG_DUMP("    %04x-%04x (@ %u+%u)", w2uint16(startCode[s]), w2uint16(endCode[s]), w2uint16(idDelta[s]), w2uint16(idRangeOffset[s]));
		if (w2uint16(startCode[s]) > w2uint16(endCode[s])) return false;
		for (char_t c=(char_t)w2uint16(startCode[s]); c<=(char_t)w2uint16(endCode[s]); ++c) {
			index_t index;
			if (w2uint16(idRangeOffset[s]) == 0) {
				index = c;
			} else {
				uint16_t off = (c - w2uint16(startCode[s])); // * sizeof(uint16_t) but is already pointer arithmetic
				off += w2uint16(idRangeOffset[s]) / 2;
				if (s + off > l - ((const char*)idRangeOffset - b)) return false;
				index = w2uint16(idRangeOffset[s + off]);
			}
			index = (index + w2uint16(idDelta[s])) % 65536u;
			if (!index) continue;

			LOG_DUMP("      %04x: %u", c, index);
			if (map.find(c) != map.end()) return false;
			map.insert(std::pair<char_t, index_t>(c, index));
		}
	}

	return true;
}


bool Cmaps::parse12(const char* b, size_t l, std::map<char_t, index_t>& map) {
	if (l < sizeof(WoffCmap12)) return false;
	WoffCmap12* cmap = (WoffCmap12*)b;
	cmap->print("  ", "charmap 12");

	if (l < sizeof(WoffCmap12) + w2uint32(cmap->nGroups) * sizeof(WoffCmap12Group)) return false;
	for (uint32_t g=0; g<w2uint32(cmap->nGroups); ++g) {
		WoffCmap12Group* group = ((WoffCmap12Group*)(cmap+1)) + g;
		group->print("    ");

		if (w2uint32(group->startCharCode) > w2uint32(group->endCharCode)) return false;
		for (char_t c=w2uint32(group->startCharCode); c<=w2uint32(group->endCharCode); ++c) {
			index_t i = w2uint32(group->startGlyphCode) + (c - w2uint32(group->startCharCode));
			if (map.find(c) != map.end()) return false;
			map.insert(std::pair<char_t, index_t>(c, i));
		}
	}
	return true;
}


bool Cmaps::parse(const char* buf, size_t len) {
	charmap.clear();

	if (len < sizeof(WoffCmapIndex)) return false;
	const WoffCmapIndex* index = (WoffCmapIndex*)buf;
	index->print("  ", "character map index");

	if (len < sizeof(WoffCmapIndex) + w2uint16(index->numberSubtables) * sizeof(WoffCmapSubtable)) return false;
	const WoffCmapSubtable* subtable = (WoffCmapSubtable*)(index+1);
	for (unsigned si=0; si<w2uint16(index->numberSubtables); ++si) {
		subtable[si].print("  ", "character map subtable");

		if (w2uint32(subtable[si].offset) + sizeof(uint16_t) >= len) return false;
		unsigned format = w2uint16(*((uint16_t*)(buf + w2uint32(subtable[si].offset)))); // peek into format

		std::map<char_t, index_t> submap;
		size_t cmaplen = len - w2uint32(subtable[si].offset); // not ordered: ((si == w2uint16(index->numberSubtables)-1)? (uint32_t)len: w2uint32(subtable[si+1].offset)) - w2uint32(subtable[si].offset);
		if (format == 0) {
			if (!parse0(buf + w2uint32(subtable[si].offset), cmaplen, submap)) {
				return false;
			}
		} else if (format == 4) {
			if (!parse4(buf + w2uint32(subtable[si].offset), cmaplen, submap)) {
				return false;
			}
		} else if (format == 12) {
			if (!parse12(buf + w2uint32(subtable[si].offset), cmaplen, submap)) {
				return false;
			}
		} else {
			LOG("unsupported cmap format %u", format);
			return false;
		}

		for (std::map<char_t, index_t>::iterator it=submap.begin(); it!=submap.end(); ++it) {
			assert(it->second != 0);
			std::map<char_t, index_t>::iterator c = charmap.find(it->first);
			if (c == charmap.end()) {
				charmap.insert(std::pair<char_t, index_t>(it->first, it->second));
			} else if (c->second != it->second) {
				LOG("char index conflict");
				return false;
			}
		}
	}

	LOG_DUMP("charmap");
	for (std::map<char_t, index_t>::const_iterator it=charmap.begin(); it!=charmap.end(); ++it) {
		LOG_DUMP("  %04x @ %u", it->first, it->second);
	}
	return true;
}


index_t Cmaps::find(char_t c) const {
	std::map<char_t, index_t>::const_iterator cit = charmap.find(c);
	if (cit == charmap.end()) {
		return 0;
	} else {
		assert(cit->second != 0);
		return cit->second;
	}
}


void Cmaps::set_op(std::vector<char_range_t>& v, std::vector<char_range_t>& rem, bool get_given) const {
	assert(rem.empty());
	std::vector<char_range_t> rv;
	for (std::map<char_t, index_t>::const_iterator it=charmap.begin(); it!=charmap.end(); ++it) {
		const char_t& c = it->first;
		bool given = false;
		for (std::vector<char_range_t>::iterator vit=v.begin(); vit!=v.end(); ++vit) {
			assert(vit->from <= vit->to);
			if (c >= vit->from && c <= vit->to) {
				given = true;
				if (vit->from == vit->to) v.erase(vit);
				break;
			}
		}
		if (given == get_given) {
			rv.push_back((char_range_t){c, c});
		} else {
			rem.push_back((char_range_t){c, c});
		}
	}
	v.swap(rv);
}


void Cmaps::set_intersect(std::vector<char_range_t>& v, std::vector<char_range_t>& rem) const {
	set_op(v, rem, true);
}


void Cmaps::set_substract(std::vector<char_range_t>& v, std::vector<char_range_t>& rem) const {
	set_op(v, rem, false);
}


void Cmaps::intersect(const std::vector<char_range_t>& v, std::vector<char_range_t>& r) {
	std::vector<char_range_t> rv;
	for (std::vector<char_range_t>::iterator rit=r.begin(); rit!=r.end(); ++rit) {
		assert(rit->from <= rit->to);
		for (char_t c=rit->from; c<=rit->to; ++c) {
			for (std::vector<char_range_t>::const_iterator vit=v.begin(); vit!=v.end(); ++vit) {
				assert(vit->from <= vit->to);
				if (c >= vit->from && c <= vit->to) {
					bool found = false;
					for (std::vector<char_range_t>::const_iterator it=rv.begin(); it!=rv.end(); ++it) {
						if (c >= it->from && c <= it->to) found = true;
					}
					if (!found) {
						rv.push_back((char_range_t){c, c});
					}
					break;
				}
			}
		}
	}
	r.swap(rv);
}
