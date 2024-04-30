// Copyright 2022 Wook-shin Han
#include <sys/stat.h>

#include <algorithm>
#include <cassert>
#include <climits>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <list>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "project1.h"

std::vector<std::string> SplitString(std::string& input_line,
                                     const std::vector<uint32_t>& lengths) {
  std::vector<std::string> result;
  result.reserve(lengths.size());

  std::size_t start = 0;
  for (uint32_t len : lengths) {
    if (start >= input_line.length()) {
      // 원본 문자열의 끝에 도달했을 경우 dirty 추가
      result.push_back("dirty");
    } else {
      // 길이만큼의 부분 문자열을 추출하여 결과에 추가
      result.push_back(input_line.substr(start, len));
      start += len;
    }
  }
  return result;
}

class Customer {
 public:
  std::vector<uint32_t> table_column_length = {
      21, 21, 21, 21, 51, 7, 4, 7, 7, 11, 21, 51, 5};  // 2 5 12 key
  std::vector<std::vector<std::string>> table_rows;
  std::vector<std::string> col_names;
  std::vector<std::string> input_lines;

  int ncolumn = table_column_length.size();
  int nrow = 0;
};

class Zonecost {
 public:
  std::vector<uint32_t> table_column_length = {7, 22, 6};  // 0, 1
  std::vector<std::vector<std::string>> table_rows;
  std::vector<std::string> col_names;
  std::vector<std::string> input_lines;

  int ncolumn = table_column_length.size();
  int nrow = 0;
};

class Lineitem {
 public:
  std::vector<uint32_t> table_column_length = {21, 11, 9, 21, 9, 8};
  std::vector<std::vector<std::string>> table_rows;
  std::vector<std::string> col_names;
  std::vector<std::string> input_lines;

  int ncolumn = table_column_length.size();
  int nrow = 0;
};

class Products {
 public:
  std::vector<uint32_t> table_column_length = {21, 11, 51, 21, 21, 8, 21, 21};
  std::vector<std::vector<std::string>> table_rows;
  std::vector<std::string> col_names;
  std::vector<std::string> input_lines;

  int ncolumn = table_column_length.size();
  int nrow = 0;
};

int main(int argc, char** argv) {
  std::string debug_path = "./debug.txt";
  std::ofstream debug_file(debug_path);

  std::vector<std::string> answers;

  debug_file << "Start Debug\n";

  // Query 1
  if (std::string(argv[1]).compare("q1") == 0) {
    Customer customer;
    Zonecost zonecost;

    std::string customer_path = std::string(argv[2]);
    std::string zonecost_path = std::string(argv[3]);

    std::ifstream customer_file(customer_path);
    if (!customer_file.is_open()) {
      std::cerr << "Unable to open customer.txt\n";
    }
    // else {
    // 	std::cerr << customer_path << " Do next step\n";
    // }

    std::string name_line;
    std::string waste;
    std::string input_line;

    std::getline(customer_file, name_line);
    std::getline(customer_file, waste);

    while (std::getline(customer_file, input_line)) {
      customer.input_lines.emplace_back(input_line);
    }
    customer.nrow = customer.input_lines.size();

    customer.col_names = SplitString(name_line, customer.table_column_length);

    for (int i = 0; i < customer.nrow; i++) {
      customer.table_rows.push_back(
          SplitString(customer.input_lines[i], customer.table_column_length));
    }

    std::ifstream zonecost_file(zonecost_path);
    if (!zonecost_file.is_open()) {
      std::cerr << "Unable to open zonecost.txt\n";
    }

    std::getline(zonecost_file, name_line);
    std::getline(zonecost_file, waste);

    while (std::getline(zonecost_file, input_line)) {
      zonecost.input_lines.emplace_back(input_line);
    }
    zonecost.nrow = zonecost.input_lines.size();

    zonecost.col_names = SplitString(name_line, zonecost.table_column_length);

    for (int i = 0; i < zonecost.nrow; i++) {
      zonecost.table_rows.push_back(
          SplitString(zonecost.input_lines[i], zonecost.table_column_length));
    }

    for (int i = 0; i < zonecost.nrow; i++) {
      size_t found = zonecost.table_rows[i][1].find(std::string("Toronto"));
      if (found != std::string::npos) {
        std::string toronto_id = zonecost.table_rows[i][0];

        for (int j = 0; j < customer.nrow; j++) {
          if (std::stoi(customer.table_rows[j][5]) == std::stoi(toronto_id) &&
              std::stoi(customer.table_rows[j][12])) {
            answers.push_back(customer.table_rows[j][2]);
          }
        }
      }
    }

    for (auto& answer : answers) {
      std::cout << answer << std::endl;
    }

    return 0;
  }

  if (std::string(argv[1]).compare("q2") == 0) {
    Lineitem lineitem;
    Products products;

    std::string lineitem_path = std::string(argv[2]);
    std::string products_path = std::string(argv[3]);

    std::ifstream lineitem_file(lineitem_path);
    if (!lineitem_file.is_open()) {
      std::cerr << "Unable to open lineitem.txt\n";
    }

    std::string name_line;
    std::string waste;
    std::string input_line;

    std::getline(lineitem_file, name_line);
    std::getline(lineitem_file, waste);

    while (std::getline(lineitem_file, input_line)) {
      lineitem.input_lines.emplace_back(input_line);
    }
    lineitem.nrow = lineitem.input_lines.size();

    lineitem.col_names = SplitString(name_line, lineitem.table_column_length);

    for (int i = 0; i < lineitem.nrow; i++) {
      lineitem.table_rows.push_back(
          SplitString(lineitem.input_lines[i], lineitem.table_column_length));
    }

    std::ifstream products_file(products_path);
    if (!products_file.is_open()) {
      std::cerr << "Unable to open products.txt\n";
    }

    std::getline(products_file, name_line);
    std::getline(products_file, waste);

    while (std::getline(products_file, input_line)) {
      products.input_lines.emplace_back(input_line);
    }
    products.nrow = products.input_lines.size();

    products.col_names = SplitString(name_line, products.table_column_length);

    for (int i = 0; i < products.nrow; i++) {
      products.table_rows.push_back(
          SplitString(products.input_lines[i], products.table_column_length));
    }

    std::unordered_map<int, int> barcode_count;
    std::vector<int> key_barcode;

    for (int i = 0; i < lineitem.nrow; i++) {
      barcode_count[std::stoi(lineitem.table_rows[i][3])] += 1;
    }

    for (auto& pair : barcode_count) {
      if (pair.second >= 2) {
        key_barcode.push_back(pair.first);
      }
    }
    std::sort(key_barcode.begin(), key_barcode.end());

    for (int i = 0; i < products.nrow; i++) {
      auto it = std::find(key_barcode.begin(), key_barcode.end(),
                          std::stoi(products.table_rows[i][0]));

      if (it != key_barcode.end()) {
        std::cout << products.table_rows[i][0] << products.table_rows[i][2]
                  << std::endl;
      }
    }
    return 0;
  }
}