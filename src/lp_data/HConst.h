/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                       */
/*    This file is part of the HiGHS linear optimization suite           */
/*                                                                       */
/*    Written and engineered 2008-2019 at the University of Edinburgh    */
/*                                                                       */
/*    Available as open-source under the MIT License                     */
/*                                                                       */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/**@file lp_data/HConst.h
 * @brief Constants for HiGHS
 * @author Julian Hall, Ivet Galabova, Qi Huangfu and Michael Feldmeier
 */
#ifndef LP_DATA_HCONST_H_
#define LP_DATA_HCONST_H_

#include <string>

const int HIGHS_CONST_I_INF = 2147483647;  // 32767;
const double HIGHS_CONST_INF = 1e200;
const double HIGHS_CONST_TINY = 1e-14;
const double HIGHS_CONST_ZERO = 1e-50;
const std::string off_string = "off";
const std::string choose_string = "choose";
const std::string on_string = "on";

constexpr double kBoundTolerance = 1e-8;

enum HighsPrintMessageLevel {
  ML_MIN = 0,
  ML_NONE = ML_MIN,
  ML_VERBOSE = 1,
  ML_DETAILED = 2,
  ML_MINIMAL = 4,
  ML_ALWAYS = ML_VERBOSE | ML_DETAILED | ML_MINIMAL,
  ML_MAX = ML_ALWAYS
};

enum class HighsOptionType { BOOL = 0, INT, DOUBLE, STRING};

enum OptionOffChooseOn { OPTION_OFF = -1, OPTION_CHOOSE, OPTION_ON };

enum SolverOption { SOLVER_OPTION_SIMPLEX = -1, SOLVER_OPTION_CHOOSE, SOLVER_OPTION_IPM }; 

const std::string FILENAME_DEFAULT = "";

/** SCIP/CPLEX-like HiGHS basis status for columns and rows. */
enum class HighsBasisStatus {
  LOWER =
      0,  // (slack) variable is at its lower bound [including fixed variables]
  BASIC,  // (slack) variable is basic
  UPPER,  // (slack) variable is at its upper bound
  ZERO,   // free variable is non-basic and set to zero
  NONBASIC, // nonbasic with no specific bound information - useful for users and postsolve
  SUPER   // Super-basic variable: non-basic and either free and
          // nonzero or not at a bound. No SCIP equivalent
};

/** Simplex nonbasicFlag status for columns and rows. Don't use enum
    class since they are used as int to replace conditional statements
    by multiplication */
const int NONBASIC_FLAG_TRUE = 1;   // Nonbasic
const int NONBASIC_FLAG_FALSE = 0;  // Basic

/** Simplex nonbasicMove status for columns and rows. Don't use enum
    class since they are used in conditional statements */
const int NONBASIC_MOVE_UP = 1;   // Free to move (only) up
const int NONBASIC_MOVE_DN = -1;  // Free to move (only) down
const int NONBASIC_MOVE_ZE = 0;   // Fixed or free to move up and down
//
// Relation between HiGHS basis and Simplex basis
//
// Data structures
// ===============
//
// HiGHS basis consists of vectors
//
// * col_status[numCol]
// * row_status[numRow]
//
// Simplex basis consists of vectors
//
// * nonbasicMove[numTot]
// * basicIndex[numRow]
// * nonbasicFlag[numTot]
//
// where nonbasicFlag is duplicate information but is used to identify
// whether a particular variable is basic or nonbasic.
//
// Basic variables
// ===============
//
// Highs: *_status value of BASIC
//
// <=>
//
// Simplex: nonbasicFlag value of NONBASIC_FLAG_FALSE
//
// Nonbasic variables
// ==================
//
// Relations complicated by the fact that
//
// * HiGHS   rows have bounds [ l,  u]
// * Simplex rows have bounds [-u, -l]
//
// Nonbasic columns
// ================
//
// Highs: col_status value of LOWER - at lower bound
// <=>
// Simplex: nonbasicMove value of NONBASIC_MOVE_UP - [l, Inf] column free to
// move up and negative dual
//
// Highs: col_status value of ZERO - at zero
// =>
// Simplex: nonbasicMove value of NONBASIC_MOVE_ZE - free variable treated
// specially in simplex
//
// Highs: col_status value of UPPER - at upper bound
// =>
// Simplex: Either
// * nonbasicMove value of NONBASIC_MOVE_DN - [-Inf, u] column free to move down
// and positive dual
// * nonbasicMove value of NONBASIC_MOVE_ZE - [   l, u] column ?? and free dual
//
// Simplex: nonbasicMove value of NONBASIC_MOVE_DN - [-Inf, u] column free to
// move down and positive dual
// =>
// Highs: col_status value of UPPER - at upper bound
//
// Simplex: nonbasicMove value of NONBASIC_MOVE_ZE - [l, u] column ?? and free
// dual
// =>
// Highs: Either
// * col_status value of UPPER - at upper bound
// * col_status value of ZERO - at zero
//
// Nonbasic rows
// =============
//
#endif /* LP_DATA_HCONST_H_ */
