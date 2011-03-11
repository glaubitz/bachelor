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

#include "spinhamiltonian.h"

#include <iostream>
#include <string>
#include <cmath>

#include "constants.h"
#include "experiment.h"
#include "spins.h"

using namespace Constants;
using namespace Eigen;
using namespace std;

// Pauli Matrices
namespace PauliMatrix {
const Matrix2c X = (Matrix2c() << 0, 0.5, 0.5, 0).finished();
const Matrix2c Y = (Matrix2c() << 0, c_fp(0, -0.5), c_fp(0, 0.5), 0).finished();
const Matrix2c Z = (Matrix2c() << 0.5, 0, 0, -0.5).finished();
}

SpinHamiltonian::SpinHamiltonian(const fp B, const Experiment& experiment)
: m_B(B)
, m_exp(experiment)
, m_spins(1 + m_exp.nProtons, m_exp.nNitrogens)
, m_staticBField(m_exp.staticBField(B))
{
  if (m_spins.states != m_exp.dimension) {
    cerr << "spins states init error" << endl;
    exit(1);
  }
}

SpinHamiltonian::~SpinHamiltonian()
{
}

inline Vector3c SpinHamiltonian::spinVector(int bra, int ket, int k) const
{
  return m_spins.spinVector(bra, ket, k);
  /*
  const int a = spinState(i, k); //spin state of state k in row i
  const int b = spinState(j, k); //spin state of state k in column j
  return (Vector3c() << PauliMatrix::X(a, b), PauliMatrix::Y(a, b), PauliMatrix::Z(a, b)).finished();
  */
}

inline bool SpinHamiltonian::spinState(int state, int k) const
{
  return m_spins.spinInState(k, state);
  /*
  // k-bit == 2^k = 0001000
  //                   ^k = 4
  const int kPow = (1 << k);
  return i & kPow;
  */
}

inline bool SpinHamiltonian::stateContributes(int bra, int ket, int k, bool ignoreElectron) const
{
  for(int n = 0; n < m_spins.elements; ++n) {
    if (n == k || (ignoreElectron && n == 0)) {
      continue;
    } else if (m_spins.spinInState(n, bra) != m_spins.spinInState(n, ket)) {
      return false;
    }
  }
  return true;
  /*
  // states are equal if: all bits except for k-bit are equal
  // k-bit == 2^k = 0001000
  //                   ^k = 4
  int kBit = (1 << k);
  if (ignoreElectron) {
    // ignore electron bit
    // electronBit == 2^nProtons == 100000...
    int electronBit = (1 << m_exp.nProtons);
    // essentially a fast variant of:
    // i % electronBit
    i &= electronBit - 1;
    j &= electronBit - 1;
  }
  return (i | kBit) == (j | kBit);
  */
}

MatrixXc SpinHamiltonian::hamiltonian() const
{
  return nuclearZeeman() + hyperFine() + electronZeeman();
}

MatrixXc SpinHamiltonian::nuclearZeeman() const
{
  //Compute nZeeman============================================================  
  MatrixXc nZeeman(m_spins.states, m_spins.states);
  nZeeman.setZero();
  //to turn off: return nZeeman;

  for (int bra = 0; bra < m_spins.states; ++bra) {
    for (int ket = 0; ket < m_spins.states; ++ket) {
      if (spinState(bra, 0 /* = Electron */) != spinState(ket, 0 /* = Electron */)) {
        continue;  //matrix elements between different electron states are zero
      }

      // k = 1 to skip electron
      for (int k = 1; k < m_spins.states; ++k) {
        if (!stateContributes(bra, ket, k)) {
          continue;
        }

        // set cell to dot product of H_B and I
        c_fp val = m_staticBField.dot(spinVector(bra, ket, k));
        if (k < m_spins.spinHalfs) {
          // J = 1/2
          val *= g_1H;
        } else {
          // J = 1
          val *= g_14N;
        }
        nZeeman(bra, ket) += val;
      }
    }
  }

  nZeeman *= -1.0 * NUC_MAGNETON;

  // DEBUG:
  // cout << nZeeman << endl;

  return nZeeman;
}

