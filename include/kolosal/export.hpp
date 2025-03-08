#pragma once

#ifdef KOLOSAL_SERVER_BUILD
#define KOLOSAL_SERVER_API __declspec(dllexport)
#else
#define KOLOSAL_SERVER_API __declspec(dllimport)
#endif