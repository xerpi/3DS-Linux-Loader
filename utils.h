#pragma once

void InvalidateEntireInstructionCache(void);
void InvalidateEntireDataCache(void);

int svcBackdoor(int (*func)(void));

