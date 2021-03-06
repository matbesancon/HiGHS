/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                       */
/*    This file is part of the HiGHS linear optimization suite           */
/*                                                                       */
/*    Written and engineered 2008-2019 at the University of Edinburgh    */
/*                                                                       */
/*    Available as open-source under the MIT License                     */
/*                                                                       */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/**@file io/HMpsFF.h
 * @brief
 * @author Julian Hall, Ivet Galabova, Qi Huangfu and Michael Feldmeier
 */
#ifndef IO_HMPSFF_H_
#define IO_HMPSFF_H_

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "io/HighsIO.h"
#include "lp_data/HConst.h"
#include "util/stringutil.h"

using Triplet = std::tuple<int, int, double>;

enum class FreeFormatParserReturnCode {
  SUCCESS,
  PARSERERROR,
  FILENOTFOUND,
  FIXED_FORMAT
};

class HMpsFF {
 public:
  HMpsFF() {}
  FreeFormatParserReturnCode loadProblem(const std::string filename,
                                         HighsLp& lp);

 private:
  int numRow;
  int numCol;
  int nnz;

  int objSense;
  double objOffset = 0;

  std::vector<int> Astart;
  std::vector<int> Aindex;
  std::vector<double> Avalue;
  std::vector<double> colCost;
  std::vector<double> colLower;
  std::vector<double> colUpper;
  std::vector<double> rowLower;
  std::vector<double> rowUpper;

  std::vector<std::string> rowNames;
  std::vector<std::string> colNames;

  std::vector<int> col_integrality;

  /// load LP from MPS file as transposed triplet matrix
  int parseFile(std::string filename);
  int fillMatrix();

  const bool any_first_non_blank_as_star_implies_comment = false;
  const bool handle_bv_in_bounds = false;

  enum class parsekey {
    ROWS,
    COLS,
    RHS,
    BOUNDS,
    RANGES,
    NONE,
    END,
    FAIL,
    COMMENT,
    FIXED_FORMAT
  };

  enum class boundtype { LE, EQ, GE, FR };
  std::vector<boundtype> row_type;
  std::vector<int> integer_column;

  std::vector<Triplet> entries;
  std::vector<std::pair<int, double>> coeffobj;

  std::unordered_map<std::string, int> rowname2idx;
  std::unordered_map<std::string, int> colname2idx;

  FreeFormatParserReturnCode parse(const std::string& filename);
  /// checks first word of strline and wraps it by it_begin and it_end
  HMpsFF::parsekey checkFirstWord(std::string& strline, int& start, int& end,
                                  std::string& word) const;

  HMpsFF::parsekey parseDefault(std::ifstream& file) const;
  HMpsFF::parsekey parseRows(std::ifstream& file);
  HMpsFF::parsekey parseCols(std::ifstream& file);
  HMpsFF::parsekey parseRhs(std::ifstream& file);
  HMpsFF::parsekey parseRanges(std::ifstream& file);
  HMpsFF::parsekey parseBounds(std::ifstream& file);
};

FreeFormatParserReturnCode HMpsFF::loadProblem(const std::string filename,
                                               HighsLp& lp) {
  FreeFormatParserReturnCode result = parse(filename);
  if (result != FreeFormatParserReturnCode::SUCCESS) return result;

  colCost.assign(numCol, 0);
  for (auto i : coeffobj) colCost[i.first] = i.second;
  int status = fillMatrix();
  if (status) return FreeFormatParserReturnCode::PARSERERROR;

  lp.numRow_ = std::move(numRow);
  lp.numCol_ = std::move(numCol);
  lp.nnz_ = Avalue.size();

  lp.sense_ = 1;
  lp.offset_ = objOffset;

  lp.Astart_ = std::move(Astart);
  lp.Aindex_ = std::move(Aindex);
  lp.Avalue_ = std::move(Avalue);
  lp.colCost_ = std::move(colCost);
  lp.colLower_ = std::move(colLower);
  lp.colUpper_ = std::move(colUpper);
  lp.rowLower_ = std::move(rowLower);
  lp.rowUpper_ = std::move(rowUpper);

  lp.row_names_ = std::move(rowNames);
  lp.col_names_ = std::move(colNames);

  lp.integrality_ = std::move(col_integrality);

  return FreeFormatParserReturnCode::SUCCESS;
}

