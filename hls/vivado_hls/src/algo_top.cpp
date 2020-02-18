#include "algo_top_parameters.h"
#include "algo_top.h"
#include <algorithm>
#include <utility>

#include "TowerMaker.h"

using namespace std;
using namespace algo;

// Each input link carries the information of a 5x5 region
// Last 16-bits are reserved for CRC
void processInputData(hls::stream<axiword> &link, ap_uint<64> words[N_WORDS_PER_FRAME])
{
//#pragma HLS INLINE
#pragma HLS PIPELINE II=6
#pragma HLS INTERFACE axis port=link
  for (size_t i = 0; i < N_WORDS_PER_FRAME; i++)
  {
#pragma HLS unroll	   //K
//#pragma HLS PIPELINE II=1 //K
//#pragma HLS PIPELINE II=6 //K

	  #ifndef __SYNTHESIS__
    // Avoid simulation warnings
    if (link.empty()) continue;
#endif

    words[i] = link.read().data;
  }
}

void processOutputData(hls::stream<axiword> &link, ap_uint<64> words[N_WORDS_PER_FRAME])
{
//#pragma HLS INLINE

#pragma HLS PIPELINE II=6	 //K
#pragma HLS INTERFACE axis port=link
  for (size_t i = 0; i < N_OUTPUT_WORDS_PER_FRAME; i++)
  	  	{

			#pragma HLS unroll	  	  //K
				axiword r;
				r.user = 0;
				if (i == (N_OUTPUT_WORDS_PER_FRAME - 1))
				{
				  r.last = 0;
				}
				else
				{
				  r.last = 1;
				}
				r.data = words[i];
				link.write(r);
			 }
}



void makeCrystals(ap_uint<64> words[N_WORDS_PER_FRAME], Crystal crystals[5][5])
{

		//#pragma HLS PIPELINE
		#pragma HLS PIPELINE II=3 //K
		#pragma HLS ARRAY_PARTITION variable=words
		#pragma HLS ARRAY_PARTITION variable=crystals

		  crystals[0][0] = Crystal(words[0].range(13,  0));
		  crystals[0][1] = Crystal(words[0].range(27, 14));
		  crystals[0][2] = Crystal(words[0].range(41, 28));
		  crystals[0][3] = Crystal(words[0].range(55, 42));
		  ap_uint<8> leftover0 = words[0].range(63, 56);
		  crystals[0][4] = Crystal(words[1].range(13,  0));
		  crystals[1][0] = Crystal(words[1].range(27, 14));
		  crystals[1][1] = Crystal(words[1].range(41, 28));
		  crystals[1][2] = Crystal(words[1].range(55, 42));
		  ap_uint<8> leftover1 = words[1].range(63, 56);
		  crystals[1][3] = Crystal(words[2].range(13,  0));
		  crystals[1][4] = Crystal(words[2].range(27, 14));
		  crystals[2][0] = Crystal(words[2].range(41, 28));
		  crystals[2][1] = Crystal(words[2].range(55, 42));
		  ap_uint<8> leftover2 = words[2].range(63, 56);
		  crystals[2][2] = Crystal(words[3].range(13,  0));
		  crystals[2][3] = Crystal(words[3].range(27, 14));
		  crystals[2][4] = Crystal(words[3].range(41, 28));
		  crystals[3][0] = Crystal(words[3].range(55, 42));
		  ap_uint<8> leftover3 = words[3].range(63, 56);
		  crystals[3][1] = Crystal(words[4].range(13,  0));
		  crystals[3][2] = Crystal(words[4].range(27, 14));
		  crystals[3][3] = Crystal(words[4].range(41, 28));
		  crystals[3][4] = Crystal(words[4].range(55, 42));
		  ap_uint<8> leftover4 = words[4].range(63, 56);
		  crystals[4][0] = Crystal(words[5].range(13,  0));
		  crystals[4][1] = Crystal(words[5].range(27, 14));
		  crystals[4][2] = Crystal(words[5].range(41, 28));
		  crystals[4][3] = Crystal(words[5].range(55, 42));
		  ap_uint<8> leftover5 = words[5].range(63, 56);
		  ap_uint<14> crystalData44 = (((ap_uint<14>) (leftover1.range(5, 0) & 0x3F)) << 8)  | ((ap_uint<14>) leftover0);
		  crystals[4][4] = Crystal(crystalData44);
		  ap_uint<32> crc = (((ap_uint<32>) leftover5) << 24) | (((ap_uint<32>) leftover4) << 16) | (((ap_uint<32>) leftover3) << 8) | ((ap_uint<32>) leftover2);
}




