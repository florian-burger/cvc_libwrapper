/*****************************************************************************
 * contractions_io.c
 *
 * PURPOSE
 * - functions for i/o of contractions, derived from propagator_io
 * - uses lime and the DML checksum
 * TODO:
 * CHANGES:
 *
 *****************************************************************************/

#define _FILE_OFFSET_BITS 64

#include "lime.h" 
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h> 
#include <sys/types.h>
#include <math.h>
#ifdef MPI
#  include <mpi.h>
#  include <unistd.h>
#endif
#include "lime.h" 
#include "cvc_complex.h"
#include "global.h"
#include "cvc_geometry.h"
#include "dml.h"
#include "io_utils.h"
#include "propagator_io.h"
#include "contractions_io.h"

/* write an N-comp. contraction to file */

/****************************************************
 * write_binary_contraction_data
 ****************************************************/

int write_binary_contraction_data(double * const s, LimeWriter * limewriter,
                                      const int prec, const int N, DML_Checksum * ans) {

  int x, y, z, t, i=0, mu, status=0;
  double *tmp;
  float  *tmp2;
  int proc_coords[4], tloc,xloc,yloc,zloc, proc_id;
  n_uint64_t bytes;
  DML_SiteRank rank;
#ifdef MPI
  int iproc, tag;
  int tgeom[2];
  double *buffer;
  MPI_Status mstatus;
#endif
  DML_checksum_init(ans);

#if !(defined PARALLELTX) && !(defined PARALLELTXY) && !(defined PARALLELTXYZ)
  tmp = (double*)malloc(2*N*sizeof(double));
  tmp2 = (float*)malloc(2*N*sizeof(float));

  if(prec == 32) bytes = (n_uint64_t)2*N*sizeof(float);
  else bytes = (n_uint64_t)2*N*sizeof(double);

  if(g_cart_id==0) {
    for(t = 0; t < T; t++) {
      for(x = 0; x < LX; x++) {
      for(y = 0; y < LY; y++) {
      for(z = 0; z < LZ; z++) {
        /* Rank should be computed by proc 0 only */
        rank = (DML_SiteRank) ((( (t+Tstart)*LX + x)*LY + y)*LZ + z);
        for(mu=0; mu<N; mu++) {
          i = _GWI(mu, g_ipt[t][x][y][z], VOLUME);
          if(prec == 32) {
            double2single((float*)(tmp2+2*mu), (s + i), 2);
          } else {
            tmp[2*mu  ] = s[i  ];
            tmp[2*mu+1] = s[i+1];
          }
        }
        if(prec == 32) {
          DML_checksum_accum(ans,rank,(char *) tmp2,2*N*sizeof(float));
          status = limeWriteRecordData((void*)tmp2, &bytes, limewriter);
        }
        else {
          status = limeWriteRecordData((void*)tmp, &bytes, limewriter);
          DML_checksum_accum(ans,rank,(char *) tmp, 2*N*sizeof(double));
        }
      }
      }
      }
    }
  }
#ifdef MPI
  tgeom[0] = Tstart;
  tgeom[1] = T;
  if( (buffer = (double*)malloc(2*N*LX*LY*LZ*sizeof(double))) == (double*)NULL ) {
    fprintf(stderr, "Error from malloc for buffer\n");
    MPI_Abort(MPI_COMM_WORLD, 1);
    MPI_Finalize();
  }
  for(iproc=1; iproc<g_nproc; iproc++) {
    if(g_cart_id==0) {
      tag = 2 * iproc;
      MPI_Recv((void*)tgeom, 2, MPI_INT, iproc, tag, g_cart_grid, &mstatus);
      fprintf(stdout, "# iproc = %d; Tstart = %d, T = %d\n", iproc, tgeom[0], tgeom[1]);
       
      for(t=0; t<tgeom[1]; t++) {
        tag = 2 * ( t*g_nproc + iproc ) + 1;
        MPI_Recv((void*)buffer, 2*N*LX*LY*LZ, MPI_DOUBLE,  iproc, tag, g_cart_grid, &mstatus);

        i=0;
        for(x=0; x<LX; x++) {
        for(y=0; y<LY; y++) {
        for(z=0; z<LZ; z++) {
          /* Rank should be computed by proc 0 only */
          rank = (DML_SiteRank) ((( (t+tgeom[0])*LX + x)*LY + y)*LZ + z);
          if(prec == 32) {
            double2single((float*)tmp2, (buffer + i), 2*N);
            DML_checksum_accum(ans,rank,(char *) tmp2,2*N*sizeof(float));
            status = limeWriteRecordData((void*)tmp2, &bytes, limewriter);
          } else {
            status = limeWriteRecordData((void*)(buffer+i), &bytes, limewriter);
            DML_checksum_accum(ans,rank,(char *) (buffer+i), 2*N*sizeof(double));
          }
          i += 2*N;
        }
        }
        }
      }
    }
    if(g_cart_id==iproc) {
      tag = 2 * iproc;
      MPI_Send((void*)tgeom, 2, MPI_INT, 0, tag, g_cart_grid);
      for(t=0; t<T; t++) {
        i=0;
        for(x=0; x<LX; x++) {  
        for(y=0; y<LY; y++) {  
        for(z=0; z<LZ; z++) {
          for(mu=0; mu<N; mu++) {
            buffer[i  ] = s[_GWI(mu,g_ipt[t][x][y][z],VOLUME)  ];
            buffer[i+1] = s[_GWI(mu,g_ipt[t][x][y][z],VOLUME)+1];
            i+=2;
          }
        }
        }
        }
        tag = 2 * ( t*g_nproc + iproc) + 1;
        MPI_Send((void*)buffer, 2*N*LX*LY*LZ, MPI_DOUBLE, 0, tag, g_cart_grid);
      }
    }
    MPI_Barrier(g_cart_grid);

  } /* of iproc = 1, ..., g_nproc-1 */
  free(buffer);
#endif

#elif (defined PARALLELTXY)  && !(defined PARALLELTX)
  tmp = (double*)malloc(2*N*LZ*sizeof(double));
  tmp2 = (float*)malloc(2*N*LZ*sizeof(float));

  if( (buffer = (double*)malloc(2*N*LZ*sizeof(double))) == (double*)NULL ) {
    fprintf(stderr, "Error from malloc for buffer\n");
    MPI_Abort(MPI_COMM_WORLD, 115);
    MPI_Finalize();
    exit(115);
  }

  if(prec == 32) bytes = (n_uint64_t)2*N*sizeof(float);   // single precision 
  else           bytes = (n_uint64_t)2*N*sizeof(double);  // double precision

  proc_coords[3] = 0;
  zloc = 0;

  for(t = 0; t < T_global; t++) {
    proc_coords[0] = t / T;
    tloc = t % T;
    for(x = 0; x < LX_global; x++) {
      proc_coords[1] = x / LX;
      xloc = x % LX;
      for(y = 0; y < LY_global; y++) {
        proc_coords[2] = y / LY;
        yloc = y % LY;
        
        MPI_Cart_rank(g_cart_grid, proc_coords, &proc_id);
        if(g_cart_id == 0) {
          // fprintf(stdout, "\t(%d,%d,%d,%d) ---> (%d,%d,%d,%d) = %d\n", t,x,y,0, \
              proc_coords[0],  proc_coords[1],  proc_coords[2],  proc_coords[3], proc_id);
        }
        tag = (t*LX+x)*LY+y;

        // a send and recv must follow
        if(g_cart_id == 0) {
          if(g_cart_id == proc_id) {
            // fprintf(stdout, "process 0 writing own data for (t,x,y)=(%d,%d,%d) / (%d,%d,%d) ...\n", t,x,y, tloc,xloc,yloc);
            i=0;
            for(z=0; z<LZ; z++) {
              for(mu=0; mu<N; mu++) {
                buffer[i  ] = s[_GWI(mu,g_ipt[tloc][xloc][yloc][z],VOLUME)  ];
                buffer[i+1] = s[_GWI(mu,g_ipt[tloc][xloc][yloc][z],VOLUME)+1];
                i+=2;
              }
            }
            i=0;
            for(z = 0; z < LZ; z++) {
              /* Rank should be computed by proc 0 only */
              rank = (DML_SiteRank) ((( t*LX_global + x)*LY_global + y)*LZ + z);
              if(prec == 32) {
                double2single((float*)tmp2, (buffer + i), 2*N);
                DML_checksum_accum(ans,rank,(char *) tmp2,2*N*sizeof(float));
                status = limeWriteRecordData((void*)tmp2, &bytes, limewriter);
              } else {
                status = limeWriteRecordData((void*)(buffer+i), &bytes, limewriter);
                DML_checksum_accum(ans,rank,(char *) (buffer+i), 2*N*sizeof(double));
              }
              i += 2*N;
            }
          } else {
            MPI_Recv(buffer, 2*N*LZ, MPI_DOUBLE, proc_id, tag, g_cart_grid, &mstatus);
            // fprintf(stdout, "process 0 receiving data from process %d for (t,x,y)=(%d,%d,%d) ...\n", proc_id, t,x,y);
            i=0;
            for(z=0; z<LZ; z++) {
              /* Rank should be computed by proc 0 only */
              rank = (DML_SiteRank) ((( t * LX_global + x ) * LY_global + y ) * LZ + z);
              // fprintf(stdout, "(%d,%d,%d,%d)---> rank = %d\n", t,x,y,z, rank);
              if(prec == 32) {
                double2single((float*)tmp2, (buffer + i), 2*N);
                DML_checksum_accum(ans,rank,(char *) tmp2,2*N*sizeof(float));
                status = limeWriteRecordData((void*)tmp2, &bytes, limewriter);
              } else {
                status = limeWriteRecordData((void*)(buffer+i), &bytes, limewriter);
                DML_checksum_accum(ans,rank,(char *) (buffer+i), 2*N*sizeof(double));
              }
              i += 2*N;
            }
          }
        } else {
          if(g_cart_id == proc_id) {
            i=0;
            for(z=0; z<LZ; z++) {
              for(mu=0; mu<N; mu++) {
                buffer[i  ] = s[_GWI(mu,g_ipt[tloc][xloc][yloc][z],VOLUME)  ];
                buffer[i+1] = s[_GWI(mu,g_ipt[tloc][xloc][yloc][z],VOLUME)+1];
                i+=2;
              }
            }
            // fprintf(stdout, "process %d sending own data...\n", g_cart_id);
            MPI_Send(buffer, 2*N*LZ, MPI_DOUBLE, 0, tag, g_cart_grid);
          }
        }
      }    // of y
    }      // of x
  }        // of t

  free(buffer);
  
