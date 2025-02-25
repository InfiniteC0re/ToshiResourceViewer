#include "pch.h"
#include <Toshi/Toshi.h>
#include <ToshiTools/T2CommandLine.h>

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

int main( int argc, char** argv )
{
	// Allocate memory for the allocator
	TMemory::Initialise( 8 * 1024 * 1024, 0 );

	// Initialise engine
	TUtil::TOSHIParams engineParams;
	engineParams.szCommandLine = GetCommandLineA();

	TUtil::ToshiCreate( engineParams );

	// Create window here...

	//TUtil::ToshiDestroy();

	return 0;
}
