#pragma once
#include "main.hpp"
#include "types.hpp"
#include <map>
#include <vector>


class Cmaps {
	private:
		static bool parse0(const char*, size_t, std::map<char_t, index_t>&);
		static bool parse4(const char*, size_t, std::map<char_t, index_t>&);
		static bool parse12(const char*, size_t, std::map<char_t, index_t>&);
		void set_op(std::vector<char_range_t>&, std::vector<char_range_t>&, bool) const;
		std::map<char_t, index_t> charmap;

	public:
		bool parse(const char*, size_t);
		index_t find(char_t) const;
		size_t size() const { return charmap.size(); }

		void set_intersect(std::vector<char_range_t>&, std::vector<char_range_t>&) const; // returns those found here and given, and the remainders
		void set_substract(std::vector<char_range_t>&, std::vector<char_range_t>&) const; // returns those found here but not given, and the remainders
		static void intersect(const std::vector<char_range_t>&, std::vector<char_range_t>&); // deletes those not found in first argument
};
