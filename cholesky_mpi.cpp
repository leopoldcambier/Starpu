#include <starpu.h>
#include <starpu_mpi.h>
#include<vector>
#include<memory>
#include<iostream>
#include <cblas.h>
#include <lapacke.h>
#include <Eigen/Core>
#include <Eigen/Cholesky>
#include <mpi.h>


#define TAG11(k)	((starpu_tag_t)( (1ULL<<60) | (unsigned long long)(k)))
#define TAG21(k,j)	((starpu_tag_t)(((3ULL<<60) | (((unsigned long long)(k))<<32)	\
					| (unsigned long long)(j))))
#define TAG22(k,i,j)	((starpu_tag_t)(((4ULL<<60) | ((unsigned long long)(k)<<32) 	\
					| ((unsigned long long)(i)<<16)	\
					| (unsigned long long)(j))))


using namespace std;
using namespace Eigen;



void task1(void *buffers[], void *cl_arg) { 

    int *A= (int *)STARPU_VARIABLE_GET_PTR(buffers[0]);
	*A+=1;
    cout<<"Incrementing *a, *a="<<*A<<endl;
    return;
     }
struct starpu_codelet cl1 = {
    .where = STARPU_CPU,
    .cpu_funcs = { task1, NULL },
    .nbuffers = 1,
    .modes = { STARPU_RW }
};

void task2(void *buffers[], void *cl_arg) { 

    int *A0= (int *)STARPU_VARIABLE_GET_PTR(buffers[0]);
	int *A1= (int *)STARPU_VARIABLE_GET_PTR(buffers[1]);
    *A1+=*A0;
    cout<<"*b + *a= "<<*A1<<endl;
    return;
     }
struct starpu_codelet cl2 = {
    .where = STARPU_CPU,
    .cpu_funcs = { task2, NULL },
    .nbuffers = 2,
    .modes = { STARPU_R, STARPU_RW }
};




void potrf(void *buffers[], void *cl_arg) { 

    double *A= (double *)STARPU_MATRIX_GET_PTR(buffers[0]);
    int nx = STARPU_MATRIX_GET_NY(buffers[0]);
	int ny = STARPU_MATRIX_GET_NX(buffers[0]);

	//LAPACKE_dpotrf(LAPACK_COL_MAJOR, 'L', A->rows(), A->data(), A->rows());
    LAPACKE_dpotrf(LAPACK_COL_MAJOR, 'L', nx, A, ny);
    //Map<MatrixXd> tt(A, nx, nx);
    //cout<<"POTRF: \n"<<tt<<"\n";
     }
struct starpu_codelet potrf_cl = {
    .where = STARPU_CPU,
    .cpu_funcs = { potrf, NULL },
    .nbuffers = 1,
    .modes = { STARPU_RW }
};

void trsm(void *buffers[], void *cl_arg) {
	double *A0= (double *)STARPU_MATRIX_GET_PTR(buffers[0]);
	double *A1= (double *)STARPU_MATRIX_GET_PTR(buffers[1]);
    int nx = STARPU_MATRIX_GET_NY(buffers[0]);
	int ny = STARPU_MATRIX_GET_NX(buffers[0]);
	cblas_dtrsm(CblasColMajor, CblasRight, CblasLower, CblasTrans, CblasNonUnit, ny, ny, 1.0, A0, nx, A1, nx);
    //Map<MatrixXd> tt(A1, nx, nx);
    //cout<<"TRSM: \n"<<tt<<"\n";
    //printf("TRSM:%llx \n", task->tag_id);
  }
struct starpu_codelet trsm_cl = {
    .where = STARPU_CPU,
    .cpu_funcs = { trsm, NULL },
    .nbuffers = 2,
    .modes = { STARPU_R, STARPU_RW }
};

