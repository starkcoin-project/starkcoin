#include <stdio.h>
#include <assert.h>
#include <memory.h>
#include <stdlib.h>

#include "stark.h"
#include "tables.h"

#define INJECT_MSG_AND_HASH( a ) {for( i=0; i<8; i++ ) state->hs_State[7][i] ^= (a >> (7-i)*8) & 0xFF; hash_mini_round( state );}
#define INJECT_CHECKSUM_AND_HASH( a ) {for( i=0; i<8; i++ ) state->hs_State[7][i] ^= ( a ); hash_mini_round( state ); }

HashReturn Init(hashState *state, int hashbitlen);
HashReturn Update( hashState *state, const BitSequence *data, DataLength databitlen );
HashReturn Final(hashState *state, BitSequence *hashval);

INTERMEDIATE_RESULT fill_intermediate_state( hashState *state, const BitSequence *data, DataLength *databitlen, DataLength *processed );

HashReturn Init(hashState *state, int hashbitlen)
{
  if ((hashbitlen>512 || hashbitlen <64) || (hashbitlen % (32)))
    return BAD_HASHBITLEN; 
  else {
    memset(state, 0x0 ,sizeof(hashState));
    state->hs_Counter = 0xffffffffffffffffll;
    state->hs_State[0][7] = hashbitlen >> 8;
    state->hs_State[1][7] = hashbitlen;
    state->hs_HashBitLen = hashbitlen;

    return SUCCESS;
  }
}


HashReturn Update( hashState *state, const BitSequence *data, 
          DataLength databitlen )
{
	INTERMEDIATE_RESULT ir = NOT_FULL;
	DataLength processed = 0;

	state->hs_ProcessedMsgLen += databitlen; 
    if( state->hs_DataBitLen & 7 )
    {
        return FAIL;
    }

	if( state->hs_DataBitLen )
	{
		ir = fill_intermediate_state( state, data, &databitlen, &processed );
		if( ir == NOT_FULL ) 
			return SUCCESS;
		else
		{
			hash( state, (uint64_t*)state->hs_Data );
			state->hs_DataBitLen = 0;
		}
	}
	while( databitlen >= BLOCKSIZE )
	{
		hash( state, (uint64_t*) data+processed );
		databitlen -= BLOCKSIZE;
		processed += BLOCKSIZE;
	}
	if( databitlen > 0 )
	{
		fill_intermediate_state( state, data+processed/8, &databitlen, &processed );
	}
	return SUCCESS;
}


