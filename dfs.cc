#include <cmath>
#include <assert.h>
#include <cstdio>
#include <vector>
#include <algorithm>

#include "dfs.hh"
#include "util.hh"

unsigned int Dfs::GetNumOfLyrFrms( unsigned int layer ) {
	if ( layer > 1)
		return m_intraperiod>>(m_numlayers-layer+1);
	else
		return m_intraperiod>>(m_numlayers-1);
}

void Dfs::Zigzag( int direction, unsigned int m, unsigned int* mark, std::vector<unsigned int> & order ) {
/* This is a recursive function to add the higher layer frames in the preference order array.
 * Since we have somewhat deviated from this zigzag frame picking method for creating the order of
 * frames, we do not use this. This is just put as future reminder. */

	if ( direction > 0 ) {
		for ( unsigned int i = 0; i < m_intraperiod; i += m )
			if ( mark[i] == 0 ) {
				order.push_back(i);
				mark[i] = 1;
			}
	} else {
		for ( int i = m_intraperiod-m; i >= 0; i -= m )
			if ( mark[i] == 0 ) {
				order.push_back(i);
				mark[i] = 1;
			}
	}

	unsigned int sum = 0;
	for ( unsigned int i = 0; i < m_intraperiod; i++ ) sum += mark[i];
	if ( sum < m_intraperiod )  Zigzag( -1*direction, m/2, mark, order );
}

std::vector<unsigned int> Dfs::CreateOrder( void ) {

	unsigned int* mark = new unsigned int[m_intraperiod]();

	std::vector<unsigned int> order;

	unsigned int dummy[32] = {0,4,8,12,16,20,24,28,30,14,22,6,26,18,10,2,31,15,23,7,3,11,19,27,29,25,21,17,13,9,5,1};

	//~ Zigzag( 1, m_gopsize, mark, order );

	for ( int i = 0; i < 32; i++)
		order.push_back(dummy[i]);

delete mark;
return order;
}

Dfs::Dfs( unsigned int _gopsize, unsigned int _intraperiod, FILE* _pFile_ )
//~ Dfs::Dfs( unsigned int _gopsize, unsigned int _intraperiod )
: m_gopsize( _gopsize )
, m_intraperiod( _intraperiod )
, m_numlayers( FloorLog2( m_gopsize ) + 1 )
, m_order( CreateOrder() )
, m_sent( 0 )
, m_offset( new unsigned int )
, pFile( _pFile_ )
{
	assert( !(m_gopsize & (m_gopsize-1)) && !(m_intraperiod%m_gopsize)  && m_gopsize < m_intraperiod );

	unsigned int i;

	m_frmperlyr  = new unsigned int*[m_numlayers];
	for ( i = 0; i < m_numlayers; i++ )
		m_frmperlyr[i] = new unsigned int[GetNumOfLyrFrms(i+1)]();

	m_lyrperfrm = new unsigned int[m_intraperiod]();

	unsigned int one = 1;

	unsigned int* write_ptr = new unsigned int[m_numlayers]();
	for ( i = 0; i < m_intraperiod; i++ ) {
		for ( int j = m_numlayers-1; j >= 0; j-- ) {
			if ( !(i & ((one<<j)-1)) ) {
				m_lyrperfrm[i] = m_numlayers-j;
				m_frmperlyr[m_numlayers-1-j][write_ptr[j]] = i;
				write_ptr[j]++;
				break;
			}
		}
	}
	delete write_ptr;

	m_est_frsizes = new unsigned int[m_intraperiod]();
	m_ewma_est = new unsigned int[m_numlayers]();
}

Dfs::~Dfs( void ) {
	delete m_est_frsizes;
	delete m_lyrperfrm;
	for ( unsigned int i = 0; i < m_numlayers; i++ ) delete m_frmperlyr[i];
	delete m_frmperlyr;
}

