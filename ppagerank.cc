/*
 * David Gleich
 * 13 December 2006
 * Copyright, Stanford University
 */
 
/**
 * @file ppagerank.cc
 * The driver file for a distributed memory implementation of 
 * pagerank.
 */
 
#include "petsc.h"
#include "petscmat.h"
#include "petscmath.h"

#include "petsc_util.h"



const static int version_major = 0;
const static int version_minor = 0;

// function prototypes

PetscErrorCode WriteHeader();
PetscErrorCode WriteSimpleMatrixStats(const char* filename, Mat A);
PetscErrorCode ComputePageRank(Mat A);


static char help[] = 
"usage: ppagerank -m <filename> [options]\n"
"\n"
"Data\n"
"  -m <filename>    (REQUIRED) Load the matrix given by filename\n"
"\n"
"PageRank parameters\n"
"  -alpha <float>   Set the value of the pagerank alpha parameter\n"
"                   default = 0.85\n"
"  -pvec <filename> Set the personalization vector\n"
"                   default = (1/n) for each entry\n"
"\n"
"Additional options\n"   
"  -noout           Do not write any output information\n"
"  -trans           The input matrix is transposed\n"
"\n"
"Matrix Loading options\n"
"  -matload_root_nz_bufsize <int>     The nonzero buffer size to read and\n"
"                                     send non-zero values to other procs\n"
"                                     default = 2^25 (33554432)\n"
"  -matload_redistribute              A binary switch indicating that the \n"
"                                     matrix will be redistributed to balance\n"
"                                     non-zeros and rows among processors\n"
"  -matload_redistribute_wnnz <int>   The weight of each non-zero in the\n"
"                                     distribution.\n"
"                                     default = 1\n"
"  -matload_redistribute_wrows <int>  The weight of each row in the\n"
"                                     distribution.\n"
"                                     default = 1\n"
"";         
 
// add code to get correct debug reports 
#undef __FUNCT__
#define __FUNCT__ "main" 
int main(int argc, char **argv)
{
    PetscErrorCode ierr;
    
    // parse the options
    ierr=PetscInitialize(&argc, &argv, (char*)0, help); CHKERRQ(ierr);

    if (argc < 2) {
        PetscPrintf(PETSC_COMM_WORLD, help);
        PetscFinalize();
        return (-1);
    }
    
    PetscTruth option_flag;
    
//    ierr=PetscOptionsHasName(PETSC_NULL,"-help",&option_flag);CHKERRQ(ierr);
//    if (option_flag) {
//        PetscFinalize();
//        return (-1);
//    }
    
    //
    // begin options parsing
    //
 
    // get the matrix filename
    char matrix_filename[PETSC_MAX_PATH_LEN];
    ierr=PetscOptionsGetString(PETSC_NULL,"-m",matrix_filename,PETSC_MAX_PATH_LEN,&option_flag);
        CHKERRQ(ierr); 
    if (!option_flag) {
        PetscPrintf(PETSC_COMM_WORLD,"\nOptions error: no matrix file specified!\n\n");
        PetscFinalize();
        return (-1);
    }
    
    PetscTruth script;
    ierr=PetscOptionsHasName(PETSC_NULL,"-script",&script);
    
    //
    // end options parsing
    //
       
    WriteHeader();
    
    Mat A;
    ierr=MatLoadBSMAT(PETSC_COMM_WORLD,matrix_filename,&A);CHKERRQ(ierr);
    
    WriteSimpleMatrixStats(matrix_filename, A);
    
    if (script) {
        // make sure there are no options left
        // PetscOptionsLeft();
        
        // PetscOptionsCreate();
        // PetscOptionsInsert(argc,argv)   
    } else {
        //ierr=ComputePageRank(A); CHKERRQ(ierr);
    }
        
    PetscFinalize();
    
    return (0); 
} 