int HMpsFF::fillMatrix() {
  int num_entries = entries.size();
  if (num_entries != nnz) return 1;

  Avalue.resize(nnz);
  Aindex.resize(nnz);
  Astart.assign(numCol + 1, 0);
  // Nothing to do if there are no entries in the matrix
  if (!num_entries) return 0;

  int newColIndex = std::get<0>(entries.at(0));

  for (int k = 0; k < nnz; k++) {
    Avalue.at(k) = std::get<2>(entries.at(k));
    Aindex.at(k) = std::get<1>(entries.at(k));

    if (std::get<0>(entries.at(k)) != newColIndex) {
      int nEmptyCols = std::get<0>(entries.at(k)) - newColIndex;
      newColIndex = std::get<0>(entries.at(k));
      if (newColIndex >= numCol) return 1;

      Astart.at(newColIndex) = k;
      for (int i = 1; i < nEmptyCols; i++) {
        Astart.at(newColIndex - i) = k;
      }
    }
  }

  for (int col = newColIndex + 1; col <= numCol; col++) Astart[col] = nnz;

  for (int i = 0; i < numCol; i++) {
    if (Astart[i] > Astart[i + 1]) {
      std::cout << "Error filling in matrix data\n";
      return 1;
    }
  }

  return 0;
}

FreeFormatParserReturnCode HMpsFF::parse(const std::string& filename) {
  std::ifstream f;
  f.open(filename.c_str(), std::ios::in);
  if (f.is_open()) {
    nnz = 0;
    HMpsFF::parsekey keyword = HMpsFF::parsekey::NONE;

    // parsing loop
    while (keyword != HMpsFF::parsekey::FAIL &&
           keyword != HMpsFF::parsekey::END) {
      switch (keyword) {
        case HMpsFF::parsekey::ROWS:
          keyword = parseRows(f);
          break;
        case HMpsFF::parsekey::COLS:
          keyword = parseCols(f);
          break;
        case HMpsFF::parsekey::RHS:
          keyword = parseRhs(f);
          break;
        case HMpsFF::parsekey::BOUNDS:
          keyword = parseBounds(f);
          break;
        case HMpsFF::parsekey::RANGES:
          keyword = parseRanges(f);
          break;
        case HMpsFF::parsekey::FAIL:
          f.close();
          return FreeFormatParserReturnCode::PARSERERROR;
        case HMpsFF::parsekey::FIXED_FORMAT:
          f.close();
          return FreeFormatParserReturnCode::FIXED_FORMAT;
        default:
          keyword = parseDefault(f);
          break;
      }
    }

    if (keyword == HMpsFF::parsekey::FAIL) {
      f.close();
      return FreeFormatParserReturnCode::PARSERERROR;
    }
  } else {
    f.close();
    return FreeFormatParserReturnCode::FILENOTFOUND;
  }

  f.close();

  assert(row_type.size() == unsigned(numRow));

  numCol = colname2idx.size();
  // No need to update nRows because the assert ensures
  // it is correct.

  return FreeFormatParserReturnCode::SUCCESS;
}

// Assuming string is not empty.
HMpsFF::parsekey HMpsFF::checkFirstWord(std::string& strline, int& start,
                                        int& end, std::string& word) const {
  start = strline.find_first_not_of(" ");
  if ((start == (int)strline.size() - 1) || is_empty(strline[start + 1])) {
    end = start + 1;
    word = strline[start];
    return HMpsFF::parsekey::NONE;
  }

  end = first_word_end(strline, start + 1);

  word = strline.substr(start, end - start);

  if (word.front() == 'R') {
    if (word == "ROWS")
      return HMpsFF::parsekey::ROWS;
    else if (word == "RHS")
      return HMpsFF::parsekey::RHS;
    else if (word == "RANGES")
      return HMpsFF::parsekey::RANGES;
    else
      return HMpsFF::parsekey::NONE;
  } else if (word == "COLUMNS")
    return HMpsFF::parsekey::COLS;
  else if (word == "BOUNDS")
    return HMpsFF::parsekey::BOUNDS;
  else if (word == "ENDATA")
    return HMpsFF::parsekey::END;
  else
    return HMpsFF::parsekey::NONE;
}

