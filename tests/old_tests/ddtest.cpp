/*
  Copyright (C) 2016 Finnish Meteorological Institute
  Copyright (C) 2016 CSC -IT Center for Science 

  This file is part of fsgrid
 
  fsgrid is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  fsgrid is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY;
  without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with fsgrid.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "../../src/fsgrid.hpp"
#include <array>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv){
  
   std::array<FsGridTools::FsSize_t,3> sys;
   std::array<FsGridTools::Task_t,3> processDomainDecomposition;

   if(argc != 5) {
      printf("Usage %s size_x size_y size_z nProcesses\n", argv[0]);
      exit(1);
   }
   
      
   sys[0] = atof(argv[1]);
   sys[1] = atof(argv[2]);
   sys[2] = atof(argv[3]);
   uint nProcs = atoi(argv[4]);

   FsGridTools::computeDomainDecomposition(sys, nProcs, processDomainDecomposition, 1, true);
   printf("DD of %ld %ld %ld for %d processes is %ld %ld %ld \n", 
          sys[0], sys[1], sys[2], nProcs,
          processDomainDecomposition[0], processDomainDecomposition[1], processDomainDecomposition[2]);


  
}