void syrk(void *buffers[], void *cl_arg) { 
    double *A0= (double *)STARPU_MATRIX_GET_PTR(buffers[0]);
	double *A1= (double *)STARPU_MATRIX_GET_PTR(buffers[1]);
    int nx = STARPU_MATRIX_GET_NY(buffers[0]);
	int ny = STARPU_MATRIX_GET_NX(buffers[0]);
	cblas_dsyrk(CblasColMajor, CblasLower, CblasNoTrans, nx, nx, -1.0, A0, nx, 1.0, A1, nx);
 }
struct starpu_codelet syrk_cl = {
    .where = STARPU_CPU,
    .cpu_funcs = { syrk, NULL },
    .nbuffers = 2,
    .modes = { STARPU_R, STARPU_RW }
};

void gemm(void *buffers[], void *cl_arg) {
    double *A0= (double *)STARPU_MATRIX_GET_PTR(buffers[0]);
	double *A1= (double *)STARPU_MATRIX_GET_PTR(buffers[1]);
    double *A2= (double *)STARPU_MATRIX_GET_PTR(buffers[2]);
    int nx = STARPU_MATRIX_GET_NY(buffers[0]);
	int ny = STARPU_MATRIX_GET_NX(buffers[0]);
    cblas_dgemm(CblasColMajor, CblasNoTrans, CblasTrans, nx, nx, ny, 
    -1.0,A0, nx, A1, nx, 1.0, A2, nx);
    //printf("GEMM:%llx \n", task->tag_id);
  }
struct starpu_codelet gemm_cl = {
    .where = STARPU_CPU,
    .cpu_funcs = { gemm, NULL },
    .nbuffers = 3,
    .modes = { STARPU_R, STARPU_R, STARPU_RW }
};



void test(int rank)  {


    
    int* a=new int(1);
    int* b=new int(1);
    int* c=new int(1);
    starpu_data_handle_t data1, data2;
    if (rank==0) {
        starpu_variable_data_register(&data1, STARPU_MAIN_RAM, (uintptr_t)a, sizeof(int));
        starpu_variable_data_register(&data2, -1, (uintptr_t)NULL, sizeof(int));
    }
    else {
        starpu_variable_data_register(&data1, -1, (uintptr_t)NULL, sizeof(int));
        starpu_variable_data_register(&data2, STARPU_MAIN_RAM, (uintptr_t)b, sizeof(int));
    }
    starpu_mpi_data_register(data1, 0, 0);
    starpu_mpi_data_register(data2, 1, 1);

    starpu_mpi_task_insert(MPI_COMM_WORLD,&cl1, STARPU_RW, data1, 0);
    starpu_mpi_task_insert(MPI_COMM_WORLD,&cl2, STARPU_R, data1,STARPU_RW, data2,0);
    starpu_task_wait_for_all();
    if (rank==0) {cout<<"*b is equal to "<<*b<<"\n";}
    MPI_Status status;
    if (rank==0) { MPI_Recv(b, 1, MPI_INT, 1, 99, MPI_COMM_WORLD, &status);}
    else { MPI_Send(b, 1, MPI_INT, 0, 99, MPI_COMM_WORLD);}

    starpu_data_unregister(data1);
    starpu_data_unregister(data2);
    cout<<"*b is now equal to "<<*b<<"on rank "<<rank<<"\n";

    /*
    int nb=2;
    int n=2;
    auto val = [&](int i, int j) { return  1/(float)((i-j)*(i-j)+1); };
    MatrixXd B=MatrixXd::NullaryExpr(n*nb,n*nb, val);
    MatrixXd L = B;
    vector<MatrixXd*> blocs(nb*nb);
    vector<starpu_data_handle_t> dataA(nb*nb);
    for (int ii=0; ii<nb; ii++) {
        for (int jj=0; jj<nb; jj++) {

                blocs[ii+jj*nb]=new MatrixXd(n,n);
                *blocs[ii+jj*nb]=L.block(ii*n,jj*n,n,n);
                //starpu_variable_data_register(&dataA[ii+jj*nb], -1, (uintptr_t)NULL, sizeof(MatrixXd));
        }
    }
    starpu_data_handle_t data1, data2;
    if (rank==0) {
        starpu_matrix_data_register(&data1, STARPU_MAIN_RAM, (uintptr_t)blocs[0]->data(), n, n, n, sizeof(double));
        starpu_matrix_data_register(&data2, -1, (uintptr_t)NULL, n, n, n, sizeof(double));
    }
    else {
        starpu_matrix_data_register(&data1, -1, (uintptr_t)NULL, n, n, n, sizeof(double));
        starpu_matrix_data_register(&data2, STARPU_MAIN_RAM, (uintptr_t)blocs[1]->data(), n, n, n, sizeof(double));
    }
    starpu_mpi_data_register(data1, 0, 0);
    starpu_mpi_data_register(data2, 1, 1);


    starpu_mpi_task_insert(MPI_COMM_WORLD,&potrf_cl, STARPU_RW, data1, 0);
    starpu_mpi_task_insert(MPI_COMM_WORLD,&trsm_cl, STARPU_R, data1,STARPU_RW, data2,0);

    starpu_data_unregister(data1);
    starpu_data_unregister(data2);

    */
    return;


}


