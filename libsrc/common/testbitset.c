#include "bitset.h"

main()
{

	BITSET * pBitSet;

	InitMemCtl();
	DMsg( "new" );
	pBitSet = NewBitSet( 65 );
	Assert( pBitSet != NULL );
	ShowBitSet( pBitSet );

	DMsg( "fill" );
	FillBitSet( pBitSet );
	ShowBitSet( pBitSet );

	DMsg( "clear" );
	ClearBitSet( pBitSet );
	ShowBitSet( pBitSet );

	DMsg( "set 0" );
	SetBitOn( pBitSet, 0 );
	ShowBitSet( pBitSet );

	DMsg( "set 15" );
	SetBitOn( pBitSet, 15 );
	ShowBitSet( pBitSet );

	DMsg( "set 23" );
	SetBitOn( pBitSet, 23 );
	ShowBitSet( pBitSet );

	DMsg( "set 36" );
	SetBitOn( pBitSet, 36 );
	ShowBitSet( pBitSet );

	DMsg( "set 63" );
	SetBitOn( pBitSet, 63 );
	ShowBitSet( pBitSet );

	DMsg( "set 64" );
	SetBitOn( pBitSet, 64 );
	ShowBitSet( pBitSet );

	DMsg( "OFF" );

	DMsg( "set 0" );
	SetBitOff( pBitSet, 0 );
	ShowBitSet( pBitSet );

	DMsg( "set 15" );
	SetBitOff( pBitSet, 15 );
	ShowBitSet( pBitSet );

	DMsg( "set 23" );
	SetBitOff( pBitSet, 23 );
	ShowBitSet( pBitSet );

	DMsg( "set 36" );
	SetBitOff( pBitSet, 36 );
	ShowBitSet( pBitSet );

	DMsg( "set 63" );
	SetBitOff( pBitSet, 63 );
	ShowBitSet( pBitSet );

	DMsg( "set 64" );
	SetBitOff( pBitSet, 64 );
	ShowBitSet( pBitSet );

 	DelBitSet( &pBitSet );

 	return 0;
}
