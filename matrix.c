#include <stdio.h>
#include <stdlib.h>
#include <time.h>


int n; 
int i,j,k;
double **a, **b, **c;
double sum;
clock_t start_time, finish_time;
double delay_time, elapsed_time, elapsed_time_sec;

void allocate() {
   //allocate matrices
   a = (double**)malloc(n*sizeof(double*));
   b = (double**)malloc(n*sizeof(double*));
   c = (double**)malloc(n*sizeof(double*));
   for(i=0; i<n; ++i) {
      a[i] = (double*) malloc(n*sizeof(double));
      b[i] = (double*) malloc(n*sizeof(double));
      c[i] = (double*) malloc(n*sizeof(double));
   }

   //fill matrices with random values
   srand(1);
   for(i=0;i<n;++i) {
      for(j=0;j<n;++j) {
         a[i][j] = rand();
         b[i][j] = rand();
         //printf("%lf ",b[i][j]);
      }
      //printf("\n");
   }
}

void multiply() {
   //multiply matrices
   start_time = clock();
   for(i=0;i<n;++i) {
      for(j=0;j<n;++j) {
         sum=0;
         for(k=0;k<n;++k) {
            sum+=a[i][k]*b[k][j];
         }
         c[i][j]=sum;
      }
   }
}

void write() {
   //write result
   FILE* f = fopen("output.bin","wb");
   fwrite(c,sizeof(double),n*n,f);
}

int main(int argc, char* argv[])
{
   //check args
   if(argc != 2) {
      fprintf(stderr,"Usage: ./matrix N\n\tN: matrix size\n");
      exit(EXIT_FAILURE);
   }

   n = atoi(argv[1]); 

   allocate();

   //determine clock() overhead
   start_time = clock();
   finish_time = clock();
   delay_time = (double) (finish_time - start_time);

   multiply();

   finish_time = clock();
   elapsed_time = finish_time - start_time - delay_time;
   elapsed_time_sec = elapsed_time/CLOCKS_PER_SEC;
   printf("%lf %lf\n",elapsed_time, elapsed_time_sec);

   write();

   //deallocate matrices
   for(i=0;i<n;++i) {
      free(a[i]);
      free(b[i]);
      free(c[i]);
   }
   free(a);
   free(b);
   free(c);

   return 0;
}