MatrixXc SpinHamiltonian::hyperFine() const
{
  //Compute Hyperfine couplings matrix=========================================
  MatrixXc hyperfine(m_spins.states, m_spins.states);
  hyperfine.setZero();

  for (int bra = 0; bra < m_spins.states; ++bra) {
    for (int ket = 0; ket < m_spins.states; ++ket) {
      //compute elements of s vector
      const Vector3c s = spinVector(bra, ket, 0 /* = Electron */);

      // k = 1 to skip electron
      for (int k = 1; k < m_spins.elements; ++k) {    //loop over nuclei
        if (!stateContributes(bra, ket, k)) {
          continue;
        }

        //multiply atensor by I
        //multiply s by atensor_I
        ///TODO: proper aTensor for different nuclei
        hyperfine(bra, ket) += s.dot(m_exp.aTensor * spinVector(bra, ket, k));
      }
    }
  }
  hyperfine *= h * 1.0E6;

  // DEBUG:
  // cout << hyperfine << endl;

  return hyperfine;
}

// equation: \beta S * g * H
// \beta: borh magneton
// S: electron Spin Operator
// g: g Tensor
// H: static B Field hamiltonian
MatrixXc SpinHamiltonian::electronZeeman() const
{
  //Compute eZeeman============================================================  
  MatrixXc eZeeman(m_spins.states, m_spins.states);
  eZeeman.setZero();

  //first multiply the g tensor with the static magnetic field hamiltonian
  const Vector3c gDotH_B = m_exp.gTensor * m_staticBField;
  //depending on the convention, i might have to tranpose the gtensor here
  for (int bra = 0; bra < m_spins.states; ++bra) {
    for (int ket = 0; ket < m_spins.states; ++ket) {
      // 0 is always the index of the electron spin
      if (!stateContributes(bra, ket, 0 /* = Electron */)) {
        continue;
      }

      eZeeman(bra, ket) = gDotH_B.dot(spinVector(bra, ket, 0 /* = Electron */));
    }
  }
  eZeeman *= Bohrm;

  // DEBUG
  // cout << eZeeman << endl;

  return eZeeman;
}

MatrixXc SpinHamiltonian::magneticMoments() const
{
  MatrixXc moments(m_spins.states, m_spins.states);

  for (int bra = 0; bra < m_spins.states; ++bra) {
    for (int ket = 0; ket < m_spins.states; ++ket) {
      //m_exp.nProtons is always the index of the electronic spin state
      moments(bra, ket) = magneticMoment(bra, ket);

//       cout << i << '\t' << j << '\t' << "FINAL:" << '\t' << moments(i, j) << endl;
    }
  }

  //cout << moments << endl;
  return moments;
}

c_fp SpinHamiltonian::magneticMoment(const int bra, const int ket) const
{
  c_fp ret = 0;
  for (int k = 0; k < m_spins.elements; ++k) {
    if (!stateContributes(bra, ket, k, IncludeElectron)) {
      continue;
    }

    const int braSpin = spinState(bra, k);  //spin state of state k in row i
    const int ketSpin = spinState(ket, k);  //spin state of state k in column j
    c_fp xMoment = PauliMatrix::X(braSpin, ketSpin);

    if (k == 0) {
      // electron
      xMoment *= 2.023 * Bohrm;
    } else if (k < m_spins.spinHalfs) {
      // J = 1/2 nucleus
      xMoment *= -1.0 * g_1H * NUC_MAGNETON;
    } else {
      // J = 1 nucleus
      xMoment *= -1.0 * g_14N * NUC_MAGNETON;
    }

//         cout << i << '\t' << j << '\t' << k << '\t' << xMoment << endl;
    ret += xMoment;
  }
  return ret;
}

