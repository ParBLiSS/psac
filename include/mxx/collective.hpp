/**
 * @file    collective.hpp
 * @author  Patrick Flick <patrick.flick@gmail.com>
 * @author  Nagakishore Jammula <njammula3@mail.gatech.edu>
 * @brief   Collective operations.
 *
 * Copyright (c) TODO
 *
 * TODO add Licence
 */


#ifndef MXX_COLLECTIVE_HPP
#define MXX_COLLECTIVE_HPP

#include <mpi.h>

#include <vector>

// mxx includes
#include "datatypes.hpp"
#include "algos.hpp"
#include "partition.hpp"

namespace mxx {

/**
 * @brief   Returns the displacements vector needed by MPI_Alltoallv.
 *
 * @param counts    The `counts` array needed by MPI_Alltoallv
 *
 * @return The displacements vector needed by MPI_Alltoallv.
 */
std::vector<int> get_displacements(const std::vector<int>& counts)
{
    // copy and do an exclusive prefix sum
    std::vector<int> result = counts;
    excl_prefix_sum(result.begin(), result.end());
    return result;
}

template <typename Iterator>
std::vector<typename std::iterator_traits<Iterator>::value_type> gather_range(Iterator begin, Iterator end, MPI_Comm comm)
{
    //static_assert(std::is_same<T, typename std::iterator_traits<Iterator>::value_type>::value, "Return type must of of same type as iterator value type");
    typedef typename std::iterator_traits<Iterator>::value_type T;
    // get MPI parameters
    int rank;
    int p;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &p);

    // get local size
    int local_size = std::distance(begin, end);

    // init result
    std::vector<T> result;

    // get type
    mxx::datatype<T> dt;
    MPI_Datatype mpi_dt = dt.type();


    // master process: receive results
    if (rank == 0)
    {
        // gather local array sizes, sizes are restricted to `int` by MPI anyway
        // therefore use int
        std::vector<int> local_sizes(p);
        MPI_Gather(&local_size, 1, MPI_INT,
                   &local_sizes[0], 1, MPI_INT,
                   0, comm);

        // gather-v to collect all the elements
        int total_size = std::accumulate(local_sizes.begin(), local_sizes.end(), 0);
        result.resize(total_size);
        std::vector<int> recv_displs = get_displacements(local_sizes);

        // gather v the vector data to the root
        MPI_Gatherv((void*)&(*begin), local_size, mpi_dt,
                    &result[0], &local_sizes[0], &recv_displs[0], mpi_dt,
                    0, comm);
    }
    // else: send results
    else
    {
        // gather local array sizes
        MPI_Gather(&local_size, 1, MPI_INT, NULL, 1, MPI_INT, 0, comm);

        // sent the actual data
        MPI_Gatherv((void*)&(*begin), local_size, mpi_dt,
                    NULL, NULL, NULL, mpi_dt,
                    0, comm);
    }

    return result;
}


/**
 * @brief   Gathers local std::vectors to the master processor inside the
 *          given communicator.
 *
 * @param local_vec The local vectors to be gathered.
 * @param comm      The communicator.
 *
 * @return (On the master processor): The vector containing the concatenation
 *                                    of all distributed vectors.
 *         (On the slave processors): An empty vector.
 */
template<typename T>
std::vector<T> gather_vectors(const std::vector<T>& local_vec, MPI_Comm comm)
{
    return gather_range(local_vec.begin(), local_vec.end(), comm);
}



template <typename InputIterator, typename OutputIterator>
void copy_n(InputIterator& in, std::size_t n, OutputIterator out)
{
    for (std::size_t i = 0u; i < n; ++i)
        *(out++) = *(in++);
}