void algo_top(hls::stream<axiword> link_in[N_INPUT_LINKS], hls::stream<axiword> link_out[N_OUTPUT_LINKS])
{
	#pragma HLS INTERFACE axis port=link_in
	#pragma HLS INTERFACE axis port=link_out

	#pragma HLS PIPELINE II=13 //k

//-----------------------------------------------------------//
// Step First: Unpack
	ap_uint<64> input[N_INPUT_LINKS][N_WORDS_PER_FRAME];
	#pragma HLS ARRAY_PARTITION variable=input
	for (size_t i = 0; i < N_INPUT_LINKS; i++)
	{
		#pragma HLS unroll //k

    processInputData(link_in[i], input[i]);
	}
//-----------------------------------------------------------//

  // Tower loop - operations not using tower pairs
  Tower towerLevelECALSummary[N_INPUT_LINKS];
  #pragma HLS ARRAY_PARTITION variable=towerLevelECALSummary

  	  Crystal crystals[N_INPUT_LINKS][5][5];
  		#pragma HLS ARRAY_PARTITION variable=crystals


  for (size_t i = 0; i < N_INPUT_LINKS; i++) //LOOP 1 in analysis persp
  	  {
			#pragma HLS unroll


			 //Crystal crystals[5][5];
			//Crystal crystals[5][5];
			//#pragma HLS ARRAY_PARTITION variable=crystals


			// Make crystals
			makeCrystals(input[i], crystals[i]);
				// Make towers
			makeTower(crystals[i], towerLevelECALSummary[i]);
  	  }

  // Compute total ECAL ET

  ap_uint<32> ecalSummary = makeECALSummary(towerLevelECALSummary);

  // Stitch neighboring towers
  
  	  Tower stitchedTowerLevelECALSummary[N_INPUT_LINKS];
	  #pragma HLS ARRAY_PARTITION variable=stitchedTowerLevelECALSummary
  	  stitchAllNeighbors(towerLevelECALSummary, stitchedTowerLevelECALSummary);


  // Pack the outputs

      ap_uint<64> output[N_OUTPUT_LINKS][N_WORDS_PER_FRAME];
      #pragma HLS ARRAY_PARTITION variable=output
      size_t l = 0;
      ap_uint<64> ecalData = ecalSummary + (ecalSummary << 32);
      for (size_t i = 0; i < N_OUTPUT_LINKS; i++)
      	  {
			#pragma HLS unroll //k

    	  	  for (size_t j = 0; j < N_OUTPUT_WORDS_PER_FRAME; j++)
    	  	  {
				#pragma HLS unroll //k
    	  		  if (l <= N_INPUT_LINKS - 2)
    	  		    {
						ap_uint<64> towerData1 = uint32_t(stitchedTowerLevelECALSummary[l]);
						ap_uint<64> towerData2 = uint32_t(stitchedTowerLevelECALSummary[l+1]);
						ap_uint<64> towerData = towerData1 | (towerData2 << 32);
						output[i][j] = towerData;
						l += 2;
					  }

    	  		 else {
    	  			 	 output[i][j] = ecalData;
    	  		 	   }
    	  	  	  }
      	  	  }

  // Step Last: Write the output

	  for (size_t i = 0; i < N_OUTPUT_LINKS; i++)
	  	 {
		  //#pragma HLS PIPELINE
		  #pragma HLS PIPELINE II=1

		  processOutputData(link_out[i], output[i]);
	  	  }

}