void Dfs::update_size_estimates( const unsigned int size, const unsigned int frameindex ) {

	const unsigned int layer = m_lyrperfrm[frameindex];

	if ( pFile ) fprintf(pFile, "updating size, fr size=%u, curr=%u\n", size, m_ewma_est[layer-1] );

	if ( frameindex > 0 ) {

		m_ewma_est[layer-1] = static_cast<unsigned int>(0.7*size+0.3*m_ewma_est[layer-1]);

		for ( unsigned int j = 0; j < GetNumOfLyrFrms(layer); j++)
			m_est_frsizes[m_frmperlyr[layer-1][j]] = m_ewma_est[layer-1];
	}

	/* PRINT FOR LOG PURPOSES */
	if ( pFile ) {
		fprintf(pFile, "Sizes updated: ");
		for ( unsigned int i = 0; i < m_numlayers; i++ )
			fprintf(pFile, "%u ", m_ewma_est[ i ]);
		fprintf(pFile, "\n");
	}

	m_est_frsizes[frameindex] = size;
}

unsigned int Dfs::update_expected( void ) {

	unsigned int rembw = m_bwbudget;

	m_expected.clear();

	unsigned int count = 0;

	// Estimate the set of frames to be sent
	for ( auto it = m_frameorder.begin(); it != m_frameorder.end(); it++ ) {
		if ( m_est_frsizes[ *it ] <= rembw ) {
			rembw -= m_est_frsizes[ *it ];
			m_expected.push_back( *it );
			count++;
		}
		else break;
	}

	/* PRINT FOR LOG PURPOSES */
	if ( pFile ) {
		fprintf(pFile, "expected = [");
		for ( auto it = m_expected.begin(); it != m_expected.end(); it++ )
			fprintf(pFile,"%u ", *it);
		fprintf(pFile, "]\n");
	}
return count;
}

unsigned int Dfs::offset = 0;

static bool isDropped( const unsigned int & i ) {

return (i <= (Dfs::offset));
}

bool Dfs::permits( unsigned int framesize, unsigned int frameindex ) {

	if ( pFile ) fprintf(pFile, "== Decision for frame %u ==\n", frameindex);

	int willsend = 0;

	if ( m_frameorder.empty() ) { // if there is no valid frame to send
		m_expected.clear(); // we cannot expect to send anything decodable

		if ( pFile ) fprintf(pFile, "No valid frames left!\n");

		return false;
	} else { // if there exist valid frames

		update_size_estimates( framesize, frameindex );
		update_budget( frameindex );
		update_expected(); // BURDA

		if ( m_expected.empty() ) { // we do not expect to send anything
			if ( frameindex == m_frameorder[0] && framesize < m_bwbudget ) {
				willsend = 1; // lucky! the current most important valid frame is affordable!
			} else {
				goto _DROP_CHILDREN;
			}
		} else { // we expect to send something, is this frame among them?
			for ( auto it = m_expected.begin(); it != m_expected.end(); it++ ) {
				if ( *it == frameindex ) {
					if ( pFile ) fprintf(pFile, "Frame sent\n");
					m_sent += framesize; // number of bytes sent is updated
					willsend = 1;
					break;
				}
			}
			if ( willsend == 0 ) { // if not, drop all descendants
_DROP_CHILDREN:
				if ( pFile ) fprintf(pFile,"This and its descendants dropped\n");
				const unsigned int layer = m_lyrperfrm[frameindex]; // first determine its layer
				 //if we drop a base layer frame, all subsequent frames must be dropped as well
				if ( layer == 1 ) m_frameorder.clear();
				else if ( layer != m_numlayers ) { // if we are dropping an enhancement layer frame, drop next frames until "offset"
					(Dfs::offset) = frameindex + (1<<(m_numlayers-layer))-1;
					m_frameorder.erase( std::remove_if(std::begin(m_frameorder), std::end(m_frameorder), isDropped), std::end(m_frameorder) );
				}
			}
		}
	}
	// a decision has been made; remove the current frame from the order of valid frames
	for ( auto itt = m_frameorder.begin(); itt != m_frameorder.end(); itt++ ) {
		if ( *itt == frameindex ) {
			m_frameorder.erase( itt );
			break;
		}
	}
return (willsend == 1);
}