template <typename Iterator>
std::vector<typename std::iterator_traits<Iterator>::value_type>
scatter_stream_block_decomp(Iterator input, uint32_t n, MPI_Comm comm)
{
    // get MPI Communicator properties
    int rank, p;
    MPI_Comm_size(comm, &p);
    MPI_Comm_rank(comm, &rank);

    typedef typename std::iterator_traits<Iterator>::value_type val_t;

    // the local vector size (MPI restricts message sizes to `int`)
    int local_size;

    // get type
    mxx::datatype<val_t> dt;
    MPI_Datatype mpi_dt = dt.type();

    // init result
    std::vector<val_t> local_elements;

    if (rank == 0)
    {
        /* I am the root process */

        //Read from the input filename
        std::vector<int> block_decomp = block_partition(n, p);

        // scatter the sizes to expect
        MPI_Scatter(&block_decomp[0], 1, MPI_INT, &local_size,
                    1, MPI_INT, 0, comm);

        // copy the first block into the masters memory
        local_elements.resize(local_size);
        copy_n(input, local_size, local_elements.begin());

        // distribute the rest
        std::vector<val_t> local_buffer(block_decomp[0]);
        for (int i = 1; i < p; ++i) {
            // copy into local buffer
            copy_n(input, block_decomp[i], local_buffer.begin());
            // send the data to processor i
            MPI_Send (&local_buffer[0], block_decomp[i], mpi_dt,
                      i, i, comm);
        }
    }
    else
    {
        /* I am NOT the root process */
        std::runtime_error("slave called master function");
    }

    // return the local vectors
    return local_elements;
}

template <typename T>
std::vector<T> scatter_stream_block_decomp_slave(MPI_Comm comm)
{
    // get MPI Communicator properties
    int rank, p;
    MPI_Comm_size(comm, &p);
    MPI_Comm_rank(comm, &rank);

    // the local vector size (MPI restricts message sizes to `int`)
    int local_size;

    // get type
    mxx::datatype<T> dt;
    MPI_Datatype mpi_dt = dt.type();

    // init result
    std::vector<T> local_elements;

    if (rank == 0)
    {
        std::runtime_error("master called slave function");
    }
    else
    {
        /* I am NOT the root process */

        // receive my new local data size
        MPI_Scatter(NULL, 1, MPI_INT,
                    &local_size, 1, MPI_INT,
                    0, comm);
        // resize local data
        local_elements.resize(local_size);
        // actually receive the data
        MPI_Status recv_status;
        MPI_Recv (&local_elements[0], local_size, mpi_dt,
                  0, rank, comm, &recv_status);
    }

    // return the local vectors
    return local_elements;
}


template <typename T>
std::vector<T> scatter_vector_block_decomp(std::vector<T>& global_vec, MPI_Comm comm)
{
    // get MPI Communicator properties
    int rank, p;
    MPI_Comm_size(comm, &p);
    MPI_Comm_rank(comm, &rank);


    // the local vector size (MPI restricts message sizes to `int`)
    int local_size;

    // get type
    mxx::datatype<T> dt;
    MPI_Datatype mpi_dt = dt.type();

    // init result
    std::vector<T> local_elements;

    if (rank == 0)
    {
        /* I am the root process */

        // get size of global array
        std::size_t n = global_vec.size();

        //Read from the input filename
        std::vector<int> block_decomp = block_partition(n, p);

        // scatter the sizes to expect
        // TODO: not necessary to scatter this, simply broadcast `n`
        MPI_Scatter(&block_decomp[0], 1, MPI_INT, &local_size, 1, MPI_INT, 0, comm);

        // scatter-v the actual data
        local_elements.resize(local_size);
        std::vector<int> displs = get_displacements(block_decomp);
        MPI_Scatterv(&global_vec[0], &block_decomp[0], &displs[0],
                     mpi_dt, &local_elements[0], local_size, mpi_dt, 0, comm);
    }
    else
    {
        /* I am NOT the root process */

        // receive the size of my local array
        MPI_Scatter(NULL, 1, MPI_INT,
                    &local_size, 1, MPI_INT,
                    0, comm);

        // resize result buffer
        local_elements.resize(local_size);
        // actually receive all the data
        MPI_Scatterv(NULL, NULL, NULL,
                     mpi_dt, &local_elements[0], local_size, mpi_dt, 0, comm);
    }

    // return local array
    return local_elements;
}

