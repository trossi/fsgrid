#include "fsgrid.hpp"

#include <algorithm>
#include <gtest/gtest.h>
#include <iterator>
#include <numeric>

TEST(DataTransfer, send_data_to_processes_and_receive) {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Create a testgrid
    constexpr std::array<uint32_t, 3> globalSize{20, 20, 1};
    constexpr std::array<bool, 3> isPeriodic{false, false, true};
    FsGridCouplingInformation gridCoupling;
    FsGrid<int, 1> testGrid(globalSize, MPI_COMM_WORLD, isPeriodic,
                            gridCoupling);
    const std::array<int, 3> localSize = testGrid.getLocalSize();

    const FsGridTools::FsIndex_t z = 0;
    for (FsGridTools::FsIndex_t y = -1; y < localSize[1] + 1; y++) {
        for (FsGridTools::FsIndex_t x = -1; x < localSize[0] + 1; x++) {
            auto ptr = testGrid.get(x, y, z);
            if (ptr != NULL) {
                *ptr = rank;
            }
        }
    }

    testGrid.updateGhostCells();

    // First, setup grid coupling to do everything from task 0
    if (rank == 0) {
        testGrid.setupForGridCoupling(globalSize[0] * globalSize[1]);
        for (uint32_t y = 0; y < globalSize[1]; y++) {
            for (uint32_t x = 0; x < globalSize[0]; x++) {
                testGrid.setGridCoupling(y * globalSize[0] + x,
                                         0); // All cells are coupled to 0
            }
        }
    } else {
        testGrid.setupForGridCoupling(0);
    }
    testGrid.finishGridCoupling();

    const std::vector<int> fillData = [&globalSize]() {
        std::vector<int> d(globalSize[0] * globalSize[1]);
        std::iota(std::begin(d), std::end(d), 0);
        return d;
    }();

    if (rank == 0) {
        // We are going to send data to all Cells
        testGrid.setupForTransferIn(globalSize[0] * globalSize[1]);
        for (FsGridTools::FsSize_t y = 0; y < globalSize[1]; y++) {
            for (FsGridTools::FsSize_t x = 0; x < globalSize[0]; x++) {
                testGrid.transferDataIn(y * globalSize[0] + x,
                                        &fillData[y * globalSize[0] + x]);
            }
        }
    } else {
        // The others simply recieve
        testGrid.setupForTransferIn(0);
    }
    testGrid.finishTransfersIn();

    // Transfer it back
    std::vector<int> returnedData(globalSize[0] * globalSize[1]);
    if (rank == 0) {
        testGrid.setupForTransferOut(globalSize[0] * globalSize[1]);
        for (FsGridTools::FsSize_t y = 0; y < globalSize[1]; y++) {
            for (FsGridTools::FsSize_t x = 0; x < globalSize[0]; x++) {
                testGrid.transferDataOut(y * globalSize[0] + x,
                                         &returnedData[y * globalSize[0] + x]);
            }
        }
    } else {
        testGrid.setupForTransferOut(0);
    }
    testGrid.finishTransfersOut();

    // Validate the result
    if (rank == 0) {
        ASSERT_EQ(returnedData.size(), fillData.size())
            << "Returned and sent data are not of equal size";
        ASSERT_TRUE(std::equal(std::begin(returnedData), returnedData.end(),
                               fillData.begin(), fillData.end()))
            << "Sent and returned data are not equal";
    }
}