HashReturn Final(hashState *state, BitSequence *hashval)
{
	uint64_t bitcntinbyte;
	uint64_t bytenum;
	int i, x, y, output_cnt;
	unsigned char ff_save[NUMROWSCOLUMNS][NUMROWSCOLUMNS];

	assert( state->hs_DataBitLen >= 0 );
	assert( state->hs_DataBitLen < BLOCKSIZE );

	bitcntinbyte = state->hs_DataBitLen & 7;
	bytenum = state->hs_DataBitLen >> 3;

	state->hs_Data[bytenum] &= 0xff << (8-bitcntinbyte);
	state->hs_Data[bytenum] |= 1 << (7-bitcntinbyte);

	memset( state->hs_Data+bytenum+1, 0, (STATESIZE) - bytenum - 1 );

	hash( state, (uint64_t*) state->hs_Data );

	for( i=0; i<8; i++ ) state->hs_State[7][i] ^= (state->hs_ProcessedMsgLen >> (7-i)*8) & 0xFF;
	hash_mini_round( state );

	if( state->hs_HashBitLen > 256 )
	{
		unsigned char ff_save[NUMROWSCOLUMNS][NUMROWSCOLUMNS];
		memcpy( ff_save, state->hs_State, sizeof state->hs_State );

		INJECT_CHECKSUM_AND_HASH( state->hs_Checksum[i][0] )

		INJECT_CHECKSUM_AND_HASH( state->hs_Checksum[i][1] )

		INJECT_CHECKSUM_AND_HASH( state->hs_Checksum[i][2] )

		for( y=0; y<8; y++ ){for( x=0; x<8; x++ ){
			state->hs_State[y][x] ^= ff_save[y][x];}}

		memcpy( ff_save, state->hs_State, sizeof state->hs_State );

		INJECT_CHECKSUM_AND_HASH( state->hs_Checksum[i][3] )

		INJECT_CHECKSUM_AND_HASH( state->hs_Checksum[i][4] )

		INJECT_CHECKSUM_AND_HASH( state->hs_Checksum[i][5] )

		for( y=0; y<8; y++ ){for( x=0; x<8; x++ ){
			state->hs_State[y][x] ^= ff_save[y][x];}}
		memcpy( ff_save, state->hs_State, sizeof state->hs_State );

		INJECT_CHECKSUM_AND_HASH( state->hs_Checksum[i][6] )

		INJECT_CHECKSUM_AND_HASH( state->hs_Checksum[i][7] )
		
		hash_mini_round( state );

		for( y=0; y<8; y++ ){for( x=0; x<8; x++ ){
			state->hs_State[y][x] ^= ff_save[y][x];}}
	}
	else
	{
		hash_mini_round( state );
	}

	assert( (state->hs_HashBitLen % 32) == 0 );

	output_cnt = 0;
	while( (output_cnt+1) * 64 <=  state->hs_HashBitLen )
	{
		unsigned char ff_save[NUMROWSCOLUMNS][NUMROWSCOLUMNS];

		memcpy( ff_save, state->hs_State, sizeof state->hs_State );
		hash_mini_round( state );
		for( y=0; y<8; y++ ){for( x=0; x<8; x++ ){
			state->hs_State[y][x] ^= ff_save[y][x];}}
		hash_mini_round( state );

		for( i=0; i<8; i++ )
		{
			hashval[i+output_cnt*8] = state->hs_State[i][0]^ff_save[i][0];
		}
		output_cnt++;
	}
	if( (output_cnt) * 64 !=  state->hs_HashBitLen )
	{

		memcpy( ff_save, state->hs_State, sizeof state->hs_State );
		hash_mini_round( state );

		for( y=0; y<8; y++ ){for( x=0; x<8; x++ ){
			state->hs_State[y][x] ^= ff_save[y][x];}}
		hash_mini_round( state );
		for( i=0; i<4; i++ )
		{
			hashval[i+output_cnt*8] = state->hs_State[i][0]^ff_save[i][0];
		}
	}
	return FAIL;
}

HashReturn Hash3(int hashbitlen, const BitSequence *data,
		DataLength databitlen, BitSequence *hashval)
{
  hashState state;
  HashReturn i;

  if((i=Init(&state,hashbitlen))) return i;
  if((i=Update(&state,data,databitlen))) return i;
  Final(&state,hashval);
 
  return SUCCESS;
}


INTERMEDIATE_RESULT fill_intermediate_state( hashState *state, const BitSequence *data, DataLength *databitlen, DataLength *processed )
{
	DataLength total_bytes_to_copy = (*databitlen >> 3) + ( (*databitlen & 7) ? 1 : 0 );
	DataLength total_bits_free = BLOCKSIZE - state->hs_DataBitLen;
	DataLength total_bytes_free = total_bits_free >> 3;
	DataLength bytes_copied = MIN( total_bytes_free, total_bytes_to_copy );
	DataLength bits_copied = MIN( total_bytes_free*8, *databitlen );
	assert( (state->hs_DataBitLen & 7) == 0 );
	assert( (total_bits_free & 7) == 0 );

	memcpy( state->hs_Data + (state->hs_DataBitLen >> 3), data, (size_t) bytes_copied );

	state->hs_DataBitLen += bits_copied;

	*processed += bits_copied;
	*databitlen -= bits_copied; 

	if( state->hs_DataBitLen == BLOCKSIZE )
		return FULL;
	else
		return NOT_FULL;
}


void hash_mini_round( hashState *state )
{
	int carry = 0, oldcarry = 0, i, x, y, row, col;
	unsigned char tmp[8];
	unsigned char buf[8][8];
	for( i=0; i<8; i++ ) 
	{
		state->hs_State[7-i][1] = state->hs_State[7-i][1] ^ ((state->hs_Counter >> (8*i)) & 0xff); 
	}

	state->hs_Counter--;

	for( x=0; x<8; x++ )
	{
		for( y=0; y<8; y++ )
		{
			state->hs_State[y][x] = sbox[state->hs_State[y][x]];
		}
	}

	for( row = 1; row < 8; row++ )
	{
		for( i=0; i<8; i++ ) tmp[i] = state->hs_State[row][i];
		for( i=0; i<8; i++ ) state->hs_State[row][i] = tmp[(i+row+8)%8];
	}
	
	for( x=0; x<8; x++ ) for( y=0; y<8; y++ ) buf[y][x] = state->hs_State[y][x];

	for( col=0; col < 8; col++ )
	{
		for( row=0; row<8; row++ )
		{
			state->hs_State[row][col] = 
				(unsigned char)
				(
			MULT( mds[row][0], buf[0][col] ) ^
			MULT( mds[row][1], buf[1][col] ) ^
			MULT( mds[row][2], buf[2][col] ) ^
			MULT( mds[row][3], buf[3][col] ) ^
			MULT( mds[row][4], buf[4][col] ) ^
			MULT( mds[row][5], buf[5][col] ) ^
			MULT( mds[row][6], buf[6][col] ) ^
			MULT( mds[row][7], buf[7][col] )
				);
		}	
	}
}