// same as scatter_vector_block_decomp, but for std::basic_string
template<typename CharT>
std::basic_string<CharT> scatter_string_block_decomp(std::basic_string<CharT>& global_str, MPI_Comm comm)
{
    // get MPI Communicator properties
    int rank, p;
    MPI_Comm_size(comm, &p);
    MPI_Comm_rank(comm, &rank);

    // the local vector size (MPI restricts message sizes to `int`)
    int local_size;

    // get the MPI data type
    typedef typename std::basic_string<CharT>::value_type val_t;
    mxx::datatype<val_t> dt;
    MPI_Datatype mpi_dt = dt.type();

    // init result
    std::basic_string<CharT> local_str;

    if (rank == 0)
    {
        /* I am the root process */

        // get size of global array
        uint32_t n = global_str.size();

        //Read from the input filename
        std::vector<int> block_decomp = block_partition(n, p);

        // scatter the sizes to expect
        MPI_Scatter(&block_decomp[0], 1, MPI_INT, &local_size, 1, MPI_INT, 0, comm);

        // scatter-v the actual data
        local_str.resize(local_size);
        std::vector<int> displs = get_displacements(block_decomp);
        MPI_Scatterv(const_cast<CharT*>(global_str.data()), &block_decomp[0], &displs[0],
                     mpi_dt, const_cast<CharT*>(local_str.data()), local_size, mpi_dt, 0, comm);
    }
    else
    {
        /* I am NOT the root process */

        // receive the size of my local array
        MPI_Scatter(NULL, 1, MPI_INT,
                    &local_size, 1, MPI_INT,
                    0, comm);

        // resize result buffer
        local_str.resize(local_size);
        // actually receive all the data
        MPI_Scatterv(NULL, NULL, NULL,
                     mpi_dt, const_cast<CharT*>(local_str.data()), local_size, mpi_dt, 0, comm);
    }

    // return local array
    return local_str;
}

template<typename T>
void striped_excl_prefix_sum(std::vector<T>& x, MPI_Comm comm)
{
    // get MPI type
    mxx::datatype<T> dt;
    MPI_Datatype mpi_dt = dt.type();

    // get sum of all buckets and the prefix sum of that
    std::vector<T> all_sum(x.size());
    MPI_Allreduce(&x[0], &all_sum[0], x.size(), mpi_dt, MPI_SUM, comm);
    excl_prefix_sum(all_sum.begin(), all_sum.end());

    // exclusive prefix scan of vectors gives the number of elements prior
    // this processor in the _same_ bucket
    MPI_Exscan(&x[0], &x[0], x.size(), mpi_dt, MPI_SUM, comm);

    int rank;
    MPI_Comm_rank(comm, &rank);
    if (rank == 0)
    {
        // set x to all_sum
        for (std::size_t i = 0; i < x.size(); ++i)
        {
            x[i] = all_sum[i];
        }
    }
    else
    {
        // sum these two vectors for all_sum
        for (std::size_t i = 0; i < x.size(); ++i)
        {
            x[i] += all_sum[i];
        }
    }
}

template<typename Iterator>
void global_prefix_sum(Iterator begin, Iterator end, MPI_Comm comm)
{
  // get MPI type
  typedef typename std::iterator_traits<Iterator>::value_type T;
  mxx::datatype<T> dt;
  MPI_Datatype mpi_dt = dt.type();

  // local sum
  T sum = std::accumulate(begin, end, static_cast<T>(0));

  // exclusive prefix scan of local sums
  MPI_Exscan(&sum, &sum, 1, mpi_dt, MPI_SUM, comm);
  // first element in MPI_Exscan is undefined, therefore set to zero
  int rank;
  MPI_Comm_rank(comm, &rank);
  if (rank == 0)
    sum = 0;

  // calculate the inclusive prefix sum of local elements by starting with
  // the global prefix sum value
  while (begin != end)
  {
    sum += *begin;
    *begin = sum;
    ++begin;
  }
}





