#ifndef VIRTUAL_STRIP_DEMOS_H
#define VIRTUAL_STRIP_DEMOS_H

class VirtualStripManager;

void setupVirtualStripDemo(int stripIndex, VirtualStripManager* stripManager, const int* numLeds, bool* basicActive);
void setupOverlappingDemo(int stripIndex, VirtualStripManager* stripManager, const int* numLeds, bool* basicActive);
void setupFourStripDemo(VirtualStripManager* stripManager, const int* numLeds, bool* basicActive, int numStrips);

#endif // VIRTUAL_STRIP_DEMOS_H