#undef __FUNCT__
#define __FUNCT__ "WriteHeader" 
PetscErrorCode WriteHeader(void)
{
    char name[MPI_MAX_PROCESSOR_NAME+1];
    int namelen;
    PetscErrorCode ierr;
    int rank, size;
    
    // print off the 
    for (int i=0; i<60; i++) { PetscPrintf(PETSC_COMM_WORLD, "%c", '='); }
    PetscPrintf(PETSC_COMM_WORLD, "\n");
    
    PetscPrintf(PETSC_COMM_WORLD, "ppagerank %i.%i\n\n", version_major, version_minor);
    PetscPrintf(PETSC_COMM_WORLD, "David Gleich\n");
    PetscPrintf(PETSC_COMM_WORLD, "Copyright, 2006\n");
    
    for (int i=0; i<60; i++) { PetscPrintf(PETSC_COMM_WORLD, "%c", '='); }
    PetscPrintf(PETSC_COMM_WORLD, "\n");
    
    ierr=MPI_Comm_size(PETSC_COMM_WORLD, &size); CHKERRQ(ierr);
    ierr=MPI_Comm_rank(PETSC_COMM_WORLD, &rank); CHKERRQ(ierr);
    ierr=MPI_Get_processor_name(name, &namelen);
    
    PetscPrintf(PETSC_COMM_WORLD, "nprocs = %i\n", size);
    
    PetscSynchronizedPrintf(PETSC_COMM_WORLD, "[%3i] %s running...\n", rank, name);
    PetscSynchronizedFlush(PETSC_COMM_WORLD);
    
    ierr=MPI_Barrier(PETSC_COMM_WORLD);CHKERRQ(ierr);
    
    return (MPI_SUCCESS);
}		
 
/**
 * Output a set of simple matrix statistics.
 * 1) num rows, num cols, num non-zeros
 * 2) min/max rows/proc
 * 3) min/max cols/proc
 * 4) min/max off diag columns/proc
 * 
 * @param filename the matrix filename
 * @param A the distributed memory Petsc matrix
 */
#undef __FUNCT__
#define __FUNCT__ "WriteSimpleMatrixStats"
PetscErrorCode WriteSimpleMatrixStats(const char* filename, Mat A)
{
    MPI_Comm comm;
    PetscErrorCode ierr;
    
    PetscInt m,n;
    PetscInt ml,nl;
    
    PetscObjectGetComm((PetscObject)A,&comm);
    
    MatGetSize(A,&m,&n);
    MatGetLocalSize(A,&ml,&nl);
    
    PetscInt max_local_rows, min_local_rows;
    PetscInt max_local_columns, min_local_columns;
    
    ierr=MPI_Reduce(&ml,&max_local_rows,1,MPI_INT,MPI_MAX,0,comm);CHKERRQ(ierr);
    ierr=MPI_Reduce(&ml,&min_local_rows,1,MPI_INT,MPI_MIN,0,comm);CHKERRQ(ierr);
    ierr=MPI_Reduce(&nl,&max_local_columns,1,MPI_INT,MPI_MAX,0,comm);CHKERRQ(ierr);
    ierr=MPI_Reduce(&nl,&min_local_columns,1,MPI_INT,MPI_MIN,0,comm);CHKERRQ(ierr);
    
    long long int total_nz = 0;
    PetscInt local_nz = 0;
    ierr=MatGetNonzeroCount(A,&total_nz, &local_nz);
    
    PetscInt max_local_nz,min_local_nz;
    
    ierr=MPI_Reduce(&local_nz,&max_local_nz,1,MPI_INT,MPI_MAX,0,comm);CHKERRQ(ierr);
    ierr=MPI_Reduce(&local_nz,&min_local_nz,1,MPI_INT,MPI_MIN,0,comm);CHKERRQ(ierr);
    
    
    PetscPrintf(comm,"matrix %s\n", filename);
    PetscPrintf(comm,"rows       = %10i\n", m);
    PetscPrintf(comm,"columns    = %10i\n", n);
    PetscPrintf(comm,"nnz        = %10lli\n", total_nz);
    PetscPrintf(comm,"local rows = (%10i,%10i)\n", min_local_rows, max_local_rows);
    PetscPrintf(comm,"local cols = (%10i,%10i)\n", min_local_columns, max_local_columns);
    PetscPrintf(comm,"local nzs  = (%10i,%10i)\n", min_local_nz, max_local_nz);
    
    return (MPI_SUCCESS);
}

/**
 * Compute a PageRank vector for a PETSc Matrix A.
 * 
 * The ComputePageRank function loads all of its options from 
 * the command line.
 * 
 * @param A the matrix used for PageRank
 * @param transp a flag indicating if the matrix is transposed so that
 * the edge from page i to page j is in the ith column of the matrix
 * instead of the ith row of the matrix.
 * @return MPI_SUCCESS unless there was an error.
 */
PetscErrorCode ComputePageRank(Mat A)
{
    return (MPI_SUCCESS);
}