// assumes only local send_counts for an all2allv operation are available
// this function scatters the send counts to get the receive counts
inline std::vector<int> all2allv_get_recv_counts(const std::vector<int>& send_counts, MPI_Comm comm)
{
    std::size_t size = send_counts.size();
    std::vector<int> recv_counts;
    recv_counts.resize(size);
    MPI_Alltoall(const_cast<int*>(&send_counts[0]), 1, MPI_INT, &recv_counts[0], 1, MPI_INT, comm);
    return recv_counts;
}

template<typename T>
std::vector<T> all2all(const std::vector<T>& send_buffer, const std::vector<int>& send_counts, MPI_Comm comm = MPI_COMM_WORLD)
{
    // get counts and displacements
    std::vector<int> send_displs = get_displacements(send_counts);
    std::vector<int> recv_counts = all2allv_get_recv_counts(send_counts, comm);
    std::vector<int> recv_displs = get_displacements(recv_counts);

    // get total size
    std::size_t recv_size = recv_displs.back() + recv_counts.back();
    std::vector<T> recv_buffer(recv_size);

    // get MPI type
    mxx::datatype<T> dt;
    MPI_Datatype mpi_dt = dt.type();

    // collective all2all!
    MPI_Alltoallv(const_cast<T*>(&send_buffer[0]), const_cast<int*>(&send_counts[0]), &send_displs[0], mpi_dt,
                  &recv_buffer[0], &recv_counts[0], &recv_displs[0], mpi_dt,
                  comm);

    // return the receive buffer
    return recv_buffer;
}

template<typename T, typename _TargetP>
void msgs_all2all(std::vector<T>& msgs, _TargetP target_p_fun, MPI_Comm comm)
{
    // get comm parameters
    int p, rank;
    MPI_Comm_size(comm, &p);
    MPI_Comm_rank(comm, &rank);

    // get MPI type
    mxx::datatype<T> dt;
    MPI_Datatype mpi_dt = dt.type();

    // bucket input by their target processor
    // TODO: in-place bucketing??
    std::vector<int> send_counts(p, 0);
    for (auto it = msgs.begin(); it != msgs.end(); ++it)
    {
        send_counts[target_p_fun(*it)]++;
    }
    std::vector<std::size_t> offset(send_counts.begin(), send_counts.end());
    excl_prefix_sum(offset.begin(), offset.end());
    std::vector<T> send_buffer;
    if (msgs.size() > 0)
        send_buffer.resize(msgs.size());
    for (auto it = msgs.begin(); it != msgs.end(); ++it)
    {
        send_buffer[offset[target_p_fun(*it)]++] = *it;
    }

    // get all2all params
    std::vector<int> recv_counts = all2allv_get_recv_counts(send_counts, comm);
    std::vector<int> send_displs = get_displacements(send_counts);
    std::vector<int> recv_displs = get_displacements(recv_counts);

    // resize messages to fit recv
    std::size_t recv_size = std::accumulate(recv_counts.begin(), recv_counts.end(), 0);
    msgs.clear();
    msgs.shrink_to_fit();
    msgs.resize(recv_size);
    //msgs = std::vector<T>(recv_size);

    // all2all
    MPI_Alltoallv(&send_buffer[0], &send_counts[0], &send_displs[0], mpi_dt,
                  &msgs[0], &recv_counts[0], &recv_displs[0], mpi_dt, comm);
    // done, result is returned in vector of input messages
}

} // namespace mxx

#endif // MXX_COLLECTIVE_HPP