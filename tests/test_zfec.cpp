// Copyright (C) 2022 Storj Labs, Inc.
// See LICENSE for copying information.
//
// Partially based a test from on the Botan project:
//
// (C) 2021 Jack Lloyd
//
// Botan is released under the Simplified BSD License (see LICENSE).

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include "gtest/gtest.h"

#include "infectious/fec.hpp"

namespace infectious {
namespace test {

#define DATADIR_ENVVAR "INFECTIOUS_TEST_DATA_DIR"

auto parse_line(std::string line) -> std::pair<std::string, std::string> {
	const std::string delimiter(" = ");
	size_t pos;
	if ((pos = line.find(delimiter)) == std::string::npos) {
		throw std::runtime_error("invalid syntax in test data file");
	}
	auto varname = line.substr(0, pos);
	auto val = line.substr(pos + delimiter.length(), std::string::npos);
	return std::make_pair(varname, val);
}

auto read_vec_data(std::istream& i) -> std::map<std::string, std::string> {
	char linec[1<<16];
	std::map<std::string, std::string> variables;

	while (i.good()) {
		i.getline(&linec[0], sizeof(linec));

		// strip beginning whitespace
		const char* p = &linec[0];
		while (*p != '\0' && std::isspace(*p)) {
			++p;
		}
		// ignore comments
		if (*p == '#') {
			continue;
		}
		// empty line is a separator between test units
		if (*p == '\0') {
			if (variables.empty()) {
				// haven't read anything for this unit yet
				continue;
			}
			return variables;
		}
		auto name_and_value = parse_line(std::string(p));
		variables.insert(std::move(name_and_value));
	}
	return variables;
}

auto unhexlify(const std::string& hexstr) -> std::vector<uint8_t> {
	if (hexstr.length() % 2 != 0) {
		throw std::invalid_argument("invalid number of characters in hex string");
	}
	std::vector<uint8_t> as_bytes;
	as_bytes.reserve(hexstr.length() / 2);
	for (size_t i = 0; i < hexstr.length(); i += 2) {
		const std::string digits = hexstr.substr(i, 2);
		uint8_t byte = static_cast<uint8_t>(std::stoi(digits, nullptr, 16));
		as_bytes.push_back(byte);
	}
	return as_bytes;
}

void perform_test(int k, int n, const std::vector<uint8_t>& data, const std::vector<uint8_t>& code) {
	ASSERT_EQ(static_cast<int>(data.size()) % k, 0) << "input string is not a multiple of " << k << " bytes";
	const auto share_size = static_cast<int>(data.size()) / k;
	ASSERT_EQ(static_cast<int>(code.size()), share_size * (n - k)) << "expected result (" << code.size() << " bytes) does not correspond to k=" << k << "/n=" << n;

	std::map<size_t, std::basic_string<uint8_t>> expected_shares;
	for (int i = 0; i < n; ++i) {
		std::basic_string<uint8_t> share(share_size, '\0');
		if (i < k) {
			std::copy(data.begin() + share_size * i, data.begin() + share_size * (i+1), &share[0]);
		} else {
			std::copy(code.begin() + share_size * (i-k), code.begin() + share_size * (i-k+1), &share[0]);
		}
		expected_shares.emplace(i, share);
	}

	infectious::FEC fec(k, n);

	// Encode the input data, check that it matches the expected encoded version

	std::map<size_t, std::basic_string<uint8_t>> shares_encoded;
	fec.Encode(data, [&](int num, const std::basic_string_view<uint8_t>& share) {
		std::basic_string<uint8_t> share_str(share.begin(), share.end());
		auto was_inserted = shares_encoded.insert(std::make_pair(num, share_str)).second;
		ASSERT_TRUE(was_inserted) << "Encode yielded share with num " << num << " more than once";
	});
	ASSERT_EQ(expected_shares, shares_encoded);

	// Decode the encoded data, check that it matches the original data

	std::vector<uint8_t> decoded(share_size * k);
	std::set<int> share_nums_seen;
	fec.DecodeTo(shares_encoded, [&](int num, const std::basic_string_view<uint8_t>& share) {
		ASSERT_TRUE(share_nums_seen.insert(num).second) << "Decode yielded share with num " << num << " more than once";
		std::copy(share.begin(), share.end(), decoded.begin() + (share_size * num));
	});
	ASSERT_EQ(data, decoded);
	ASSERT_EQ(k, static_cast<int>(share_nums_seen.size())) << "wrong number of shares " << share_nums_seen.size() << " yielded from decode";

	// remove the first n-k shares and decode again
	for (int i = 0; i < (n-k); ++i) {
		shares_encoded.erase(i);
	}
	share_nums_seen.clear();
	decoded.clear();
	decoded.resize(share_size * k);
	fec.DecodeTo(shares_encoded, [&](int num, const std::basic_string_view<uint8_t>& share) {
		ASSERT_TRUE(share_nums_seen.insert(num).second) << "Second decode yielded share with num " << num << " more than once";
		std::copy(share.begin(), share.end(), decoded.begin() + (share_size * num));
	});
	ASSERT_EQ(data, decoded);
	ASSERT_EQ(k, static_cast<int>(share_nums_seen.size())) << "wrong number of shares " << share_nums_seen.size() << " yielded from decode";
}

// for some reason, clang-tidy doesn't much care for gtest's macros.
// NOLINTBEGIN(cert-err58-cpp,cppcoreguidelines-avoid-non-const-global-variables,cppcoreguidelines-owning-memory,modernize-use-trailing-return-type)

TEST(FEC, OutputMatchesZfec) {
	const char* data_dir = std::getenv(DATADIR_ENVVAR);
	if (data_dir == nullptr) {
		data_dir = "./tests/data";
	}
	auto data_file_path = std::filesystem::path(data_dir) / "zfec.vec";
	std::ifstream dataf(data_file_path);
	ASSERT_TRUE(!!dataf) << "Can not find test data file (set $" DATADIR_ENVVAR ")";

	int num_tests = 0;
	while (dataf.good()) {
		auto test_data = read_vec_data(dataf);
		if (test_data.empty()) {
			break;
		}

		auto k_string = test_data["K"];
		auto n_string = test_data["N"];
		auto data_string = test_data["Data"];
		auto code_string = test_data["Code"];

		if (k_string.empty() || n_string.empty() || data_string.empty() || code_string.empty()) {
			throw std::runtime_error("incomplete test unit definition in data file");
		}
		++num_tests;

		auto k = std::stoi(k_string);
		auto n = std::stoi(n_string);
		if (k < 1 || k > 1024 || n <= k || n > 1024) {
			throw std::runtime_error("K or N out of range in data file");
		}

		auto data = unhexlify(data_string);
		auto code = unhexlify(code_string);

		perform_test(k, n, data, code);
	}

	ASSERT_GE(num_tests, 80);
}

} // namespace test
} // namespace infectious
