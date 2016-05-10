#ifndef DFS_HH
#define DFS_HH

#include <cstdio>
#include <stdio.h>
#include <vector>
#include <cmath>

class Dfs {

private:

	const unsigned int m_gopsize;
	const unsigned int m_intraperiod;
	const unsigned int m_numlayers;
	const std::vector<unsigned int> m_order;

	unsigned int m_bwbudget;
	unsigned int m_totalbw; // new
	unsigned int m_sent;
	std::vector<unsigned int> m_frameorder;
	std::vector<unsigned int> m_expected;

	unsigned int*  m_est_frsizes; // estimated size of each frame in the intraperiod
	unsigned int*  m_lyrperfrm;   // mapping from frame indices to layers
	unsigned int** m_frmperlyr;   // set of frame indices belonging to each layer

	unsigned int* m_ewma_est;

	unsigned int* m_offset;

	FILE* pFile;

public:

	Dfs( unsigned int, unsigned int, FILE* );
	~Dfs( void );

	static unsigned int offset;

	std::vector<unsigned int> CreateOrder( void );
	unsigned int GetNumOfLyrFrms( unsigned int );
	void Zigzag( int, unsigned int, unsigned int*, std::vector<unsigned int>& );
	void update_size_estimates( unsigned int, unsigned int );
	unsigned int update_expected( void );
	bool permits( unsigned int, unsigned int );
	void set_bw_budget( int bw ) { // bw is in bytes (budget for 1 intraperiod)
		static int count = 0;
		m_bwbudget = (unsigned int)bw; // m_bwbudget is in bytes
		m_totalbw = m_bwbudget;
		m_sent = 0;
		if ( pFile ) fprintf(pFile, "###################\nIntraperiod %u - budget set: %u\n", ++count, m_bwbudget);
	};
	void update_budget( unsigned int fr_index ) {
		double temp1 = static_cast<double>(fr_index);
		unsigned int temp = static_cast<unsigned int>(temp1*m_totalbw/m_intraperiod);
		m_bwbudget = m_totalbw - ( m_sent >= temp ? m_sent : temp );

		if ( pFile ) fprintf(pFile, "budget update: %u\n", m_bwbudget);
	}
	void refresh_order( void ) { m_frameorder = m_order; };
};

#endif