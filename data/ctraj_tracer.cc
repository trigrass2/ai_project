//
// Copywrite 2004 Peter Mills.  All rights reserved.
//

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
//#include <getopt.h>
#include <stdlib.h>

#include "error_codes.h"

#include "peteys_tmpl_lib.h"
#include "parse_command_opts.h"

#include "sparse.h"

#include "tcoord_defs.h"
#include "coordtran.h"
#include "ctraj_defaults.h"

#include "ctraj_vfield_standard.h"
#include "ctraj_vfield_2d.h"
#include "ctraj_vfield_anal.h"

#include "ctraj_tfield_standard.h"
#include "ctraj_tfield_nd.h"

#define MAXLL 200
#define DATE_WIDTH 23

#define RMAX 10000

int main(int argc, char *argv[]) {
  char *initfile=NULL;
  FILE *initfs;
  char line[MAXLL];

  //main data file names:
  char outfile[MAXLL];

  char c;
  size_t ncon;

  //basic data structures/engine:
  ctraj_vfield_base<float> *vfield;
  ctraj_tfield_base<float> *tracer;
  sparse<int32_t, float> map;

  //composite dataset containing velocity field:
  //time step info:
  ind_type n;				//number of time steps
  int32_t nfine=TSTEP_NFINE;		//number of trajectory time steps
  double tstep_fine;			//time step for trajectory calc.
  float tstep=TSTEP_COARSE;		//time step for tracer field

  //general date variables:
  char date_str[DATE_WIDTH];		//a date as a string

  //time indices:
  double tind1, tind2;
  double *tind;				//vector of time indices
  ind_type lt;				//time index as integer

  //dimensions:
  int32_t ndim=2;
  int32_t nwt, nwtmax;
  int32_t nmap;

  //stuff for integration:
  float **x0;				//initial conditions
  int32_t *d0;				//initial domain 
  float **result;			//integrated values

  void *param[2];

  az_eq_t<float> *metric;

  //interpolation indices:
  int32_t *ind;
  double *wt;
  int32_t domain;

  FILE *outfun;				//output file unit

  int32_t vtype=0;
  int qflag=0;		//just print out the dates...
  int argc0;

  int flag[20];
  void *optarg[20];

  tind1=0;

  optarg[0]=&tstep;
  optarg[1]=&nfine;
  optarg[2]=&tind1;
  optarg[3]=&n;
  optarg[4]=&vtype;
  optarg[9]=&ndim;

  argc0=argc;

  argc=parse_command_opts(argc, argv, "hk0NVifQ?L", "%g%d%lg%d%d%s%s%%%d", optarg, flag, OPT_WHITESPACE+2);
  if (argc < 0) {
    fprintf(stderr, "ctraj_tracer: error parsing command line");
    argc=-argc;
  }
  qflag=flag[7];

  switch (vtype) {
    case (0):
      vfield=new ctraj_vfield_standard<float>();
      tracer=new ctraj_tfield_standard<float>();
      break;
    case (1):
      vfield=new ctraj_vfield_2d<float>();
      tracer=new ctraj_tfield_nd<float>(2);
      break;
    case (2):
      vfield=new ctraj_vfield_anal<float>(ndim);
      tracer=new ctraj_tfield_nd<float>(ndim);
      break;
    default:
      vfield=new ctraj_vfield_standard<float>();
      tracer=new ctraj_tfield_standard<float>();
      break;
  }

  if ((argc <= 2) || flag[8]) {
    FILE *docfs;
    int err;
    if (flag[3]) {
      docfs=stdout;
      err=0;
    } else {
      docfs=stderr;
      err=INSUFFICIENT_COMMAND_ARGS;
    }

    fprintf(docfs, "Purpose: two-dimensional, 'semi-Lagrangian' tracer simulation driven \n");
    fprintf(docfs, "by globally gridded wind fields.  For each time step, outputs a sparse \n");
    fprintf(docfs, "matrix defining the mapping from one tracer field to the next\n");
    fprintf(docfs, "\n");
    fprintf(docfs, "syntax:\n");
    fprintf(docfs, "ctraj_tracer [-Q] [-h dt] [-k nfine] [tracer arguments]\n");
    fprintf(docfs, "               [v-field arguments] outfile}\n");
    fprintf(docfs, "\n");
    fprintf(docfs, " where:\n");
    fprintf(docfs, "   outfile   is the binary output file\n");
    fprintf(docfs, "\n");
    fprintf(docfs, "options:\n");
    ctraj_optargs(docfs, "hkif0NVQ?", 1);
    if (flag[4]) {
      fprintf(docfs, "\n");
      vfield->help(docfs);
      fprintf(docfs, "\n");
      tracer->help(docfs);
    } else {
      fprintf(docfs, "\nFor tracer and velocity field options, set -V flag\n");
    }
    fprintf(docfs, "\n");
    return err;
  }

  tracer->setup(argc, argv);
  nwtmax=tracer->nwt();

  vfield->setup(argc, argv);
  //for (int i=0; i<argc0; i++) printf("%s ", argv[i]);
  //printf("\n");

  if (typeid(*vfield)==typeid(ctraj_vfield_standard<float>) &&
  		typeid(*tracer)==typeid(ctraj_tfield_standard<float>)) {
    metric=((ctraj_vfield_standard<float> *) vfield)->get_metric();
    ((ctraj_tfield_standard<float> *) tracer)->set_metric(metric);
  }

  if (flag[5]) tind1=vfield->get_tind((char *) optarg[5]);
  if (flag[6]) {
    double tindf=vfield->get_tind((char *) optarg[6]);
    if (tindf<tind1) {
      n=ceil(tind1-tindf);
      tstep=-tstep;
    } else {
      n=ceil(vfield->get_tind((char *) optarg[6])-tind1);
    }
  } else if (flag[3]==0) {
    n=vfield->maxt()-tind1;
  }

  strcpy(outfile, argv[1]);

  tind=new double[n+1];
  //figure out its location in relation to the velocity field:
  tind[0]=tind1;
  tstep_fine=-tstep/nfine;

  for (long i=1; i<=n; i++) tind[i]=tind[i-1]+tstep;

  nmap=tracer->nel();

  //initialize the sparse matrix:
  map.extend(nmap*nwtmax);

  //initialize variables for initial conditions:
  x0=new float * [nmap];
  x0[0]=new float[nmap*ndim];
  for (int32_t i=1; i<nmap; i++) x0[i]=x0[0]+i*ndim;
  d0=new int32_t[nmap];			//initial domain

  //get the initial conditions:
  for (int32_t i=0; i<nmap; i++) {
    d0[i]=tracer->get_loc(i, x0[i]);
    //printf("%3d ", d0[i]);
    //for (int32_t j=0; j<ndim; j++) printf("%12.4g ", x0[i][j]);
    //printf("\n");
  }

  //initialize interpolation variables:
  wt=new double[nwtmax];
  ind=new int32_t[nwtmax];

  //initialize the vector of results for the Runge-Kutta integrations:
  result=new float * [nfine+1];
  result[0]=new float[ndim*(nfine+1)];
  for (int32_t i=1; i<=nfine; i++) result[i]=result[0]+i*ndim;

  param[0]=vfield;

  //open the output file and write the headers:
  if (qflag==0) outfun=fopen(outfile, "w");

  for (ind_type it=0; it<n; it++) {
    vfield->get_t(tind[it], date_str);

    printf("%d %s\n", it, date_str);

    if (qflag) continue;

    map.reset(nmap, nmap);

    for (int32_t i=0; i<nmap; i++) {
      //set initial conditions:
      param[1]=d0+i;

      //do a Runge-Kutta integration:
      rk_dumb(tind[it+1], x0[i], ndim, tstep_fine, nfine, result, (void *) param, &ctraj_deriv);

      nwt=tracer->interpolate(d0[i], result[nfine], ind, wt);

      float tw=0;
      if (nwt < 0) {
        //map.add_el(1., i, i);
        int32_t nmiss=0;
        //exclude mssing indices and weights less than 0:
        for (int32_t j=0; j<nwtmax; j++) {
          if (ind<0) {
            nmiss=1;
          }
          ind[j]=ind[j+nmiss];
          wt[j]=wt[j+nmiss];
          if (wt[j]<0) wt[j]=0;
          tw+=wt[j];
        }
        nwt=nwtmax-nmiss;
        //re-normalize the weights:
        for (int32_t j=0; j<nwt; j++) wt[j]=wt[j]/tw;
   
        //printf("trajectory has exited domain\n");
      } 

      tw=0;
      for (int32_t j=0; j<nwt; j++) {
        map.add_el(wt[j], i, ind[j]);
        tw+=wt[j];
        //  printf("[%5d, %5d]; (%2d %7.1f, %7.1f)-(%2d %7.1f, %7.1f): %lg\n",
//		i, ind[j], d0[i], x0[i][0], x0[i][1], domain, result[nfine][0], 
//		result[nfine][1], wt[j]);
      }
//        printf("\n");
      if (tw > 1.000001) {
        fprintf(stderr, "ctraj_tracer: warning: total of weights at row %d exceeds unity:\n", i);
        fprintf(stderr, "              %g", wt[0]);
        for (int32_t j=1; j<nwt; j++) fprintf(stderr, "+%g", wt[j]);
        fprintf(stderr, "=%g\n", tw);
      }
    }

    //write the date and the tracer field to a file:
    map.write(outfun);
    //map.print(stdout);

  }

  if (qflag==0) fclose(outfun);

  //clean up:
  delete [] tind;

  delete tracer;
  delete vfield;

  delete [] x0[0];
  delete [] x0;
  delete [] d0;
  delete [] result[0];
  delete [] result;

  delete [] wt;
  delete [] ind;
}
