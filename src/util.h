#ifndef REDIS_UTIL_H_
#define REDIS_UTIL_H_

#include <string>

std::string Base64(const std::string &data);

bool Debase64(const std::string &data, std::string *decoded);

#endif  // REDIS_UTIL_H_
