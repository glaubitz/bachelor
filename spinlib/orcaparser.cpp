/*
 * This file is part of my bachelor thesis.
 *
 * Copyright 2011 Milian Wolff <mail@milianw.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "orcaparser.h"

#include "constants.h"
#include "nucleus.h"

#include <fstream>
#include <iostream>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/regex.hpp>
#include <boost/lexical_cast.hpp>

using namespace std;

OrcaParser::OrcaParser(const string& file)
{
  parseFile(file);
}

OrcaParser::~OrcaParser()
{

}

Matrix3c OrcaParser::electronGMatrix() const
{
  return m_gMatrix;
}

vector<Nucleus> OrcaParser::nuclei() const
{
  return m_nuclei;
}

void OrcaParser::parseFile(const string& file)
{
  ifstream stream(file.data(), ios::in);
  if (!stream.is_open()) {
    throw "could not open input file";
  }

  string line;
  while(!stream.eof()) {
    getline(stream, line);
    if (boost::starts_with(line,"ELECTRONIC G-MATRIX")) {
      getline(stream, line); // ----...
      getline(stream, line); //
      getline(stream, line); // The g-matrix:
      for(int i = 0; i < 3; ++i) {
        for(int j = 0; j < 3; ++j) {
          stream >> m_gMatrix(i, j);
        }
      }
    } else if (boost::starts_with(line, " Nucleus")) {
      static boost::regex pattern("\\s*Nucleus\\s*([^:]+) :"
                                  " A:ISTP=\\s*\\d+"
                                  " I=\\s*([0-9]+\\.[0-9]+)"
                                  " P=\\s*([0-9]+\\.[0-9]+) (.+)$");
      boost::smatch matches;
      if (!boost::regex_match(line, matches, pattern)) {
        cerr << "could not parse line:" << line << endl;
        throw "parser error";
      }

      string nucleus(matches.str(1));
      fp spin = boost::lexical_cast<fp>(matches.str(2));
      fp P = boost::lexical_cast<fp>(matches.str(3));
      string pUnit(matches.str(4));
      if (pUnit != "MHz/au**3") {
        cerr << "unhandled unit for g of nucleus in line:" << line << endl;
        throw "parser error";
      }
      fp g;
      ///TODO: calculate g out of P?
      switch(*(nucleus.end() - 1)) {
        case 'N':
          g = Constants::g_14N;
          break;
        case 'H':
          g = Constants::g_1H;
          break;
        default:
          cerr << "unhandled nucleus type, could not instantiate g-factor in line:" << line << endl;
          throw "parser error";
      }

      Matrix3c A;
      while(!stream.eof()) {
        getline(stream, line);
        if(boost::starts_with(line, " Orientation:")) {
          string devZero;
          for(int i = 0; i < 3; ++i) {
            stream >> devZero; // X,Y,Z
            for(int j = 0; j < 3; ++j) {
              stream >> A(i, j);
            }
          }
          break;
        }
      }
      m_nuclei.push_back(Nucleus(nucleus, spin * 2, A, g));
    }
  }
}