HMpsFF::parsekey HMpsFF::parseDefault(std::ifstream& file) const {
  std::string strline, word;
  getline(file, strline);
  int s, e;
  return checkFirstWord(strline, s, e, word);
}

HMpsFF::parsekey HMpsFF::parseRows(std::ifstream& file) {
  std::string strline, word;
  size_t nrows = 0;
  bool hasobj = false;
  std::string objectiveName = "";

  while (getline(file, strline)) {
    if (is_empty(strline) || strline[0] == '*') continue;

    bool isobj = false;
    bool isFreeRow = false;

    int start = 0;
    int end = 0;

    HMpsFF::parsekey key = checkFirstWord(strline, start, end, word);

    // start of new section?
    if (key != HMpsFF::parsekey::NONE) {
      numRow = int(nrows);
      if (!hasobj) {
        std::cout << "WARNING: no objective row found" << std::endl;
        rowname2idx.emplace("artificial_empty_objective", -1);
      };
      return key;
    }

    if (strline[start] == 'G') {
      rowLower.push_back(0.0);
      rowUpper.push_back(HIGHS_CONST_INF);
      row_type.push_back(boundtype::GE);
    } else if (strline[start] == 'E') {
      rowLower.push_back(0.0);
      rowUpper.push_back(0.0);
      row_type.push_back(boundtype::EQ);
    } else if (strline[start] == 'L') {
      rowLower.push_back(-HIGHS_CONST_INF);
      rowUpper.push_back(0.0);
      row_type.push_back(boundtype::LE);
    } else if (strline[start] == 'N') {
      if (objectiveName == "") {
        isobj = true;
        hasobj = true;
      } else {
        isFreeRow = true;
      }
    } else {
      std::cerr << "reading error in ROWS section " << std::endl;
      return HMpsFF::parsekey::FAIL;
    }

    std::string rowname = first_word(strline, start + 1);
    int rowname_end = first_word_end(strline, start + 1);

    // Detect if file is in fixed format.
    if (!is_end(strline, rowname_end)) {
      std::string name = strline.substr(start + 1);
      name = trim(name);
      if (name.size() > 8)
        return HMpsFF::parsekey::FAIL;
      else
        return HMpsFF::parsekey::FIXED_FORMAT;
    }

    // Do not add to matrix if row is free.
    if (isFreeRow) {
      rowname2idx.emplace(rowname, -2);
      continue;
    }

    // so in rowname2idx -1 is the objective, -2 is all the free rows
    auto ret = rowname2idx.emplace(rowname, isobj ? (-1) : (nrows++));

    // Else is enough here because all free rows are ignored.
    if (!isobj)
      rowNames.push_back(rowname);
    else
      objectiveName = rowname;

    if (!ret.second) {
      std::cerr << "duplicate row " << rowname << std::endl;
      return HMpsFF::parsekey::FAIL;
    }
  }

  // Update numRow in case there is free rows. They won't be added to the
  // constraint matrix.
  numRow = rowLower.size();

  return HMpsFF::parsekey::FAIL;
}

