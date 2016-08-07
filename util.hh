#ifndef UTIL_HH
#define UTIL_HH

#include <cmath>
#include <algorithm>
#include <list>
#include <stdint.h>

#define RBR_START_RLS 15
#define RBR_RUNNING_PERC_LEN 30
#define RBR_RUNNING_PERC 0.05

uint64_t GetTimeNow( void );

void errexit( const char* );

unsigned int FloorLog2( unsigned int );

double absol( double );

class Rls {

private:

	const int M; // number of past values used in prediction
	double** P; // the infamous P matrix
	double* x; // past capacity values
	double* w; // transversal filter taps
	double lambda; // the forgetting factor
	double dhat0 = 0; 	// a priori estimation
	const double delta = 0.001; // to initialize P

public:

	Rls( int, double );
	~Rls( void );
	double update( double );
};

class Lms {

private:

	const int M;
	const double mu;
	double* x;
	double* w;
	double xhat = 0;

public:

	Lms( int _M_, double _mu_ ) : M( _M_ ), mu( _mu_ )
	{
		x = new double[M]();
		w = new double[M]();
	};

	~Lms( void ) { delete x; delete w; };

	double update( double );

};

class Predictor {

private:

	Rls* rls;
	std::list<double> errors;
	
	int    step 	  = 0;
	double percentile = 1;
	double forecast   = 0;

public:

	Predictor( int _M_, double _lambda_ ) { rls = new Rls( _M_, _lambda_ ); };
	~Predictor() { delete rls; };

	double running_percentile( double, double );
	double update( double );

	double get_lastforecast ( void ) { return forecast; };
};

#endif