#if !(defined PARALLELTX) && !(defined PARALLELTXY) && (defined PARALLELTXYZ) 
  tmp = (double*)malloc(2*N*sizeof(double));
  tmp2 = (float*)malloc(2*N*sizeof(float));

  if(prec == 32) bytes = (n_uint64_t)2*N*sizeof(float);
  else bytes = (n_uint64_t)2*N*sizeof(double);

  if(g_cart_id==0) {
    for(t = 0; t < T; t++) {
      for(x = 0; x < LX; x++) {
      for(y = 0; y < LY; y++) {
      for(z = 0; z < LZ; z++) {
        /* Rank should be computed by proc 0 only */
        rank = (DML_SiteRank) ((( (t+Tstart)*LX + x)*LY + y)*LZ + z);
        for(mu=0; mu<N; mu++) {
          i = _GWI(mu, g_ipt[t][x][y][z], VOLUME);
          if(prec == 32) {
            double2single((float*)(tmp2+2*mu), (s + i), 2);
          } else {
            tmp[2*mu  ] = s[i  ];
            tmp[2*mu+1] = s[i+1];
          }
        }
        if(prec == 32) {
          DML_checksum_accum(ans,rank,(char *) tmp2,2*N*sizeof(float));
          status = limeWriteRecordData((void*)tmp2, &bytes, limewriter);
        }
        else {
          status = limeWriteRecordData((void*)tmp, &bytes, limewriter);
          DML_checksum_accum(ans,rank,(char *) tmp, 2*N*sizeof(double));
        }
      }
      }
      }
    }
  }