typename HMpsFF::parsekey HMpsFF::parseCols(std::ifstream& file) {
  std::string colname = "";
  std::string strline, word;
  int rowidx, start, end;
  int ncols = 0;
  numCol = 0;
  bool integral_cols = false;

  // if (any_first_non_blank_as_star_implies_comment) {
  //   printf("In free format MPS reader: treating line as comment if first
  //   non-blank character is *\n");
  // } else {
  //   printf("In free format MPS reader: treating line as comment if first
  //   character is *\n");
  // }
  auto parsename = [&rowidx, this](std::string name) {
    auto mit = rowname2idx.find(name);

    assert(mit != rowname2idx.end());
    rowidx = mit->second;

    if (rowidx >= 0)
      this->nnz++;
    else
      assert(-1 == rowidx || -2 == rowidx);
  };

  auto addtuple = [&rowidx, &ncols, this](double coeff) {
    if (rowidx >= 0)
      entries.push_back(std::make_tuple(ncols - 1, rowidx, coeff));
    else if (rowidx == -1)
      coeffobj.push_back(std::make_pair(ncols - 1, coeff));
  };

  while (getline(file, strline)) {
    if (any_first_non_blank_as_star_implies_comment) {
      trim(strline);
      if (strline.size() == 0 || strline[0] == '*') continue;
    } else {
      if (strline.size() > 0) {
        // Just look for comment character in column 1
        if (strline[0] == '*') continue;
      }
      trim(strline);
      if (strline.size() == 0) continue;
    }

    HMpsFF::parsekey key = checkFirstWord(strline, start, end, word);

    // start of new section?
    if (key != parsekey::NONE) return key;

    // check for integrality marker
    std::string marker = first_word(strline, end);
    int end_marker = first_word_end(strline, end);

    if (marker == "'MARKER'") {
      marker = first_word(strline, end_marker);

      if ((integral_cols && marker != "'INTEND'") ||
          (!integral_cols && marker != "'INTORG'")) {
        std::cerr << "integrality marker error " << std::endl;
        return parsekey::FAIL;
      }
      integral_cols = !integral_cols;

      continue;
    }

    // Detect if file is in fixed format.
    // end_marker should be the end index of the row name:
    // more than 13 minus the 4 whitespaces we have trimmed from the start so
    // more than 9
    if (end_marker < 9) {
      std::string name = strline.substr(0, 10);
      name = trim(name);
      if (name.size() > 8)
        return HMpsFF::parsekey::FAIL;
      else
        return HMpsFF::parsekey::FIXED_FORMAT;
    }

    // new column?
    if (!(word == colname)) {
      colname = word;
      auto ret = colname2idx.emplace(colname, ncols++);
      numCol++;
      colNames.push_back(colname);

      if (!ret.second) {
        std::cerr << "duplicate column " << std::endl;
        return parsekey::FAIL;
      }

      col_integrality.push_back((int)integral_cols);

      // initialize with default bounds
      if (integral_cols) {
        colLower.push_back(0.0);
        colUpper.push_back(1.0);
      } else {
        colLower.push_back(0.0);
        colUpper.push_back(HIGHS_CONST_INF);
      }
    }

    assert(ncols > 0);

    // here marker is the row name and end marks its end
    word = "";
    word = first_word(strline, end_marker);
    end = first_word_end(strline, end_marker);

    if (word == "") {
      HighsLogMessage(HighsMessageType::ERROR,
                      "No coefficient given for column %s", marker.c_str());
      return HMpsFF::parsekey::FAIL;
    }

    auto mit = rowname2idx.find(marker);
    if (mit == rowname2idx.end()) {
      HighsPrintMessage(
          ML_ALWAYS, "COLUMNS section contains row %s not in ROWS section.\n",
          marker.c_str());
    } else {
      parsename(marker);  // rowidx set
      double value = atof(word.c_str());
      addtuple(value);
    }

    if (!is_end(strline, end)) {
      // parse second coefficient
      marker = first_word(strline, end);
      if (word == "") {
        HighsLogMessage(HighsMessageType::ERROR,
                        "No coefficient given for column %s", marker.c_str());
        return HMpsFF::parsekey::FAIL;
      }
      end_marker = first_word_end(strline, end);

      // here marker is the row name and end marks its end
      word = "";
      end_marker++;
      word = first_word(strline, end_marker);
      end = first_word_end(strline, end_marker);

      assert(is_end(strline, end));

      auto mit = rowname2idx.find(marker);
      if (mit == rowname2idx.end()) {
        HighsPrintMessage(
            ML_ALWAYS, "COLUMNS section contains row %s not in ROWS section.\n",
            marker.c_str());
        continue;
      };

      double value = atof(word.c_str());
      parsename(marker);  // rowidx set
      addtuple(value);
    }
  }

  return parsekey::FAIL;
}

