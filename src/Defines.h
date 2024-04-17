#pragma once

#ifdef __DOS__
// Define modern C++ keywords that aren't supported by OpenWatcom
#define override

#ifndef nullptr
#ifdef __cplusplus
#if !defined(_M_I86) || defined(__SMALL__) || defined(__MEDIUM__)
#define nullptr 0
#else
#define nullptr 0L
#endif 
#else
#define nullptr ((void *)0)
#endif
#endif

#endif
