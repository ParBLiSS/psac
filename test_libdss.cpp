#include <mpi.h>

#include <iostream>
#include <string>

#include "test_sac_libdss.hpp"
#include "mpi_sa_test.hpp"

int main(int argc, char *argv[])
{
    // set up MPI
    MPI_Init(&argc, &argv);

    // get communicator size and my rank
    MPI_Comm comm = MPI_COMM_WORLD;
    int p, rank;
    MPI_Comm_size(comm, &p);
    MPI_Comm_rank(comm, &rank);

    // test PSAC against libdivsufsort
    //std::string str = "mississippi";
    /*
    for (int i = 2*p; i <= 1031; ++i)
    {
        std::string str = rand_dna(i, 0);
        if (!test_compare_divsufsort_psac(str,comm))
        {
            std::cerr << "Failed with i = " << i << std::endl;
            exit(1);
        }
    }
    */
    std::string str;
    if (argc >= 2)
    {
        std::ifstream t(argv[1]);
        std::stringstream buffer;
        buffer << t.rdbuf();
        str = buffer.str();
    }
    else
    {
        std::cerr << "Warning: no input file provided, testing with random DNA string." << std::endl;
        str = rand_dna(1000,0);
    }
    if (!test_compare_divsufsort_psac(str,comm))
    {
        std::cerr << "Failed with p = " << p << std::endl;
        exit(1);
    }

    // finalize MPI
    MPI_Finalize();
    return 0;
}
