#include "util.hh"
#include <sys/time.h>
#include <cstdlib>
#include <cstdio>

uint64_t GetTimeNow () {

	struct timeval now;
	gettimeofday( &now, nullptr );

	return 1000000*now.tv_sec + now.tv_usec;
}

void errexit( const char* message ) {

	fprintf( stderr, "%s\n", message );
	exit( EXIT_FAILURE );
}

unsigned int FloorLog2( unsigned int n ) {

	unsigned int r = 0; // r will be lg(v)

	while ( n >>= 1 ) r++;

return r; // O(r) complexity
}

double absol( double m ) {

	return (m>=0 ? m : -1*m);
}

Rls::Rls( int _M, double _lambda )
: M( _M )
, lambda( _lambda )
{
	P = new double* [M];
	for ( int i = 0; i < M; i++ ) P[i] = new double[M];
	for ( int i = 0; i < M; i++ )
		for ( int j = 0; j < M; j++ )
			if ( j == i )
				P[i][j] = 1 / delta;
			else
				P[i][j] = 0;

	x = new double[M]();
	w = new double[M]();
}

Rls::~Rls( void ) {
	delete x;
	delete w;
	for ( int i = 0; i < M; i++ ) delete P[i]; // double check later
}

double Lms::update( double xnew ) {

	double e = xnew - xhat; // apriori error
	for ( int p = 0; p <= M; p++ ) w[p] += 2*mu*e*x[p]; // update the filter taps
	for ( int i = M-2; i >= 0; --i) { x[i+1] = x[i]; } x[0] = xnew; // slide the input vector
	xhat = 0;
	for ( int p = 0; p < M; p++ ) xhat += x[p] * w[p]; // update the estimation

return xhat;
}

double Rls::update( double d_new ) {

	double k0[M], k1[M], mu, nu = 0;
	int i,j;

	for ( i = 0; i < M; i++)
		for ( k0[i] = 0, j = 0; j < M; j++)
			 k0[i] += P[i][j] * x[j] / lambda;

	for ( i = 0; i < M; i++ )
		nu += k0[i] * x[i];

	mu = 1 / (1 + nu);

	for ( i = 0; i < M; i++ )
		k1[i] = mu * k0[i];

	for ( i = 0; i < M; i++ )
		 for (j = 0; j <= i; j++ ) {
			P[i][j] = P[i][j] / lambda - k1[i] * k0[j];
			P[j][i] = P[i][j];
		 }

	double e0 = d_new - dhat0; // a priori estimation error

	//printf("w = [ ");
	// update the transversal filter taps
	for ( i = 0; i < M; i++ ) {
		w[i] += e0 * k1[i];
		//printf("%4.2f ",w[i]);
	}
	//printf("]\n");

	// update the x vector
	for ( i = M-2; i >= 0; --i) x[i+1] = x[i];
	x[0] = d_new;

    dhat0 = 0;
	// update the a priori estimation
	for ( i = 0; i < M; i++ )
		dhat0 += w[i] * x[i];

return dhat0;
}

/* update the a priori prediction error estimates */
double Predictor::running_percentile( double num, double den )
{
	if ( den == 0 )
	{
		printf("[RBR] divison by zero @running_percentile\n");
	}
	else
	{	
		double new_value = num / den;
		
		/* push back the new data point */
		if ( errors.size() == RBR_RUNNING_PERC_LEN )
			errors.pop_front();
		errors.push_back( new_value );

		if ( errors.size() == 1 )
		{
			percentile = new_value;
		}
		else
		{	
			double pos = (errors.size() - 1) * RBR_RUNNING_PERC;
			unsigned int ind = (unsigned int)pos;
			double delta = pos - ind;

			std::vector<double> w( errors.size() );
			std::copy(errors.begin(), errors.end(), w.begin());
			std::nth_element(w.begin(), w.begin() + ind, w.end());

			double i1 = *(w.begin() + ind);
			double i2 = *std::min_element(w.begin() + ind + 1, w.end());

			percentile = i1 * (1.0 - delta) + i2 * delta;
		}
	}
return percentile > 1 ? 1 : percentile;
}

double Predictor::update( double new_val ) {

	forecast = rls->update( new_val );
	if ( step++ < RBR_START_RLS ) forecast = new_val;

return forecast > 0 ? forecast : 0;
}
