/****************************************************************************
 * Copyright (c) 2018-2019 by the Cabana authors                            *
 * All rights reserved.                                                     *
 *                                                                          *
 * This file is part of the Cabana library. Cabana is distributed under a   *
 * BSD 3-clause license. For the licensing terms see the LICENSE file in    *
 * the top-level directory.                                                 *
 *                                                                          *
 * SPDX-License-Identifier: BSD-3-Clause                                    *
 ****************************************************************************/

#include <force_nnp_cabana_neigh.h>
#include <string.h>
#include <string>
#include <iostream>

#define MAX_SF 30 //TODO: hardcoded 

template<class t_neighbor, class t_neigh_parallel, class t_angle_parallel>
ForceNNP<t_neighbor, t_neigh_parallel, t_angle_parallel>::ForceNNP(System* system, bool half_neigh_):Force(system,half_neigh) {
  ntypes = system->ntypes;
  N_local = 0;
  step = 0;
  half_neigh = half_neigh_;
  //resize nnp_data to have size as big as number of atoms in system
  //AoSoA_NNP nnp_data ("ForceNNP::nnp_data", system->N);
  t_AoSoA_NNP_G aosoa_G("ForceNNP::G", system->N);
  t_AoSoA_NNP_G aosoa_dEdG("ForceNNP::dEdG", system->N);
  t_AoSoA_fl aosoa_energy("ForceNNP::energy", system->N);
}


template<class t_neighbor, class t_neigh_parallel, class t_angle_parallel>
void ForceNNP<t_neighbor, t_neigh_parallel, t_angle_parallel>::create_neigh_list(System* system) {
  N_local = system->N_local;
  double grid_min[3] = {system->sub_domain_lo_x - system->sub_domain_x,
                        system->sub_domain_lo_y - system->sub_domain_y,
                        system->sub_domain_lo_z - system->sub_domain_z};
  double grid_max[3] = {system->sub_domain_hi_x + system->sub_domain_x,
                        system->sub_domain_hi_y + system->sub_domain_y,
                        system->sub_domain_hi_z + system->sub_domain_z};

  auto x = Cabana::slice<0>(system->aosoa_x);

  t_neighbor list( x, 0, N_local, neigh_cut, 1.0, grid_min, grid_max );
  neigh_list = list;
}


template<class t_neighbor, class t_neigh_parallel, class t_angle_parallel>
const char* ForceNNP<t_neighbor, t_neigh_parallel, t_angle_parallel>::name() {return half_neigh?"Force:NNPCabanaVerletHalf":"Force:NNPCabanaVerletFull";}

template<class t_neighbor, class t_neigh_parallel, class t_angle_parallel>
void ForceNNP<t_neighbor, t_neigh_parallel, t_angle_parallel>::init_coeff(T_X_FLOAT neigh_cutoff, char** args) {
  neigh_cut = neigh_cutoff;
  mode = new(nnpCbn::Mode);
  mode->initialize();
  std::string settingsfile = std::string(args[3]) + "/input.nn"; //arg[3] gives directory path
  mode->loadSettingsFile(settingsfile);
  mode->setupNormalization();
  mode->setupElementMap();
  atomicEnergyOffset = mode->setupElements(atomicEnergyOffset);
  mode->setupCutoff();
  h_numSFperElem = mode->setupSymmetryFunctions(h_numSFperElem);
  d_numSFperElem = t_mass("ForceNNP::numSymmetryFunctionsPerElement", mode->numElements);
  mode->setupSymmetryFunctionGroups();
  mode->setupNeuralNetwork();
  std::string scalingfile = std::string(args[3]) + "/scaling.data";
  mode->setupSymmetryFunctionScaling(scalingfile);
  std::string weightsfile = std::string(args[3]) + "/weights.%03zu.data";
  mode->setupSymmetryFunctionStatistics(false, false, true, false);
  mode->setupNeuralNetworkWeights(weightsfile);

}

template<class t_neighbor, class t_neigh_parallel, class t_angle_parallel>
void ForceNNP<t_neighbor, t_neigh_parallel, t_angle_parallel>::compute(System* s) {
  //nnp_data.resize(s->N_local);
  aosoa_G.resize(s->N_local);
  aosoa_dEdG.resize(s->N_local);
  aosoa_energy.resize(s->N_local);
  Kokkos::deep_copy(d_numSFperElem, h_numSFperElem);
  mode->calculateSymmetryFunctionGroups<t_neighbor, t_neigh_parallel, t_angle_parallel>(s, aosoa_G, neigh_list);
  mode->calculateAtomicNeuralNetworks<t_neighbor, t_neigh_parallel, t_angle_parallel>(s, aosoa_G, aosoa_dEdG, aosoa_energy, d_numSFperElem);
  mode->calculateForces<t_neighbor, t_neigh_parallel, t_angle_parallel>(s, aosoa_dEdG, neigh_list);
}

template<class t_neighbor, class t_neigh_parallel, class t_angle_parallel>
T_V_FLOAT ForceNNP<t_neighbor, t_neigh_parallel, t_angle_parallel>::compute_energy(System* s) {
    
  auto energy = Cabana::slice<0>(aosoa_energy);
  T_V_FLOAT system_energy=0.0;
  // Loop over all atoms and add atomic contributions to total energy.
  Kokkos::parallel_reduce("ForceNNPCabanaNeigh::compute_energy", s->N_local, KOKKOS_LAMBDA (const size_t i, T_V_FLOAT & updated_energy)
  {
      updated_energy += energy(i);
  }, system_energy);

  Kokkos::fence();
  system_energy += s->N*atomicEnergyOffset(0); //TODO: replace hardcoded
  system_energy /= mode->convEnergy;
  //std::cout << "Mean energy: " << mode->meanEnergy << std::endl;
  //std::cout << "conv energy: " << mode->convEnergy << std::endl;
  //std::cout << "Mean energy: " << mode->meanEnergy << std::endl;
  //std::cout << "cfenergy: " << s->cfenergy << std::endl;
  system_energy += s->N*mode->meanEnergy;
  system_energy *= 27.211384021355236; //hartree to eV conversion (TODO: look into this)
  step++;
  return system_energy;
}