HMpsFF::parsekey HMpsFF::parseRhs(std::ifstream& file) {
  std::string strline;

  auto parsename = [this](const std::string& name, int& rowidx) {
    auto mit = rowname2idx.find(name);

    assert(mit != rowname2idx.end());
    rowidx = mit->second;

    assert(rowidx < numRow);
  };

  auto addrhs = [this](double val, int rowidx) {
    if (rowidx > -1) {
      if (row_type[rowidx] == boundtype::EQ ||
          row_type[rowidx] == boundtype::LE) {
        assert(size_t(rowidx) < rowUpper.size());
        rowUpper[rowidx] = val;
      }

      if (row_type[rowidx] == boundtype::EQ ||
          row_type[rowidx] == boundtype::GE) {
        assert(size_t(rowidx) < rowLower.size());
        rowLower[rowidx] = val;
      }
    } else if (rowidx == -1) {
      // objective shift
      objOffset = val;
    }
  };

  while (getline(file, strline)) {
    if (any_first_non_blank_as_star_implies_comment) {
      trim(strline);
      if (strline.size() == 0 || strline[0] == '*') continue;
    } else {
      if (strline.size() > 0) {
        // Just look for comment character in column 1
        if (strline[0] == '*') continue;
      }
      trim(strline);
      if (strline.size() == 0) continue;
    }

    int begin = 0;
    int end = 0;
    std::string word;
    HMpsFF::parsekey key = checkFirstWord(strline, begin, end, word);

    // start of new section?
    if (key != parsekey::NONE && key != parsekey::RHS) return key;

    int rowidx;

    std::string marker = first_word(strline, end);
    int end_marker = first_word_end(strline, end);

    // here marker is the row name and end marks its end
    word = "";
    word = first_word(strline, end_marker);
    end = first_word_end(strline, end_marker);

    if (word == "") {
      HighsLogMessage(HighsMessageType::ERROR, "No bound given for row %s",
                      marker.c_str());
      return HMpsFF::parsekey::FAIL;
    }

    auto mit = rowname2idx.find(marker);
    if (mit == rowname2idx.end()) {
      HighsPrintMessage(ML_ALWAYS,
                        "RHS section contains row %s not in ROWS section.\n",
                        marker.c_str());
    } else {
      parsename(marker, rowidx);
      double value = atof(word.c_str());
      addrhs(value, rowidx);
    }

    if (!is_end(strline, end)) {
      // parse second coefficient
      marker = first_word(strline, end);
      if (word == "") {
        HighsLogMessage(HighsMessageType::ERROR,
                        "No coefficient given for rhs of row %s",
                        marker.c_str());
        return HMpsFF::parsekey::FAIL;
      }
      end_marker = first_word_end(strline, end);

      // here marker is the row name and end marks its end
      word = "";
      end_marker++;
      word = first_word(strline, end_marker);
      end = first_word_end(strline, end_marker);

      assert(is_end(strline, end));

      auto mit = rowname2idx.find(marker);
      if (mit == rowname2idx.end()) {
        HighsPrintMessage(ML_ALWAYS,
                          "RHS section contains row %s not in ROWS section.\n",
                          marker.c_str());
        continue;
      };

      parsename(marker, rowidx);
      double value = atof(word.c_str());
      addrhs(value, rowidx);
    }
  }

  return parsekey::FAIL;
}

