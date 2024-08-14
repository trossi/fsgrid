#include "fsgrid.hpp"
#include <array>
#include <gtest/gtest.h>

TEST(DomainDecomposition, size_256_256_256_nprocs_32) {
    constexpr uint32_t n_procs = 32;
    constexpr std::array<FsGridTools::FsSize_t, 3> sys_size{256, 256, 256};

    std::array<FsGridTools::Task_t, 3> dd;
    FsGridTools::computeDomainDecomposition(sys_size, n_procs, dd, 1, false);

    ASSERT_EQ(dd[0], 1);
    ASSERT_EQ(dd[1], 1);
    ASSERT_EQ(dd[2], 32);
    printf("DD of %d %d %d for %d processes is %d %d %d \n", sys_size[0],
           sys_size[1], sys_size[2], n_procs, dd[0], dd[1], dd[2]);
}
