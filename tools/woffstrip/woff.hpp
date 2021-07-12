#pragma once
#include "main.hpp"
#include "cmaps.hpp"
#include <vector>


class Woff {
	private:
		const char* const orig_buf;
		const size_t orig_len;

		WoffHeader* header;

		unsigned ntables;
		WoffTableDirectoryEntry* tables;
		char** table_data;

		unsigned indexToLocFormat;
		unsigned nloca;
		range_t* loca;

		Cmaps cmaps;

		static wuint32_t checksum(const wuint32_t*, size_t, wuint32_t=0);
		wuint32_t sfnt_checksum() const;
		bool update_sfnt_checksum();
		static wuint32_t table_checksum(const char*, const char*, size_t);

		int get_table_index(const char*) const;
		WoffTableDirectoryEntry* get_table(const char*, char** =NULL);
		bool set_table(const char*, const char*, size_t, bool=true);

		WoffGlyph* get_glyph(size_t, size_t, char*&, size_t&);
		int align_glyph(WoffGlyph*, int);
		size_t delete_glyph(size_t, size_t);

		bool update_offsets();

	public:
		Woff(char* b, size_t l);
		~Woff();

		bool parseHeader();
		bool parseTables();
		bool parseCharMaps();
		bool parseLoca();

		const Cmaps& getCharMap() const { return cmaps; }
		bool deleteCharIndex(index_t index);
		unsigned getMinAlignment(const std::vector<char_range_t>&, unsigned);
		bool alignCharIndex(index_t, unsigned);

		bool finalize();
		char* toBuf(size_t&);
};