#ifdef MPI
  tgeom[0] = Tstart;
  tgeom[1] = T;
  if( (buffer = (double*)malloc(2*N*LX*LY*LZ*sizeof(double))) == (double*)NULL ) {
    fprintf(stderr, "Error from malloc for buffer\n");
    MPI_Abort(MPI_COMM_WORLD, 1);
    MPI_Finalize();
  }
  for(iproc=1; iproc<g_nproc; iproc++) {
    if(g_cart_id==0) {
      tag = 2 * iproc;
      MPI_Recv((void*)tgeom, 2, MPI_INT, iproc, tag, g_cart_grid, &mstatus);
      fprintf(stdout, "# iproc = %d; Tstart = %d, T = %d\n", iproc, tgeom[0], tgeom[1]);
       
      for(t=0; t<tgeom[1]; t++) {
        tag = 2 * ( t*g_nproc + iproc ) + 1;
        MPI_Recv((void*)buffer, 2*N*LX*LY*LZ, MPI_DOUBLE,  iproc, tag, g_cart_grid, &mstatus);

        i=0;
        for(x=0; x<LX; x++) {
        for(y=0; y<LY; y++) {
        for(z=0; z<LZ; z++) {
          /* Rank should be computed by proc 0 only */
          rank = (DML_SiteRank) ((( (t+tgeom[0])*LX + x)*LY + y)*LZ + z);
          if(prec == 32) {
            double2single((float*)tmp2, (buffer + i), 2*N);
            DML_checksum_accum(ans,rank,(char *) tmp2,2*N*sizeof(float));
            status = limeWriteRecordData((void*)tmp2, &bytes, limewriter);
          } else {
            status = limeWriteRecordData((void*)(buffer+i), &bytes, limewriter);
            DML_checksum_accum(ans,rank,(char *) (buffer+i), 2*N*sizeof(double));
          }
          i += 2*N;
        }
        }
        }
      }
    }
    if(g_cart_id==iproc) {
      tag = 2 * iproc;
      MPI_Send((void*)tgeom, 2, MPI_INT, 0, tag, g_cart_grid);
      for(t=0; t<T; t++) {
        i=0;
        for(x=0; x<LX; x++) {  
        for(y=0; y<LY; y++) {  
        for(z=0; z<LZ; z++) {
          for(mu=0; mu<N; mu++) {
            buffer[i  ] = s[_GWI(mu,g_ipt[t][x][y][z],VOLUME)  ];
            buffer[i+1] = s[_GWI(mu,g_ipt[t][x][y][z],VOLUME)+1];
            i+=2;
          }
        }
        }
        }
        tag = 2 * ( t*g_nproc + iproc) + 1;
        MPI_Send((void*)buffer, 2*N*LX*LY*LZ, MPI_DOUBLE, 0, tag, g_cart_grid);
      }
    }
    MPI_Barrier(g_cart_grid);

  } /* of iproc = 1, ..., g_nproc-1 */
  free(buffer);