void cholesky(int n, int nb, int rank, int size) {
    auto val = [&](int i, int j) { return  1/(float)((i-j)*(i-j)+1); };
    auto distrib = [&](int i, int j) { return  ((i+j*nb) % size == rank); };
    MatrixXd B=MatrixXd::NullaryExpr(n*nb,n*nb, val);
    MatrixXd L = B;
    vector<MatrixXd*> blocs(nb*nb);
    vector<starpu_data_handle_t> dataA(nb*nb);

    //cout<<"rank "<<rank<<" registering data....\n";
    for (int ii=0; ii<nb; ii++) {
        for (int jj=0; jj<nb; jj++) {

                blocs[ii+jj*nb]=new MatrixXd(n,n);
                *blocs[ii+jj*nb]=L.block(ii*n,jj*n,n,n);
                //starpu_variable_data_register(&dataA[ii+jj*nb], -1, (uintptr_t)NULL, sizeof(MatrixXd));
                int mpi_rank = ((ii+jj*nb)%size);
                if (mpi_rank == rank) {
                    starpu_matrix_data_register(&dataA[ii+jj*nb], STARPU_MAIN_RAM, (uintptr_t)blocs[ii+jj*nb]->data(), n, n, n, sizeof(double));
                    //starpu_variable_data_register(&dataA[ii+jj*nb], STARPU_MAIN_RAM, (uintptr_t)blocs[ii+jj*nb], sizeof(MatrixXd));
                }
                else {
                    starpu_matrix_data_register(&dataA[ii+jj*nb], -1, (uintptr_t)NULL, n, n, n, sizeof(double));
                    //starpu_variable_data_register(&dataA[ii+jj*nb], -1, (uintptr_t)NULL, sizeof(MatrixXd));
                }
                if (dataA[ii+jj*nb]) {
                    starpu_mpi_data_register(dataA[ii+jj*nb], ii+jj*nb, mpi_rank);

                }
        }
    }
    MatrixXd* A=&B;
    //cout<<A->size()<<"\n";
    //Test
    //cout<<"rank "<<rank<<" inserting tasks....\n";
    double start = starpu_timing_now();
    for (int kk = 0; kk < nb; ++kk) {
            starpu_mpi_task_insert(MPI_COMM_WORLD,&potrf_cl,STARPU_RW, dataA[kk+kk*nb],0);
        for (int ii = kk+1; ii < nb; ++ii) {
            starpu_mpi_task_insert(MPI_COMM_WORLD,&trsm_cl,STARPU_R, dataA[kk+kk*nb],STARPU_RW, dataA[ii+kk*nb],0);
            starpu_mpi_cache_flush(MPI_COMM_WORLD, dataA[kk+kk*nb]);
            for (int jj=kk+1; jj < nb; ++jj) {         
                if (jj <= ii) {
                    if (jj==ii) {
                        starpu_mpi_task_insert(MPI_COMM_WORLD,&syrk_cl, STARPU_R, dataA[ii+kk*nb],STARPU_RW, dataA[ii+jj*nb],0);
                    }
                    else {
                        starpu_mpi_task_insert(MPI_COMM_WORLD,&gemm_cl,STARPU_R, dataA[ii+kk*nb],STARPU_R, dataA[jj+kk*nb],STARPU_RW, dataA[ii+jj*nb],0);
                    }
                }
            }
            starpu_mpi_cache_flush(MPI_COMM_WORLD, dataA[ii+kk*nb]);
        }
    }
    

    starpu_task_wait_for_all();
    double end = starpu_timing_now();



    for (int ii=0; ii<nb; ii++) {
        for (int jj=0; jj<nb; jj++) {
            if (jj <= ii) {
             if ((ii+jj*nb)%size == rank) {
                //cout<<ii<<" "<<jj<<":\n"<<*blocs[ii+jj*nb]<<endl;
            }
            //cout<<ii<<" "<<jj<<endl;
            //starpu_data_release(dataA[ii+jj*nb]);
            
            }
        }
    }
    LLT<Ref<MatrixXd>> llt(L);
    //cout<<"Ref:\n"<<L<<endl;
    MPI_Status status;
   for (int ii=0; ii<nb; ii++) {
        for (int jj=0; jj<nb; jj++) {
            if (jj<=ii)  {
            if (rank==0 && rank!=(ii+jj*nb)%size) {
                MPI_Recv(blocs[ii+jj*nb]->data(), n*n, MPI_DOUBLE, (ii+jj*nb)%size, (ii+jj*nb)%size, MPI_COMM_WORLD, &status);
                }

            else if (rank==(ii+jj*nb)%size) {
                MPI_Send(blocs[ii+jj*nb]->data(), n*n, MPI_DOUBLE, 0, (ii+jj*nb)%size, MPI_COMM_WORLD);
                }
            }
        }
    }


    if (rank==0) {
        printf("Elapsed time: %0.4f \n", (end-start)/1000000);
    for (int ii=0; ii<nb; ii++) {
        for (int jj=0; jj<nb; jj++) {
            L.block(ii*n,jj*n,n,n)=*blocs[ii+jj*nb];
            free(blocs[ii+jj*nb]);
        }
    }
    auto L1=L.triangularView<Lower>();
    VectorXd x = VectorXd::Random(n * nb);
    VectorXd b = B*x;
    VectorXd bref = b;
    L1.solveInPlace(b);
    L1.transpose().solveInPlace(b);
    double error = (b - x).norm() / x.norm();
    cout << "Error solve: " << error << endl;
    cout<<"rank "<<rank<<" finished....\n";
    }


    for (int ii=0; ii<nb; ii++) {
        for (int jj=0; jj<nb; jj++) {
            starpu_data_unregister(dataA[ii+jj*nb]); 
        }
    }
}





//Test
int main(int argc, char **argv)
{
    starpu_mpi_init_conf(&argc, &argv, 1, MPI_COMM_WORLD, NULL);
    int rank, size;
    starpu_mpi_comm_rank(MPI_COMM_WORLD, &rank);
    starpu_mpi_comm_size(MPI_COMM_WORLD, &size);
    //printf("Rank %d of %d ranks\n", rank, size);
    int n=10;
    int nb=1;
    if (argc >= 2)
    {
        n = atoi(argv[1]);
    }
    if (argc >= 3)
    {
        nb = atoi(argv[2]);
    }

    cholesky(n,nb, rank, size);
    //test(rank);
    starpu_mpi_shutdown();
    return 0;
}