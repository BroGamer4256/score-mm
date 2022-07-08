#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <windows.h>

// Signature scan in specified memory region
void *sigScan_memory (const char *signature, const char *mask, size_t sigSize,
					  void *memory, size_t memorySize);

// Signature scan in current process
void *sigScan (const char *signature, const char *mask, void *hint = nullptr);

// Automatically scanned signatures, these are expected to exist in all game
// versions sigValid is going to be false if any automatic signature scan fails
extern bool sigValid;

#define SIG_SCAN(x, y, ...)                                                   \
	void *x ();                                                               \
	void *x##Addr = x ();                                                     \
	void *x () {                                                              \
		constexpr const char *x##Data[] = { __VA_ARGS__ };                    \
		constexpr size_t x##Size = _countof (x##Data);                        \
		if (!x##Addr) {                                                       \
			if constexpr (x##Size == 2) {                                     \
				x##Addr = sigScan (x##Data[0], x##Data[1], (void *)(y));      \
				if (x##Addr)                                                  \
					return x##Addr;                                           \
			} else {                                                          \
				for (int i = 0; i < x##Size; i += 2) {                        \
					x##Addr                                                   \
						= sigScan (x##Data[i], x##Data[i + 1], (void *)(y));  \
					if (x##Addr)                                              \
						return x##Addr;                                       \
				}                                                             \
			}                                                                 \
			sigValid = false;                                                 \
		}                                                                     \
		return x##Addr;                                                       \
	}
