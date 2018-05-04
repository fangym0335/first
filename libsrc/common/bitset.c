#include "bitset.h"

#ifndef	CHAR_BIT
#define	CHAR_BIT	8
#endif

#ifndef UCHAR_MAX
#define UCHAR_MAX 0xFF
#endif

BITSET * NewBitSet( int maxBitCnt )
{
	BITSET *	pBitSet;
	size_t		size;

	Assert( maxBitCnt != 0 );

	size = ( maxBitCnt%CHAR_BIT == 0 ) ? ( maxBitCnt/CHAR_BIT ) : ( maxBitCnt/CHAR_BIT + 1 );

	pBitSet = ( BITSET* )Malloc( sizeof(BITSET) );
	if( pBitSet == NULL )
	{
		RptLibErr( errno );
		return NULL;
	}

	pBitSet->bitCnt = maxBitCnt;
	pBitSet->size = size;
	pBitSet->p = ( unsigned char* )Malloc( size );
	if( pBitSet->p == NULL )
	{
		Free( pBitSet );
		RptLibErr( errno );
		return NULL;
	}

	memset( pBitSet->p, 0, size );

	return pBitSet;
}

BITSET *NewBitSetWithValue( const unsigned char *p, size_t len ) /*以实际的串赋值.如"0x010x02"代表00010010*/
{
	BITSET *	pBitSet;
	size_t		size;

	Assert( p != NULL && len != 0 );

	size = len;

	pBitSet = ( BITSET* )Malloc( sizeof(BITSET) );
	if( pBitSet == NULL )
	{
		RptLibErr( errno );
		return NULL;
	}

	pBitSet->bitCnt = len * CHAR_BIT;
	pBitSet->size = size;
	pBitSet->p = ( unsigned char* )Malloc( size );
	if( pBitSet->p == NULL )
	{
		Free( pBitSet );
		RptLibErr( errno );
		return NULL;
	}

	memcpy( pBitSet->p, p, len );	/*p中可能存在'\0',需要用memcpy*/

	return pBitSet;
}

BITSET *NewBitSetWithBinArray( const unsigned char *p, size_t len )	/*以01的字符串赋值,如'0001010'*/
{
	BITSET *	pBitSet;
	size_t		size;
	uint			i;

	Assert( p != NULL && len != 0 );

	size = ( len%CHAR_BIT == 0 ) ? ( len/CHAR_BIT ) : ( len/CHAR_BIT + 1 );

	pBitSet = ( BITSET* )Malloc( sizeof(BITSET) );
	if( pBitSet == NULL )
	{
		RptLibErr( errno );
		return NULL;
	}

	pBitSet->bitCnt = len;
	pBitSet->size = size;
	pBitSet->p = ( unsigned char* )Malloc( size );
	if( pBitSet->p == NULL )
	{
		Free( pBitSet );
		RptLibErr( errno );
		return NULL;
	}

	memset( pBitSet->p, 0, size );

	for( i = 0; i < len; i++ )
		if( p[i] == '1' )
			SetBitOn( pBitSet, i );

	return pBitSet;
}

/*将某位置开.setBit从0开始*/
void SetBitOn( BITSET *pBitSet, int setBit )
{
	int	divisor, remainder;

	divisor = setBit/CHAR_BIT;
	remainder = setBit%CHAR_BIT;

	Assert( pBitSet != NULL && pBitSet->p != NULL );
	Assert( pBitSet->bitCnt > setBit );	/*setBit从0开始.所以小于bitCnt*/

	pBitSet->p[divisor] |= ( 1 << remainder );

	return;
}

/*将某位关.setBit从0开始*/
void SetBitOff( BITSET *pBitSet, int setBit )
{
	int	divisor, remainder;

	divisor = setBit/CHAR_BIT;
	remainder = setBit%CHAR_BIT;

	Assert( pBitSet != NULL && pBitSet->p != NULL );
	Assert( pBitSet->bitCnt > setBit );	/*setBit从0开始.所以小于bitCnt*/

	pBitSet->p[divisor] &= ~( 1 << remainder );

	return;
}

void FillBitSet( BITSET *pBitSet )
{
	Assert( pBitSet != NULL && pBitSet->p != NULL );
	memset( pBitSet->p, UCHAR_MAX, pBitSet->size );
}

void ClearBitSet( BITSET *pBitSet )
{
	Assert( pBitSet != NULL && pBitSet->p != NULL );
	memset( pBitSet->p, 0, pBitSet->size );
}

int IsBitOn( const BITSET *pBitSet, int testBit )
{
	int	divisor, remainder;

	divisor = testBit/CHAR_BIT;
	remainder = testBit%CHAR_BIT;

	Assert( pBitSet != NULL && pBitSet->p != NULL );
	Assert( pBitSet->bitCnt > testBit );	/*testBit从0开始.所以小于bitCnt*/

	if( pBitSet->p[divisor] & ( 1 << remainder ) )
		return TRUE;
	else
		return FALSE;
}

void DelBitSet( BITSET **pBitSet )
{
	if( (*pBitSet) != NULL && (*pBitSet)->p != NULL )
		Free( (*pBitSet)->p );

	if( (*pBitSet) != NULL )
		Free( *pBitSet );

	return;
}

void ShowBitSet( const BITSET *pBitSet )
{
	size_t size;
	int i;
	char *p;

	Assert( pBitSet != NULL && pBitSet->p != NULL );

	size = pBitSet->bitCnt + 1;

	p = (char*)Malloc( size );
	if( p == NULL )
	{
		RptLibErr( errno );
		return;
	}

	for( i = 0; i < pBitSet->bitCnt; i++ )
	{
		if( IsBitOn( pBitSet, i ) )
			p[i] = '1';
		else
			p[i] = '0';
	}
	p[i] = 0;

	DMsg( "BitSet = [%s]", p );

	Free(p);
}

char* DumpBitSetToBinArray( const BITSET *pBitSet, char *p, size_t bufsize )
{
	size_t size;
	int i;

	Assert( pBitSet != NULL && pBitSet->p != NULL );
	Assert( p != NULL );

	size = pBitSet->bitCnt  + 1;
	Assert( size <= bufsize );

	for( i = 0; i < pBitSet->bitCnt; i++ )
	{
		if( IsBitOn( pBitSet, i ) )
			p[i] = '1';
		else
			p[i] = '0';
	}
	p[i] = 0;

	return p;
}

int DumpBitSet( const BITSET *pBitSet, int *bitCnt, char *pBit, size_t bufsize )
{

	Assert( pBitSet != NULL && pBitSet->p != NULL );
	Assert( pBit != NULL );
	Assert( pBitSet->size <= bufsize );

	memcpy( pBit, pBitSet->p, pBitSet->size );
	*bitCnt = pBitSet->bitCnt;

	return pBitSet->size;
}
