/**
 * @file    mpi_sa_test.hpp
 * @author  Patrick Flick <patrick.flick@gmail.com>
 * @brief   Tests for the distibuted parallel suffix array construction.
 *
 * Copyright (c) 2014 Georgia Institute of Technology. All Rights Reserved.
 *
 * TODO add Licence
 */

#ifndef MPI_SA_TEST_HPP
#define MPI_SA_TEST_HPP

#include <mpi.h>

#include <vector>
#include <iostream>
#include <string>

#include "mpi_utils.hpp"
#include "mpi_sa_constr.hpp"

/*****************************
 *  create random DNA input  *
 *****************************/

inline char rand_dna_char()
{
    char DNA[4] = {'A', 'C', 'G', 'T'};
    return DNA[rand() % 2];
}

std::string rand_dna(std::size_t size, int seed)
{
    srand(1337*seed);
    std::string str;
    str.resize(size, ' ');
    for (std::size_t i = 0; i < size; ++i)
    {
        str[i] = rand_dna_char();
    }
    return str;
}

/**************************************
 *  test correctness of suffix array  *
 **************************************/

void test_sa(MPI_Comm comm, std::size_t input_size, bool test_correct = false)
{
    // get comm parameters
    int p, rank;
    MPI_Comm_size(comm, &p);
    MPI_Comm_rank(comm, &rank);

    //std::string local_str = "missisippi";
    std::string local_str = rand_dna(input_size, rank);

    std::vector<std::size_t> local_SA;
    std::vector<std::size_t> local_ISA;

    // construct local SA for input string
    sa_construction(local_str, local_SA, local_ISA, comm);

    // final SA and ISA
    if (test_correct)
    {
        // gather SA and ISA to local
        std::vector<std::size_t> global_SA = gather_vectors(local_SA, comm);
        std::vector<std::size_t> global_ISA = gather_vectors(local_ISA, comm);
        std::vector<char> global_str_vec = gather_range(local_str.begin(), local_str.end(), comm);
        std::string global_str(global_str_vec.begin(), global_str_vec.end());
        if (rank == 0)
        {
#if 0
            std::cerr << "##################################################" << std::endl;
            std::cerr << "#               Final SA and ISA                 #" << std::endl;
            std::cerr << "##################################################" << std::endl;
            std::cerr << "STR: " << global_str << std::endl;
            std::cerr << "SA : "; print_vec(global_SA);
            std::cerr << "ISA: "; print_vec(global_ISA);
#endif

            // check if correct
            if (!gl_check_correct_SA(global_SA, global_ISA, global_str))
            {
                std::cerr << "[ERROR] Test unsuccessful" << std::endl;
                exit(1);
            }
            else
            {
                std::cerr << "[SUCCESS]" << std::endl;
            }
        }
    }
    std::cerr << " === Rank " << rank << " is finished === " << std::endl;
}


#endif // MPI_SA_TEST_HPP