///TODO: optimize!
MatrixX SpinHamiltonian::intensityMatrix(const MatrixXc& eigenVectors) const {
  const MatrixXc moments = magneticMoments();
  ///TODO: take direction of B0 and B1 into account, integrate over plane
  MatrixX intensities(m_spins.states, m_spins.states);
  for(int i = 0; i < m_spins.states; ++i) {
    /// right part: M | Psi_i >
    const MatrixXc mTimesPsiI = moments * eigenVectors.col(i);
    for(int j = 0; j < m_spins.states; ++j) {
      if (i != j) {
        /// left part with < Psi_j | and abs squared: |< Psi_j | M | Psi_i >|^2
        intensities(i, j) = (eigenVectors.col(j).adjoint() * mTimesPsiI).norm();
        ///TODO: eq 3-24, p 52 says: |< j|M|i > dot H_1|^2
        ///meaning: what about H_1?
      } else {
        intensities(i, j) = 0;
      }
    }
  }
  return intensities;
}

fp SpinHamiltonian::calculateIntensity() const
{
  //Diagonalize the total Hamiltonian matrix===================================
  SelfAdjointEigenSolver<MatrixXc> eigenSolver(hamiltonian());
  const VectorX eigenValues = eigenSolver.eigenvalues();
  const MatrixXc eigenVectors = eigenSolver.eigenvectors();

  const MatrixXc moments = magneticMoments();

  fp intensity = 0;
  ///TODO: take direction of B0 and B1 into account, integrate over plane
  for (int i = 0;i < m_exp.dimension; ++i) {
    for (int j = i + 1; j < m_exp.dimension; ++j) {
      // transition frequency:
      const fp freq = (1.0/h/1.0E9 * abs(eigenValues(i) - eigenValues(j)));
      // assume it's only seen when energy is below frequency threshold
      if (abs(m_exp.mwFreqGHz/freq - 1.0) > 5.0E-4) {
        continue;
      }
      /// < Psi_j | and abs squared: |< Psi_j | M | Psi_i >|^2 M | Psi_i >
      intensity += (eigenVectors.col(j).adjoint() * moments * eigenVectors.col(i)).norm();
      ///TODO: eq 3-24, p 52 says: |< j|M|i > dot H_1|^2
      ///meaning: what about H_1?
    }
  }
  return (intensity * 2.0 * M_PI * (Bohrm / hbar) * (Bohrm / hbar));
}

void SpinHamiltonian::calculateTransitions() const
{
  //Diagonalize the total Hamiltonian matrix===================================
  SelfAdjointEigenSolver<MatrixXc> eigenSolver(hamiltonian());
  const VectorX eigenValues = eigenSolver.eigenvalues();
  const MatrixXc eigenVectors = eigenSolver.eigenvectors();

  MatrixX probabilities = intensityMatrix(eigenVectors);
  probabilities /= probabilities.maxCoeff();
  
  cout << "\n\nAllowed Transitions: \t\t\tB= " << fixed << m_B << endl;
  cout << "---------------------------------------------------\n";
  cout << "Transition\tFrequency (GHz)\t\tProbability" << endl;
  cout << "---------------------------------------------------\n";
  int transitions = 1;
  for (int i = 0;i < m_exp.dimension; ++i) {
    for (int j = i + 1; j < m_exp.dimension; ++j) {
      const fp probability = probabilities(i, j);
      if (probability > 1.0E-6) {
        cout.width(10);
        cout << right << transitions++ << '\t';
        cout.width(14);
        cout.precision(5);
        // transition frequency:
        cout << right << (1.0/h/1.0E9 * abs(eigenValues(i) - eigenValues(j))) << "\t\t";
        cout.precision(8);
        cout << probability << endl;
      }
    }
  }
  cout << "---------------------------------------------------\n";
  cout << transitions << " transitions in total" << endl;
}

VectorX SpinHamiltonian::calculateEigenValues() const
{
  //Diagonalize the total Hamiltonian matrix===================================
  SelfAdjointEigenSolver<MatrixXc> eigenSolver(hamiltonian(), EigenvaluesOnly);
  return eigenSolver.eigenvalues();
}