#endif

#elif (defined PARALLELTXYZ)  && !(defined PARALLELTXY) && !(defined PARALLELTX)
  tmp = (double*)malloc(2*N*sizeof(double));
  tmp2 = (float*)malloc(2*N*sizeof(float));

  if( (buffer = (double*)malloc(2*N*sizeof(double))) == (double*)NULL ) {
    fprintf(stderr, "Error from malloc for buffer\n");
    MPI_Abort(MPI_COMM_WORLD, 115);
    MPI_Finalize();
    exit(115);
  }

  if(prec == 32) bytes = (n_uint64_t)2*N*sizeof(float);   // single precision 
  else           bytes = (n_uint64_t)2*N*sizeof(double);  // double precision


  for(t = 0; t < T_global; t++) {
    proc_coords[0] = t / T;
    tloc = t % T;
    for(x = 0; x < LX_global; x++) {
      proc_coords[1] = x / LX;
      xloc = x % LX;
      for(y = 0; y < LY_global; y++) {
        proc_coords[2] = y / LY;
        yloc = y % LY;
        for(z = 0; z < LZ_global; z++) {        
	  proc_coords[3] = z / LZ;
	  zloc = z % LZ;
	  
	  MPI_Cart_rank(g_cart_grid, proc_coords, &proc_id);
	  if(g_cart_id == 0) {
	    // fprintf(stdout, "\t(%d,%d,%d,%d) ---> (%d,%d,%d,%d) = %d\n", t,x,y,0, \
		proc_coords[0],  proc_coords[1],  proc_coords[2],  proc_coords[3], proc_id);
	  }
	  tag = ((t*LX+x)*LY+y)*LZ+z;

	  // a send and recv must follow
	  if(g_cart_id == 0) {
	    if(g_cart_id == proc_id) {
	      // fprintf(stdout, "process 0 writing own data for (t,x,y)=(%d,%d,%d) / (%d,%d,%d) ...\n", t,x,y, tloc,xloc,yloc);
	      i=0;
	      for(mu=0; mu<N; mu++) {
		buffer[i  ] = s[_GWI(mu,g_ipt[tloc][xloc][yloc][z],VOLUME)  ];
		buffer[i+1] = s[_GWI(mu,g_ipt[tloc][xloc][yloc][z],VOLUME)+1];
		i+=2;
	      }
	      i=0;
	      for(z = 0; z < LZ; z++) {
		/* Rank should be computed by proc 0 only */
		rank = (DML_SiteRank) ((( t*LX_global + x)*LY_global + y)*LZ_global + z);
		if(prec == 32) {
		  double2single((float*)tmp2, (buffer + i), 2*N);
		  DML_checksum_accum(ans,rank,(char *) tmp2,2*N*sizeof(float));
		  status = limeWriteRecordData((void*)tmp2, &bytes, limewriter);
		} else {
		  status = limeWriteRecordData((void*)(buffer+i), &bytes, limewriter);
		  DML_checksum_accum(ans,rank,(char *) (buffer+i), 2*N*sizeof(double));
		}
		i += 2*N;
	      }
	    } else {
	      MPI_Recv(buffer, 2*N, MPI_DOUBLE, proc_id, tag, g_cart_grid, &mstatus);
	      // fprintf(stdout, "process 0 receiving data from process %d for (t,x,y)=(%d,%d,%d) ...\n", proc_id, t,x,y);
	      i=0;
	      /* Rank should be computed by proc 0 only */
	      rank = (DML_SiteRank) ((( t * LX_global + x ) * LY_global + y ) * LZ_global + z);
	      // fprintf(stdout, "(%d,%d,%d,%d)---> rank = %d\n", t,x,y,z, rank);
	      if(prec == 32) {
		double2single((float*)tmp2, (buffer + i), 2*N);
		DML_checksum_accum(ans,rank,(char *) tmp2,2*N*sizeof(float));
		status = limeWriteRecordData((void*)tmp2, &bytes, limewriter);
	      } else {
		status = limeWriteRecordData((void*)(buffer+i), &bytes, limewriter);
		DML_checksum_accum(ans,rank,(char *) (buffer+i), 2*N*sizeof(double));
	      }
	      i += 2*N;
	    }
	  } else {
	    if(g_cart_id == proc_id) {
	      i=0;
	      for(mu=0; mu<N; mu++) {
		buffer[i  ] = s[_GWI(mu,g_ipt[tloc][xloc][yloc][zloc],VOLUME)  ];
		buffer[i+1] = s[_GWI(mu,g_ipt[tloc][xloc][yloc][zloc],VOLUME)+1];
		i+=2;
	      }
	      // fprintf(stdout, "process %d sending own data...\n", g_cart_id);
	      MPI_Send(buffer, 2*N, MPI_DOUBLE, 0, tag, g_cart_grid);
	    }
	  }
	}  // of z
      }    // of y
    }      // of x
  }        // of t

  free(buffer);  
  
  
  
