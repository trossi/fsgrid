#pragma once
/*
  Copyright (C) 2016 Finnish Meteorological Institute
  Copyright (C) 2016-2024 CSC -IT Center for Science

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

#include <algorithm>
#include <array>
#include <cassert>
#include <iostream>
#include <limits>
#include <mpi.h>
#include <stdint.h>
#include <stdio.h>
#include <vector>

#ifndef FS_MASTER_RANK
#define FS_MASTER_RANK 0
#endif

struct FsGridTools {
    // Size type for global array indices
    // Size type for global/local array indices, incl. possible negative values
    typedef uint32_t FsSize_t;
    typedef int32_t FsIndex_t;

    typedef int64_t LocalID;
    typedef int64_t GlobalID;
    typedef int Task_t;

    // clang-format off
    //! Helper function: calculate position of the local coordinate space for
    //! the given dimension
    // \param globalCells Number of cells in the global Simulation, in this
    // dimension
    // \param ntasks Total number of tasks in this dimension
    // \param my_n This task's position in this dimension
    // \return Cell number at which this task's domains cells start (actual cells, not counting ghost cells)
    // clang-format on
    static FsIndex_t calcLocalStart(FsSize_t globalCells, Task_t ntasks,
                                    Task_t my_n) {
        FsIndex_t n_per_task = globalCells / ntasks;
        FsIndex_t remainder = globalCells % ntasks;

        if (my_n < remainder) {
            return my_n * (n_per_task + 1);
        } else {
            return my_n * n_per_task + remainder;
        }
    }

    //! Helper function: given a global cellID, calculate the global cell
    //! coordinate from it.
    // This is then used do determine the task responsible for this cell, and
    // the local cell index in it.
    static std::array<FsIndex_t, 3>
    globalIDtoCellCoord(GlobalID id, std::array<FsSize_t, 3> &globalSize) {
        // Transform globalID to global cell coordinate
        std::array<FsIndex_t, 3> cell;

        assert(id >= 0);
        assert(id < globalSize[0] * globalSize[1] * globalSize[2]);

        int stride = 1;
        for (int i = 0; i < 3; i++) {
            cell[i] = (id / stride) % globalSize[i];
            stride *= globalSize[i];
        }

        return cell;
    }

    // clang-format off
    //! Helper function: calculate size of the local coordinate space for the
    //! given dimension
    // \param globalCells Number of cells in the global Simulation, in this
    // dimension
    // \param ntasks Total number of tasks in this dimension
    // \param my_n This task's position in this dimension
    // \return Number of cells for this task's local domain (actual cells, not counting ghost cells)
    // clang-format on
    static FsIndex_t calcLocalSize(FsSize_t globalCells, Task_t ntasks,
                                   Task_t my_n) {
        FsIndex_t n_per_task = globalCells / ntasks;
        FsIndex_t remainder = globalCells % ntasks;
        if (my_n < remainder) {
            return n_per_task + 1;
        } else {
            return n_per_task;
        }
    }

    //! Helper function to optimize decomposition of this grid over the given
    //! number of tasks
    static void computeDomainDecomposition(
        const std::array<FsSize_t, 3> &GlobalSize, Task_t nProcs,
        std::array<Task_t, 3> &processDomainDecomposition, int stencilSize = 1,
        int verbose = 0) {
        // Juhana TODO:
        // - should return the array instead of take the return value
        // as the third argument
        // - should return a struct called Decomposition instead of an array
        int myRank, MPI_flag;
        MPI_Initialized(&MPI_flag);
        if (MPI_flag) {
            MPI_Comm_rank(MPI_COMM_WORLD,
                          &myRank); // TODO allow for separate communicator
        } else {
            myRank = FS_MASTER_RANK;
        }

        std::array<FsSize_t, 3> systemDim;
        std::array<FsSize_t, 3> processBox;
        std::array<FsSize_t, 3> minDomainSize;
        int64_t optimValue = std::numeric_limits<int64_t>::max();

        std::vector<std::pair<int64_t, std::array<Task_t, 3>>>
            scored_decompositions;

        scored_decompositions.push_back(
            std::pair<int64_t, std::array<Task_t, 3>>(optimValue, {0, 0, 0}));

        for (int i = 0; i < 3; i++) {
            systemDim[i] = GlobalSize[i];
            if (GlobalSize[i] == 1) {
                // In 2D simulation domains, the "thin" dimension can be a
                // single cell thick.
                minDomainSize[i] = 1;
            } else {
                // Otherwise, it needs to be at least as large as our ghost
                // stencil, so that ghost communication remains consistent.
                minDomainSize[i] = stencilSize;
            }
        }

        processDomainDecomposition = {1, 1, 1};
        for (Task_t i = 1;
             i <= std::min(nProcs, (Task_t)(GlobalSize[0] / minDomainSize[0]));
             i++) {
            for (Task_t j = 1;
                 j <=
                 std::min(nProcs, (Task_t)(GlobalSize[1] / minDomainSize[1]));
                 j++) {
                if (i * j > nProcs) {
                    break;
                }
                Task_t k = nProcs / (i * j);
                // No need to optimize an incompatible DD, also checks for
                // missing remainders
                if (i * j * k != nProcs) {
                    continue;
                }
                if (k > (Task_t)(GlobalSize[2] / minDomainSize[2])) {
                    continue;
                }

                processBox[0] = calcLocalSize(systemDim[0], i, 0);
                processBox[1] = calcLocalSize(systemDim[1], j, 0);
                processBox[2] = calcLocalSize(systemDim[2], k, 0);

                int64_t value = (i > 1 ? processBox[1] * processBox[2] : 0) +
                                (j > 1 ? processBox[0] * processBox[2] : 0) +
                                (k > 1 ? processBox[0] * processBox[1] : 0);

                // account for singular domains
                if (i != 1 && j != 1 && k != 1) {
                    value *= 13; // 26 neighbours to communicate to
                }
                if (i == 1 && j != 1 && k != 1) {
                    value *= 4; // 8 neighbours to communicate to
                }
                if (i != 1 && j == 1 && k != 1) {
                    value *= 4; // 8 neighbours to communicate to
                }
                if (i != 1 && j != 1 && k == 1) {
                    value *= 4; // 8 neighbours to communicate to
                }
                // else: 2 neighbours to communicate to, no need to adjust

                if (value <= optimValue) {
                    optimValue = value;
                    if (value < scored_decompositions.back().first) {
                        scored_decompositions.clear();
                    }
                    scored_decompositions.push_back(
                        std::pair<int64_t, std::array<Task_t, 3>>(value,
                                                                  {i, j, k}));
                }
            }
        }

        if (myRank == FS_MASTER_RANK && verbose) {
            std::cout << "(FSGRID) Number of equal minimal-surface "
                         "decompositions found: "
                      << scored_decompositions.size() << "\n";
            for (auto kv : scored_decompositions) {
                std::cout << "(FSGRID) Decomposition " << kv.second[0] << ","
                          << kv.second[1] << "," << kv.second[2] << " "
                          << " for processBox size "
                          << systemDim[0] / kv.second[0] << " "
                          << systemDim[1] / kv.second[1] << " "
                          << systemDim[2] / kv.second[2] << "\n";
            }
        }

        // Taking the first scored_decomposition (smallest X decomposition)
        processDomainDecomposition[0] = scored_decompositions[0].second[0];
        processDomainDecomposition[1] = scored_decompositions[0].second[1];
        processDomainDecomposition[2] = scored_decompositions[0].second[2];

        if (optimValue == std::numeric_limits<int64_t>::max() ||
            (Task_t)(processDomainDecomposition[0] *
                     processDomainDecomposition[1] *
                     processDomainDecomposition[2]) != nProcs) {
            if (myRank == 0) {
                std::cerr << "(FSGRID) Domain decomposition failed, are you "
                             "running on a prime number of tasks?"
                          << std::endl;
            }
            throw std::runtime_error(
                "FSGrid computeDomainDecomposition failed");
        }
        if (myRank == 0 && verbose) {
            std::cout << "(FSGRID) decomposition chosen as "
                      << processDomainDecomposition[0] << " "
                      << processDomainDecomposition[1] << " "
                      << processDomainDecomposition[2]
                      << ", for processBox sizes "
                      << systemDim[0] / processDomainDecomposition[0] << " "
                      << systemDim[1] / processDomainDecomposition[1] << " "
                      << systemDim[2] / processDomainDecomposition[2] << " \n";
        }
    }
};

struct FsGridCouplingInformation {
    //!< MPI rank that each cell is being communicated to externally
    std::vector<FsGridTools::Task_t> externalRank;

    void setCouplingSize(size_t totalStorageSize) {
        // Set up coupling information to external grid (fill with MPI_PROC_NULL
        // to begin with) Before actual grid coupling can be done, this
        // information has to be filled in.
        if (externalRank.size() != totalStorageSize) {
            externalRank.resize(totalStorageSize);
        }
        for (size_t i = 0; i < externalRank.size(); i++) {
            externalRank[i] = MPI_PROC_NULL;
        }
    }
};

// clang-format off
/*! Simple cartesian, non-loadbalancing MPI Grid for use with the fieldsolver
 *
 * \param T datastructure containing the field in each cell which this grid
 * manages
 * \param stencil ghost cell width of this grid
 */
