#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>

#define SIZE 1200            //size of the image
#define MASTER 0              //the master core
#define max_iterations 12000  //maximum iterations before the default fail
#define SUBWIDTH 4            //width of the subobject in MPI

/*Struct Definition*/
typedef struct Complex Complex;
struct Complex{
    float x;
    float y;
};

/* function declariations*/
void plot(int *image, FILE *img){
	int i, j;
	unsigned char *line = (unsigned char *)malloc(3*SIZE*sizeof(unsigned char));
	for(i=0; i<SIZE; i++) {
		for(j=0; j<SIZE; j++) {
			if (image[i+SIZE*j]<=63){
				line[3*j] = 255;
				line[3*j+1] = line[3*j+2] = 255 - 4*image[i+SIZE*j];
			}
			else{
				line[3*j] = 255;
				line[3*j+1] = image[i+SIZE*j]-63;
				line[3*j+2] = 0;
			}
			if (image[i+SIZE*j]==320){ line[3*j] = line[3*j+1] = line[3*j+2] = 255;
            }
		}
		fwrite(line, 1, 3*SIZE, img);
	}
	free(line);
}
int fracFun(Complex z0){
	int i;
	Complex z, z1;
	Complex c = {-0.4,0.6};

	z.x = z0.x;
	z.y = z0.y;
	for(i=0; i<max_iterations; i++){
		z1.x = z.x*z.x - z.y*z.y + c.x;
		z1.y = z.x*z.y*2. + c.y;
		z = z1;
		if ( (z.x*z.x + z.y*z.y) > 4.0 ) break;
	}
	return i < max_iterations ? i : max_iterations;
}


int main(int argc, char *argv[]){
    /*MPI definitions*/
    MPI_Init(&argc, &argv);             
    int   numtasks,     //The number of processors
          taskid;       //The number of the processor being observed
    MPI_Comm_rank(MPI_COMM_WORLD, &taskid);    //Set taskid
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);  //Set numtasks
    double t_start=MPI_Wtime();

    MPI_Status status;  
    /*Define the status to allow Send and Recv to probe and then communicate*/
    int chunk;
    int *chunk_buffer =(int*) malloc(5*sizeof(int));
    int totalChunks=((SIZE/SUBWIDTH)*SIZE);    
    Complex z;

    if(taskid==MASTER){
        int i,j;
        int *image = (int*)malloc(SIZE*SIZE*sizeof(int));
	
        FILE *img = fopen("fractal_script.ppm","w"); //Open the file in the master only
        fprintf(img,"P6\n%d %d 255\n",SIZE,SIZE);

        for(i=1;i<totalChunks;i++){
            MPI_Probe(1,MPI_ANY_TAG,MPI_COMM_WORLD, &status); 
            MPI_Recv(chunk_buffer, (SUBWIDTH+1), MPI_INT, status.MPI_SOURCE, status.MPI_TAG, 
            MPI_COMM_WORLD, MPI_STATUS_IGNORE);  //The master receives the processed data
            
            chunk=i;
            MPI_Send(&chunk, 1, MPI_INT,status.MPI_TAG, 52, MPI_COMM_WORLD);

            int a0=chunk_buffer[0];
    	    for(j=1;j<(SUBWIDTH+1);j++){
                image[a0+(j-1)]=chunk_buffer[j]; //Assign the other 4 elements using the first as a guide
            }
        }
    /*Write to image*/
        plot(image, img);
        fclose(img);
        free(image);
    }
    else{
            chunk=taskid;
            int i;
        while(chunk<totalChunks-1){
            chunk_buffer[0]=taskid;

            for(i=1;i<(SUBWIDTH+1);i++){
		        int pixel = (chunk*SUBWIDTH)+(i-1);    //Denote the first pixel in a chunk
    		    int x=pixel%SIZE;               //Calculate the x coordinate from the start pixel
	    	    int y=(pixel-x)/SIZE;           //Calculate the y coordinate from the start pixel

		        z.x=(((float) y / (float) SIZE) *2.f)-1.f;      //Map a pixel on the image to a coordinate  
		        z.y=-((((float) x / (float) SIZE) *2.f)-1.f);   //in the space (-1,1)x(-1,1)

		        chunk_buffer[i] = fracFun(z);   //Add the solution to the chunk_buffer
	        }

            MPI_Send(chunk_buffer, (SUBWIDTH+1), MPI_INT,0, taskid, MPI_COMM_WORLD);

            MPI_Probe(0,MPI_ANY_TAG,MPI_COMM_WORLD, &status); 
            MPI_Recv(&chunk, 1, MPI_INT, status.MPI_SOURCE, status.MPI_TAG, 
            MPI_COMM_WORLD, MPI_STATUS_IGNORE); //The master receives the processed data

            
        }
    }
    double t_end=MPI_Wtime();
    printf("t=%lf\n",t_end-t_start);
    MPI_Finalize();
    return 0;

}