#elif (defined PARALLELTX)  && !(defined PARALLELTXY)
  if(g_cart_id == 0) {
    fprintf(stderr, "\n[write_binary_contraction_data] Error: no version implemented for PARALLELTX\n");
  }
  return(-1);
#endif

  free(tmp2);
  free(tmp);

#ifdef MPI
  MPI_Barrier(g_cart_grid);
#endif

//#ifdef MPI
//  DML_checksum_combine(ans);
//#endif
//   if(g_cart_id == 0) printf("\n# [write_binary_contraction_data] The final checksum is %#lx %#lx\n", (*ans).suma, (*ans).sumb);
   
  return(0);
}

/************************************************************
 * read_binary_contraction_data
 ************************************************************/

int read_binary_contraction_data(double * const s, LimeReader * limereader, 
			    const int prec, const int N, DML_Checksum *ans) {

  int status=0, mu;
  n_uint64_t bytes, ix;
  double *tmp;
  DML_SiteRank rank;
  float *tmp2;
  int t, x, y, z;

  DML_checksum_init(ans);
  rank = (DML_SiteRank) 0;
 
  if( (tmp = (double*)malloc(2*N*sizeof(double))) == (double*)NULL ) {
#ifdef MPI
    MPI_Abort(MPI_COMM_WORLD, 1);
    MPI_Finalize();
#endif
    exit(500);
  }
  if( (tmp2 = (float*)malloc(2*N*sizeof(float))) == (float*)NULL ) {
#ifdef MPI
    MPI_Abort(MPI_COMM_WORLD, 1);
    MPI_Finalize();
#endif
    exit(501);
  }
 
 
  if(prec == 32) bytes = 2*N*sizeof(float);
  else bytes = 2*N*sizeof(double);
#ifdef MPI
  limeReaderSeek(limereader,(n_uint64_t) (Tstart*(LX*g_nproc_x)*(LY*g_nproc_y)*LZ + LXstart*(LY*g_nproc_y)*LZ + LYstart*LZ)*bytes, SEEK_SET);
#endif
  for(t = 0; t < T; t++){
  for(x = 0; x < LX; x++){
  for(y = 0; y < LY; y++){
  for(z = 0; z < LZ; z++){
    ix = g_ipt[t][x][y][z];
    rank = (DML_SiteRank) (((t+Tstart)*(LX*g_nproc_x) + LXstart + x)*(LY*g_nproc_y) + LYstart + y)*LZ + z;
    if(prec == 32) {
      status = limeReaderReadData(tmp2, &bytes, limereader);
      DML_checksum_accum(ans,rank,(char *) tmp2, bytes);	    
    }
    else {
      status = limeReaderReadData(tmp, &bytes, limereader);
      DML_checksum_accum(ans,rank,(char *) tmp, bytes);
    }
 
    for(mu=0; mu<N; mu++) {
      if(prec == 32) {
        single2double(s + _GWI(mu,ix,VOLUME), (float*)(tmp2+2*mu), 2);
      }
      else {
        s[_GWI(mu, ix,VOLUME)  ] = tmp[2*mu  ];
        s[_GWI(mu, ix,VOLUME)+1] = tmp[2*mu+1];
      }
    }

    if(status < 0 && status != LIME_EOR) {
      return(-1);
    }
  }
  }
  }
  }
#ifdef MPI
  DML_checksum_combine(ans);
#endif
  if(g_cart_id == 0) printf("\n# [read_binary_contraction_data] The final checksum is %#lx %#lx\n", (*ans).suma, (*ans).sumb);

  free(tmp2); free(tmp);
  return(0);
}

