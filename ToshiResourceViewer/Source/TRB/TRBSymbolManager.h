#pragma once
#include "TRBSymbol.h"

#include <Toshi/T2String.h>

namespace TRBSymbolManager
{

// Registers symbol in the list of all possible resources that can be viewed by the TRV
void RegisterSymbol( TRBSymbol* pSymbol );

// Removes symbols from a list of all registered symbols
void UnregisterSymbol( TRBSymbol* pSymbol );

// Looks for this symbol in the list of registered and returns it if found
TRBSymbol* GetSymbol( Toshi::T2ConstString8 strName );

} // namespace TRBSymbolManager