// clang-format on
template <typename T, int stencil> class FsGrid : public FsGridTools {
    template <typename ArrayT> void swapArray(std::array<ArrayT, 3> &array) {
        ArrayT a = array[0];
        array[0] = array[2];
        array[2] = a;
    }

  public:
    // Legacy constructor from coupling reference
    FsGrid(std::array<FsSize_t, 3> globalSize, MPI_Comm parent_comm,
           std::array<bool, 3> isPeriodic, FsGridCouplingInformation &coupling,
           const std::array<Task_t, 3> &decomposition = {0, 0, 0},
           bool verbose = false)
        : FsGrid(globalSize, parent_comm, isPeriodic, &coupling, decomposition,
                 verbose) {}

    // clang-format off
    /*! Constructor for this grid.
     * \param globalSize Cell size of the global simulation domain.
     * \param MPI_Comm The MPI communicator this grid should use.
     * \param isPeriodic An array specifying, for each dimension, whether it is
     * to be treated as periodic.
     */
    // clang-format on
    FsGrid(std::array<FsSize_t, 3> globalSize, MPI_Comm parent_comm,
           std::array<bool, 3> isPeriodic, FsGridCouplingInformation *coupling,
           const std::array<Task_t, 3> &decomposition = {0, 0, 0},
           bool verbose = false)
        : globalSize(globalSize), coupling(coupling) {
        int status;
        int size;

        // Get parent_comm info
        int parentRank;
        MPI_Comm_rank(parent_comm, &parentRank);
        int parentSize;
        MPI_Comm_size(parent_comm, &parentSize);

        // If environment variable FSGRID_PROCS is set,
        // use that for determining the number of FS-processes
        size = parentSize;
        if (getenv("FSGRID_PROCS") != NULL) {
            const int fsgridProcs = atoi(getenv("FSGRID_PROCS"));
            if (fsgridProcs > 0 && fsgridProcs < size)
                size = fsgridProcs;
        }

        std::array<Task_t, 3> emptyarr = {0, 0, 0};
        if (decomposition == emptyarr) {
            // If decomposition isn't pre-defined, heuristically choose a good
            // domain decomposition for our field size
            computeDomainDecomposition(globalSize, size, ntasksPerDim, stencil,
                                       verbose);
        } else {
            ntasksPerDim = decomposition;
            if (ntasksPerDim[0] * ntasksPerDim[1] * ntasksPerDim[2] != size) {
                std::cerr
                    << "Given decomposition (" << ntasksPerDim[0] << " "
                    << ntasksPerDim[1] << " " << ntasksPerDim[2]
                    << ") does not distribute to the number of tasks given"
                    << std::endl;
                throw std::runtime_error(
                    "Given decomposition does not distribute to the number of "
                    "tasks given");
            }
            ntasksPerDim[0] = decomposition[0];
            ntasksPerDim[1] = decomposition[1];
            ntasksPerDim[2] = decomposition[2];
        }

        // set private array
        periodic = isPeriodic;
        // set temporary int arrays for MPI_Cart_create
        std::array<int, 3> isPeriodicInt, ntasksInt;
        for (unsigned int i = 0; i < isPeriodic.size(); i++) {
            isPeriodicInt[i] = (int)isPeriodic[i];
            ntasksInt[i] = (int)ntasksPerDim[i];
        }

        // Create a temporary FS subcommunicator for the MPI_Cart_create
        int colorFs = (parentRank < size) ? 1 : MPI_UNDEFINED;
        MPI_Comm_split(parent_comm, colorFs, parentRank, &comm1d);

        if (colorFs != MPI_UNDEFINED) {
            // Create cartesian communicator. Note, that reorder is false so
            // ranks match the ones in parent_comm
            status = MPI_Cart_create(comm1d, 3, ntasksPerDim.data(),
                                     isPeriodicInt.data(), 0, &comm3d);

            if (status != MPI_SUCCESS) {
                std::cerr << "Creating cartesian communicatior failed when "
                             "attempting to create FsGrid!"
                          << std::endl;
                throw std::runtime_error("FSGrid communicator setup failed");
            }

            status = MPI_Comm_rank(comm3d, &rank);
            if (status != MPI_SUCCESS) {
                std::cerr
                    << "Getting rank failed when attempting to create FsGrid!"
                    << std::endl;

                // Without a rank, there's really not much we can do. Just
                // return an uninitialized grid (I suppose we'll crash after
                // this, anyway)
                return;
            }

            // Determine our position in the resulting task-grid
            status = MPI_Cart_coords(comm3d, rank, 3, taskPosition.data());
            if (status != MPI_SUCCESS) {
                std::cerr << "Rank " << rank
                          << " unable to determine own position in cartesian "
                             "communicator when attempting to create FsGrid!"
                          << std::endl;
            }
        }

        // Create a temporary aux subcommunicator for the (Aux) MPI_Cart_create
        int colorAux = (parentRank > (parentSize - 1) % size)
                           ? (parentRank - (parentSize % size)) / size
                           : MPI_UNDEFINED;
        MPI_Comm_split(parent_comm, colorAux, parentRank, &comm1d_aux);

        int rankAux;
        std::array<int, 3> taskPositionAux;

        if (colorAux != MPI_UNDEFINED) {
            // Create an aux cartesian communicator corresponding to the comm3d
            // (but shidted).
            status = MPI_Cart_create(comm1d_aux, 3, ntasksPerDim.data(),
                                     isPeriodicInt.data(), 0, &comm3d_aux);
            if (status != MPI_SUCCESS) {
                std::cerr << "Creating cartesian communicatior failed when "
                             "attempting to create FsGrid!"
                          << std::endl;
                throw std::runtime_error("FSGrid communicator setup failed");
            }
            status = MPI_Comm_rank(comm3d_aux, &rankAux);
            if (status != MPI_SUCCESS) {
                std::cerr
                    << "Getting rank failed when attempting to create FsGrid!"
                    << std::endl;
                return;
            }
            // Determine our position in the resulting task-grid
            status =
                MPI_Cart_coords(comm3d_aux, rankAux, 3, taskPositionAux.data());
            if (status != MPI_SUCCESS) {
                std::cerr << "Rank " << rankAux
                          << " unable to determine own position in cartesian "
                             "communicator when attempting to create FsGrid!"
                          << std::endl;
            }
        }

#ifdef FSGRID_DEBUG
        // All FS ranks send their true comm3d rank and taskPosition data to
        // dest
        MPI_Request *request = new MPI_Request[(parentSize - 1) / size * 2 + 2];
        for (int i = 0; i < (parentSize - 1) / size; i++) {
            int dest = (colorFs != MPI_UNDEFINED)
                           ? parentRank + i * size + (parentSize - 1) % size + 1
                           : MPI_PROC_NULL;
            if (dest >= parentSize)
                dest = MPI_PROC_NULL;
            MPI_Isend(&rank, 1, MPI_INT, dest, 9274, parent_comm,
                      &request[2 * i]);
            MPI_Isend(taskPosition.data(), 3, MPI_INT, dest, 9275, parent_comm,
                      &request[2 * i + 1]);
        }

        // All Aux ranks receive the true comm3d rank and taskPosition data from
        // source and then compare that it matches their aux data
        std::array<int, 3> taskPositionRecv;
        int rankRecv;
        int source =
            (colorAux != MPI_UNDEFINED)
                ? parentRank -
                      (parentRank - (parentSize % size)) / size * size -
                      parentSize % size
                : MPI_PROC_NULL;

        MPI_Irecv(&rankRecv, 1, MPI_INT, source, 9274, parent_comm,
                  &request[(parentSize - 1) / size * 2]);
        MPI_Irecv(taskPositionRecv.data(), 3, MPI_INT, source, 9275,
                  parent_comm, &request[(parentSize - 1) / size * 2 + 1]);
        MPI_Waitall((parentSize - 1) / size * 2 + 2, request,
                    MPI_STATUS_IGNORE);

        if (colorAux != MPI_UNDEFINED) {
            if (rankRecv != rankAux ||
                taskPositionRecv[0] != taskPositionAux[0] ||
                taskPositionRecv[1] != taskPositionAux[1] ||
                taskPositionRecv[2] != taskPositionAux[2]) {
                std::cerr << "Rank: " << parentRank
                          << ". Aux cartesian communicator 'comm3d_aux' does "
                             "not match with 'comm3d' !"
                          << std::endl;
                throw std::runtime_error(
                    "FSGrid aux communicator setup failed.");
            }
        }
        delete[] request;
#endif // FSGRID_DEBUG

        // Set correct task position for non-FS ranks
        if (colorFs == MPI_UNDEFINED) {
            for (int i = 0; i < 3; i++) {
                taskPosition[i] = taskPositionAux[i];
            }
        }

        // Determine size of our local grid
        for (int i = 0; i < 3; i++) {
            localSize[i] =
                calcLocalSize(globalSize[i], ntasksPerDim[i], taskPosition[i]);
            localStart[i] =
                calcLocalStart(globalSize[i], ntasksPerDim[i], taskPosition[i]);
        }

        if (localSize[0] == 0 ||
            (globalSize[0] > stencil && localSize[0] < stencil) ||
            localSize[1] == 0 ||
            (globalSize[1] > stencil && localSize[1] < stencil) ||
            localSize[2] == 0 ||
            (globalSize[2] > stencil && localSize[2] < stencil)) {
            std::cerr << "FSGrid space partitioning leads to a space that is "
                         "too small on Rank "
                      << rank << "." << std::endl;
            std::cerr << "Please run with a different number of Tasks, so that "
                         "space is better divisible."
                      << std::endl;
            throw std::runtime_error("FSGrid too small domains");
        }

        // If non-FS process, set rank to -1 and localSize to zero and return
        if (colorFs == MPI_UNDEFINED) {
            rank = -1;
            localSize[0] = 0;
            localSize[1] = 0;
            localSize[2] = 0;
            comm3d = comm3d_aux;
            comm3d_aux = MPI_COMM_NULL;
            return;
        }

        // Allocate the array of neighbours
        for (int i = 0; i < size; i++) {
            neighbour_index.push_back(MPI_PROC_NULL);
        }
        for (int i = 0; i < 27; i++) {
            neighbour[i] = MPI_PROC_NULL;
        }

        // Get the IDs of the 26 direct neighbours
        for (int x = -1; x <= 1; x++) {
            for (int y = -1; y <= 1; y++) {
                for (int z = -1; z <= 1; z++) {
                    std::array<Task_t, 3> neighPosition;

                    /*
                     * Figure out the coordinates of the neighbours in all three
                     * directions
                     */
                    neighPosition[0] = taskPosition[0] + x;
                    if (isPeriodic[0]) {
                        neighPosition[0] += ntasksPerDim[0];
                        neighPosition[0] %= ntasksPerDim[0];
                    }

                    neighPosition[1] = taskPosition[1] + y;
                    if (isPeriodic[1]) {
                        neighPosition[1] += ntasksPerDim[1];
                        neighPosition[1] %= ntasksPerDim[1];
                    }

                    neighPosition[2] = taskPosition[2] + z;
                    if (isPeriodic[2]) {
                        neighPosition[2] += ntasksPerDim[2];
                        neighPosition[2] %= ntasksPerDim[2];
                    }

                    /*
                     * If those coordinates exist, figure out the responsible
                     * CPU and store its rank
                     */
                    if (neighPosition[0] >= 0 &&
                        neighPosition[0] < ntasksPerDim[0] &&
                        neighPosition[1] >= 0 &&
                        neighPosition[1] < ntasksPerDim[1] &&
                        neighPosition[2] >= 0 &&
                        neighPosition[2] < ntasksPerDim[2]) {

                        // Calculate the rank
                        int neighRank;
                        status = MPI_Cart_rank(comm3d, neighPosition.data(),
                                               &neighRank);
                        if (status != MPI_SUCCESS) {
                            std::cerr << "Rank " << rank
                                      << " can't determine neighbour rank at "
                                         "position [";
                            for (int i = 0; i < 3; i++) {
                                std::cerr << neighPosition[i] << ", ";
                            }
                            std::cerr << "]" << std::endl;
                        }

                        // Forward lookup table
                        neighbour[(x + 1) * 9 + (y + 1) * 3 + (z + 1)] =
                            neighRank;

                        // Reverse lookup table
                        if (neighRank >= 0 && neighRank < size) {
                            neighbour_index[neighRank] =
                                (char)((x + 1) * 9 + (y + 1) * 3 + (z + 1));
                        }
                    } else {
                        neighbour[(x + 1) * 9 + (y + 1) * 3 + (z + 1)] =
                            MPI_PROC_NULL;
                    }
                }
            }
        }

        // Allocate local storage array
        size_t totalStorageSize = 1;
        for (int i = 0; i < 3; i++) {
            if (globalSize[i] <= 1) {
                // Collapsed dimension => only one cell thick
                storageSize[i] = 1;
            } else {
                // Size of the local domain + 2* size for the ghost cell stencil
                storageSize[i] = (localSize[i] + stencil * 2);
            }
            totalStorageSize *= storageSize[i];
        }
        data.resize(totalStorageSize);
        coupling->setCouplingSize(totalStorageSize);

        MPI_Datatype mpiTypeT;
        MPI_Type_contiguous(sizeof(T), MPI_BYTE, &mpiTypeT);
        for (int x = -1; x <= 1; x++) {
            for (int y = -1; y <= 1; y++) {
                for (int z = -1; z <= 1; z++) {
                    neighbourSendType[(x + 1) * 9 + (y + 1) * 3 + (z + 1)] =
                        MPI_DATATYPE_NULL;
                    neighbourReceiveType[(x + 1) * 9 + (y + 1) * 3 + (z + 1)] =
                        MPI_DATATYPE_NULL;
                }
            }
        }

        // Compute send and receive datatypes

        // loop through the shifts in the different directions
        for (int x = -1; x <= 1; x++) {
            for (int y = -1; y <= 1; y++) {
                for (int z = -1; z <= 1; z++) {
                    std::array<int, 3> subarraySize;
                    std::array<int, 3> subarrayStart;
                    const int shiftId = (x + 1) * 9 + (y + 1) * 3 + (z + 1);

                    if ((storageSize[0] == 1 && x != 0) ||
                        (storageSize[1] == 1 && y != 0) ||
                        (storageSize[2] == 1 && z != 0) ||
                        (x == 0 && y == 0 && z == 0)) {
                        // skip flat dimension for 2 or 1D simulations, and self
                        neighbourSendType[shiftId] = MPI_DATATYPE_NULL;
                        neighbourReceiveType[shiftId] = MPI_DATATYPE_NULL;
                        continue;
                    }

                    subarraySize[0] = (x == 0) ? localSize[0] : stencil;
                    subarraySize[1] = (y == 0) ? localSize[1] : stencil;
                    subarraySize[2] = (z == 0) ? localSize[2] : stencil;

                    if (x == 0 || x == -1)
                        subarrayStart[0] = stencil;
                    else if (x == 1)
                        subarrayStart[0] = storageSize[0] - 2 * stencil;
                    if (y == 0 || y == -1)
                        subarrayStart[1] = stencil;
                    else if (y == 1)
                        subarrayStart[1] = storageSize[1] - 2 * stencil;
                    if (z == 0 || z == -1)
                        subarrayStart[2] = stencil;
                    else if (z == 1)
                        subarrayStart[2] = storageSize[2] - 2 * stencil;

                    for (int i = 0; i < 3; i++)
                        if (storageSize[i] == 1)
                            subarrayStart[i] = 0;

                    std::array<int, 3> swappedStorageSize = {
                        (int)storageSize[0], (int)storageSize[1],
                        (int)storageSize[2]};
                    swapArray(swappedStorageSize);
                    swapArray(subarraySize);
                    swapArray(subarrayStart);
                    MPI_Type_create_subarray(
                        3, swappedStorageSize.data(), subarraySize.data(),
                        subarrayStart.data(), MPI_ORDER_C, mpiTypeT,
                        &(neighbourSendType[shiftId]));

                    if (x == 1)
                        subarrayStart[0] = 0;
                    else if (x == 0)
                        subarrayStart[0] = stencil;
                    else if (x == -1)
                        subarrayStart[0] = storageSize[0] - stencil;
                    if (y == 1)
                        subarrayStart[1] = 0;
                    else if (y == 0)
                        subarrayStart[1] = stencil;
                    else if (y == -1)
                        subarrayStart[1] = storageSize[1] - stencil;
                    if (z == 1)
                        subarrayStart[2] = 0;
                    else if (z == 0)
                        subarrayStart[2] = stencil;
                    else if (z == -1)
                        subarrayStart[2] = storageSize[2] - stencil;
                    for (int i = 0; i < 3; i++)
                        if (storageSize[i] == 1)
                            subarrayStart[i] = 0;

                    swapArray(subarrayStart);
                    MPI_Type_create_subarray(
                        3, swappedStorageSize.data(), subarraySize.data(),
                        subarrayStart.data(), MPI_ORDER_C, mpiTypeT,
                        &(neighbourReceiveType[shiftId]));
                }
            }
        }

        for (int i = 0; i < 27; i++) {
            if (neighbourReceiveType[i] != MPI_DATATYPE_NULL)
                MPI_Type_commit(&(neighbourReceiveType[i]));
            if (neighbourSendType[i] != MPI_DATATYPE_NULL)
                MPI_Type_commit(&(neighbourSendType[i]));
        }
    }

    std::vector<T> &getData() { return this->data; }

    void copyData(FsGrid &other) {
        this->data = other.getData(); // Copy assignment
    }

    /*! Finalize instead of destructor, as the MPI calls fail after the main
     * program called MPI_Finalize(). Cleans up the cartesian communicator
     */
    void finalize() {
        // If not a non-FS process
        if (rank != -1) {
            for (int i = 0; i < 27; i++) {
                if (neighbourReceiveType[i] != MPI_DATATYPE_NULL)
                    MPI_Type_free(&(neighbourReceiveType[i]));
                if (neighbourSendType[i] != MPI_DATATYPE_NULL)
                    MPI_Type_free(&(neighbourSendType[i]));
            }
        }

        if (comm3d != MPI_COMM_NULL)
            MPI_Comm_free(&comm3d);
        if (comm3d_aux != MPI_COMM_NULL)
            MPI_Comm_free(&comm3d_aux);
        if (comm1d != MPI_COMM_NULL)
            MPI_Comm_free(&comm1d);
        if (comm1d_aux != MPI_COMM_NULL)
            MPI_Comm_free(&comm1d_aux);
    }

    // clang-format off
    /*! Returns the task responsible, and its localID for handling the cell with
     * the given GlobalID
     * \param id GlobalID of the cell for which task is to be determined
     * \return a task for the grid's cartesian communicator
     */
    // clang-format on
    std::pair<int, LocalID> getTaskForGlobalID(GlobalID id) {
        // Transform globalID to global cell coordinate
        std::array<FsIndex_t, 3> cell =
            FsGridTools::globalIDtoCellCoord(id, globalSize);

        // Find the index in the task grid this Cell belongs to
        std::array<int, 3> taskIndex;
        for (int i = 0; i < 3; i++) {
            int n_per_task = globalSize[i] / ntasksPerDim[i];
            int remainder = globalSize[i] % ntasksPerDim[i];

            if (cell[i] < remainder * (n_per_task + 1)) {
                taskIndex[i] = cell[i] / (n_per_task + 1);
            } else {
                taskIndex[i] =
                    remainder +
                    (cell[i] - remainder * (n_per_task + 1)) / n_per_task;
            }
        }

        // Get the task number from the communicator
        std::pair<int, LocalID> retVal;
        int status = MPI_Cart_rank(comm3d, taskIndex.data(), &retVal.first);
        if (status != MPI_SUCCESS) {
            std::cerr << "Unable to find FsGrid rank for global ID " << id
                      << " (coordinates [";
            for (int i = 0; i < 3; i++) {
                std::cerr << cell[i] << ", ";
            }
            std::cerr << "]" << std::endl;
            return std::pair<int, LocalID>(MPI_PROC_NULL, 0);
        }

        // Determine localID of that cell within the target task
        std::array<FsIndex_t, 3> thatTasksStart;
        std::array<FsIndex_t, 3> thatTaskStorageSize;
        for (int i = 0; i < 3; i++) {
            thatTasksStart[i] =
                calcLocalStart(globalSize[i], ntasksPerDim[i], taskIndex[i]);
            thatTaskStorageSize[i] =
                calcLocalSize(globalSize[i], ntasksPerDim[i], taskIndex[i]) +
                2 * stencil;
        }

        retVal.second = 0;
        int stride = 1;
        for (int i = 0; i < 3; i++) {
            if (globalSize[i] <= 1) {
                // Collapsed dimension, doesn't contribute.
                retVal.second += 0;
            } else {
                retVal.second +=
                    stride * (cell[i] - thatTasksStart[i] + stencil);
                stride *= thatTaskStorageSize[i];
            }
        }

        return retVal;
    }

    // clang-format off
    /*! Transform global cell coordinates into the local domain.
     * If the coordinates are out of bounds, (-1,-1,-1) is returned.
     * \param x The cell's global x coordinate
     * \param y The cell's global y coordinate
     * \param z The cell's global z coordinate
     */
    // clang-format on
    std::array<FsIndex_t, 3> globalToLocal(FsSize_t x, FsSize_t y, FsSize_t z) {
        std::array<FsIndex_t, 3> retval;
        retval[0] = (FsIndex_t)x - localStart[0];
        retval[1] = (FsIndex_t)y - localStart[1];
        retval[2] = (FsIndex_t)z - localStart[2];

        if (retval[0] >= localSize[0] || retval[1] >= localSize[1] ||
            retval[2] >= localSize[2] || retval[0] < 0 || retval[1] < 0 ||
            retval[2] < 0) {
            return {-1, -1, -1};
        }

        return retval;
    }

    // clang-format off
    /*! Determine the cell's GlobalID from its local x,y,z coordinates
     * \param x The cell's task-local x coordinate
     * \param y The cell's task-local y coordinate
     * \param z The cell's task-local z coordinate
     */
    // clang-format on
    GlobalID GlobalIDForCoords(int x, int y, int z) {
        return x + localStart[0] + globalSize[0] * (y + localStart[1]) +
               globalSize[0] * globalSize[1] * (z + localStart[2]);
    }

    // clang-format off
    /*! Determine the cell's LocalID from its local x,y,z coordinates
     * \param x The cell's task-local x coordinate
     * \param y The cell's task-local y coordinate
     * \param z The cell's task-local z coordinate
     */
    // clang-format on
    LocalID LocalIDForCoords(int x, int y, int z) {
        LocalID index = 0;
        if (globalSize[2] > 1) {
            index += storageSize[0] * storageSize[1] * (stencil + z);
        }
        if (globalSize[1] > 1) {
            index += storageSize[0] * (stencil + y);
        }
        if (globalSize[0] > 1) {
            index += stencil + x;
        }

        return index;
    }

    // clang-format off
    /*! Prepare for transfer of Rank information from a coupled grid.
     * Setup MPI Irecv requests to recieve data for each grid cell, and prepare
     * buffer space to hold the MPI requests of the sends.
     *
     * \param cellsToCouple How many cells are going to be coupled
     */
    // clang-format on
    void setupForGridCoupling(int cellsToCouple) {
        int status;
        // Make sure we have sufficient buffer space to store our mpi
        // requests. Here only for receives, sends are done with blocking
        // routine.
        requests.resize(localSize[0] * localSize[1] * localSize[2]);
        numRequests = 0;

        // If previous coupling information was present, remove it.
        for (uint i = 0; i < coupling->externalRank.size(); i++) {
            coupling->externalRank[i] = MPI_PROC_NULL;
        }

        for (FsIndex_t z = 0; z < localSize[2]; z++) {
            for (FsIndex_t y = 0; y < localSize[1]; y++) {
                for (FsIndex_t x = 0; x < localSize[0]; x++) {
                    // Calculate LocalID for this cell
                    LocalID thisCell = LocalIDForCoords(x, y, z);
                    assert(numRequests < requests.size());
                    assert(thisCell < (int64_t)coupling->externalRank.size());
                    status = MPI_Irecv(&coupling->externalRank[thisCell], 1,
                                       MPI_INT, MPI_ANY_SOURCE, thisCell,
                                       comm3d, &requests[numRequests++]);
                    if (status != MPI_SUCCESS) {
                        std::cerr << "Error setting up MPI Irecv in "
                                     "FsGrid::setupForGridCoupling"
                                  << std::endl;
                    }
                }
            }
        }
    }

    // clang-format off
    /*! Set the MPI rank responsible for external communication with the given
     * cell. If, for example, a separate grid is used in another part of the
     * code, this would be the rank in that grid, responsible for the
     * information in this cell.
     *
     * This only needs to be called if the association changes (such as: on
     * startup, and in a load balancing step)
     *
     * \param id Global cell ID to be addressed
     * \iparam cellRank Rank that owns the cell in the other external grid.
     */
    // clang-format on
    void setGridCoupling(GlobalID id, int cellRank) {
        // Determine Task and localID that this cell belongs to
        std::pair<int, LocalID> TaskLid = getTaskForGlobalID(id);
        int status;
        status = MPI_Send(&cellRank, 1, MPI_INT, TaskLid.first, TaskLid.second,
                          comm3d);
        if (status != MPI_SUCCESS) {
            std::cerr << "Error setting up MPI Isend in FsGrid::setGridCoupling"
                      << std::endl;
        }
    }

    /*! Called after setting up the transfers into or out of this grid.
     * Basically only does a MPI_Waitall for all requests.
     */
    void finishGridCoupling() {
        assert(numRequests == requests.size());
        MPI_Waitall(numRequests, requests.data(), MPI_STATUSES_IGNORE);
        numRequests = 0;
        MPI_Barrier(comm3d);
    }

    // clang-format off
    /*! Prepare for transfer of grid cell data into this grid.
     * Setup MPI Irecv requests to recieve data for each grid cell, and prepare
     * buffer space to hold the MPI requests of the sends.
     *
     * \param cellsToSend How many cells are going to be sent by this task
     */
    // clang-format on
    void setupForTransferIn(int cellsToSend) {
        int status;
        // Make sure we have sufficient buffer space to store our mpi requests
        requests.resize(localSize[0] * localSize[1] * localSize[2] +
                        cellsToSend);
        numRequests = 0;

        for (FsIndex_t z = 0; z < localSize[2]; z++) {
            for (FsIndex_t y = 0; y < localSize[1]; y++) {
                for (FsIndex_t x = 0; x < localSize[0]; x++) {
                    // Calculate LocalID for this cell
                    LocalID thisCell = LocalIDForCoords(x, y, z);
                    assert(numRequests < requests.size());
                    status =
                        MPI_Irecv(get(thisCell), sizeof(T), MPI_BYTE,
                                  coupling->externalRank[thisCell], thisCell,
                                  comm3d, &requests[numRequests++]);
                    if (status != MPI_SUCCESS) {
                        std::cerr << "Error setting up MPI Irecv in "
                                     "FsGrid::setupForTransferIn"
                                  << std::endl;
                    }
                }
            }
        }
    }

    // clang-format off
    /*! Set grid cell wtih the given ID to the value specified. Note that this
     * doesn't need to be a local cell. Note that this function should be
     * concurrently called by all tasks, to allow for point-to-point
     * communication.
     * \param id Global cell ID to be filled
     * \iparam value New value to be assigned to it
     */
    // clang-format on
    void transferDataIn(GlobalID id, T *value) {

        // Determine Task and localID that this cell belongs to
        std::pair<int, LocalID> TaskLid = getTaskForGlobalID(id);

        // Build the MPI Isend request for this cell
        int status;
        assert(numRequests < requests.size());
        status = MPI_Isend(value, sizeof(T), MPI_BYTE, TaskLid.first,
                           TaskLid.second, comm3d, &requests[numRequests++]);
        if (status != MPI_SUCCESS) {
            std::cerr << "Error setting up MPI Isend in FsGrid::transferDataIn"
                      << std::endl;
        }
    }

    /*! Called after setting up the transfers into or out of this grid.
     * Basically only does a MPI_Waitall for all requests.
     */
    void finishTransfersIn() {
        assert(numRequests == requests.size());
        MPI_Waitall(numRequests, requests.data(), MPI_STATUSES_IGNORE);
        numRequests = 0;
    }

    // clang-format off
    /*! Prepare for transfer of grid cell data out of this grid.
     * Setup MPI Isend requests to send data for each grid cell, and prepare
     * buffer space to hold the MPI requests of the sends.
     *
     * \param cellsToSend How many cells are going to be sent by this task
     */
    // clang-format on
    void setupForTransferOut(int cellsToReceive) {
        // Make sure we have sufficient buffer space to store our mpi requests
        requests.resize(localSize[0] * localSize[1] * localSize[2] +
                        cellsToReceive);
        numRequests = 0;
    }

    // clang-format off
    /*! Get the value of the grid cell wtih the given ID. Note that this doesn't
     * need to be a local cell.
     * \param id Global cell ID to be read
     * \iparam target Location that the result is to be stored in
     */
    // clang-format on
    void transferDataOut(GlobalID id, T *target) {

        // Determine Task and localID that this cell belongs to
        std::pair<int, LocalID> TaskLid = getTaskForGlobalID(id);

        // Build the MPI Irecv request for this cell
        int status;
        assert(numRequests < requests.size());
        status = MPI_Irecv(target, sizeof(T), MPI_BYTE, TaskLid.first,
                           TaskLid.second, comm3d, &requests[numRequests++]);
        if (status != MPI_SUCCESS) {
            std::cerr << "Error setting up MPI Irecv in FsGrid::transferDataOut"
                      << std::endl;
        }
    }

    // clang-format off
    /*! Finalize the transfer of data out of this grid, re-integrating it back
     *! into the locations where it came from.
     * This method assumes that transferDataOut() has been called for each cell
     * beforehand, so that appropriate recieve buffers exist in the target ranks.
     */
    // clang-format on
    void finishTransfersOut() {
        int status;
        for (FsIndex_t z = 0; z < localSize[2]; z++) {
            for (FsIndex_t y = 0; y < localSize[1]; y++) {
                for (FsIndex_t x = 0; x < localSize[0]; x++) {
                    // Calculate LocalID for this cell
                    LocalID thisCell = LocalIDForCoords(x, y, z);
                    assert(numRequests < requests.size());
                    status =
                        MPI_Isend(get(thisCell), sizeof(T), MPI_BYTE,
                                  coupling->externalRank[thisCell], thisCell,
                                  comm3d, &requests[numRequests++]);
                    if (status != MPI_SUCCESS) {
                        std::cerr << "Error setting up MPI Isend in "
                                     "FsGrid::setupForTransferOut"
                                  << std::endl;
                    }
                }
            }
        }
        assert(numRequests == requests.size());
        MPI_Waitall(numRequests, requests.data(), MPI_STATUSES_IGNORE);
        numRequests = 0;
    }

    /*! Perform ghost cell communication.
     */
    void updateGhostCells() {

        if (rank == -1)
            return;

        // TODO, faster with simultaneous isends& ireceives?
        std::array<MPI_Request, 27> receiveRequests;
        std::array<MPI_Request, 27> sendRequests;

        for (int i = 0; i < 27; i++) {
            receiveRequests[i] = MPI_REQUEST_NULL;
            sendRequests[i] = MPI_REQUEST_NULL;
        }

        for (int x = -1; x <= 1; x++) {
            for (int y = -1; y <= 1; y++) {
                for (int z = -1; z <= 1; z++) {
                    int shiftId = (x + 1) * 9 + (y + 1) * 3 + (z + 1);
                    int receiveId = (1 - x) * 9 + (1 - y) * 3 + (1 - z);
                    if (neighbour[receiveId] != MPI_PROC_NULL &&
                        neighbourSendType[shiftId] != MPI_DATATYPE_NULL) {
                        MPI_Irecv(data.data(), 1, neighbourReceiveType[shiftId],
                                  neighbour[receiveId], shiftId, comm3d,
                                  &(receiveRequests[shiftId]));
                    }
                }
            }
        }

        for (int x = -1; x <= 1; x++) {
            for (int y = -1; y <= 1; y++) {
                for (int z = -1; z <= 1; z++) {
                    int shiftId = (x + 1) * 9 + (y + 1) * 3 + (z + 1);
                    int sendId = shiftId;
                    if (neighbour[sendId] != MPI_PROC_NULL &&
                        neighbourSendType[shiftId] != MPI_DATATYPE_NULL) {
                        MPI_Isend(data.data(), 1, neighbourSendType[shiftId],
                                  neighbour[sendId], shiftId, comm3d,
                                  &(sendRequests[shiftId]));
                    }
                }
            }
        }
        MPI_Waitall(27, receiveRequests.data(), MPI_STATUSES_IGNORE);
        MPI_Waitall(27, sendRequests.data(), MPI_STATUSES_IGNORE);
    }

    /*! Get the size of the local domain handled by this grid.
     */
    std::array<FsIndex_t, 3> &getLocalSize() { return localSize; }

    /*! Get the start coordinates of the local domain handled by this grid.
     */
    std::array<FsIndex_t, 3> &getLocalStart() { return localStart; }

    /*! Get global size of the fsgrid domain
     */
    std::array<FsSize_t, 3> &getGlobalSize() { return globalSize; }

    // clang-format off
    /*! Calculate global cell position (XYZ in global cell space) from local
     * cell coordinates.
     *
     * \param x x-Coordinate, in cells
     * \param y y-Coordinate, in cells
     * \param z z-Coordinate, in cells
     *
     * \return Global cell coordinates
     */
    // clang-format on
    std::array<FsIndex_t, 3> getGlobalIndices(int64_t x, int64_t y, int64_t z) {
        std::array<FsIndex_t, 3> retval;
        retval[0] = localStart[0] + x;
        retval[1] = localStart[1] + y;
        retval[2] = localStart[2] + z;

        return retval;
    }

    // clang-format off
    /*! Get a reference to the field data in a cell
     * \param x x-Coordinate, in cells
     * \param y y-Coordinate, in cells
     * \param z z-Coordinate, in cells
     * \return A reference to cell data in the given cell
     */
    // clang-format on
    T *get(int x, int y, int z) {
        // Keep track which neighbour this cell actually belongs to (13 =
        // ourself)
        int isInNeighbourDomain = 13;
        int coord_shift[3] = {0, 0, 0};
        if (x < 0) {
            isInNeighbourDomain -= 9;
            coord_shift[0] = 1;
        }
        if (x >= localSize[0]) {
            isInNeighbourDomain += 9;
            coord_shift[0] = -1;
        }
        if (y < 0) {
            isInNeighbourDomain -= 3;
            coord_shift[1] = 1;
        }
        if (y >= localSize[1]) {
            isInNeighbourDomain += 3;
            coord_shift[1] = -1;
        }
        if (z < 0) {
            isInNeighbourDomain -= 1;
            coord_shift[2] = 1;
        }
        if (z >= localSize[2]) {
            isInNeighbourDomain += 1;
            coord_shift[2] = -1;
        }

        // Santiy-Check that the requested cell is actually inside our domain
        // TODO: ugh, this is ugly.
#ifdef FSGRID_DEBUG
        bool inside = true;
        if (localSize[0] <= 1 && !periodic[0]) {
            if (x != 0) {
                std::cerr
                    << "x != 0 despite non-periodic x-axis with only one cell."
                    << std::endl;
                inside = false;
            }
        } else {
            if (x < -stencil || x >= localSize[0] + stencil) {
                std::cerr << "x = " << x << " is outside of [ " << -stencil
                          << ", " << localSize[0] + stencil << "[!"
                          << std::endl;
                inside = false;
            }
        }

        if (localSize[1] <= 1 && !periodic[1]) {
            if (y != 0) {
                std::cerr
                    << "y != 0 despite non-periodic y-axis with only one cell."
                    << std::endl;
                inside = false;
            }
        } else {
            if (y < -stencil || y >= localSize[1] + stencil) {
                std::cerr << "y = " << y << " is outside of [ " << -stencil
                          << ", " << localSize[1] + stencil << "[!"
                          << std::endl;
                inside = false;
            }
        }

        if (localSize[2] <= 1 && !periodic[2]) {
            if (z != 0) {
                std::cerr
                    << "z != 0 despite non-periodic z-axis with only one cell."
                    << std::endl;
                inside = false;
            }
        } else {
            if (z < -stencil || z >= localSize[2] + stencil) {
                inside = false;
                std::cerr << "z = " << z << " is outside of [ " << -stencil
                          << ", " << localSize[2] + stencil << "[!"
                          << std::endl;
            }
        }
        if (!inside) {
            std::cerr
                << "Out-of bounds access in FsGrid::get! Expect weirdness."
                << std::endl;
            return NULL;
        }
#endif // FSGRID_DEBUG

        if (isInNeighbourDomain != 13) {
            // Check if the corresponding neighbour exists
            if (neighbour[isInNeighbourDomain] == MPI_PROC_NULL) {
                // Neighbour doesn't exist, we must be an outer boundary cell
                // (or something is quite wrong)
                return NULL;

            } else if (neighbour[isInNeighbourDomain] == rank) {
                // For periodic boundaries, where the neighbour is actually
                // ourself, return our own actual cell instead of the ghost
                x += coord_shift[0] * localSize[0];
                y += coord_shift[1] * localSize[1];
                z += coord_shift[2] * localSize[2];
            }
            // Otherwise we return the ghost cell
        }
        LocalID index = LocalIDForCoords(x, y, z);

        return &data[index];
    }

    T *get(LocalID id) {
        if (id < 0 || (unsigned int)id > data.size()) {
            std::cerr << "Out-of-bounds access in FsGrid::get!" << std::endl
                      << "(LocalID = " << id << ", but storage space is "
                      << data.size() << ". Expect weirdness." << std::endl;
            return NULL;
        }
        return &data[id];
    }

    /*! Physical grid spacing and physical coordinate space start.
     * TODO: Should this be private and have accesor-functions?
     */
    double DX, DY, DZ;
    std::array<double, 3> physicalGlobalStart;

    // clang-format off
    /*! Get the physical coordinates in the global simulation space for
     * the given cell.
     *
     * \param x local x-Coordinate, in cells
     * \param y local y-Coordinate, in cells
     * \param z local z-Coordinate, in cells
     */
    // clang-format on
    std::array<double, 3> getPhysicalCoords(int x, int y, int z) {
        std::array<double, 3> coords;
        coords[0] = physicalGlobalStart[0] + (localStart[0] + x) * DX;
        coords[1] = physicalGlobalStart[1] + (localStart[1] + y) * DY;
        coords[2] = physicalGlobalStart[2] + (localStart[2] + z) * DZ;

        return coords;
    }

    // clang-format off
    /*! Debugging output helper function. Allows for nicely formatted printing
     * of grid contents. Since the grid data format is varying, the actual
     * printing should be done in a lambda passed to this function. Example
     * usage to plot |B|:
     *
     * perBGrid.debugOutput([](const std::array<Real, fsgrids::bfield::N_BFIELD>& a) -> void {
     *     cerr << sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]) << ", ";
     * });
     *
     * \param func Function pointer (or lambda) which is called with a cell
     * reference, in order. Use std::cerr in it to print desired value.
     */
    // clang-format on
    void debugOutput(void(func)(const T &)) {
        int xmin = 0, xmax = 1;
        int ymin = 0, ymax = 1;
        int zmin = 0, zmax = 1;
        if (localSize[0] > 1) {
            xmin = -stencil;
            xmax = localSize[0] + stencil;
        }
        if (localSize[1] > 1) {
            ymin = -stencil;
            ymax = localSize[1] + stencil;
        }
        if (localSize[2] > 1) {
            zmin = -stencil;
            zmax = localSize[2] + stencil;
        }
        for (int z = zmin; z < zmax; z++) {
            for (int y = ymin; y < ymax; y++) {
                for (int x = xmin; x < xmax; x++) {
                    func(*get(x, y, z));
                }
                std::cerr << std::endl;
            }
            std::cerr << " - - - - - - - - " << std::endl;
        }
    }

    /*! Get the rank of this CPU in the FsGrid communicator */
    int getRank() { return rank; }

    /*! Get the number of ranks in the FsGrid communicator */
    int getSize() {
        return ntasksPerDim[0] * ntasksPerDim[1] * ntasksPerDim[2];
    }

    /*! Get in which directions, if any, this grid is periodic */
    std::array<bool, 3> &getPeriodic() { return periodic; }

    /*! Perform an MPI_Allreduce with this grid's internal communicator
     * Function syntax is identical to MPI_Allreduce, except the final
     * (communicator argument will not be needed) */
    int Allreduce(void *sendbuf, void *recvbuf, int count,
                  MPI_Datatype datatype, MPI_Op op) {

        // If a normal FS-rank
        if (rank != -1) {
            return MPI_Allreduce(sendbuf, recvbuf, count, datatype, op, comm3d);
        }
        // If a non-FS rank, no need to communicate
        else {
            int datatypeSize;
            MPI_Type_size(datatype, &datatypeSize);
            for (int i = 0; i < count * datatypeSize; ++i)
                ((char *)recvbuf)[i] = ((char *)sendbuf)[i];
            return MPI_ERR_RANK; // This is ok for a non-FS rank
        }
    }

    /*! Get the decomposition array*/
    std::array<Task_t, 3> &getDecomposition() { return ntasksPerDim; }

  private:
    //! MPI Cartesian communicator used in this grid
    MPI_Comm comm1d = MPI_COMM_NULL;
    MPI_Comm comm1d_aux = MPI_COMM_NULL;
    MPI_Comm comm3d = MPI_COMM_NULL;
    MPI_Comm comm3d_aux = MPI_COMM_NULL;
    int rank; //!< This task's rank in the communicator
    std::vector<MPI_Request> requests;
    uint numRequests;

    std::array<int, 27>
        neighbour; //!< Tasks of the 26 neighbours (plus ourselves)
    std::vector<char> neighbour_index; //!< Lookup table from rank to index in
                                       //!< the neighbour array

    // We have, fundamentally, two different coordinate systems we're dealing
    // with: 1) Task grid in the MPI_Cartcomm
    std::array<Task_t, 3> ntasksPerDim; //!< Number of tasks in each direction
    std::array<Task_t, 3>
        taskPosition; //!< This task's position in the 3d task grid
    // 2) Cell numbers in global and local view

    std::array<bool, 3>
        periodic; //!< Information about whether a given direction is periodic
    std::array<FsSize_t, 3>
        globalSize; //!< Global size of the simulation space, in cells
    std::array<FsIndex_t, 3>
        localSize; //!< Local size of simulation space handled by this task
                   //!< (without ghost cells)
    std::array<FsIndex_t, 3>
        storageSize; //!< Local size of simulation space handled by this task
                     //!< (including ghost cells)
    std::array<FsIndex_t, 3> localStart; //!< Offset of the local
                                         //! coordinate system against
                                         //! the global one

    FsGridCouplingInformation
        *coupling; // Information required to couple to external grids

    std::array<MPI_Datatype, 27>
        neighbourSendType; //!< Datatype for sending data
    std::array<MPI_Datatype, 27>
        neighbourReceiveType; //!< Datatype for receiving data

    //! Actual storage of field data
    std::vector<T> data;
};