/**************************************************************
 * write_contraction_format
 **************************************************************/

int write_contraction_format(char * filename, const int prec, const int N, char * type, const int gid, const int sid) {
  FILE * ofs = NULL;
  LimeWriter * limewriter = NULL;
  LimeRecordHeader * limeheader = NULL;
  int status = 0;
  int ME_flag=0, MB_flag=1;
  char message[500];
  n_uint64_t bytes;

  if(g_cart_id==0) {
    ofs = fopen(filename, "w");
  
    if(ofs == (FILE*)NULL) {
      fprintf(stderr, "Could not open file %s for writing!\n Aborting...\n", filename);
#ifdef MPI
      MPI_Abort(MPI_COMM_WORLD, 1);
      MPI_Finalize();
#endif
      exit(500);
    }
    limewriter = limeCreateWriter( ofs );
    if(limewriter == (LimeWriter*)NULL) {
      fprintf(stderr, "LIME error in file %s for writing!\n Aborting...\n", filename);
#ifdef MPI
      MPI_Abort(MPI_COMM_WORLD, 1);
      MPI_Finalize();
#endif
      exit(500);
    }
  
    sprintf(message, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<cvcFormat>\n<type>%s</type>\n<precision>%d</precision>\n<components>%d</components>\n<lx>%d</lx>\n<ly>%d</ly>\n<lz>%d</lz>\n<lt>%d</lt>\n<nconf>%d</nconf>\n<source>%d</source>\n</cvcFormat>", type, prec, N, LX*g_nproc_x, LY*g_nproc_y, LZ, T_global, gid, sid);
    bytes = strlen( message );
    limeheader = limeCreateHeader(MB_flag, ME_flag, "cvc-contraction-format", bytes);
    status = limeWriteRecordHeader( limeheader, limewriter);
    if(status < 0 ) {
      fprintf(stderr, "LIME write header error %d\n", status);
#ifdef MPI
      MPI_Abort(MPI_COMM_WORLD, 1);
      MPI_Finalize();
#endif
      exit(500);
    }
    limeDestroyHeader( limeheader );
    limeWriteRecordData(message, &bytes, limewriter);
  
    limeDestroyWriter( limewriter );
    fflush(ofs);
    fclose(ofs);
  }
  return(0);
}


/***********************************************************
 * write_lime_contraction
 ***********************************************************/

int write_lime_contraction(double * const s, char * filename, const int prec, const int N, char * type, const int gid, const int sid) {

  FILE * ofs = NULL;
  LimeWriter * limewriter = NULL;
  LimeRecordHeader * limeheader = NULL;
  int status = 0;
  int ME_flag=0, MB_flag=0;
  n_uint64_t bytes;
  DML_Checksum checksum;

  write_contraction_format(filename, prec, N, type, gid, sid);

  if(g_cart_id==0) {
    ofs = fopen(filename, "a");
    if(ofs == (FILE*)NULL) {
      fprintf(stderr, "Could not open file %s for writing!\n Aborting...\n", filename);
#ifdef MPI
      MPI_Abort(MPI_COMM_WORLD, 1);
      MPI_Finalize();
#endif
      exit(500);
    }
  
    limewriter = limeCreateWriter( ofs );
    if(limewriter == (LimeWriter*)NULL) {
      fprintf(stderr, "LIME error in file %s for writing!\n Aborting...\n", filename);
#ifdef MPI
      MPI_Abort(MPI_COMM_WORLD, 1);
      MPI_Finalize();
#endif
      exit(500);
    }

    bytes = LX*g_nproc_x*LY*g_nproc_y*LZ*T_global*(n_uint64_t)2*N*sizeof(double)*prec/64;
    MB_flag=0; ME_flag=1;
    limeheader = limeCreateHeader(MB_flag, ME_flag, "scidac-binary-data", bytes);
    status = limeWriteRecordHeader( limeheader, limewriter);
    if(status < 0 ) {
      fprintf(stderr, "LIME write header (scidac-binary-data) error %d\n", status);
#ifdef MPI
      MPI_Abort(MPI_COMM_WORLD, 1);
      MPI_Finalize();
#endif
      exit(500);
    }
    limeDestroyHeader( limeheader );
  }
  
  status = write_binary_contraction_data(s, limewriter, prec, N, &checksum);
  if(g_cart_id==0) {
    printf("# Final check sum is (%#lx  %#lx)\n", checksum.suma, checksum.sumb);
    if(ferror(ofs)) {
      fprintf(stderr, "Warning! Error while writing to file %s \n", filename);
    }
    limeDestroyWriter( limewriter );
    fflush(ofs);
    fclose(ofs);
  }
  write_checksum(filename, &checksum);
  return(0);
}

/***********************************************************
 * read_lime_contraction
 ***********************************************************/

int read_lime_contraction(double * const s, char * filename, const int N, const int position) {
  FILE *ifs=(FILE*)NULL;
  int status=0, getpos=-1;
  n_uint64_t bytes;
  char * header_type;
  LimeReader * limereader;
  int prec = 32;
  DML_Checksum checksum;

  if((ifs = fopen(filename, "r")) == (FILE*)NULL) {
    if(g_proc_id == 0) {
      fprintf(stderr, "Error opening file %s\n", filename);
    }
    return(106);
  }

  limereader = limeCreateReader( ifs );
  if( limereader == (LimeReader *)NULL ) {
    if(g_proc_id == 0) {
      fprintf(stderr, "Unable to open LimeReader\n");
    }
    return(-1);
  }
  while( (status = limeReaderNextRecord(limereader)) != LIME_EOF ) {
    if(status != LIME_SUCCESS ) {
      fprintf(stderr, "limeReaderNextRecord returned error with status = %d!\n", status);
      status = LIME_EOF;
      break;
    }
    header_type = limeReaderType(limereader);
    if(strcmp("scidac-binary-data",header_type) == 0) getpos++;
    if(getpos == position) break;
  }
  if(status == LIME_EOF) {
    if(g_proc_id == 0) {
      fprintf(stderr, "no scidac-binary-data record found in file %s\n",filename);
    }
    limeDestroyReader(limereader);
    fclose(ifs);
    if(g_proc_id == 0) {
      fprintf(stderr, "try to read in non-lime format\n");
    }
    return(read_contraction(s, NULL, filename, N));
  }
  bytes = limeReaderBytes(limereader);
  if((size_t)bytes == (LX*g_nproc_x)*(LY*g_nproc_y)*LZ*T_global*2*N*sizeof(double)) prec = 64;
  else if((size_t)bytes == (LX*g_nproc_x)*(LY*g_nproc_y)*LZ*T_global*2*N*sizeof(float)) prec = 32;
  else {
    if(g_proc_id == 0) {
      fprintf(stderr, "wrong length in contraction: bytes = %lu, not %d. Aborting read!\n", bytes, (LX*g_nproc_x)*(LY*g_nproc_y)*LZ*T_global*2*N*sizeof(float));
    }
    return(-1);
  }
  if(g_proc_id == 0) {
    printf("# %d Bit precision read\n", prec);
  }

  status = read_binary_contraction_data(s, limereader, prec, N, &checksum);

  if(g_proc_id == 0) {
    printf("# checksum for contractions in file %s position %d is %#x %#x\n",
           filename, position, checksum.suma, checksum.sumb);
  }

  if(status < 0) {
    fprintf(stderr, "LIME read error occured with status = %d while reading file %s!\n Aborting...\n",
            status, filename);
#ifdef MPI
    MPI_Abort(MPI_COMM_WORLD, 1);
    MPI_Finalize();
#endif
    exit(500);
  }

  limeDestroyReader(limereader);
  fclose(ifs);
  return(0);
}

