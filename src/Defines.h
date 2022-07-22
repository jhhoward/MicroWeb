#pragma once

#ifdef __DOS__
// Define modern C++ keywords that aren't supported by OpenWatcom
#define override
#define nullptr NULL
#endif