HMpsFF::parsekey HMpsFF::parseBounds(std::ifstream& file) {
  std::string strline, word;

  int num_mi = 0;
  int num_pl = 0;
  int num_bv = 0;
  int num_li = 0;
  int num_ui = 0;
  auto parsename = [this](const std::string& name, int& colidx) {
    auto mit = colname2idx.find(name);
    // assert(mit != colname2idx.end());
    // No check because if mit = end we add an empty column with the
    // corresponding bound.
    if (mit == colname2idx.end())
      colidx = numCol;
    else
      colidx = mit->second;
    assert(colidx >= 0);
  };

  while (getline(file, strline)) {
    if (any_first_non_blank_as_star_implies_comment) {
      trim(strline);
      if (strline.size() == 0 || strline[0] == '*') continue;
    } else {
      if (strline.size() > 0) {
        // Just look for comment character in column 1
        if (strline[0] == '*') continue;
      }
      trim(strline);
      if (strline.size() == 0) continue;
    }

    int begin = 0;
    int end = 0;
    std::string word;
    HMpsFF::parsekey key = checkFirstWord(strline, begin, end, word);

    // start of new section?
    if (key != parsekey::NONE) {
      if (num_mi) printf("Number of MI entries in BOUNDS section is %d\n", num_mi);
      if (num_pl) printf("Number of PL entries in BOUNDS section is %d\n", num_pl);
      if (num_bv) printf("Number of BV entries in BOUNDS section is %d\n", num_bv);
      if (num_li) printf("Number of LI entries in BOUNDS section is %d\n", num_li);
      if (num_ui) printf("Number of UI entries in BOUNDS section is %d\n", num_ui);
      return key;
    }
    bool islb = false;
    bool isub = false;
    bool isintegral = false;
    bool isdefaultbound = false;
    if (word == "UP")  // lower bound
      isub = true;
    else if (word == "LO")  // upper bound
      islb = true;
    else if (word == "FX")  // fixed
    {
      islb = true;
      isub = true;
    } else if (word == "MI")  // infinite lower bound
    {
      islb = true;
      isdefaultbound = true;
      num_mi++;
    } else if (word == "PL")  // infinite upper bound (redundant)
    {
      isub = true;
      isdefaultbound = true;
      num_pl++;
    } else if (word == "BV")  // binary
    {
      islb = true;
      isub = true;
      isintegral = true;
      isdefaultbound = true;
      num_bv++;
    } else if (word == "LI")  // integer lower bound
    {
      islb = true;
      isintegral = true;
      num_li++;
    } else if (word == "UI")  // integer upper bound
    {
      isub = true;
      isintegral = true;
      num_ui++;
    } else if (word == "FR")  // free variable
    {
      islb = true;
      isub = true;
      isdefaultbound = true;
    } else {
      std::cerr << "unknown bound type " << word << std::endl;
      exit(1);
    }

    // The first word is the bound name, which should be ignored.
    int end_bound_name = first_word_end(strline, end);
    std::string marker = first_word(strline, end_bound_name);
    int end_marker = first_word_end(strline, end_bound_name);

    auto mit = colname2idx.find(marker);
    if (mit == colname2idx.end()) {
      HighsPrintMessage(ML_ALWAYS,
                        "BOUNDS section contains col %s not in COLS section.\n",
                        marker.c_str());
      continue;
    };

    int colidx;
    parsename(marker, colidx);

    // If empty column with empty cost add column
    if (colidx == numCol) {
      std::string colname = marker;
      // auto ret = colname2idx.emplace(colname, numCol++);
      colNames.push_back(colname);

      col_integrality.push_back(0);

      // initialize with default bounds
      colLower.push_back(0.0);
      colUpper.push_back(HIGHS_CONST_INF);
      numCol++;
    }
    if (isdefaultbound) {
      // MI, PL, BV or FR
      if (isintegral)
	// binary: BV
      {
        if (islb) colLower[colidx] = 0.0;
        if (isub) colUpper[colidx] = 1.0;
        col_integrality[colidx] = true;
      } else {
	// continuous: MI, PL or FR
        if (islb) colLower[colidx] = -HIGHS_CONST_INF;
        if (isub) colUpper[colidx] = HIGHS_CONST_INF;
      }
      continue;
    }
    // Bounds now are UP, LO, FX, LI or UI
    // here marker is the col name and end marks its end
    word = "";
    word = first_word(strline, end_marker);
    end = first_word_end(strline, end_marker);

    if (word == "") {
      HighsLogMessage(HighsMessageType::ERROR, "No bound given for row %s",
                      marker.c_str());
      return HMpsFF::parsekey::FAIL;
    }
    double value = atof(word.c_str());
    if (isintegral) {
      // Must be LI or UI
      //
      // Bounds will be either [-inf, value] (if LI) or [value, inf] (if UI)
      //
      // Slightly clunky, but set bounds to be [-inf, inf] and one
      // bounds will be over-written by value according to islb/isub
      colLower[colidx] = -HIGHS_CONST_INF;
      colUpper[colidx] = HIGHS_CONST_INF;
      //
      // Also, value should be integer
      int i_value = static_cast<int>(value);
      double dl = value-i_value;
      if (dl) HighsLogMessage(HighsMessageType::ERROR, "Bound for for LI/UI row %s is %g: not integer", marker.c_str(), value);
    }
    if (islb) colLower[colidx] = value;
    if (isub) colUpper[colidx] = value;
    if (isintegral) col_integrality[colidx] = true;
  }

  return parsekey::FAIL;
}