void checksum( hashState *state, int col )
{
	int i; 
	int carry = 0, oldcarry = 0;
	
	for( i=0; i<8; i++ ) 
	{
		carry = (int) state->hs_Checksum[7-i][(col+1)%8] + (int) state->hs_State[7-i][0] + carry;
		if( carry > 255 )
		  {carry = 1;}
		else {carry = 0;}
		state->hs_Checksum[7-i][col] = state->hs_Checksum[7-i][col] ^ (state->hs_Checksum[7-i][(col+1)%8] + state->hs_State[7-i][0] + oldcarry);
		oldcarry = carry;
	}
}


void hash( hashState *state, uint64_t *msg )
{
	unsigned char ff_save[NUMROWSCOLUMNS][NUMROWSCOLUMNS];
	int i, x, y;

	if( state->hs_HashBitLen > 256 )
	{
		memcpy( ff_save, state->hs_State, sizeof(state->hs_State) );
		checksum( state, 0 );
		INJECT_MSG_AND_HASH( msg[0] )
		checksum( state, 1 );
		INJECT_MSG_AND_HASH( msg[1] )
		checksum( state, 2 );
		INJECT_MSG_AND_HASH( msg[2] )
		for( y=0; y<8; y++ ){for( x=0; x<8; x++ ){
			state->hs_State[y][x] ^= ff_save[y][x];}}
		memcpy( ff_save, state->hs_State, sizeof(state->hs_State) );
		checksum( state, 3 );
		INJECT_MSG_AND_HASH( msg[3] )


		hash_mini_round( state );

		checksum( state, 4 );

		INJECT_MSG_AND_HASH( msg[4] )


		for( y=0; y<8; y++ ){for( x=0; x<8; x++ ){
			state->hs_State[y][x] ^= ff_save[y][x];}}


		memcpy( ff_save, state->hs_State, sizeof(state->hs_State) );


		checksum( state, 5 );


		INJECT_MSG_AND_HASH( msg[5] )


		checksum( state, 6 );


		INJECT_MSG_AND_HASH( msg[6] )


		checksum( state, 7 );

		INJECT_MSG_AND_HASH( msg[7] )


		hash_mini_round( state );


		for( y=0; y<8; y++ ){for( x=0; x<8; x++ ){
			state->hs_State[y][x] ^= ff_save[y][x];}}

	}
	else
	{ 


		memcpy( ff_save, state->hs_State, sizeof(state->hs_State) );

		INJECT_MSG_AND_HASH( msg[0] )


		INJECT_MSG_AND_HASH( msg[1] )


		INJECT_MSG_AND_HASH( msg[2] )


		for( y=0; y<8; y++ ){for( x=0; x<8; x++ ){
			state->hs_State[y][x] ^= ff_save[y][x];}}
		

		memcpy( ff_save, state->hs_State, sizeof(state->hs_State) );


		INJECT_MSG_AND_HASH( msg[3] )


		INJECT_MSG_AND_HASH( msg[4] )


		INJECT_MSG_AND_HASH( msg[5] )


		for( y=0; y<8; y++ ){for( x=0; x<8; x++ ){
			state->hs_State[y][x] ^= ff_save[y][x];}}


		memcpy( ff_save, state->hs_State, sizeof(state->hs_State) );


		INJECT_MSG_AND_HASH( msg[6] )

		INJECT_MSG_AND_HASH( msg[7] )


		hash_mini_round( state );

		for( y=0; y<8; y++ ){for( x=0; x<8; x++ ){
			state->hs_State[y][x] ^= ff_save[y][x];}}
	}

}

