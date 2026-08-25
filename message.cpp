#include <algorithm>
#include <cmath>

#include "message.h"

//function called Message within a struct called Message within a namespace called Message :)
Message::Message::Message(uint64_t i, uint64_t j, Edge& e)
  : i(i), j(j), e(e), reverse(nullptr)
{
    std::fill(logMu.begin(), logMu.end(), std::log(1.0 / LENGTH));
}
