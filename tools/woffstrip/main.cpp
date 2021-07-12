#include "main.hpp"
#include "woff.hpp"
#include "io.hpp"
#include <vector>


config_s config = {};

static void usage(const char* name) {
	LOG(
		"usage: %s [-v] [-d] [-e|-i range1[,range2[,...]]] [-a range1[,range2[,...]] -b num] infile.woff [outfile.woff]\n"
		"       -v: be verbose (to stderr)\n"
		"       -d: dump woff information (to stdout)\n"
		"       -e: exclude/strip following ranges from input file\n"
		"       -i: include/keep only following ranges from input file\n"
		"       -a: align character bounding boxes to a determined minimum baseline (can be combined with -i or -e)\n"
		"           for an empty range argument, all (leftover) characters are assumed\n"
		"       -b: when aligning, use this y-coordinate above the baseline instead (> 0)\n"
		"       ranges are a list of ASCII/UTF character codes in hex notation, e.g: 20-7e,F001-F008,E12a"
		, name
	);
}

static bool parse_range_list(std::vector<char_range_t>& v, char* a) {
	char* p = a;
	while (*p) {
		char* e = strchr(p, ',');
		if (e) *e = '\0';
		char* d = strchr(p, '-');
		if (d) *d = '\0';

		char_t from, to;
		if (sscanf(p, "%x", &from) != 1) return false;
		if (d) {
			if (sscanf(d+1, "%x", &to) != 1) return false;
		} else {
			to = from;
		}
		v.push_back((char_range_t){from, to});

		if (!e) break;
		p = e+1;
	}
	return true;
}

int main(int argc, char** argv) {
	bool charcodes_exclude = false;
	bool charcodes_set = false;
	std::vector<char_range_t> charcodes;
	int align_to = 0;
	bool align_charcodes_set = false;
	std::vector<char_range_t> align_charcodes;

	int opt;
	while ((opt = getopt(argc, argv, "vde:i:a:b:")) != -1) {
		switch (opt) {
			case 'v':
				config.verbose = true;
				break;
			case 'd':
				config.dump = true;
				break;
			case 'e':
			case 'i':
				charcodes_exclude = (opt == 'e');
				if (charcodes_set || !parse_range_list(charcodes, optarg)) {
					usage(argv[0]);
					return 1;
				}
				charcodes_set = true;
				break;
			case 'a':
				if (align_charcodes_set || !parse_range_list(align_charcodes, optarg)) {
					usage(argv[0]);
					return 1;
				}
				align_charcodes_set = true;
				break;
			case 'b':
				if (align_to || (align_to = atoi(optarg)) <= 0) {
					usage(argv[0]);
					return 1;
				}
				break;
			default:
				usage(argv[0]);
				return 1;
		}
	}
	if (optind+2 < argc) {
		usage(argv[0]);
		return 1;
	}
	const char* infile = (optind <= argc)? argv[optind]: NULL;
	const char* outfile = (optind < argc)? argv[optind+1]: NULL;

	char* buf;
	size_t len;
	if (!infile) {
		usage(argv[0]);
		return 1;
	}
	if (!file_read(infile, buf, len)) {
		return 1;
	}

	Woff woff(buf, len);
	if (!woff.parseHeader()) {
		LOG("cannot parse header");
		return 1;
	}
	if (!woff.parseTables()) {
		LOG("cannot parse tables");
		return 1;
	}
	if (!woff.parseCharMaps()) {
		LOG("cannot parse character maps");
		return 1;
	}
	LOG("found %zu chars", woff.getCharMap().size());
	if (!woff.parseLoca()) {
		LOG("cannot parse character indices");
		return 1;
	}

	std::vector<char_range_t> remainders;
	if (charcodes_set) {
		if (charcodes_exclude) {
			woff.getCharMap().set_intersect(charcodes, remainders);
		} else {
			woff.getCharMap().set_substract(charcodes, remainders);
		}
	} else {
		std::vector<char_range_t> tmp;
		woff.getCharMap().set_intersect(tmp, remainders);
	}
	if (align_charcodes_set) {
		if (align_charcodes.empty()) {
			align_charcodes.swap(remainders);
		} else {
			Cmaps::intersect(remainders, align_charcodes);
		}
	}
	remainders.clear();

	if (charcodes.empty()) {
		LOG("not removing any char glyphs");
	} else {
		LOG("removing %zu char glyphs", charcodes.size());
		for (std::vector<char_range_t>::const_iterator it=charcodes.begin(); it!=charcodes.end(); ++it) {
			for (char_t c=it->from; c<=it->to; ++c) {
				index_t index = woff.getCharMap().find(c);
				assert(index); // buggy set operation otherwise
				LOG_INFO("char %04x @ %u", c, index);
				if (!woff.deleteCharIndex(index)) {
					LOG("cannot delete char %04x at index %u", c, index);
					return 1;
				}
			}
		}
	}

	if (align_charcodes.empty()) {
		LOG("not aligning any char glyphs");
	} else {
		assert(align_to >= 0);
		align_to = woff.getMinAlignment(align_charcodes, (unsigned)align_to);
		if (!align_to) {
			LOG("cannot infer or validate baseline alignment");
			return 1;
		}
		LOG("aligning %zu char glyphs to %d", align_charcodes.size(), align_to);
		for (std::vector<char_range_t>::const_iterator it=align_charcodes.begin(); it!=align_charcodes.end(); ++it) {
			for (char_t c=it->from; c<=it->to; ++c) {
				index_t index = woff.getCharMap().find(c);
				LOG_INFO("char %04x @ %u", c, index);
				if (!woff.alignCharIndex(index, (unsigned)align_to)) {
					LOG("cannot align char %04x", c);
					return 1;
				}
			}
		}
	}

	if (charcodes.empty() && align_charcodes.empty()) {
		LOG("nothing to do");
		return 0;
	}

	if (!woff.finalize()) return 1;
	if (!outfile) return 0;

	size_t outlen;
	char* outbuf = woff.toBuf(outlen);
	if (!outbuf) {
		LOG("cannot pack result");
		return 1;
	}
	if (!file_write(outfile, outbuf, outlen)) {
		free(outbuf);
		return 1;
	}
	free(outbuf);
	LOG("wrote to '%s' - done.", outfile);

	return 0;
}