HMpsFF::parsekey HMpsFF::parseRanges(std::ifstream& file) {
  std::string strline, word;

  auto parsename = [this](const std::string& name, int& rowidx) {
    auto mit = rowname2idx.find(name);

    assert(mit != rowname2idx.end());
    rowidx = mit->second;

    assert(rowidx >= 0);
    assert(rowidx < numRow);
  };

  auto addrhs = [this](double val, int& rowidx) {
    if ((row_type[rowidx] == boundtype::EQ && val < 0) ||
        row_type[rowidx] == boundtype::LE) {
      assert(rowUpper.at(rowidx) < HIGHS_CONST_INF);
      rowLower.at(rowidx) = rowUpper.at(rowidx) - fabs(val);
    }

    else if ((row_type[rowidx] == boundtype::EQ && val > 0) ||
             row_type[rowidx] == boundtype::GE) {
      assert(rowLower.at(rowidx) > (-HIGHS_CONST_INF));
      rowUpper.at(rowidx) = rowLower.at(rowidx) + fabs(val);
    }
  };

  while (getline(file, strline)) {
    if (any_first_non_blank_as_star_implies_comment) {
      trim(strline);
      if (strline.size() == 0 || strline[0] == '*') continue;
    } else {
      if (strline.size() > 0) {
        // Just look for comment character in column 1
        if (strline[0] == '*') continue;
      }
      trim(strline);
      if (strline.size() == 0) continue;
    }

    int begin, end;
    std::string word;
    HMpsFF::parsekey key = checkFirstWord(strline, begin, end, word);

    if (key != parsekey::NONE) return key;

    int rowidx;

    std::string marker = first_word(strline, end);
    int end_marker = first_word_end(strline, end);

    // here marker is the row name and end marks its end
    word = "";
    word = first_word(strline, end_marker);
    end = first_word_end(strline, end_marker);

    if (word == "") {
      HighsLogMessage(HighsMessageType::ERROR, "No range given for row %s",
                      marker.c_str());
      return HMpsFF::parsekey::FAIL;
    }

    auto mit = rowname2idx.find(marker);
    if (mit == rowname2idx.end()) {
      HighsPrintMessage(
          ML_ALWAYS, "RANGES section contains row %s not in ROWS    section\n",
          marker.c_str());
      continue;
    } else {
      parsename(marker, rowidx);
      double value = atof(word.c_str());
      addrhs(value, rowidx);
    }

    if (!is_end(strline, end)) {
      string marker = first_word(strline, end);
      int end_marker = first_word_end(strline, end);

      // here marker is the row name and end marks its end
      word = "";
      word = first_word(strline, end_marker);
      end = first_word_end(strline, end_marker);

      if (word == "") {
        HighsLogMessage(HighsMessageType::ERROR, "No range given for row %s",
                        marker.c_str());
        return HMpsFF::parsekey::FAIL;
      }

      auto mit = rowname2idx.find(marker);
      if (mit == rowname2idx.end()) {
        HighsPrintMessage(
            ML_ALWAYS,
            "RANGES section contains row %s not in ROWS    section\n",
            marker.c_str());
        continue;
      };

      parsename(marker, rowidx);
      double value = atof(word.c_str());
      addrhs(value, rowidx);

      if (!is_end(strline, end)) {
        HighsLogMessage(HighsMessageType::ERROR,
                        "Unknown specifiers in RANGES section for row %s.\n",
                        marker.c_str());
        return HMpsFF::parsekey::FAIL;
      }
    }
  }

  return HMpsFF::parsekey::FAIL;
}

#endif /* IO_HMPSFF_H_ */
